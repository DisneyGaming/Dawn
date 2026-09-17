#requires -Version 5.1
<# Integration tests in disposable fixture folders. Never installs into the supplied game.
The real game EXE is hard-linked read-only for version inspection; it is never launched.
Run with Windows PowerShell 5.1 as well as current PowerShell. #>
[CmdletBinding()]
param(
    [string] $GameExecutable = 'C:\Destiny 2 Development\destiny2.exe',
    [string] $ReleaseDll
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$testRoot = Join-Path $repo ('build/installer-tests/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)
$package = Join-Path $testRoot 'package'
$arguments = @{ Release = 'installer-test'; OutputDirectory = $package }
if ($ReleaseDll) { $arguments.DllPath = $ReleaseDll }
$null = & (Join-Path $repo 'tools/install/New-DawnRelease.ps1') @arguments
$installer = Join-Path $package 'Install-Dawn.ps1'
$manifestPath = Join-Path $package 'release.json'
$manifestText = [IO.File]::ReadAllText($manifestPath)
$manifest = $manifestText | ConvertFrom-Json
$script:passed = 0

function Assert-True([bool] $Value, [string] $Message) {
    if (-not $Value) { throw "ASSERTION FAILED: $Message" }
}
function Write-TestJson([string] $Path, $Value) {
    [IO.File]::WriteAllText($Path, (ConvertTo-Json -InputObject $Value -Depth 100 -Compress), $utf8)
}
function Pass([string] $Message) { $script:passed++; Write-Host "PASS: $Message" }
function Snapshot([string] $Root) {
    return (@(Get-ChildItem -LiteralPath $Root -Recurse -File | Where-Object {
        $_.FullName -notlike "$Root\.dawn\*" -and $_.Name -ne 'destiny2.exe'
    } | Sort-Object FullName | ForEach-Object {
        $_.FullName.Substring($Root.Length + 1) + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash
    }) -join "`n")
}
function New-Game([string] $Name) {
    $root = Join-Path $testRoot $Name
    foreach ($relative in @('', 'bin/x64', 'Sunrise', 'Dawn/scripts', 'bin/x64/Dawn/scripts')) {
        [IO.Directory]::CreateDirectory((Join-Path $root $relative)) | Out-Null
    }
    # Tests and the supplied executable must be on the same NTFS volume for this link.
    New-Item -ItemType HardLink -Path (Join-Path $root 'destiny2.exe') -Target $GameExecutable | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'steam_api64.dll'), 'old root DLL')
    [IO.File]::WriteAllText((Join-Path $root 'bin/x64/steam_api64.dll'), 'old bin DLL')
    [IO.File]::WriteAllText((Join-Path $root 'Dawn/scripts/stale.lua'), 'obsolete content')
    [IO.File]::WriteAllText((Join-Path $root 'bin/x64/Dawn/scripts/stale.lua'), 'obsolete bin content')
    [IO.File]::WriteAllText((Join-Path $root 'Dawn/unknown-personal.txt'), 'keep in backup')
    $settings = Get-Content -LiteralPath (Join-Path $package 'payload/Dawn/settings.json') -Raw | ConvertFrom-Json
    $settings.steam.user.persona_name = 'Fixture Player'
    $settings.client.fade_release = $false
    $settings.state.activity.arrival_overrides[0].bubble = 777
    $settings.state.activity.arrival_overrides += [pscustomobject]@{ package_name = 'fixture_custom'; bubble = 123; slice_set = 984; spawn_set_hash = '0x00000001' }
    Write-TestJson (Join-Path $root 'Sunrise/settings.json') $settings
    Write-TestJson (Join-Path $root 'Sunrise/hud.json') ([pscustomobject]@{ dawn_card = $true })
    Write-TestJson (Join-Path $root 'Sunrise/player.json') ([pscustomobject]@{ infinite_ammo_enabled = $false })
    foreach ($name in @('player-state.db', 'player-state.db-wal', 'player-state.db-shm', 'player-state.db-journal',
            'device_identity.key', 'roster_exclude_keys.txt', 'event_music.txt')) {
        [IO.File]::WriteAllText((Join-Path $root "Sunrise/$name"), "fixture bytes: $name")
    }
    return $root
}
function Expect-Failure([scriptblock] $Action, [string] $Pattern) {
    $message = $null
    try { & $Action | Out-Null } catch { $message = $_.Exception.Message }
    Assert-True ($null -ne $message) "Expected rejection matching $Pattern"
    Assert-True ($message -like $Pattern) "Wrong rejection: $message (expected $Pattern)"
}

$game = New-Game 'migration'
$before = Snapshot $game
& $installer -GameRoot $game -SourceRuntime Sunrise -WhatIf
Assert-True ((Snapshot $game) -ceq $before) 'WhatIf changed installation files'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $game '.dawn'))) 'WhatIf created state'
Pass 'WhatIf validates without creating or replacing files'

