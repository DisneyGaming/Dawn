<#
.SYNOPSIS
    Builds Dawn from this checkout and installs it over an existing Sunrise release.

.DESCRIPTION
    For testers who installed Sunrise with the official installer and have never built from source.
    Finds the install, builds the DLL, backs up everything it is about to touch, deploys the DLL and
    mission scripts, makes sure the mission arrival overrides exist in settings.json, and then proves
    which DLL the game actually mapped.

    Nothing here is destructive without a backup. Use -Restore to undo.

.PARAMETER GameRoot
    The Destiny 2 install directory (the one containing destiny2.exe). Auto-detected if omitted.

.PARAMETER SkipBuild
    Deploy the existing build output instead of rebuilding.

.PARAMETER NoLaunch
    Deploy but do not start the game. Skips the mapped-module verification.

.PARAMETER Restore
    Roll back to the most recent backup this script made and exit.

.EXAMPLE
    .\tools\install\Install-Dawn.ps1
.EXAMPLE
    .\tools\install\Install-Dawn.ps1 -GameRoot "D:\Sunrise"
.EXAMPLE
    .\tools\install\Install-Dawn.ps1 -Restore
#>

[CmdletBinding()]
param(
    [string] $GameRoot,
    [switch] $SkipBuild,
    [switch] $NoLaunch,
    [switch] $Restore
)

$ErrorActionPreference = 'Stop'

function Step  ($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Note  ($m) { Write-Host "    $m" -ForegroundColor Gray }
function Good  ($m) { Write-Host "    $m" -ForegroundColor Green }
function Warn  ($m) { Write-Host "!!  $m" -ForegroundColor Yellow }
function Die   ($m) { Write-Host "XX  $m" -ForegroundColor Red; exit 1 }

# Path relative to the game root, for readable log lines.
function Get-Relative ($absolute) { $absolute.Substring($root.Length).Trim([char]92) }

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project  = Join-Path $RepoRoot 'Sunrise\Sunrise.vcxproj'
$BuiltDll = Join-Path $RepoRoot 'build\x64\Release\steam_api64.dll'
$Scripts  = Join-Path $RepoRoot 'Sunrise\scripts'
$Defaults = Join-Path $RepoRoot 'Sunrise\resources\default_settings.json'

# ---------------------------------------------------------------- locate the install

function Find-GameRoot {
    if ($GameRoot) {
        if (-not (Test-Path (Join-Path $GameRoot 'destiny2.exe'))) {
            Die "No destiny2.exe in '$GameRoot'."
        }
        return (Resolve-Path $GameRoot).Path
    }

    $running = Get-Process destiny2 -ErrorAction SilentlyContinue
    if ($running) { return Split-Path -Parent $running.Path }

    foreach ($drive in (Get-PSDrive -PSProvider FileSystem)) {
        foreach ($guess in @('Sunrise', 'Destiny 2', 'SteamLibrary\steamapps\common\Destiny 2')) {
            $candidate = Join-Path $drive.Root $guess
            if (Test-Path (Join-Path $candidate 'destiny2.exe')) { return $candidate }
        }
    }
    Die "Could not find the game. Pass -GameRoot ""<path to destiny2.exe folder>""."
}

# Both places a steam_api64.dll can live. The exe sits at the root, so a DLL beside it shadows
# bin\x64 - which is where the Sunrise installer puts it. Only one is ever mapped, so write both
# and let the running process tell us which won.
function Get-DllTargets ($root) {
    @(
        (Join-Path $root 'bin\x64\steam_api64.dll'),
        (Join-Path $root 'steam_api64.dll')
    )
}
function Get-RuntimeTrees ($root) {
    @(
        (Join-Path $root 'bin\x64\Sunrise'),
        (Join-Path $root 'Sunrise')
    )
}

# ---------------------------------------------------------------- restore

function Invoke-Restore ($root) {
    $backupRoot = Join-Path $root '.dawn\backup'
    if (-not (Test-Path $backupRoot)) { Die "No Dawn backups under '$backupRoot'." }

    $latest = Get-ChildItem $backupRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $latest) { Die "No Dawn backups under '$backupRoot'." }

    Step "Restoring from $($latest.Name)"
    Stop-Game
    Get-ChildItem $latest.FullName -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($latest.FullName.Length + 1)
        $target   = Join-Path $root $relative
        $dir      = Split-Path -Parent $target
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        Copy-Item $_.FullName $target -Force
        Note "restored $relative"
    }
    Good "Restored. The pristine Steam DLL is still at .sunrise\original\steam_api64.dll if you need it."
    exit 0
}

