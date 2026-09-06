[CmdletBinding()]
param(
    [string]$ValidationDirectory = 'build\coo\validation-opening',
    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string]$ExpectedInstalledHash = '677B39E4FD50ECB83A9E5318F22E8A35A6568CCF40AD1A7956519CF6ADF91435',
    [string]$Mode = 'omega-opening',
    [string]$PreviousScriptPath,
    [string]$ExpectedPreviousScriptHash
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation = [IO.Path]::GetFullPath((Join-Path $taskRoot $ValidationDirectory))
$validationRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'build\coo')) + '\'
if (-not $validation.StartsWith($validationRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Validation output must stay under build/coo.'
}
if (Test-Path -LiteralPath (Join-Path $validation 'installation.json')) {
    throw 'This candidate already has an installation receipt; preserve that evidence.'
}
$results = Get-Content -Raw -LiteralPath (Join-Path $validation 'results.json') | ConvertFrom-Json
$candidate = @($results | Where-Object { $_.project -eq 'Sunrise' })
if ($candidate.Count -ne 1) { throw 'Expected one validated candidate DLL.' }
$candidate = $candidate[0]
$source = [IO.Path]::GetFullPath($candidate.dll)
if (-not $source.StartsWith($validation + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Candidate must be inside the validated output directory.'
}
$expectedBaseline = $ExpectedInstalledHash
$target = Join-Path $taskRoot 'steam_api64.dll'
$settingsPath = Join-Path $taskRoot 'Sunrise\settings.json'
$scriptPath = Join-Path $taskRoot 'Sunrise\scripts\omega.json'
$scriptHash = $null
if ($Mode -in @('omega-json-script', 'omega-generic-script')) {
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $validation 'candidate-source.json') | ConvertFrom-Json
    $scriptHash = $manifest.'Sunrise/scripts/omega.json'
    if (-not $scriptHash -or (Get-FileHash -LiteralPath $scriptPath).Hash -ne $scriptHash) {
        throw 'Omega JSON differs from the validated candidate. Validate the edited script before installing.'
    }
}

$previousScript = $null
if ($Mode -eq 'omega-generic-script') {
    if (-not $PreviousScriptPath -or $ExpectedPreviousScriptHash -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'The format migration requires the accepted previous script and its hash for rollback.'
    }
    $previousScript = [IO.Path]::GetFullPath((Join-Path $taskRoot $PreviousScriptPath))
    if (-not $previousScript.StartsWith($taskRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        (Get-FileHash -LiteralPath $previousScript).Hash -ne $ExpectedPreviousScriptHash) {
        throw 'Previous script path or accepted hash mismatch.'
    }
}
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Close Destiny 2 before installation.' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $candidate.sha256) { throw 'Candidate DLL hash mismatch.' }
$beforeDll = (Get-FileHash -LiteralPath $target).Hash
$beforeSettings = (Get-FileHash -LiteralPath $settingsPath).Hash
if ($beforeDll -ne $expectedBaseline) { throw 'Installed DLL differs from the expected accepted candidate; review before installing.' }
$text = [IO.File]::ReadAllText($settingsPath)
$pattern = '(?s)("experiments"\s*:\s*\{\s*"omega"\s*:\s*\{)([^{}]*)(\})'
$matches = [regex]::Matches($text, $pattern)
if ($matches.Count -ne 1) { throw 'Expected one flat experiments.omega settings object.' }
$match = $matches[0]
$body = $match.Groups[2].Value
if ($body -match '"coo_executor"\s*:') {
    if ([regex]::Matches($body, '"coo_executor"\s*:\s*(true|false)').Count -ne 1) {
        throw 'Invalid or duplicate executor selector.'
    }
    $body = [regex]::Replace($body, '"coo_executor"\s*:\s*(true|false)', '"coo_executor": true')
} else {
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $separator = if ($body.Trim().Length -gt 0) { ',' } else { '' }
    $body = $newline + '      "coo_executor": true' + $separator + $body
}
$updated = $text.Substring(0, $match.Index) + $match.Groups[1].Value + $body +
    $match.Groups[3].Value + $text.Substring($match.Index + $match.Length)
if (-not ($updated | ConvertFrom-Json).experiments.omega.coo_executor) { throw 'Staged selector failed validation.' }
$backup = Join-Path $taskRoot ('.sunrise\backups\coo-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$null = New-Item -ItemType Directory -Path $backup
Copy-Item -LiteralPath $target -Destination (Join-Path $backup 'steam_api64.dll')
Copy-Item -LiteralPath $settingsPath -Destination (Join-Path $backup 'settings.json')
if ($previousScript) {
    Copy-Item -LiteralPath $previousScript -Destination (Join-Path $backup 'previous-omega.json')
    if ((Get-FileHash -LiteralPath (Join-Path $backup 'previous-omega.json')).Hash -ne $ExpectedPreviousScriptHash) {
        throw 'Previous script rollback backup mismatch.'
    }
}

if ($scriptHash) {
    Copy-Item -LiteralPath $scriptPath -Destination (Join-Path $backup 'candidate-omega.json')
    if ((Get-FileHash -LiteralPath (Join-Path $backup 'candidate-omega.json')).Hash -ne $scriptHash) {
        throw 'Candidate script backup verification failed.'
    }
}

if ((Get-FileHash -LiteralPath (Join-Path $backup 'steam_api64.dll')).Hash -ne $beforeDll -or
    (Get-FileHash -LiteralPath (Join-Path $backup 'settings.json')).Hash -ne $beforeSettings) {
    throw 'Backup verification failed.'
}
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Destiny 2 started during staging.' }
if ((Get-FileHash -LiteralPath $target).Hash -ne $beforeDll -or
    (Get-FileHash -LiteralPath $settingsPath).Hash -ne $beforeSettings) { throw 'Installation changed during staging.' }
try {
    Copy-Item -LiteralPath $source -Destination $target -Force
    [IO.File]::WriteAllText($settingsPath, $updated, [Text.UTF8Encoding]::new($false))
    if ((Get-FileHash -LiteralPath $target).Hash -ne $candidate.sha256) { throw 'Installed DLL hash mismatch.' }
    if ($scriptHash -and (Get-FileHash -LiteralPath $scriptPath).Hash -ne $scriptHash) {
        throw 'Omega JSON changed during installation.'
    }

    if (-not (Get-Content -Raw -LiteralPath $settingsPath | ConvertFrom-Json).experiments.omega.coo_executor) {
        throw 'Installed selector validation failed.'
    }
} catch {
    Copy-Item -LiteralPath (Join-Path $backup 'steam_api64.dll') -Destination $target -Force
    Copy-Item -LiteralPath (Join-Path $backup 'settings.json') -Destination $settingsPath -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne $beforeDll -or
        (Get-FileHash -LiteralPath $settingsPath).Hash -ne $beforeSettings) {
        throw "Rollback verification failed. Restore files from $backup."
    }
    if ($previousScript) {
        Copy-Item -LiteralPath $scriptPath -Destination (Join-Path $backup 'failed-install-omega.json')
        Copy-Item -LiteralPath (Join-Path $backup 'previous-omega.json') -Destination $scriptPath -Force
        if ((Get-FileHash -LiteralPath $scriptPath).Hash -ne $ExpectedPreviousScriptHash) { throw 'Script rollback verification failed.' }
    }
    throw
}
[ordered]@{ dllSha256=$candidate.sha256; previousDllSha256=$beforeDll; backup=$backup;
    previousSettingsSha256=$beforeSettings; settingsSha256=(Get-FileHash -LiteralPath $settingsPath).Hash;
    installedAt=(Get-Date).ToString('o'); mode=$Mode; gameLaunched=$false;
    scriptPath=$(if ($scriptHash) { $scriptPath } else { $null }); scriptSha256=$scriptHash; previousScriptSha256=$ExpectedPreviousScriptHash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $validation 'installation.json') -Encoding utf8
Write-Output "Installed validated CoO candidate. Backup: $backup"
Write-Output 'Launch through your usual CMD. This script does not launch Destiny or clear the cache.'
