param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\projection-observe-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$source=Get-Content -LiteralPath (Join-Path $root 'src\base_producer.h') -Raw
$predicate=[regex]::Match($source,'for\(unsigned i=0;i<16;\+\+i\) if\(!isfinite\(projection\.m\[i\]\).*?bp_fail\("owned_projection_changed_before_capture"\);',[Text.RegularExpressions.RegexOptions]::Singleline).Value
if(!$predicate){throw 'Exact production projection predicate not found'}
Set-Content -LiteralPath (Join-Path $out 'projection_product_predicate.inc') -Value $predicate -Encoding utf8NoBOM
$telemetry=Get-Content -LiteralPath (Join-Path $root 'src\telemetry.h') -Raw
$start=$telemetry.IndexOf('static BOOL telemetry_utf8(')
$end=$telemetry.IndexOf('typedef struct TelemetryStats')
if($start -lt 0 -or $end -le $start){throw 'Existing telemetry parser/escape boundary missing'}
Set-Content -LiteralPath (Join-Path $out 'projection_telemetry_json.inc') -Value $telemetry.Substring($start,$end-$start) -Encoding utf8NoBOM
$exe=Join-Path $out 'projection_observe_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$workspace" "-I$out" (Join-Path $PSScriptRoot 'projection_observe_test.c') -o $exe
if($LASTEXITCODE){throw 'Production projection observer fixture compile failed'}
$result=& $exe
$result | Set-Content -LiteralPath (Join-Path $out 'results.txt') -Encoding utf8NoBOM
if($LASTEXITCODE){$result | Select-Object -Last 2;throw "Projection observer fixture failed; evidence $out"}
$json=@($result | Where-Object { $_.StartsWith('JSON ') } | ForEach-Object { $_.Substring(5) | ConvertFrom-Json })
$equal=@($json | Where-Object { $_.case -eq 'equal' })
if($equal.Count -ne 8){throw 'Complete checkpoint JSON record count'}
$schema=$equal | Where-Object event -eq 'base_projection_schema'
$rows=@($equal | Where-Object event -eq 'base_projection_samples' | ForEach-Object samples)
if($rows.Count -ne 12 -or @($rows | Where-Object { $_.a.Length -ne 128 -or $_.bad -ne 0 -or $_.raw -ne 0 -or $_.max -ne '0000000000000000' }).Count){throw 'Raw production measurements/zero deltas invalid'}
if($schema.e[0].Length -ne 128 -or $schema.e[1].Length -ne 128){throw 'Expected16 header missing'}
$footer=$equal | Where-Object event -eq 'base_projection_footer'
if($footer.seen_mask -ne 4095 -or $footer.submitted_mask -ne 4095 -or $footer.missing_mask -ne 0){throw 'Full checkpoint masks'}
foreach($entry in $json | Where-Object event -eq 'base_projection_samples') {
 foreach($row in $entry.samples){
  if($row.s -eq 0 -and ($row.a -notmatch '^[0-9A-F]{128}$')){throw 'Invalid matrix bit encoding'}
  if($row.at -ge 0 -and $row.max -notmatch '^[0-9A-F]{16}$'){throw 'Invalid finite delta bit encoding'}
  if($row.nf[0] -or $row.nf[1]){if($null -ne $row.max -or $row.at -ne -1){throw 'Nonfinite delta must be null'}}
 }
}
$json | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $out 'parsed-records.json') -Encoding utf8NoBOM
$result | Select-Object -Last 1
Write-Host "JSON PASS $($json.Count) records / 12 complete samples; evidence $out"