function Stop-Game {
    $p = Get-Process destiny2 -ErrorAction SilentlyContinue
    if ($p) {
        Note 'Closing destiny2.exe (the DLL is locked while it runs)'
        $p | Stop-Process -Force
        Start-Sleep -Seconds 6
    }
}

# ---------------------------------------------------------------- preflight

$root = Find-GameRoot
Step "Game install: $root"

if ($Restore) { Invoke-Restore $root }

$state = Join-Path $root '.sunrise\install-state.json'
if (Test-Path $state) {
    $s = Get-Content $state -Raw | ConvertFrom-Json
    Good "Sunrise $($s.releaseTag) installed $($s.installedAtUtc)"
} else {
    Warn "No .sunrise\install-state.json - cannot confirm a Sunrise install is present."
    Warn "Dawn needs a Sunrise install that already boots and has generated its caches."
}

if (-not (Test-Path $Project)) { Die "Not inside a Dawn checkout - expected $Project" }

# ---------------------------------------------------------------- build

if (-not $SkipBuild) {
    $msbuild = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $msbuild) {
        Die @"
MSBuild 18 Build Tools not found.
Dawn needs the v145 toolset. VS2022 Community only carries v143 and will fail.
Install 'Visual Studio Build Tools 18' with Desktop development with C++.
Never downgrade PlatformToolset in the project to work around this.
"@
    }

    Step 'Building Dawn (Release x64)'
    Note (Split-Path -Leaf $msbuild)
    & $msbuild $Project -p:Configuration=Release -p:Platform=x64 -m -v:minimal -nologo
    if ($LASTEXITCODE -ne 0) { Die 'Build failed. Warnings are errors in this project (/W4 /WX).' }
}

if (-not (Test-Path $BuiltDll)) { Die "No build output at $BuiltDll - run without -SkipBuild." }
$builtHash  = (Get-FileHash $BuiltDll -Algorithm SHA256).Hash
$builtShort = $builtHash.Substring(0, 16)
$builtMb    = [math]::Round((Get-Item $BuiltDll).Length / 1MB, 1)
Good "DLL $builtShort  ($builtMb MB)"

# ---------------------------------------------------------------- backup

Stop-Game

$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDir = Join-Path $root ".dawn\backup\$stamp"
Step "Backing up to .dawn\backup\$stamp"

