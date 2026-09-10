[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$CandidateRoot,
    [ValidateSet('MSBuild', 'CMake')]
    [string]$BuildSystem = 'MSBuild',
    [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [ValidateRange(1, 2)]
    [int]$ReproBuilds = 2,
    [string]$ReplicaRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'source_manifest.ps1')

function Get-RequiredMatch {
    param([string]$Path, [string]$Pattern, [string]$Name)
    $text = [IO.File]::ReadAllText($Path)
    $match = [regex]::Match($text, $Pattern)
    if (-not $match.Success) { throw "Could not read $Name from $Path" }
    return $match.Groups[1].Value
}

function Get-VisualStudioPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'vswhere.exe is required for the authoritative Windows build'
    }
    $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($path)) {
        throw 'No Visual Studio installation with MSBuild was found'
    }
    return $path.Trim()
}

function Invoke-Captured {
    param([string]$FilePath, [string[]]$Arguments, [string]$WorkingDirectory)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $FilePath
    $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { [void]$start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        StandardOutput = $stdoutTask.Result
        StandardError = $stderrTask.Result
        Text = $stdoutTask.Result + $stderrTask.Result
    }
}

function Invoke-Logged {
    param([string]$FilePath, [string[]]$Arguments, [string]$WorkingDirectory, [string]$LogPath)
    $result = Invoke-Captured -FilePath $FilePath -Arguments $Arguments `
        -WorkingDirectory $WorkingDirectory
    Write-BytesIfChanged -Path $LogPath `
        -Bytes ([Text.UTF8Encoding]::new($false).GetBytes($result.Text))
    if ($result.ExitCode -ne 0) {
        throw "$FilePath failed with exit code $($result.ExitCode); see $LogPath"
    }
}

function Assert-OutsideSourceRoot {
    param([string]$Path, [string]$LiveRoot, [string]$Name)
    $relative = [IO.Path]::GetRelativePath($LiveRoot, $Path).Replace('\', '/')
    if ($relative -eq '.' -or (-not $relative.StartsWith('../', [StringComparison]::Ordinal))) {
        throw "$Name must be outside the live source repository: $Path"
    }
}

function Assert-NewDirectory {
    param([string]$Path, [string]$Name)
    if ([IO.Directory]::Exists($Path) -or [IO.File]::Exists($Path)) {
        throw "$Name must not already exist: $Path"
    }
}

function Assert-FrozenSource {
    param([string]$FrozenRoot, [byte[]]$ExpectedManifest, [string]$Stage)
    $actual = Get-SourceManifest -SourceRoot $FrozenRoot -Kind Tree
    if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$ExpectedManifest, [byte[]]$actual.Bytes)) {
        throw "Frozen-source guard failed at $Stage"
    }
}

function Copy-ManifestTree {
    param([string]$FromRoot, [string]$ToRoot, [string[]]$Paths)
    [IO.Directory]::CreateDirectory($ToRoot) | Out-Null
    foreach ($relative in $Paths) {
        $native = $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $source = [IO.Path]::Combine($FromRoot, $native)
        $destination = [IO.Path]::Combine($ToRoot, $native)
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy($source, $destination, $false)
    }
}

function Protect-FrozenFiles {
    param([string]$Root)
    foreach ($file in Get-ChildItem -LiteralPath $Root -File -Recurse -Force) {
        $file.IsReadOnly = $true
    }
}

function Get-ActualToolchainIdentity {
    param([string]$Msbuild, [string]$FrozenRoot)
    $project = Join-Path $FrozenRoot 'Sunrise\Sunrise.vcxproj'
    $result = Invoke-Captured -FilePath $Msbuild -WorkingDirectory $FrozenRoot -Arguments @(
        $project, '/nologo', "/p:Configuration=$Configuration", "/p:Platform=$Platform",
        '-getProperty:PlatformToolset,VCToolsVersion,VCToolsInstallDir,WindowsTargetPlatformVersion,WindowsSdkDir')
    if ($result.ExitCode -ne 0) { throw "MSBuild toolchain evaluation failed: $($result.Text)" }
    $evaluated = $result.StandardOutput | ConvertFrom-Json
    $properties = $evaluated.Properties
    foreach ($name in @('PlatformToolset', 'VCToolsVersion', 'VCToolsInstallDir',
                         'WindowsTargetPlatformVersion', 'WindowsSdkDir')) {
        if ([string]::IsNullOrWhiteSpace($properties.$name)) {
            throw "Evaluated MSBuild toolchain property is empty: $name"
        }
    }
    $compilerPath = Join-Path $properties.VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
        throw "Evaluated compiler does not exist: $compilerPath"
    }
    return [pscustomobject]@{
        Compiler = "MSVC-$((Get-Item -LiteralPath $compilerPath).VersionInfo.FileVersion)"
        Toolset = "$($properties.PlatformToolset)-$($properties.VCToolsVersion)"
        PlatformToolset = $properties.PlatformToolset
        VCToolsVersion = $properties.VCToolsVersion
        WindowsSdk = $properties.WindowsTargetPlatformVersion
        CompilerPath = $compilerPath
    }
}

