[CmdletBinding()]
param([switch]$FinalizeOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$pipelineRoot = Join-Path $taskRoot 'build/omega-full-20260905'
$contract = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'contract.json') -Raw | ConvertFrom-Json
if ((Get-FileHash -LiteralPath (Join-Path $taskRoot 'destiny2_unpacked.bin')).Hash -cne $contract.native_image_sha256) {
    throw 'Native image changed. Requalify the addresses, ABIs and fixtures before building.'
}
Push-Location $taskRoot
try {
    $collectorTests = & python (Join-Path $PSScriptRoot 'test_collect.py') 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Collector tests failed: $collectorTests" }
    if (!$FinalizeOnly) {
        & (Join-Path $pipelineRoot 'freeze_candidate.ps1')
        & python (Join-Path $pipelineRoot 'build_rescue_phase.py')
        if ($LASTEXITCODE -ne 0) { throw 'Frozen build or native regression tests failed.' }
    }
    $candidate = [IO.Path]::GetFullPath((Get-Content -LiteralPath (Join-Path $pipelineRoot 'candidate-path.txt') -Raw).Trim())
    if (!$candidate.StartsWith($pipelineRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Candidate is outside the expected pipeline directory.'
    }
    $manifestPath = Join-Path $candidate 'candidate-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.tests.Count -ne 18 -or $manifest.status -notin @(
        'verified candidate; installation waiting for game to close',
        'verified discovery candidate; native mapping incomplete')) { throw 'Full candidate verification has not completed.' }
    if ((Get-FileHash -LiteralPath (Join-Path $candidate 'out/steam_api64.dll')).Hash -cne $manifest.dll_sha256) {
        throw 'Candidate DLL differs from the tested artifact.'
    }
    . (Join-Path $taskRoot 'tools/build/source_manifest.ps1')
    $paths = Get-Content -LiteralPath (Join-Path $candidate 'evidence/source-manifest.tsv') | ForEach-Object { ($_ -split "`t",3)[2] }
    $working = New-CanonicalSourceManifest -SourceRoot $taskRoot -Paths $paths
    if ($working.SourceSha256 -cne $manifest.source_sha256) { throw 'Working source differs from tested frozen source.' }
    $manifest | Add-Member -NotePropertyName working_source_matches_frozen -NotePropertyValue $true -Force
    $manifest | Add-Member -NotePropertyName eye_diagnostics -NotePropertyValue $contract -Force
    $manifest.live_validation = $contract.live_validation
    if (!$contract.replay_ready) { $manifest.status = 'verified discovery candidate; native mapping incomplete' }
    $evidence = Join-Path $candidate 'evidence/troubleshooting'
    [IO.Directory]::CreateDirectory($evidence) | Out-Null
    $hashes = @()
    foreach ($name in @('build.ps1', 'collect.py', 'test_collect.py', 'contract.json', 'README.md')) {
        $source = Join-Path $PSScriptRoot $name
        Copy-Item -LiteralPath $source -Destination (Join-Path $evidence $name) -Force
        $hashes += [ordered]@{ name = $name; sha256 = (Get-FileHash -LiteralPath $source).Hash }
    }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'tool-hashes.json') -Encoding utf8
    $research = Join-Path $evidence 'native-research'
    [IO.Directory]::CreateDirectory($research) | Out-Null
    $researchHashes = @()
    foreach ($item in Get-ChildItem -LiteralPath $pipelineRoot -File | Where-Object { $_.Name -like 'eye-*.txt' }) {
        Copy-Item -LiteralPath $item.FullName -Destination (Join-Path $research $item.Name) -Force
        $researchHashes += [ordered]@{ name = $item.Name; sha256 = (Get-FileHash -LiteralPath $item.FullName).Hash }
    }
    $researchHashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $research 'evidence-hashes.json') -Encoding utf8
    $collectorTests | Set-Content -LiteralPath (Join-Path $evidence 'collector-tests.log') -Encoding utf8
    $manifest | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    "Verified $($manifest.build_id): $($manifest.status)"
    "Manifest: $manifestPath"
} finally { Pop-Location }