& $installer -GameRoot $game -SourceRuntime Sunrise
Assert-True (-not (Test-Path -LiteralPath (Join-Path $game 'Dawn/scripts/stale.lua'))) 'Obsolete script survived'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $game 'bin/x64/Dawn/scripts/stale.lua'))) 'Obsolete bin script survived'
foreach ($name in @('player-state.db', 'player-state.db-wal', 'player-state.db-shm', 'player-state.db-journal',
        'device_identity.key', 'roster_exclude_keys.txt', 'event_music.txt')) {
    $expected = (Get-FileHash -LiteralPath (Join-Path $game "Sunrise/$name")).Hash
    foreach ($runtime in @('Dawn', 'bin/x64/Dawn')) {
        Assert-True ((Get-FileHash -LiteralPath (Join-Path $game "$runtime/$name")).Hash -eq $expected) "Personal data changed: $runtime/$name"
    }
}
$actual = Get-Content -LiteralPath (Join-Path $game 'Dawn/settings.json') -Raw | ConvertFrom-Json
$default = Get-Content -LiteralPath (Join-Path $package 'payload/Dawn/settings.json') -Raw | ConvertFrom-Json
Assert-True ($actual.steam.user.persona_name -eq 'Fixture Player') 'Player name was reset'
Assert-True ($actual.client.fade_release -eq $false) 'Personal choice was reset'
Assert-True ($actual.state.activity.arrival_overrides[0].bubble -eq $default.state.activity.arrival_overrides[0].bubble) 'Shipped arrival was not updated'
Assert-True (@($actual.state.activity.arrival_overrides | Where-Object { $_.package_name -eq 'fixture_custom' }).Count -eq 1) 'Custom arrival was lost'
$hud = Get-Content -LiteralPath (Join-Path $game 'Dawn/hud.json') -Raw | ConvertFrom-Json
Assert-True ($hud.dawn_card -eq $true -and $null -ne $hud.PSObject.Properties['session']) 'HUD preference/default merge failed'
foreach ($entry in $manifest.files) {
    if ($entry.path -match '^Dawn/(settings|hud|movement|player)\.json$') { continue }
    foreach ($prefix in @('', 'bin/x64/')) {
        Assert-True ((Get-FileHash -LiteralPath (Join-Path $game ($prefix + $entry.path))).Hash -eq $entry.sha256) "Payload mismatch: $prefix$($entry.path)"
    }
}
Pass 'Full migration replaces content, preserves database sidecars/preferences, and updates mission arrivals'

$state = Get-Content -LiteralPath (Join-Path $game '.dawn/release.json') -Raw | ConvertFrom-Json
Assert-True (Test-Path -LiteralPath (Join-Path $state.backup 'previous/Dawn/unknown-personal.txt')) 'Unknown original file was not backed up'
[IO.File]::WriteAllText((Join-Path $game 'Dawn/player-state.db'), 'progress since installation')
& $installer -GameRoot $game -Restore
Assert-True ((Snapshot $game) -ceq $before) 'Rollback did not restore exact original installation'
$retained = @(Get-ChildItem -LiteralPath (Join-Path $state.backup 'after-restore') -Filter player-state.db -Recurse -File)
Assert-True (@($retained | Where-Object { [IO.File]::ReadAllText($_.FullName) -eq 'progress since installation' }).Count -eq 1) 'Rollback lost post-install progress'
Pass 'Rollback restores the original files and separately retains newer player progress'

$game = New-Game 'upgrade'
& $installer -GameRoot $game -SourceRuntime Sunrise
[IO.File]::WriteAllText((Join-Path $game 'Dawn/player-state.db'), 'current Dawn progress')
[IO.File]::WriteAllText((Join-Path $game 'Dawn/scripts/removed-in-new-release.lua'), 'old release script')
$before = Snapshot $game
& $installer -GameRoot $game
Assert-True ([IO.File]::ReadAllText((Join-Path $game 'Dawn/player-state.db')) -eq 'current Dawn progress') 'Upgrade selected an old Sunrise profile'
Assert-True ([IO.File]::ReadAllText((Join-Path $game 'bin/x64/Dawn/player-state.db')) -eq 'current Dawn progress') 'The two runtime profiles disagree'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $game 'Dawn/scripts/removed-in-new-release.lua'))) 'Upgrade retained obsolete content'
# A standalone copy can restore without needing any of the package's payload files.
$recovery = Join-Path $testRoot 'Recover-Dawn.ps1'
Copy-Item -LiteralPath $installer -Destination $recovery
& $recovery -GameRoot $game -Restore
Assert-True ((Snapshot $game) -ceq $before) 'Upgrade rollback did not restore previous Dawn state'
Pass 'An existing Dawn upgrade selects current saves, removes stale content, and restores without a payload'

