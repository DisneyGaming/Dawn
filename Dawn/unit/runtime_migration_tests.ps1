$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/install/RuntimeMigration.ps1')
$testRoot = Join-Path $PSScriptRoot "../../build/unit/runtime-migration/$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$source = Join-Path $testRoot 'previous-runtime'
$target = Join-Path $testRoot 'Dawn'
New-Item -ItemType Directory -Path (Join-Path $source 'scripts') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $source 'settings.json') -Value '{"existing":true}'
Set-Content -LiteralPath (Join-Path $source 'player-state.db') -Value 'test account bytes'
Set-Content -LiteralPath (Join-Path $source 'scripts/mission.lua') -Value 'preserve existing script'
$original = (Get-FileHash -LiteralPath (Join-Path $source 'player-state.db')).Hash
Copy-DawnRuntime -Destination $target
if ((Get-FileHash -LiteralPath (Join-Path $target 'player-state.db')).Hash -ne $original) { throw 'Save changed during migration' }
if ((Get-FileHash -LiteralPath (Join-Path $source 'player-state.db')).Hash -ne $original) { throw 'Source changed during migration' }
if (-not (Test-Path -LiteralPath (Join-Path $target 'scripts/mission.lua'))) { throw 'Script missing after migration' }
Set-Content -LiteralPath (Join-Path $target 'player-state.db') -Value 'new Dawn save'
$edited = (Get-FileHash -LiteralPath (Join-Path $target 'player-state.db')).Hash
Copy-DawnRuntime -Destination $target
if ((Get-FileHash -LiteralPath (Join-Path $target 'player-state.db')).Hash -ne $edited) { throw 'Repeated migration overwrote Dawn save' }
Remove-Item -LiteralPath (Join-Path $target 'settings.json')
Copy-DawnRuntime -Destination $target
if ((Get-FileHash -LiteralPath (Join-Path $target 'player-state.db')).Hash -ne $edited) { throw 'Settings reset triggered account overwrite' }
$ambiguous = Join-Path $testRoot 'ambiguous'
foreach ($name in @('one','two')) {
    New-Item -ItemType Directory -Path (Join-Path $ambiguous $name) -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $ambiguous "$name/settings.json") -Value '{}'
}
$rejected = $false
try { Copy-DawnRuntime -Destination (Join-Path $ambiguous 'Dawn') } catch { $rejected = $true }
if (-not $rejected) { throw 'Ambiguous runtime was silently selected' }
Write-Host 'PASS: account, settings, scripts, source preservation, idempotence, and ambiguity checks.'