function Backup-File ($absolute) {
    if (-not (Test-Path $absolute)) { return }
    $relative = $absolute.Substring($root.Length).TrimStart('\')
    $target   = Join-Path $backupDir $relative
    $dir      = Split-Path -Parent $target
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Copy-Item $absolute $target -Force
    Note "saved $relative"
}

foreach ($dll in Get-DllTargets $root) { Backup-File $dll }
foreach ($tree in Get-RuntimeTrees $root) {
    Backup-File (Join-Path $tree 'settings.json')
    $scriptDir = Join-Path $tree 'scripts'
    if (Test-Path $scriptDir) {
        Get-ChildItem $scriptDir -Filter *.lua -ErrorAction SilentlyContinue |
            ForEach-Object { Backup-File $_.FullName }
    }
}

# ---------------------------------------------------------------- deploy

Step 'Deploying DLL and mission scripts'

foreach ($dll in Get-DllTargets $root) {
    $dir = Split-Path -Parent $dll
    if (-not (Test-Path $dir)) { continue }        # only write into directories that exist
    Copy-Item $BuiltDll $dll -Force
    Note "dll  -> $($dll.Substring($root.Length).TrimStart('\'))"
}

$luaCount = (Get-ChildItem $Scripts -Filter *.lua).Count
foreach ($tree in Get-RuntimeTrees $root) {
    $parent = Split-Path -Parent $tree
    if (-not (Test-Path $parent)) { continue }
    $scriptDir = Join-Path $tree 'scripts'
    if (-not (Test-Path $scriptDir)) { New-Item -ItemType Directory -Path $scriptDir -Force | Out-Null }
    Copy-Item (Join-Path $Scripts '*.lua') $scriptDir -Force
    Note "lua  -> $($scriptDir.Substring($root.Length).TrimStart('\'))  ($luaCount scripts)"
}

# ---------------------------------------------------------------- settings

# Settings parsing starts from the compiled defaults and overlays the file, so an ABSENT key keeps
# Dawn's default. But arrival_overrides is an array: if the file has its own, it replaces Dawn's
# entirely - so a stock Sunrise install takes the DLL and still has no way to reach the missions.
# Add only the overrides Dawn ships that the file is missing. Never reorder or drop the user's own.

Step 'Checking mission arrival overrides'

$wanted = (Get-Content $Defaults -Raw | ConvertFrom-Json).state.activity.arrival_overrides

foreach ($tree in Get-RuntimeTrees $root) {
    $settingsPath = Join-Path $tree 'settings.json'
    if (-not (Test-Path $settingsPath)) { continue }

    $json  = Get-Content $settingsPath -Raw
    $doc   = $json | ConvertFrom-Json
    $have  = @($doc.state.activity.arrival_overrides)
    $names = @($have | ForEach-Object { $_.package_name })
    $added = @()

    foreach ($o in $wanted) {
        if ($names -notcontains $o.package_name) { $have += $o; $added += $o.package_name }
    }

    $label = $settingsPath.Substring($root.Length).TrimStart('\')
    if ($added.Count -eq 0) { Note "$label already has all $($wanted.Count) overrides"; continue }

    $doc.state.activity.arrival_overrides = $have
    $out = $doc | ConvertTo-Json -Depth 100

    # settings.json has a 64 KB hard cap. Refuse rather than corrupt it.
    $bytes = [System.Text.Encoding]::UTF8.GetByteCount($out)
    if ($bytes -ge 65536) {
        Warn "$label would reach $bytes bytes, past the 64 KB cap. Left unchanged - add these by hand: $($added -join ', ')"
        continue
    }

    Set-Content -Path $settingsPath -Value $out -Encoding UTF8
    Good "$label  +$($added.Count) overrides: $($added -join ', ')  ($bytes bytes)"
}

# ---------------------------------------------------------------- verify

if ($NoLaunch) {
    Step 'Done (not launching)'
    Note 'Verify manually: start the game, then run'
    Note '  Get-Process destiny2 | % { $_.Modules | ? { $_.ModuleName -like ''steam_api64*'' } | select FileName }'
    exit 0
}

Step 'Launching and verifying'
Start-Process -FilePath (Join-Path $root 'destiny2.exe') -WorkingDirectory $root
Note 'Waiting for the module to map (boot is 60-110s)...'

$mapped = $null
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Seconds 5
    $p = Get-Process destiny2 -ErrorAction SilentlyContinue
    if (-not $p) { continue }
    $m = $p.Modules | Where-Object { $_.ModuleName -like 'steam_api64*' } | Select-Object -First 1
    if ($m) { $mapped = $m.FileName; break }
}

if (-not $mapped) {
    Warn 'Could not read the mapped module. Check the game actually started.'
    exit 1
}

Good "Mapped: $mapped"
$mappedHash = (Get-FileHash $mapped -Algorithm SHA256).Hash
if ($mappedHash -eq $builtHash) {
    Good 'This is the build that was just deployed.'
} else {
    Warn 'The mapped DLL is NOT the build just deployed.'
    Warn "  built:  $($builtHash.Substring(0,32))"
    Warn "  mapped: $($mappedHash.Substring(0,32))"
    Warn 'Something else is shadowing it. Do not trust a playtest until this matches.'
}

$liveTree = Split-Path -Parent $mapped
$log      = Join-Path $liveTree 'Sunrise\logs\sunrise.log'
Note "Runtime tree: $(Join-Path $liveTree 'Sunrise')"
Note "Log:          $log"
Write-Host ''
Note 'Once you enter an activity, confirm the graph that loaded:'
Note "  Select-String 'ev=coo_script' '$log'"