function Assert-NoEmbeddedRoots {
    param([string]$BinaryPath, [string[]]$Roots)
    $bytes = [IO.File]::ReadAllBytes($BinaryPath)
    $ascii = [Text.Encoding]::UTF8.GetString($bytes)
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    foreach ($root in $Roots) {
        foreach ($value in @($root.TrimEnd('\', '/'),
                             $root.TrimEnd('\', '/').Replace('\', '/'))) {
            if ($ascii.Contains($value, [StringComparison]::OrdinalIgnoreCase) -or
                $unicode.Contains($value, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Binary embeds an absolute candidate path: $value in $BinaryPath"
            }
        }
    }
}

function Build-FrozenRoot {
    param(
        [string]$Root,
        [byte[]]$ExpectedManifest,
        [int]$Count,
        [string]$LabelPrefix,
        $Toolchain,
        [string]$Msbuild,
        [string]$Cmake
    )
    $frozen = Join-Path $Root 'source'
    $evidence = Join-Path $Root 'evidence'
    [IO.Directory]::CreateDirectory($evidence) | Out-Null
    $outputs = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $Count; ++$index) {
        $label = if ($Count -eq 1 -and $LabelPrefix) { $LabelPrefix } `
                 else { [string][char]([int][char]'a' + $index) }
        Assert-FrozenSource -FrozenRoot $frozen -ExpectedManifest $ExpectedManifest `
            -Stage "pre-build-$label"
        $buildRelative = "build\$BuildSystem\$label"
        $outputRelative = "out\$BuildSystem\$label"
        $buildRoot = Join-Path $Root $buildRelative
        $outputRoot = Join-Path $Root $outputRelative
        $logPath = Join-Path $evidence "build-$BuildSystem-$label.log"
        [IO.Directory]::CreateDirectory($buildRoot) | Out-Null
        [IO.Directory]::CreateDirectory($outputRoot) | Out-Null
        if ($BuildSystem -eq 'MSBuild') {
            $binaryLogRelative = "evidence\build-MSBuild-$label.binlog"
            $projectRelative = if (Test-Path -LiteralPath (Join-Path $Root 'source\Sunrise.sln')) {
                'source\Sunrise.sln'
            } else {
                'source\Sunrise\Sunrise.vcxproj'
            }
            Invoke-Logged -FilePath $Msbuild -WorkingDirectory $Root -LogPath $logPath -Arguments @(
                $projectRelative, '/m', '/nologo', '/v:minimal', '/t:Rebuild',
                "/bl:$binaryLogRelative", "/p:Configuration=$Configuration", "/p:Platform=$Platform",
                '/p:SunriseGeneratedIncludeDir=..\..\generated',
                '/p:SunriseReproRoot=..\..',
                "/p:OutDir=..\..\$outputRelative\", "/p:IntDir=..\..\$buildRelative\")
            $dllPath = Join-Path $outputRoot 'steam_api64.dll'
        } else {
            $configureLog = Join-Path $evidence "configure-CMake-$label.log"
            Invoke-Logged -FilePath $Cmake -WorkingDirectory $Root -LogPath $configureLog -Arguments @(
                '-S', 'source', '-B', $buildRelative, '-A', 'x64',
                '-T', "$($Toolchain.PlatformToolset),version=$($Toolchain.VCToolsVersion)",
                '-DSUNRISE_GENERATED_INCLUDE_DIR=generated',
                '-DSUNRISE_REPRO_ROOT=.',
                "-DSUNRISE_OUTPUT_ROOT=$outputRelative", '-DBUILD_TESTING=OFF')
            Invoke-Logged -FilePath $Cmake -WorkingDirectory $Root -LogPath $logPath -Arguments @(
                '--build', $buildRelative, '--config', $Configuration,
                '--target', 'steam_api64', '--clean-first')
            $dllPath = Join-Path $outputRoot "$Configuration\steam_api64.dll"
        }
        Assert-FrozenSource -FrozenRoot $frozen -ExpectedManifest $ExpectedManifest `
            -Stage "post-build-$label"
        if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
            throw "Build did not produce $dllPath"
        }
        $outputs.Add([pscustomobject]@{
            Label = $label
            DllPath = $dllPath
            DllSha256 = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash.ToUpperInvariant()
        })
    }
    return $outputs.ToArray()
}

$liveRoot = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\', '/')
$candidate = [IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\', '/')
Assert-NewDirectory -Path $candidate -Name CandidateRoot
Assert-OutsideSourceRoot -Path $candidate -LiveRoot $liveRoot -Name CandidateRoot
$replica = $null
if (-not [string]::IsNullOrWhiteSpace($ReplicaRoot)) {
    $replica = [IO.Path]::GetFullPath($ReplicaRoot).TrimEnd('\', '/')
    Assert-NewDirectory -Path $replica -Name ReplicaRoot
    Assert-OutsideSourceRoot -Path $replica -LiveRoot $liveRoot -Name ReplicaRoot
    if ($replica -ceq $candidate) { throw 'ReplicaRoot must differ from CandidateRoot' }
}

# Capture Git metadata/status/content before creating any candidate state, and prove the same tuple
# still describes the live worktree after the exact file copy.
$captureBefore = Get-GitCaptureState -RepositoryRoot $liveRoot
[IO.Directory]::CreateDirectory($candidate) | Out-Null
$frozenRoot = Join-Path $candidate 'source'
$generatedRoot = Join-Path $candidate 'generated'
$evidenceRoot = Join-Path $candidate 'evidence'
Copy-ManifestTree -FromRoot $liveRoot -ToRoot $frozenRoot -Paths $captureBefore.Manifest.Paths
[IO.Directory]::CreateDirectory($generatedRoot) | Out-Null
[IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$captureAfter = Get-GitCaptureState -RepositoryRoot $liveRoot
if (-not (Test-GitCaptureStateEqual -Left $captureBefore -Right $captureAfter)) {
    throw 'Live Git HEAD/branch/status/content changed while freezing the candidate'
}
$frozenManifest = Get-SourceManifest -SourceRoot $frozenRoot -Kind Tree
if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$captureBefore.Manifest.Bytes,
                                          [byte[]]$frozenManifest.Bytes)) {
    throw 'Frozen copy does not match the coherently captured Git overlay manifest'
}
Write-BytesIfChanged -Path (Join-Path $evidenceRoot 'source-manifest.txt') `
    -Bytes $frozenManifest.Bytes

$cacheFormat = [uint32](Get-RequiredMatch `
    -Path (Join-Path $frozenRoot 'Sunrise\src\state\build_data\cache\records\version.h') `
    -Pattern 'kCacheFormatVersion\s*=\s*(\d+)' -Name 'cache format')
$settingsVersion = [uint32](Get-RequiredMatch `
    -Path (Join-Path $frozenRoot 'Sunrise\src\core\settings\settings.h') `
    -Pattern 'kSettingsVersion\s*=\s*(\d+)' -Name 'settings version')

$visualStudio = Get-VisualStudioPath
$msbuild = Join-Path $visualStudio 'MSBuild\Current\Bin\MSBuild.exe'
$cmake = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) { throw "Missing MSBuild: $msbuild" }
if ($BuildSystem -eq 'CMake' -and -not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw "Missing Visual Studio CMake: $cmake"
}
$toolchain = Get-ActualToolchainIdentity -Msbuild $msbuild -FrozenRoot $frozenRoot
$identity = New-BuildIdentityHeader -SourceSha256 $frozenManifest.SourceSha256 `
    -Head $captureBefore.Head -Branch $captureBefore.Branch -Dirty $captureBefore.Dirty `
    -BuildConfiguration $Configuration -BuildPlatform $Platform -Compiler $toolchain.Compiler `
    -Toolset $toolchain.Toolset -Sdk $toolchain.WindowsSdk -Format $cacheFormat `
    -Settings $settingsVersion
$generatedHeader = Join-Path $generatedRoot 'sunrise_build_identity.generated.h'
Write-BytesIfChanged -Path $generatedHeader -Bytes $identity.Bytes
Protect-FrozenFiles -Root $frozenRoot
(Get-Item -LiteralPath $generatedHeader).IsReadOnly = $true

$outputs = @(Build-FrozenRoot -Root $candidate -ExpectedManifest $frozenManifest.Bytes `
    -Count $ReproBuilds -LabelPrefix '' -Toolchain $toolchain -Msbuild $msbuild -Cmake $cmake)
if ($outputs.Count -eq 2 -and $outputs[0].DllSha256 -cne $outputs[1].DllSha256) {
    throw "Same-root reproducibility mismatch: $($outputs[0].DllSha256) != $($outputs[1].DllSha256)"
}

$allOutputs = [Collections.Generic.List[object]]::new()
foreach ($output in $outputs) { $allOutputs.Add($output) }
if ($null -ne $replica) {
    [IO.Directory]::CreateDirectory($replica) | Out-Null
    Copy-ManifestTree -FromRoot $frozenRoot -ToRoot (Join-Path $replica 'source') `
        -Paths $frozenManifest.Paths
    [IO.Directory]::CreateDirectory((Join-Path $replica 'generated')) | Out-Null
    [IO.File]::Copy($generatedHeader,
                    (Join-Path $replica 'generated\sunrise_build_identity.generated.h'),
                    $false)
    [IO.Directory]::CreateDirectory((Join-Path $replica 'evidence')) | Out-Null
    Write-BytesIfChanged -Path (Join-Path $replica 'evidence\source-manifest.txt') `
        -Bytes $frozenManifest.Bytes
    Assert-FrozenSource -FrozenRoot (Join-Path $replica 'source') `
        -ExpectedManifest $frozenManifest.Bytes -Stage 'replica-copy'
    Protect-FrozenFiles -Root (Join-Path $replica 'source')
    (Get-Item -LiteralPath (Join-Path $replica 'generated\sunrise_build_identity.generated.h')).IsReadOnly = $true
    $replicaOutputs = @(Build-FrozenRoot -Root $replica -ExpectedManifest $frozenManifest.Bytes `
        -Count 1 -LabelPrefix 'root' -Toolchain $toolchain -Msbuild $msbuild -Cmake $cmake)
    if ($replicaOutputs[0].DllSha256 -cne $outputs[0].DllSha256) {
        throw "Different-root reproducibility mismatch: $($replicaOutputs[0].DllSha256) != $($outputs[0].DllSha256)"
    }
    $allOutputs.Add($replicaOutputs[0])
}

$scanRoots = @($candidate)
if ($null -ne $replica) { $scanRoots += $replica }
foreach ($output in $allOutputs) {
    Assert-NoEmbeddedRoots -BinaryPath $output.DllPath -Roots $scanRoots
}

$candidateManifest = [Text.StringBuilder]::new()
[void]$candidateManifest.Append("schema=1`n")
[void]$candidateManifest.Append("build_system=$BuildSystem`n")
[void]$candidateManifest.Append("build_id=$($identity.BuildId)`n")
[void]$candidateManifest.Append("source_sha256=$($frozenManifest.SourceSha256)`n")
[void]$candidateManifest.Append("compiler=$($toolchain.Compiler)`n")
[void]$candidateManifest.Append("toolset=$($toolchain.Toolset)`n")
[void]$candidateManifest.Append("windows_sdk=$($toolchain.WindowsSdk)`n")
[void]$candidateManifest.Append("source_manifest=evidence/source-manifest.txt`n")
[void]$candidateManifest.Append("generated_header=generated/sunrise_build_identity.generated.h`n")
$generatedHash = (Get-FileHash -LiteralPath $generatedHeader -Algorithm SHA256).Hash.ToUpperInvariant()
[void]$candidateManifest.Append("generated_header_sha256=$generatedHash`n")
for ($index = 0; $index -lt $outputs.Count; ++$index) {
    $relative = [IO.Path]::GetRelativePath($candidate, $outputs[$index].DllPath).Replace('\', '/')
    [void]$candidateManifest.Append("dll_$index=$relative`n")
    [void]$candidateManifest.Append("dll_sha256_$index=$($outputs[$index].DllSha256)`n")
    $pdbPath = [IO.Path]::ChangeExtension($outputs[$index].DllPath, '.pdb')
    if (Test-Path -LiteralPath $pdbPath -PathType Leaf) {
        $pdbRelative = [IO.Path]::GetRelativePath($candidate, $pdbPath).Replace('\', '/')
        $pdbHash = (Get-FileHash -LiteralPath $pdbPath -Algorithm SHA256).Hash.ToUpperInvariant()
        [void]$candidateManifest.Append("pdb_$index=$pdbRelative`n")
        [void]$candidateManifest.Append("pdb_sha256_$index=$pdbHash`n")
    }
}
if ($null -ne $replica) {
    [void]$candidateManifest.Append("replica_root_verified=1`n")
    [void]$candidateManifest.Append("replica_dll_sha256=$($allOutputs[$allOutputs.Count - 1].DllSha256)`n")
}
Write-BytesIfChanged -Path (Join-Path $evidenceRoot 'candidate-manifest.txt') `
    -Bytes ([Text.UTF8Encoding]::new($false).GetBytes($candidateManifest.ToString()))

"candidate=$candidate"
"source_sha256=$($frozenManifest.SourceSha256)"
"build_id=$($identity.BuildId)"
for ($index = 0; $index -lt $outputs.Count; ++$index) {
    "dll_sha256_$index=$($outputs[$index].DllSha256)"
}
if ($null -ne $replica) { "replica_dll_sha256=$($allOutputs[$allOutputs.Count - 1].DllSha256)" }
