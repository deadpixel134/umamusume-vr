param([string]$ZigPath = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspace = (Resolve-Path (Join-Path $root '..\..')).Path
$source = Get-Content -LiteralPath (Join-Path $root 'src\immersive.c') -Raw
$unchecked = @([regex]::Matches($source, '(?m)^.*(?<![\w])snprintf\(.*$') | ForEach-Object Value)
if ($unchecked.Count -ne 2 -or @($unchecked | Where-Object { $_ -notlike '*int needle_length = snprintf(needle,*' }).Count) {
    throw 'Unchecked telemetry producer formatting found (only the two checked settings-key builders are exempt)'
}
if ($source.Contains('first_stereo_pair_submitted') -or !$source.Contains('XrResult release_result = pxrReleaseSwapchainImage') -or
    !$source.Contains('xr_cache_telemetry = game_pair_telemetry;') -or !$source.Contains('stereo_end_accepted(&xr_frame_telemetry, (long)endr)')) {
    throw 'Frame event/release/cache integration missing'
}
if (!$ZigPath) { $ZigPath = Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe' }
$testRoot = Join-Path $root ('build\telemetry-tests-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$exe = Join-Path $testRoot 'telemetry_harness.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'telemetry_harness.c') -o $exe
if ($LASTEXITCODE) { throw 'Telemetry harness compile failed' }
$savedLocalAppData = $env:LOCALAPPDATA
$strictUtf8 = [Text.UTF8Encoding]::new($false,$true)
$results = @()
try {
    foreach ($mode in @('format','quota','byte_quota','concurrent','write_failure','short_write','size_failure','open_failure','primary_open_failure','health_then_veh','veh_then_health','health_veh_concurrent','health_veh_recursive','health_write_failure','health_short_write','veh','veh_write_failure','veh_short_write','conflict','rotate')) {
        $env:LOCALAPPDATA = Join-Path $testRoot $mode
        $dir = Join-Path $env:LOCALAPPDATA 'UmaVR'
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        if ($mode -eq 'rotate') {
            [IO.File]::WriteAllBytes((Join-Path $dir 'immersive-001.log'), [byte[]]::new(65536))
            [IO.File]::WriteAllBytes((Join-Path $dir 'immersive-001.emergency.log'), [byte[]]::new(1024))
            [IO.File]::WriteAllBytes((Join-Path $dir 'immersive-001.health.log'), [byte[]]::new(512))
        }
        $proc = Start-Process -FilePath $exe -ArgumentList $mode -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $testRoot "$mode.stderr")
        if (!$proc.WaitForExit(15000)) { $proc.Kill(); throw "Telemetry $mode timed out (possible logger reentry deadlock)" }
        if ($proc.ExitCode -ne 0) { throw "Telemetry $mode failed: $(Get-Content (Join-Path $testRoot "$mode.stderr") -Raw)" }
        $log = Join-Path $dir 'immersive-001.log'
        $rows = @()
        if ((Test-Path $log) -and $mode -ne 'short_write') {
            $raw = $strictUtf8.GetString([IO.File]::ReadAllBytes($log))
            foreach ($line in ($raw -split "`r`n" | Where-Object { $_ })) {
                $doc = [Text.Json.JsonDocument]::Parse($line); $doc.Dispose()
                $rows += $line | ConvertFrom-Json
            }
            if ((Get-Item $log).Length -gt 65536 -or $rows.Count -gt 256) { throw "$mode exceeded primary bounds" }
            $seq = @($rows.seq | Where-Object { $null -ne $_ })
            if (@($seq | Select-Object -Unique).Count -ne $seq.Count) { throw "$mode duplicate sequence" }
        }
        if ($mode -in @('quota','byte_quota','concurrent')) {
            $expectedTerminal = if ($mode -eq 'concurrent') { 1 } else { 2 }
            if (@($rows | Where-Object event -eq 'session_summary').Count -ne $expectedTerminal -or @($rows | Where-Object event -eq 'telemetry_summary').Count -ne $expectedTerminal) { throw "$mode lost terminal reserve" }
            if (@($rows | Where-Object event -eq 'telemetry_saturated').Count -ne 1) { throw "$mode saturation notification unbounded/missing" }
            if ($mode -ne 'concurrent' -and @($rows | Where-Object event -eq 'disabled').Count -lt 1) { throw "$mode lost important error reserve" }
        }
        if ($mode -eq 'format') {
            $generation = @($rows | Where-Object event -eq 'source_generation_created')
            if ($generation.Count -ne 1 -or !$generation[0].original_camera_restored -or $generation[0].context -ne 'Home') { throw 'Generation JSON remains truncated' }
            if (@($rows | Where-Object event -eq 'unicode')[0].text -ne "quote`"slash\`n한글") { throw 'UTF-8 escaping failed' }
            if (@($rows | Where-Object event -eq 'telemetry_format_error').Count -lt 1) { throw 'Outer format failure not observable' }
            $accepted = @($rows | Where-Object event -eq 'first_stereo_frame_end_accepted')[0]
            if ($accepted.end_result -ne 0 -or !$accepted.end_called -or !$accepted.stereo_layer -or !$accepted.cache_reused -or $accepted.pair_serial -ne 101) { throw 'Frame acceptance/cache attribution failed' }
        }
        foreach ($terminal in @($rows | Where-Object { $_.event -in @('session_summary','telemetry_summary') })) {
            $terminalLine = @($raw -split "`r`n" | Where-Object { $_ -like ('*"seq":' + $terminal.seq + ',*') })[0]
            if ($strictUtf8.GetByteCount($terminalLine + "`r`n") -gt 512) { throw 'Terminal record exceeded 512 bytes' }
        }
        $primaryFailures = @('write_failure','short_write','size_failure','primary_open_failure')
        $healthCases = $primaryFailures + @('health_then_veh','veh_then_health','health_veh_concurrent','health_veh_recursive')
        if ($mode -in $healthCases) {
            $healthPath = Join-Path $dir 'immersive-001.health.log'
            $healthRaw = $strictUtf8.GetString([IO.File]::ReadAllBytes($healthPath))
            $doc = [Text.Json.JsonDocument]::Parse($healthRaw); $doc.Dispose()
            $health = $healthRaw | ConvertFrom-Json
            $expectedStage = switch ($mode) { 'primary_open_failure' {'primary_open'} 'size_failure' {'primary_size'} 'short_write' {'primary_short_write'} default {'primary_write'} }
            $expectedError = switch ($mode) { 'primary_open_failure' {5} 'size_failure' {5} 'short_write' {0} default {112} }
            if ($health.event -ne 'telemetry_health' -or $health.stage -ne $expectedStage -or $health.win32_error -ne $expectedError -or
                $health.build_id -ne 'test-source-identity' -or $health.pid -le 0 -or $health.attach_id -le 0 -or $health.utc.Count -ne 7) { throw "$mode lost exact primary health failure/run identity" }
            if (($mode -eq 'primary_open_failure' -and ($health.requested -ne 0 -or $health.written -ne 0)) -or
                ($mode -ne 'primary_open_failure' -and $health.requested -le 0) -or
                ($mode -ne 'short_write' -and $health.written -ne 0)) { throw "$mode incorrect requested/written bytes" }
            if ($mode -eq 'short_write' -and ($health.written -ne [math]::Floor($health.requested/2) -or $health.written -ne (Get-Item $log).Length)) { throw 'Short-write health byte counts incorrect' }
            if (@($healthRaw -split "`r`n" | Where-Object {$_}).Count -ne 1) { throw 'Health latch did not bound records' }
        }
        if ($mode -eq 'health_write_failure' -and (Get-Item (Join-Path $dir 'immersive-001.health.log')).Length -ne 0) { throw 'Failed health write unexpectedly emitted' }
        if ($mode -eq 'health_short_write') {
            $partialHealth = $strictUtf8.GetString([IO.File]::ReadAllBytes((Join-Path $dir 'immersive-001.health.log')))
            $partialRejected = $false
            try { $doc = [Text.Json.JsonDocument]::Parse($partialHealth); $doc.Dispose() } catch { $partialRejected = $true }
            if (!$partialRejected) { throw 'Short health write fixture must demonstrate incomplete JSON limitation' }
        }
        if ($mode -in ($healthCases + @('veh','health_write_failure','health_short_write'))) {
            $side = Join-Path $dir 'immersive-001.emergency.log'
            $line = $strictUtf8.GetString([IO.File]::ReadAllBytes($side))
            $doc = [Text.Json.JsonDocument]::Parse($line); $doc.Dispose()
            if ((Get-Item $side).Length -gt 512 -or @($line -split "`r`n" | Where-Object {$_}).Count -ne 1) { throw 'Emergency fault bound failed' }
            $crash = $line | ConvertFrom-Json
            if ($crash.event -ne 'emergency_fault' -or $crash.code -ne 3221225477) { throw 'Health suppressed/corrupted crash evidence' }
            if ($mode -in $healthCases -and ($crash.pid -ne $health.pid -or $crash.attach_id -ne $health.attach_id -or $crash.build_id -ne $health.build_id)) { throw 'Health/crash run identity mismatch' }
        }
        $sideBytes = (Get-ChildItem $dir -File | Where-Object { $_.Name -match '^immersive-001\.(health|emergency)\.log$' } | Measure-Object -Sum Length).Sum
        if ($sideBytes -gt 1024 -or @(Get-ChildItem $dir -File | Where-Object { $_.Name -match '^immersive-001\.(health|emergency)\.log$' -and $_.Length -gt 512 }).Count) { throw "$mode emergency slots exceeded 512+512 bytes" }
        if ($mode -eq 'rotate') {
            if ((Get-Item (Join-Path $dir 'immersive-001.previous.log')).Length -ne 65536) { throw 'Old full log was not retained' }
            if (!$rows.Count) { throw 'Old full log blocked new run' }
            $bytes = (Get-ChildItem $dir -Filter '*.log' | Measure-Object -Sum Length).Sum
            if ($bytes -gt 133120) { throw 'Current+previous retention exceeded 130KiB' }
            # A second process/run rotates once more: retain exactly current+previous.
            $firstAttach = $rows[0].attach_id
            & $exe rotate
            if ($LASTEXITCODE) { throw 'Second rotation failed' }
            $second = Get-Content $log | ForEach-Object { $_ | ConvertFrom-Json }
            if ($second[0].attach_id -eq $firstAttach) { throw 'Attach identity was reused' }
            if (@(Get-ChildItem $dir -Filter '*.log').Count -ne 6) { throw 'More than current+previous primary/health/crash retained' }
            if ((Get-ChildItem $dir -Filter '*.log' | Measure-Object -Sum Length).Sum -gt 133120) { throw 'Second rotation exceeded 130KiB' }
        }
        $results += [pscustomobject]@{ mode=$mode; result='PASS'; records=$rows.Count }
    }
} finally { $env:LOCALAPPDATA=$savedLocalAppData }
$results | ConvertTo-Json | Set-Content (Join-Path $testRoot 'results.json') -Encoding utf8NoBOM
$results | Format-Table -AutoSize
Write-Host "Telemetry harness evidence: $testRoot"
