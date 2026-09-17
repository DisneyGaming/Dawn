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

# Runtime rules are read beside settings.json; event presets remain selectable files.
$runtimeResources = @(
    Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Sunrise\resources\vendor_rules') -File | ForEach-Object {
        [pscustomobject]@{ Source = $_.FullName; Relative = $_.Name }
    }
    Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Sunrise\resources\event_presets') -File | ForEach-Object {
        [pscustomobject]@{ Source = $_.FullName; Relative = "event_presets\$($_.Name)" }
    }
)

# JSON activity graphs are runtime inputs too (including Mercury's population
# and quarter-hour war settings). Keep both supported script formats together.
function Get-RuntimeScripts ($directory) {
    Get-ChildItem -LiteralPath $directory -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @('.lua', '.json') }
}

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

# Dawn is a DLL replacement. It cannot supply the game: packages\ is thousands of files and tens
# of gigabytes. Missing game data is fatal and silent at runtime, so stop here rather than let the
# install look like it worked.
$packages = Join-Path $root 'packages'
$pkgCount = if (Test-Path $packages) { (Get-ChildItem $packages -File -ErrorAction SilentlyContinue).Count } else { 0 }
if ($pkgCount -lt 100) {
    Die @"
No game data at $packages ($pkgCount files).
Dawn replaces steam_api64.dll - it does not install Destiny 2. Point -GameRoot at a real
build 86657 install, or install Sunrise first and confirm it boots.
"@
}
Good "Game data: $pkgCount packages"

# The pristine Steam DLL is the only route back to an unmodified game. Dawn's own backup only
# returns you to whatever was installed before this run.
$original = Join-Path $root '.sunrise\original\steam_api64.dll'
$state    = Join-Path $root '.sunrise\install-state.json'
if (Test-Path $state) {
    $s = Get-Content $state -Raw | ConvertFrom-Json
    Good "Sunrise $($s.releaseTag) installed $($s.installedAtUtc)"
} elseif (Test-Path $original) {
    Note 'No install-state.json, but the Sunrise installer left its original DLL - treating this as a Sunrise install.'
} else {
    Warn 'No Sunrise installer record here. Dawn needs a Sunrise install that has already booted'
    Warn 'once, so its caches and activity SDK pack exist. A first boot without them takes several'
    Warn 'minutes longer than usual and can look like a hang.'
}
if (-not (Test-Path $original)) {
    Warn 'No .sunrise\original\steam_api64.dll - there is no pristine Steam DLL to roll back to.'
    Warn 'This run still backs up whatever is installed now, so -Restore returns you to that.'
}

