# Copy a unique previous runtime beside the DLL into Dawn, preserving the source.
# Copy settings last: its presence marks a completed migration.
function Copy-DawnRuntime {
    param([Parameter(Mandatory)] [string] $Destination)
    $destinationPath = [IO.Path]::GetFullPath($Destination)
    if ((Test-Path -LiteralPath (Join-Path $destinationPath 'settings.json')) -or
        (Test-Path -LiteralPath (Join-Path $destinationPath 'player-state.db'))) { return }
    $parent = Split-Path -Parent $destinationPath
    if (-not (Test-Path -LiteralPath $parent)) { return }
    $sources = @(Get-ChildItem -LiteralPath $parent -Directory | Where-Object {
        $_.FullName -ne $destinationPath -and
        -not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'settings.json') -PathType Leaf)
    })
    if ($sources.Count -gt 1) { throw "Multiple previous runtimes beside $destinationPath. Migration cannot choose an account." }
    if ($sources.Count -eq 0) { return }
    $source = $sources[0].FullName
    New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
    foreach ($leaf in @('player-state.db','player-state.db-wal','player-state.db-shm',
            'hud.json','movement.json','player.json','scripts','cache','event_presets')) {
        $from = Join-Path $source $leaf
        if (Test-Path -LiteralPath $from) {
            if ((Get-Item -LiteralPath $from).Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if (Test-Path -LiteralPath $from -PathType Container) {
                $to = Join-Path $destinationPath $leaf
                New-Item -ItemType Directory -Path $to -Force | Out-Null
                # Runtime trees contain ordinary package caches and scripts. Refuse nested links.
                if (@(Get-ChildItem -LiteralPath $from -Recurse -Attributes ReparsePoint).Count) {
                    throw "Runtime migration does not follow links inside $from."
                }
                Get-ChildItem -LiteralPath $from | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $to -Recurse -Force }
            } else {
                Copy-Item -LiteralPath $from -Destination (Join-Path $destinationPath $leaf) -Force
            }
        }
    }
    Copy-Item -LiteralPath (Join-Path $source 'settings.json') -Destination (Join-Path $destinationPath 'settings.json')
    Write-Host "    Existing account and runtime copied into $destinationPath"
}
