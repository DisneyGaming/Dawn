# sunrise-dev.ps1 - build the Sunrise fork and deploy it into this game install.
#
#   .\sunrise-dev.ps1                 build Release + deploy
#   .\sunrise-dev.ps1 -Config Debug   build Debug + deploy
#   .\sunrise-dev.ps1 -BuildOnly      compile, don't touch the game folder
#   .\sunrise-dev.ps1 -Restore        put the last backed-up DLL back
#   .\sunrise-dev.ps1 -ResetSettings  archive settings.json so defaults regenerate
#   .\sunrise-dev.ps1 -ClearCache     drop build_data.bin / content_manifest.bin

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')] [string] $Config = 'Release',
    [switch] $BuildOnly,
    [switch] $Restore,
    [switch] $ResetSettings,
    [switch] $ClearCache
)

$ErrorActionPreference = 'Stop'

$GameRoot   = $PSScriptRoot
$Repo       = Join-Path $GameRoot 'Sunrise-src'
$Solution   = Join-Path $Repo 'Sunrise.sln'
$LiveDll    = Join-Path $GameRoot 'bin\x64\steam_api64.dll'
$SunriseDir = Join-Path $GameRoot 'bin\x64\Sunrise'
$BackupDir  = Join-Path $GameRoot '.sunrise\backup'

function Step($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Warn($m) { Write-Host "!!  $m" -ForegroundColor Yellow }

# The game must not be running for anything that touches its folder: the DLL is
# locked and SQLite may have an open player-state database. Pure builds are exempt.
function Assert-GameClosed {
    if (Get-Process destiny2 -ErrorAction SilentlyContinue) {
        throw 'destiny2.exe is running. Close the game first.'
    }
}

if ($Restore) {
    Assert-GameClosed
    $last = Get-ChildItem $BackupDir -Filter 'steam_api64.*.dll' -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $last) { throw "No backups in $BackupDir" }
    Copy-Item $last.FullName $LiveDll -Force
    Step "Restored $($last.Name)"
    return
}

if ($ResetSettings) {
    Assert-GameClosed
    $s = Join-Path $SunriseDir 'settings.json'
    if (Test-Path $s) {
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        Move-Item $s (Join-Path $SunriseDir "settings.$stamp.json.bak")
        Step "Archived settings.json - defaults regenerate on next launch"
    }
}

if ($ClearCache) {
    Assert-GameClosed
    $c = Join-Path $SunriseDir 'cache'
    if (Test-Path $c) {
        Remove-Item (Join-Path $c '*.bin') -Force -ErrorAction SilentlyContinue
        Step 'Cleared content cache - rebuilds from packages on next launch (slow first boot)'
    }
}

# --- locate MSBuild -----------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found - install Visual Studio Build Tools.' }

$msbuild = & $vswhere -latest -prerelease -products * `
    -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1

if (-not $msbuild) {
    throw 'No MSBuild with the C++ toolset found. Install the "Desktop development with C++" workload.'
}
Step "MSBuild: $msbuild"

# --- build --------------------------------------------------------------------
Step "Building $Config|x64"
& $msbuild $Solution /nologo /m /v:minimal /p:Configuration=$Config /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

$built = Join-Path $Repo "build\x64\$Config\steam_api64.dll"
if (-not (Test-Path $built)) { throw "Expected output missing: $built" }
Step "Built $((Get-Item $built).Length) bytes"

if ($BuildOnly) { return }

# --- deploy -------------------------------------------------------------------
Assert-GameClosed
# bin\x64\steam_api64.dll is the Sunrise proxy. The REAL Steam API lives at
# .sunrise\original\steam_api64.dll and must never be overwritten.
New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null
if (Test-Path $LiveDll) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    Copy-Item $LiveDll (Join-Path $BackupDir "steam_api64.$stamp.dll")
    Step "Backed up current DLL to .sunrise\backup\steam_api64.$stamp.dll"
}

Copy-Item $built $LiveDll -Force
Step "Deployed to $LiveDll"

# The official installer tracks a hash of the release DLL it placed. A local
# build will not match, so re-running the Sunrise installer overwrites this.
$state = Join-Path $GameRoot '.sunrise\install-state.json'
if (Test-Path $state) {
    Warn 'install-state.json still points at release 0.2.1 - rerunning the Sunrise installer will replace this build.'
}
Write-Host "Done. Launch destiny2.exe." -ForegroundColor Green