# First boot regenerates the build_data cache and the activity SDK pack, which is a few hundred MB.
$free = (Get-PSDrive -Name (Split-Path -Qualifier $root).TrimEnd(':') -ErrorAction SilentlyContinue).Free
if ($free -and $free -lt 2GB) {
    Warn "Only $([math]::Round($free/1GB,1)) GB free on $(Split-Path -Qualifier $root). First boot"
    Warn 'regenerates caches and a few hundred MB of activity SDK pack, and will fail if it runs out.'
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
    # PreferredToolArchitecture=x64 picks the 64-bit cl.exe. The project sets
    # MultiProcessorCompilation, so with -m on a many-core machine the 32-bit compiler runs out of
    # address space on the heavier package translation units and fails with
    # "error C1060: compiler is out of heap space". That only happens on a from-scratch build,
    # which is why an incremental build never shows it and a tester's first build always does.
    & $msbuild $Project -p:Configuration=Release -p:Platform=x64 -p:PreferredToolArchitecture=x64 -m -v:minimal -nologo
    if ($LASTEXITCODE -ne 0) {
        Warn 'Build failed. Read the FIRST error above, not the last line.'
        Warn '  C1060 out of heap space  -> the 64-bit toolchain was not used. Check that'
        Warn '     the Hostx64 x64 cl.exe exists under VC/Tools/MSVC/<version>/bin, then retry'
        Warn '     with fewer parallel jobs by changing -m to -m:4 in this script.'
        Warn '  C2220 / warning as error -> this project is /W4 /WX. Fix the warning.'
        Warn '  MSB8020 / v145 not found -> install the v145 build tools. Never downgrade'
        Warn '     PlatformToolset in the .vcxproj to make an older Visual Studio accept it.'
        Die  'Build failed.'
    }
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
foreach ($dll in Get-DllTargets $root) { Backup-File ([IO.Path]::ChangeExtension($dll, '.pdb')) }
foreach ($tree in Get-RuntimeTrees $root) {
    foreach ($resource in $runtimeResources) { Backup-File (Join-Path $tree $resource.Relative) }
    foreach ($name in @('settings.json', 'hud.json', 'movement.json', 'player.json',
                        'player-state.db', 'player-state.db-wal', 'player-state.db-shm')) {
        Backup-File (Join-Path $tree $name)
    }
    $scriptDir = Join-Path $tree 'scripts'
    if (Test-Path $scriptDir) {
        Get-RuntimeScripts $scriptDir |
            ForEach-Object { Backup-File $_.FullName }
    }
}

# ---------------------------------------------------------------- deploy

Step 'Deploying DLL and mission scripts'

foreach ($dll in Get-DllTargets $root) {
    $dir = Split-Path -Parent $dll
    if (-not (Test-Path $dir)) { continue }        # only write into directories that exist
    Copy-Item $BuiltDll $dll -Force
    $builtPdb = [IO.Path]::ChangeExtension($BuiltDll, '.pdb')
    if (Test-Path -LiteralPath $builtPdb) {
        Copy-Item -LiteralPath $builtPdb -Destination ([IO.Path]::ChangeExtension($dll, '.pdb')) -Force
    }
    Note "dll  -> $($dll.Substring($root.Length).TrimStart('\'))"
}

$runtimeScripts = @(Get-RuntimeScripts $Scripts)
$scriptCount = $runtimeScripts.Count
foreach ($tree in Get-RuntimeTrees $root) {
    $parent = Split-Path -Parent $tree
    if (-not (Test-Path $parent)) { continue }
    $scriptDir = Join-Path $tree 'scripts'
    if (-not (Test-Path $scriptDir)) { New-Item -ItemType Directory -Path $scriptDir -Force | Out-Null }
    foreach ($src in $runtimeScripts) {
        $dst = Join-Path $scriptDir $src.Name
        # A development checkout may also be the game root. Its source tree is
        # already current; Copy-Item rejects copying a file onto itself.
        if ([IO.Path]::GetFullPath($src.FullName) -ne [IO.Path]::GetFullPath($dst)) {
            Copy-Item -LiteralPath $src.FullName -Destination $dst -Force
        }
    }
    Note "scripts -> $($scriptDir.Substring($root.Length).TrimStart('\'))  ($scriptCount scripts)"

    foreach ($resource in $runtimeResources) {
        $dst = Join-Path $tree $resource.Relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        if ([IO.Path]::GetFullPath($resource.Source) -ne [IO.Path]::GetFullPath($dst)) {
            Copy-Item -LiteralPath $resource.Source -Destination $dst -Force
        }
    }

    # hud/movement/player are optional at runtime - each store keeps its compiled defaults when the
    # file is absent - but a tester who never gets them runs on those defaults instead of Dawn's
    # tuning. Seed them only when missing, so an existing tester's own settings survive.
    foreach ($name in @('hud.json', 'movement.json', 'player.json')) {
        $src = Join-Path $RepoRoot "Sunrise\resources\default_$name"
        $dst = Join-Path $tree $name
        if ((Test-Path $src) -and -not (Test-Path $dst)) {
            Copy-Item $src $dst -Force
            Note "cfg  -> $($dst.Substring($root.Length).TrimStart('\'))"
        }
    }
}

# ---------------------------------------------------------------- settings

# Settings parsing starts from the compiled defaults and overlays the file, so an ABSENT key keeps
# Dawn's default. But arrival_overrides is an array: if the file has its own, it replaces Dawn's
# entirely - so a stock Sunrise install takes the DLL and still has no way to reach the missions.
# Add only the overrides Dawn ships that the file is missing. Never reorder or drop the user's own.

Step 'Checking settings and mission arrival overrides'

$defaultDoc = Get-Content $Defaults -Raw | ConvertFrom-Json
$wanted     = $defaultDoc.state.activity.arrival_overrides

# The real limit is 1 MiB, not 64 KB: core/settings/settings_runtime.cpp sets
# kConfigCapacity = 1024*1024 and rejects a larger settings.json with fail("too_large"). An earlier
# 64 KB figure here refused perfectly valid installs - the shipped defaults alone are 72 KB on disk.
$SettingsCapacity = 1MB

# Always write compact anyway. Pretty-printing is what makes this file enormous: a 44 KB document
# re-rendered with indentation reached 578 KB. ConvertTo-Json -Compress round-trips this document
# structurally unchanged, verified against the live settings file.
function Write-Settings ($path, $doc, $label, $note) {
    $out   = $doc | ConvertTo-Json -Depth 100 -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetByteCount($out)
    if ($bytes -ge $SettingsCapacity) {
        Warn "$label would be $bytes bytes, past the 1 MiB cap the loader enforces. Left unchanged."
        return $false
    }
    Set-Content -Path $path -Value $out -Encoding UTF8 -NoNewline
    Good "$label  $note  ($bytes bytes)"
    return $true
}

foreach ($tree in Get-RuntimeTrees $root) {
    $parent = Split-Path -Parent $tree
    if (-not (Test-Path $parent)) { continue }          # this layout does not exist here
    if (-not (Test-Path $tree)) { New-Item -ItemType Directory -Path $tree -Force | Out-Null }

    $settingsPath = Join-Path $tree 'settings.json'
    $label        = Get-Relative $settingsPath
    $doc          = $null

    if (Test-Path $settingsPath) {
        try {
            $doc = Get-Content $settingsPath -Raw | ConvertFrom-Json
        } catch {
            # A broken install often has a truncated or hand-edited settings.json. Keep it for
            # diagnosis, then seed a known-good one rather than aborting a half-finished deploy.
            $broken = "$settingsPath.broken-$stamp"
            Move-Item $settingsPath $broken -Force
            Warn "$label was not valid JSON. Kept as $(Split-Path -Leaf $broken); writing Dawn defaults."
        }
    }

    if (-not $doc) {
        # No file, or an unusable one. The shipped defaults already carry every override.
        [void](Write-Settings $settingsPath $defaultDoc $label 'written from Dawn defaults')
        continue
    }

    # A settings file can be valid JSON and still be missing the whole branch.
    if ($null -eq $doc.state) {
        $doc | Add-Member -NotePropertyName state -NotePropertyValue ([pscustomobject]@{}) -Force
    }
    if ($null -eq $doc.state.activity) {
        $doc.state | Add-Member -NotePropertyName activity -NotePropertyValue ([pscustomobject]@{}) -Force
    }
    if ($null -eq $doc.state.activity.arrival_overrides) {
        $doc.state.activity | Add-Member -NotePropertyName arrival_overrides -NotePropertyValue @() -Force
    }

    $have  = @($doc.state.activity.arrival_overrides)
    $names = @($have | ForEach-Object { $_.package_name })
    $added = @()
    foreach ($o in $wanted) {
        if ($names -notcontains $o.package_name) { $have += $o; $added += $o.package_name }
    }

    if ($added.Count -eq 0) { Note "$label already has all $($wanted.Count) overrides"; continue }

    $doc.state.activity.arrival_overrides = $have
    [void](Write-Settings $settingsPath $doc $label "+$($added.Count) overrides: $($added -join ', ')")
}

# ---------------------------------------------------------------- verify

# Verify the on-disk install even when the game is not launched. Record the
# exact deployed payload and backup location so a playtest can identify it.
$installedFiles = @()
foreach ($dll in Get-DllTargets $root) {
    if (-not (Test-Path -LiteralPath (Split-Path -Parent $dll))) { continue }
    $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ($hash -ne $builtHash) { Die "Installed DLL differs from build: $dll" }
    $installedFiles += [pscustomobject]@{ path = (Get-Relative $dll); sha256 = $hash }
    $builtPdb = [IO.Path]::ChangeExtension($BuiltDll, '.pdb')
    if (Test-Path -LiteralPath $builtPdb) {
        $installedPdb = [IO.Path]::ChangeExtension($dll, '.pdb')
        $pdbHash = (Get-FileHash -LiteralPath $installedPdb -Algorithm SHA256).Hash
        if ($pdbHash -ne (Get-FileHash -LiteralPath $builtPdb -Algorithm SHA256).Hash) {
            Die "Installed debug symbols differ from build: $installedPdb"
        }
        $installedFiles += [pscustomobject]@{ path = (Get-Relative $installedPdb); sha256 = $pdbHash }
    }
}
foreach ($tree in Get-RuntimeTrees $root) {
    if (-not (Test-Path -LiteralPath (Split-Path -Parent $tree))) { continue }
    foreach ($resource in $runtimeResources) {
        $dst = Join-Path $tree $resource.Relative
        $hash = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
        if ($hash -ne (Get-FileHash -LiteralPath $resource.Source -Algorithm SHA256).Hash) {
            Die "Installed Festival resource differs from source: $dst"
        }
        $installedFiles += [pscustomobject]@{ path = (Get-Relative $dst); sha256 = $hash }
    }
    foreach ($src in $runtimeScripts) {
        $dst = Join-Path (Join-Path $tree 'scripts') $src.Name
        $hash = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
        if ($hash -ne (Get-FileHash -LiteralPath $src.FullName -Algorithm SHA256).Hash) {
            Die "Installed activity script differs from source: $dst"
        }
        $installedFiles += [pscustomobject]@{ path = (Get-Relative $dst); sha256 = $hash }
    }
}
$receiptDir = Join-Path $root '.dawn\installations'
New-Item -ItemType Directory -Path $receiptDir -Force | Out-Null
$receiptPath = Join-Path $receiptDir "$stamp.json"
[pscustomobject]@{
    installedAtUtc = [DateTime]::UtcNow.ToString('o')
    backupDirectory = $backupDir
    buildDll = $BuiltDll
    verification = 'on-disk hashes; gameplay not verified by this receipt'
    files = $installedFiles
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptPath -Encoding UTF8
Good "Installed DLLs and all $scriptCount activity scripts match their build/source hashes."
Note "Install receipt: $receiptPath"

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
$healthy    = $true
if ($mappedHash -eq $builtHash) {
    Good 'This is the build that was just deployed.'
} else {
    $healthy = $false
    Warn 'The mapped DLL is NOT the build just deployed.'
    Warn "  built:  $($builtHash.Substring(0,32))"
    Warn "  mapped: $($mappedHash.Substring(0,32))"
    Warn 'Something else is shadowing it. Do not trust a playtest until this matches.'
}

# The DLL reads its runtime tree from beside ITSELF. Deploying the DLL to both locations but the
# scripts to only one produces a mission whose C++ and Lua disagree, which reads as "my change did
# nothing". Check the tree that actually won, not the one we hoped would.
$liveRoot    = Split-Path -Parent $mapped
$liveScripts = Join-Path $liveRoot 'Sunrise\scripts'
if (-not (Test-Path $liveScripts)) {
    $healthy = $false
    Warn "No scripts directory beside the mapped DLL: $liveScripts"
} else {
    $stale = @()
    foreach ($src in $runtimeScripts) {
        $dst = Join-Path $liveScripts $src.Name
        if (-not (Test-Path $dst)) { $stale += "$($src.Name) (missing)"; continue }
        if ((Get-FileHash $dst -Algorithm SHA256).Hash -ne (Get-FileHash $src.FullName -Algorithm SHA256).Hash) {
            $stale += "$($src.Name) (differs)"
        }
    }
    if ($stale.Count -eq 0) {
        Good "All $scriptCount activity scripts match beside the mapped DLL."
    } else {
        $healthy = $false
        Warn "Mission scripts beside the mapped DLL do not match this checkout: $($stale -join ', ')"
    }
}

# A settings.json over the loader's cap, or unreadable, fails before the log sinks exist - the
# boot just goes quiet. Catch it here rather than letting the tester stare at a blank screen.
$liveSettings = Join-Path $liveRoot 'Sunrise\settings.json'
if (-not (Test-Path $liveSettings)) {
    Warn "No settings.json beside the mapped DLL: $liveSettings"
} else {
    $sz = (Get-Item $liveSettings).Length
    if ($sz -ge $SettingsCapacity) {
        $healthy = $false
        Warn "settings.json beside the mapped DLL is $sz bytes, past the 1 MiB cap. The loader will reject it."
    } else {
        Good "settings.json beside the mapped DLL: $sz bytes"
    }
}

$log = Join-Path $liveRoot 'Sunrise\logs\sunrise.log'
Note "Runtime tree: $(Join-Path $liveRoot 'Sunrise')"
Note "Log:          $log"

# The log only appears once the host is actually running. Its absence after a successful map is
# the clearest sign the DLL loaded but the host did not come up.
for ($i = 0; $i -lt 12; $i++) {
    if (Test-Path $log) { break }
    Start-Sleep -Seconds 5
}
if (Test-Path $log) {
    Good "Host is writing the log ($((Get-Item $log).Length) bytes)."
} else {
    $healthy = $false
    Warn 'The DLL mapped but no log appeared. The host did not start.'
    Warn 'Usual causes: a settings.json the loader rejected, or a missing Sunrise runtime tree.'
}

Write-Host ''
if ($healthy) {
    Good 'Install verified. Dawn is running the build in this checkout.'
} else {
    Warn 'Install completed with problems - see the warnings above before playtesting.'
    Warn 'Roll back with:  .\tools\install\Install-Dawn.ps1 -Restore'
}
Note 'Once you enter an activity, confirm the graph that loaded:'
Note "  Select-String 'ev=coo_script' '$log'"
if (-not $healthy) { exit 1 }
