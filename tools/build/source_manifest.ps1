[CmdletBinding()]
param(
    [ValidateSet('Git', 'Tree')]
    [string]$CommandMode,
    [string]$CommandRoot,
    [string]$CommandManifestPath,
    [string]$CommandGeneratedHeaderPath,
    [string]$CommandGitHead,
    [string]$CommandGitBranch,
    [ValidateSet('0', '1')]
    [string]$CommandGitDirty,
    [string]$CommandConfiguration,
    [string]$CommandPlatform,
    [string]$CommandCompilerId,
    [string]$CommandToolsetId,
    [string]$CommandWindowsSdk,
    [uint32]$CommandCacheFormat,
    [uint32]$CommandSettingsVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false, $true)

function Get-Sha256Hex {
    param([Parameter(Mandatory)][AllowEmptyCollection()][byte[]]$Bytes)
    $hash = [System.Security.Cryptography.SHA256]::HashData($Bytes)
    return [Convert]::ToHexString($hash)
}

function Invoke-GitBytes {
    param(
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [Parameter(Mandatory)][string[]]$Arguments
    )
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = 'git.exe'
    $start.WorkingDirectory = [IO.Path]::GetFullPath($RepositoryRoot)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        [void]$start.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::Start($start)
    $memory = [IO.MemoryStream]::new()
    $process.StandardOutput.BaseStream.CopyTo($memory)
    $errorText = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "git $($Arguments -join ' ') failed: $errorText"
    }
    # Unary comma prevents PowerShell from unrolling an empty byte array into `$null`.
    return ,$memory.ToArray()
}

function Compare-Utf8Ordinal {
    param([string]$Left, [string]$Right)
    $leftBytes = $script:Utf8NoBom.GetBytes($Left)
    $rightBytes = $script:Utf8NoBom.GetBytes($Right)
    $count = [Math]::Min($leftBytes.Length, $rightBytes.Length)
    for ($index = 0; $index -lt $count; ++$index) {
        if ($leftBytes[$index] -lt $rightBytes[$index]) { return -1 }
        if ($leftBytes[$index] -gt $rightBytes[$index]) { return 1 }
    }
    return $leftBytes.Length.CompareTo($rightBytes.Length)
}

function ConvertFrom-NulUtf8 {
    param([Parameter(Mandatory)][AllowEmptyCollection()][byte[]]$Bytes)
    if ($Bytes.Length -eq 0) { return @() }
    if ($Bytes[$Bytes.Length - 1] -ne 0) { throw 'NUL-delimited Git output did not end in NUL' }
    $values = [Collections.Generic.List[string]]::new()
    $start = 0
    for ($index = 0; $index -lt $Bytes.Length; ++$index) {
        if ($Bytes[$index] -eq 0) {
            if ($index -eq $start) { throw 'Git returned an empty path' }
            $values.Add($script:Utf8NoBom.GetString($Bytes, $start, $index - $start))
            $start = $index + 1
        }
    }
    return [string[]]$values.ToArray()
}

