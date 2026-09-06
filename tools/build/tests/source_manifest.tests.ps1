[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\source_manifest.ps1')

$failures = 0
function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        Write-Error -ErrorAction Continue "FAILED: $Message"
        $script:failures++
    }
}
function Assert-Throws {
    param([scriptblock]$Action, [string]$Message)
    try {
        & $Action
        Assert-True $false $Message
    } catch {
        Assert-True $true $Message
    }
}
function Write-Utf8 {
    param([string]$Path, [string]$Text)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllBytes($Path, [Text.UTF8Encoding]::new($false).GetBytes($Text))
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("sunrise-manifest-tests-" + [guid]::NewGuid().ToString('N'))
$rootA = Join-Path $testRoot 'absolute-a'
$rootB = Join-Path $testRoot 'different-absolute-b'
[IO.Directory]::CreateDirectory($rootA) | Out-Null
try {
    & git -C $rootA init --quiet
    & git -C $rootA config user.email 'manifest-test@example.invalid'
    & git -C $rootA config user.name 'Manifest Test'
    & git -C $rootA config commit.gpgsign false
    $repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
    [IO.File]::Copy((Join-Path $repositoryRoot '.gitignore'), (Join-Path $rootA '.gitignore'))
    Write-Utf8 (Join-Path $rootA 'zeta.cpp') "tracked`n"
    Write-Utf8 (Join-Path $rootA 'nested/alpha.h') "alpha`n"
    [IO.File]::WriteAllBytes((Join-Path $rootA 'empty.resource'), [byte[]]::new(0))
    & git -C $rootA add -- .
    & git -C $rootA commit --quiet -m baseline
    if ($LASTEXITCODE -ne 0) { throw 'temporary Git baseline commit failed' }

    $first = Get-SourceManifest -SourceRoot $rootA -Kind Git
    $second = Get-SourceManifest -SourceRoot $rootA -Kind Git
    Assert-True ([Linq.Enumerable]::SequenceEqual([byte[]]$first.Bytes, [byte[]]$second.Bytes)) `
        'same overlay produces byte-identical manifests'
    Assert-True ($first.SourceSha256 -ceq $second.SourceSha256) `
        'same overlay produces the same source digest'
    Assert-True ($first.Bytes.Length -gt 0 -and $first.Bytes[0] -ne 0xEF) `
        'manifest is nonempty UTF-8 without BOM'
    Assert-True ($first.Bytes[$first.Bytes.Length - 1] -eq 0x0A) 'manifest has a final LF'
    $manifestText = [Text.UTF8Encoding]::new($false, $true).GetString($first.Bytes)
    Assert-True ($manifestText.IndexOf("nested/alpha.h") -lt $manifestText.IndexOf("zeta.cpp")) `
        'paths use ordinal UTF-8 ordering and forward slashes'
    Assert-True ($manifestText.Contains(
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`t0`tempty.resource`n")) `
        'tracked empty file uses the deterministic zero-length SHA-256 row'

    $captureA = Get-GitCaptureState -RepositoryRoot $rootA
    $captureB = Get-GitCaptureState -RepositoryRoot $rootA
    Assert-True (Test-GitCaptureStateEqual -Left $captureA -Right $captureB) `
        'coherent HEAD/branch/status/content capture is stable without mutation'
    Write-Utf8 (Join-Path $rootA 'zeta.cpp') "capture mutation`n"
    $captureChanged = Get-GitCaptureState -RepositoryRoot $rootA
    Assert-True (-not (Test-GitCaptureStateEqual -Left $captureA -Right $captureChanged)) `
        'coherent capture guard rejects a changed worktree overlay'
    Write-Utf8 (Join-Path $rootA 'zeta.cpp') "tracked`n"

    [IO.File]::Delete((Join-Path $rootA 'nested/alpha.h'))
    $trackedDeletion = Get-SourceManifest -SourceRoot $rootA -Kind Git
    Assert-True (-not ($trackedDeletion.Paths -ccontains 'nested/alpha.h')) `
        'intentional tracked deletion is represented by an absent manifest row'
    Assert-True ($trackedDeletion.SourceSha256 -cne $first.SourceSha256) `
        'intentional tracked deletion changes source digest'
    Write-Utf8 (Join-Path $rootA 'nested/alpha.h') "alpha`n"
    Assert-Throws {
        New-CanonicalSourceManifest -SourceRoot $rootA -Paths @('missing-during-capture.cpp')
    } 'a missing path requested by one capture fails instead of becoming a deletion'

    [IO.Directory]::CreateDirectory($rootB) | Out-Null
    foreach ($path in $first.Paths) {
        $source = Join-Path $rootA $path
        $destination = Join-Path $rootB $path
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy($source, $destination)
    }
    $copied = Get-SourceManifest -SourceRoot $rootB -Kind Tree
    Assert-True ($first.SourceSha256 -ceq $copied.SourceSha256) `
        'different absolute roots produce the same source digest'

    Write-Utf8 (Join-Path $rootA 'zeta.cpp') "tracked changed`n"
    $trackedChanged = Get-SourceManifest -SourceRoot $rootA -Kind Git
    Assert-True ($trackedChanged.SourceSha256 -cne $first.SourceSha256) `
        'modified tracked file changes source digest'

    Write-Utf8 (Join-Path $rootA 'untracked.cpp') "untracked`n"
    $untrackedChanged = Get-SourceManifest -SourceRoot $rootA -Kind Git
    Assert-True ($untrackedChanged.SourceSha256 -cne $trackedChanged.SourceSha256) `
        'non-ignored untracked source changes source digest'

    Write-Utf8 (Join-Path $rootA 'scratch.obj') 'ignored output'
    Write-Utf8 (Join-Path $rootA 'build/product.bin') 'ignored build tree'
    Write-Utf8 (Join-Path $rootA 'session.log') 'ignored log'
    Write-Utf8 (Join-Path $rootA 'Sunrise/build/nested/test-product.exe') 'ignored project build'
    Write-Utf8 (Join-Path $rootA 'Sunrise/standalone-test.obj') 'ignored nested object'
    Write-Utf8 (Join-Path $rootA 'standalone-test.exe') 'ignored root executable'
    $ignored = Get-SourceManifest -SourceRoot $rootA -Kind Git
    Assert-True ($ignored.SourceSha256 -ceq $untrackedChanged.SourceSha256) `
        'ignored outputs do not change source digest'
    Assert-Throws { Assert-SourcePathHygiene -Paths @('Sunrise/build/escaped.obj') } `
        'explicit source hygiene rejects a build-tree artifact even if ignore rules regress'

    $beforeGuard = Get-SourceManifest -SourceRoot $rootB -Kind Tree
    Write-Utf8 (Join-Path $rootB 'zeta.cpp') 'mutation'
    $afterGuard = Get-SourceManifest -SourceRoot $rootB -Kind Tree
    Assert-True ($beforeGuard.SourceSha256 -cne $afterGuard.SourceSha256) `
        'frozen-source mutation is detected by pre/post manifest guard'
    [IO.File]::Delete((Join-Path $rootB 'nested/alpha.h'))
    $afterDelete = Get-SourceManifest -SourceRoot $rootB -Kind Tree
    Assert-True ($afterGuard.SourceSha256 -cne $afterDelete.SourceSha256) `
        'frozen-source deletion is detected'
    Write-Utf8 (Join-Path $rootB 'extra.cpp') 'extra'
    $afterAdd = Get-SourceManifest -SourceRoot $rootB -Kind Tree
    Assert-True ($afterDelete.SourceSha256 -cne $afterAdd.SourceSha256) `
        'frozen-source addition is detected'

    $lf = New-CanonicalSourceManifest -SourceRoot $rootA -Paths @('zeta.cpp')
    [IO.File]::WriteAllBytes((Join-Path $rootA 'zeta.cpp'), [byte[]](0x61, 0x0D, 0x0A))
    $crlf = New-CanonicalSourceManifest -SourceRoot $rootA -Paths @('zeta.cpp')
    Assert-True ($lf.SourceSha256 -cne $crlf.SourceSha256) `
        'raw CRLF and LF bytes remain distinct'
    Assert-Throws { Sort-CanonicalPaths -Paths @("bad`tpath") } `
        'delimiter-bearing paths are rejected'

    $linkPath = Join-Path $rootA 'source-link.cpp'
    try {
        [IO.File]::CreateSymbolicLink($linkPath, (Join-Path $rootA 'zeta.cpp')) | Out-Null
        Assert-Throws {
            New-CanonicalSourceManifest -SourceRoot $rootA -Paths @('source-link.cpp')
        } 'reparse-point source files are rejected rather than followed'
    } catch [System.UnauthorizedAccessException] {
        Write-Warning 'reparse-point regression skipped because symlink creation is not permitted'
    } finally {
        if ([IO.File]::Exists($linkPath)) { [IO.File]::Delete($linkPath) }
    }

    $headerA = New-BuildIdentityHeader -SourceSha256 $first.SourceSha256 `
        -Head '0123456789abcdef0123456789abcdef01234567' -Branch 'test' -Dirty $true `
        -BuildConfiguration Release -BuildPlatform x64 -Compiler 'MSVC-test' `
        -Toolset 'v145-test' -Sdk '10.0.test' -Format 48 -Settings 6
    $headerB = New-BuildIdentityHeader -SourceSha256 $first.SourceSha256 `
        -Head '0123456789abcdef0123456789abcdef01234567' -Branch 'test' -Dirty $true `
        -BuildConfiguration Release -BuildPlatform x64 -Compiler 'MSVC-test' `
        -Toolset 'v145-test' -Sdk '10.0.test' -Format 48 -Settings 6
    Assert-True ($headerA.BuildId -ceq $headerB.BuildId) 'build ID is deterministic'
    Assert-True ([Linq.Enumerable]::SequenceEqual([byte[]]$headerA.Bytes, [byte[]]$headerB.Bytes)) `
        'generated header is byte-identical'
    $headerText = [Text.Encoding]::UTF8.GetString($headerA.Bytes)
    Assert-True (-not $headerText.Contains($rootA) -and -not $headerText.Contains($rootB)) `
        'generated header contains no absolute source root'
    Assert-True ($headerText -notmatch '\d{4}-\d{2}-\d{2}T') `
        'generated header contains no timestamp'
    Assert-True (-not $headerText.Contains([Environment]::UserName)) `
        'generated header contains no user name'
} finally {
    $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTestRoot.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) `
        -and [IO.Directory]::Exists($resolvedTestRoot)) {
        Get-ChildItem -LiteralPath $resolvedTestRoot -File -Recurse -Force |
            ForEach-Object { $_.IsReadOnly = $false }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}

if ($failures -ne 0) { throw "$failures source-manifest test(s) failed" }
'all source-manifest tests passed'
