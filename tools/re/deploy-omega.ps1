[CmdletBinding()]
param(
    [string]$SourcePath = (Join-Path $PSScriptRoot 'build\omega-src-port\Release\steam_api64.dll'),
    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string]$ExpectedSha256 = 'F4FFA03E31DDC8A0F5927D4038448AA71DE2870A649E0064873D1EB34F1FE92F'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Install in the game root. Keep a verified backup before replacing the DLL.
$gameRoot = $PSScriptRoot
$targetPath = Join-Path $gameRoot 'steam_api64.dll'
if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'destiny2.exe') -PathType Leaf)) {
    throw 'Run this script from the Destiny 2 game directory containing destiny2.exe.'
}
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) {
    throw 'Close Destiny 2 before deploying Omega.'
}

$resolvedSource = (Resolve-Path -LiteralPath $SourcePath).ProviderPath
$sourceHash = (Get-FileHash -LiteralPath $resolvedSource -Algorithm SHA256).Hash
if ($sourceHash -ne $ExpectedSha256) {
    throw "Source hash mismatch. Expected $ExpectedSha256; found $sourceHash."
}

$previousHash = $null
if (Test-Path -LiteralPath $targetPath -PathType Leaf) {
    $previousHash = (Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash
    if ($previousHash -eq $sourceHash) {
        Write-Output "Omega is already installed: $targetPath"
        Write-Output "SHA-256: $sourceHash"
        return
    }
}

$deploymentId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([guid]::NewGuid().ToString('N').Substring(0, 8))
$backupDirectory = Join-Path $gameRoot ('.sunrise\backups\omega-' + $deploymentId)
$null = New-Item -ItemType Directory -Path $backupDirectory
$backupPath = $null
if ($previousHash) {
    $backupPath = Join-Path $backupDirectory 'steam_api64.dll'
    Copy-Item -LiteralPath $targetPath -Destination $backupPath
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -ne $previousHash) {
        throw "Backup verification failed: $backupPath"
    }
}

# Check again immediately before installation in case the game was launched.
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) {
    throw "Close Destiny 2 before deploying Omega. Backup: $backupPath"
}
try {
    Copy-Item -LiteralPath $resolvedSource -Destination $targetPath -Force
    if ((Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash -ne $sourceHash) {
        throw 'Installed DLL verification failed.'
    }
} catch {
    $installationError = $_
    if ($backupPath) {
        Copy-Item -LiteralPath $backupPath -Destination $targetPath -Force
        if ((Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash -ne $previousHash) {
            throw "Deployment and rollback verification failed. Restore the backup at $backupPath."
        }
        Write-Warning 'Deployment failed; the previous DLL has been restored.'
    }
    throw $installationError
}

$receiptPath = Join-Path $backupDirectory 'deployment.json'
[ordered]@{
    deployedAt = (Get-Date).ToString('o')
    source = $resolvedSource
    installed = $targetPath
    sha256 = $sourceHash
    backup = $backupPath
    previousSha256 = $previousHash
} | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8

Write-Output "Deployed Omega: $targetPath"
Write-Output "SHA-256: $sourceHash"
Write-Output "Previous DLL backup: $backupPath"
Write-Output "Deployment receipt: $receiptPath"