function Sort-CanonicalPaths {
    param([Parameter(Mandatory)][string[]]$Paths)
    $list = [Collections.Generic.List[string]]::new()
    foreach ($path in $Paths) { $list.Add($path.Replace('\', '/')) }
    $comparison = [Comparison[string]]{
        param([string]$left, [string]$right)
        return Compare-Utf8Ordinal -Left $left -Right $right
    }
    $list.Sort($comparison)
    $previous = $null
    foreach ($path in $list) {
        if ($path.IndexOfAny([char[]]@([char]0, "`r", "`n", "`t")) -ge 0) {
            throw "Manifest path contains a forbidden delimiter"
        }
        if ($null -ne $previous -and $path -ceq $previous) {
            throw "Duplicate canonical manifest path: $path"
        }
        $previous = $path
    }
    return [string[]]$list.ToArray()
}

function Get-GitSourcePaths {
    param([Parameter(Mandatory)][string]$RepositoryRoot)
    $bytes = Invoke-GitBytes -RepositoryRoot $RepositoryRoot -Arguments @(
        '-c', 'core.quotepath=false', 'ls-files', '-z', '--cached', '--others', '--exclude-standard')
    $deletedBytes = Invoke-GitBytes -RepositoryRoot $RepositoryRoot -Arguments @(
        '-c', 'core.quotepath=false', 'ls-files', '-z', '--deleted')
    $deleted = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in ConvertFrom-NulUtf8 -Bytes $deletedBytes) {
        [void]$deleted.Add($path.Replace('\', '/'))
    }
    $visible = [Collections.Generic.List[string]]::new()
    foreach ($path in ConvertFrom-NulUtf8 -Bytes $bytes) {
        $canonical = $path.Replace('\', '/')
        if (-not $deleted.Contains($canonical)) { $visible.Add($canonical) }
    }
    $paths = Sort-CanonicalPaths -Paths $visible.ToArray()
    Assert-SourcePathHygiene -Paths $paths
    return $paths
}

function Assert-SourcePathHygiene {
    param([Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Paths)
    $outputPattern =
        '(^|/)(build|out|obj|CMakeFiles|\.vs)(/|$)|\.(obj|o|exe|dll|pdb|ilk|exp|lib|res|iobj|ipdb|tlog|lastbuildstate|binlog|recipe)$'
    foreach ($path in $Paths) {
        $hygienePath = if ($path.StartsWith('tools/build/', [StringComparison]::Ordinal)) {
            $path.Substring('tools/build/'.Length)
        } else {
            $path
        }
        if ($hygienePath -match $outputPattern) {
            throw "Source set contains a build/output artifact: $path"
        }
    }
}

function Get-GitCaptureState {
    param([Parameter(Mandatory)][string]$RepositoryRoot)
    $readBoundary = {
        $head = $script:Utf8NoBom.GetString(
            (Invoke-GitBytes -RepositoryRoot $RepositoryRoot -Arguments @('rev-parse', 'HEAD'))).Trim()
        $branch = $script:Utf8NoBom.GetString(
            (Invoke-GitBytes -RepositoryRoot $RepositoryRoot -Arguments @('branch', '--show-current'))).Trim()
        if ([string]::IsNullOrWhiteSpace($branch)) { $branch = 'DETACHED' }
        $statusBytes = Invoke-GitBytes -RepositoryRoot $RepositoryRoot -Arguments @(
            '-c', 'core.quotepath=false', 'status', '--porcelain=v1', '-z', '--untracked-files=all')
        return [pscustomobject]@{
            Head = $head
            Branch = $branch
            Dirty = $statusBytes.Length -ne 0
            StatusSha256 = Get-Sha256Hex -Bytes $statusBytes
        }
    }

    # Bracket the content walk with the Git identity/overlay tuple. This prevents a
    # manifest from being paired with HEAD, branch, or dirty state sampled at a
    # different instant. The outer release wrapper repeats this guard around copy.
    $before = & $readBoundary
    $manifest = Get-SourceManifest -SourceRoot $RepositoryRoot -Kind Git
    $after = & $readBoundary
    if ($before.Head -cne $after.Head -or $before.Branch -cne $after.Branch -or
        $before.Dirty -ne $after.Dirty -or
        $before.StatusSha256 -cne $after.StatusSha256) {
        throw 'Git HEAD/branch/status changed during source-manifest capture'
    }
    return [pscustomobject]@{
        Head = $before.Head
        Branch = $before.Branch
        Dirty = $before.Dirty
        StatusSha256 = $before.StatusSha256
        Manifest = $manifest
    }
}

function Test-GitCaptureStateEqual {
    param([Parameter(Mandatory)]$Left, [Parameter(Mandatory)]$Right)
    return $Left.Head -ceq $Right.Head -and $Left.Branch -ceq $Right.Branch `
        -and $Left.Dirty -eq $Right.Dirty `
        -and $Left.StatusSha256 -ceq $Right.StatusSha256 `
        -and [Linq.Enumerable]::SequenceEqual(
            [byte[]]$Left.Manifest.Bytes, [byte[]]$Right.Manifest.Bytes)
}

function Get-TreeSourcePaths {
    param([Parameter(Mandatory)][string]$TreeRoot)
    $resolved = [IO.Path]::GetFullPath($TreeRoot).TrimEnd('\', '/')
    $paths = [Collections.Generic.List[string]]::new()
    foreach ($file in Get-ChildItem -LiteralPath $resolved -File -Recurse -Force) {
        $relative = [IO.Path]::GetRelativePath($resolved, $file.FullName).Replace('\', '/')
        $paths.Add($relative)
    }
    return Sort-CanonicalPaths -Paths $paths.ToArray()
}

function New-CanonicalSourceManifest {
    param(
        [Parameter(Mandatory)][string]$SourceRoot,
        [Parameter(Mandatory)][string[]]$Paths
    )
    $resolved = [IO.Path]::GetFullPath($SourceRoot)
    $builder = [Text.StringBuilder]::new()
    foreach ($path in $Paths) {
        $native = $path.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $full = [IO.Path]::GetFullPath([IO.Path]::Combine($resolved, $native))
        $relativeCheck = [IO.Path]::GetRelativePath($resolved, $full).Replace('\', '/')
        if ($relativeCheck -cne $path -or $relativeCheck.StartsWith('../', [StringComparison]::Ordinal)) {
            throw "Manifest path escapes or aliases the source root: $path"
        }
        $item = Get-Item -LiteralPath $full -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Source manifest does not follow reparse points: $path"
        }
        $bytes = [IO.File]::ReadAllBytes($full)
        [void]$builder.Append((Get-Sha256Hex -Bytes $bytes))
        [void]$builder.Append("`t")
        [void]$builder.Append($bytes.LongLength.ToString([Globalization.CultureInfo]::InvariantCulture))
        [void]$builder.Append("`t")
        [void]$builder.Append($path)
        [void]$builder.Append("`n")
    }
    $manifestBytes = $script:Utf8NoBom.GetBytes($builder.ToString())
    return [pscustomobject]@{
        Paths = $Paths
        Bytes = $manifestBytes
        SourceSha256 = Get-Sha256Hex -Bytes $manifestBytes
    }
}

function Write-BytesIfChanged {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][AllowEmptyCollection()][byte[]]$Bytes
    )
    $full = [IO.Path]::GetFullPath($Path)
    $directory = [IO.Path]::GetDirectoryName($full)
    if ($directory) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
    if ([IO.File]::Exists($full)) {
        $existing = [IO.File]::ReadAllBytes($full)
        if ([Linq.Enumerable]::SequenceEqual([byte[]]$existing, [byte[]]$Bytes)) { return }
    }
    [IO.File]::WriteAllBytes($full, $Bytes)
}

function Escape-CDefine {
    param([Parameter(Mandatory)][string]$Value)
    if ($Value.IndexOfAny([char[]]@([char]0, "`r", "`n", "`t")) -ge 0) {
        throw 'Generated identity value contains a forbidden control character'
    }
    return $Value.Replace('\', '\\').Replace('"', '\"')
}

function New-BuildIdentityHeader {
    param(
        [Parameter(Mandatory)][string]$SourceSha256,
        [Parameter(Mandatory)][string]$Head,
        [Parameter(Mandatory)][string]$Branch,
        [Parameter(Mandatory)][bool]$Dirty,
        [Parameter(Mandatory)][string]$BuildConfiguration,
        [Parameter(Mandatory)][string]$BuildPlatform,
        [Parameter(Mandatory)][string]$Compiler,
        [Parameter(Mandatory)][string]$Toolset,
        [Parameter(Mandatory)][string]$Sdk,
        [Parameter(Mandatory)][uint32]$Format,
        [Parameter(Mandatory)][uint32]$Settings
    )
    if ($SourceSha256 -cnotmatch '^[0-9A-F]{64}$') { throw 'Source SHA-256 must be uppercase hex' }
    if ($Head -cnotmatch '^[0-9a-fA-F]{40}$') { throw 'Git HEAD must be 40 hexadecimal digits' }
    foreach ($required in @($Branch, $BuildConfiguration, $BuildPlatform, $Compiler, $Toolset, $Sdk)) {
        if ([string]::IsNullOrWhiteSpace($required)) { throw 'A mandatory identity field is empty' }
    }
    $tuple = @(
        'sunrise-build-identity-v1'
        "schema=1"
        "git_head=$($Head.ToUpperInvariant())"
        "git_branch=$Branch"
        "dirty=$([int]$Dirty)"
        "source_sha256=$SourceSha256"
        "configuration=$BuildConfiguration"
        "platform=$BuildPlatform"
        "compiler=$Compiler"
        "toolset=$Toolset"
        "windows_sdk=$Sdk"
        "cache_format=$Format"
        "settings_version=$Settings"
    ) -join "`n"
    $tuple += "`n"
    $buildId = Get-Sha256Hex -Bytes $script:Utf8NoBom.GetBytes($tuple)
    $lines = @(
        '#pragma once'
        '#define SUNRISE_PROVENANCE_SCHEMA 1'
        "#define SUNRISE_BUILD_ID `"$buildId`""
        "#define SUNRISE_GIT_HEAD `"$($Head.ToUpperInvariant())`""
        "#define SUNRISE_GIT_BRANCH `"$(Escape-CDefine $Branch)`""
        "#define SUNRISE_GIT_DIRTY $([int]$Dirty)"
        "#define SUNRISE_SOURCE_SHA256 `"$SourceSha256`""
        "#define SUNRISE_BUILD_CONFIGURATION `"$(Escape-CDefine $BuildConfiguration)`""
        "#define SUNRISE_BUILD_PLATFORM `"$(Escape-CDefine $BuildPlatform)`""
        "#define SUNRISE_COMPILER_ID `"$(Escape-CDefine $Compiler)`""
        "#define SUNRISE_TOOLSET_ID `"$(Escape-CDefine $Toolset)`""
        "#define SUNRISE_WINDOWS_SDK `"$(Escape-CDefine $Sdk)`""
        "#define SUNRISE_CACHE_FORMAT $Format"
        "#define SUNRISE_SETTINGS_VERSION $Settings"
        ''
    ) -join "`n"
    return [pscustomobject]@{
        BuildId = $buildId
        Bytes = $script:Utf8NoBom.GetBytes($lines)
    }
}

function Get-SourceManifest {
    param([Parameter(Mandatory)][string]$SourceRoot, [ValidateSet('Git', 'Tree')][string]$Kind)
    $paths = if ($Kind -eq 'Git') {
        Get-GitSourcePaths -RepositoryRoot $SourceRoot
    } else {
        Get-TreeSourcePaths -TreeRoot $SourceRoot
    }
    return New-CanonicalSourceManifest -SourceRoot $SourceRoot -Paths $paths
}

if ($MyInvocation.InvocationName -ne '.') {
    if ([string]::IsNullOrWhiteSpace($CommandMode) -or
        [string]::IsNullOrWhiteSpace($CommandRoot)) {
        throw 'CommandMode and CommandRoot are required'
    }
    $manifest = Get-SourceManifest -SourceRoot $CommandRoot -Kind $CommandMode
    if ($CommandManifestPath) {
        Write-BytesIfChanged -Path $CommandManifestPath -Bytes $manifest.Bytes
    }
    if ($CommandGeneratedHeaderPath) {
        $header = New-BuildIdentityHeader -SourceSha256 $manifest.SourceSha256 `
            -Head $CommandGitHead -Branch $CommandGitBranch -Dirty ($CommandGitDirty -eq '1') `
            -BuildConfiguration $CommandConfiguration -BuildPlatform $CommandPlatform `
            -Compiler $CommandCompilerId -Toolset $CommandToolsetId -Sdk $CommandWindowsSdk `
            -Format $CommandCacheFormat -Settings $CommandSettingsVersion
        Write-BytesIfChanged -Path $CommandGeneratedHeaderPath -Bytes $header.Bytes
        "build_id=$($header.BuildId)"
    }
    "source_sha256=$($manifest.SourceSha256)"
}