$game = New-Game 'failed-copy'
$before = Snapshot $game
$lockedFile = [IO.File]::Open((Join-Path $game 'bin/x64/steam_api64.dll'), 'Open', 'ReadWrite', 'None')
try { Expect-Failure { & $installer -GameRoot $game -SourceRuntime Sunrise } '*previous files were restored*' }
finally { $lockedFile.Dispose() }
Assert-True ((Snapshot $game) -ceq $before) 'Failed replacement did not roll back'
Pass 'A locked second DLL rolls back already-replaced DLL/runtime files'

$game = New-Game 'tampered'
$before = Snapshot $game
$tampered = Join-Path $package 'payload/Dawn/scripts/omega.lua'
$original = [IO.File]::ReadAllBytes($tampered)
try {
    [IO.File]::AppendAllText($tampered, '-- changed')
    Expect-Failure { & $installer -GameRoot $game -SourceRuntime Sunrise } '*missing or changed*'
} finally { [IO.File]::WriteAllBytes($tampered, $original) }
Assert-True ((Snapshot $game) -ceq $before) 'Tampered package touched game'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $game '.dawn'))) 'Tampered package created state'
Pass 'Checksum mismatch rejects the package before changes'

try {
    $bad = $manifestText | ConvertFrom-Json
    $bad.files[0].path = '../outside.dll'
    Write-TestJson $manifestPath $bad
    Expect-Failure { & $installer -GameRoot $game -SourceRuntime Sunrise } '*Invalid or duplicate*'
} finally { [IO.File]::WriteAllText($manifestPath, $manifestText, $utf8) }
Pass 'Manifest traversal is rejected'

$game = New-Game 'ambiguous'
Copy-Item -LiteralPath (Join-Path $game 'Sunrise/settings.json') -Destination (Join-Path $game 'Dawn/settings.json')
$before = Snapshot $game
Expect-Failure { & $installer -GameRoot $game } '*Several player runtimes*'
Assert-True ((Snapshot $game) -ceq $before) 'Ambiguous source changed files'
Pass 'Ambiguous player data requires an explicit source'

$game = New-Game 'old-settings'
$settingsPath = Join-Path $game 'Sunrise/settings.json'
$oldSettings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
$oldSettings.version = 5
Write-TestJson $settingsPath $oldSettings
$before = Snapshot $game
Expect-Failure { & $installer -GameRoot $game -SourceRuntime Sunrise } '*unsupported layout*'
Assert-True ((Snapshot $game) -ceq $before) 'Unsupported settings changed files'
Pass 'Unsupported settings layout cannot silently reset a profile'

$game = New-Game 'interrupted'
$before = Snapshot $game
& $installer -GameRoot $game -SourceRuntime Sunrise
$state = Get-Content -LiteralPath (Join-Path $game '.dawn/release.json') -Raw | ConvertFrom-Json
$journalPath = Join-Path $state.backup 'journal.json'
$journal = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
$journal.state = 'installing'
Write-TestJson $journalPath $journal
Expect-Failure { & $installer -GameRoot $game } '*interrupted installation needs recovery*'
& $installer -GameRoot $game -Restore -BackupPath $state.backup
Assert-True ((Snapshot $game) -ceq $before) 'Interrupted transaction did not recover'
Pass 'Interrupted installations block another update and can be recovered'

$game = New-Game 'junction'
$external = Join-Path $testRoot 'outside-runtime'
[IO.Directory]::CreateDirectory($external) | Out-Null
[IO.File]::WriteAllText((Join-Path $external 'keep.txt'), 'outside data')
New-Item -ItemType Junction -Path (Join-Path $game 'Dawn/linked') -Target $external | Out-Null
Expect-Failure { & $installer -GameRoot $game -SourceRuntime Sunrise } '*Linked entry*'
Assert-True ([IO.File]::ReadAllText((Join-Path $external 'keep.txt')) -eq 'outside data') 'Junction target changed'
Pass 'Nested junctions cannot redirect runtime replacement'

$game = Join-Path $testRoot 'wrong-build'
[IO.Directory]::CreateDirectory($game) | Out-Null
Copy-Item -LiteralPath "$env:WINDIR\System32\whoami.exe" -Destination (Join-Path $game 'destiny2.exe')
Expect-Failure { & $installer -GameRoot $game } '*Unsupported Destiny build*'
Pass 'Wrong executable version is rejected'

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($package + '.zip')
try {
    $extra = @($zip.Entries | Where-Object { $_.FullName -match '(^|/)(src|tools|tests|cache|logs)/|\.pdb$|player-state\.db' })
    Assert-True ($extra.Count -eq 0) 'Player ZIP contains development/private files'
} finally { $zip.Dispose() }
Pass 'Distributable ZIP excludes development tools, symbols, caches, and saves'
Write-Host "$script:passed integration checks passed on PowerShell $($PSVersionTable.PSVersion). Fixtures: $testRoot"
