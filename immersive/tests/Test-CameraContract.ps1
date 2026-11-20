param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\camera-contract-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
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
$exe=Join-Path $out 'camera_contract_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$workspace" "-I$out" (Join-Path $PSScriptRoot 'camera_contract_test.c') -o $exe
if($LASTEXITCODE){throw 'Production camera-contract fixture compile failed'}
$result=& $exe
$result | Set-Content -LiteralPath (Join-Path $out 'results.txt') -Encoding utf8NoBOM
if($LASTEXITCODE){$result | Select-Object -Last 2;throw "Camera-contract fixture failed; evidence $out"}
$json=@($result | Where-Object { $_.StartsWith('JSON ') } | ForEach-Object { $_.Substring(5) | ConvertFrom-Json })
$complete=@($json | Where-Object case -eq 'complete')
if($complete.Count -ne 5){throw 'Complete group must contain exactly five records'}
$footer=$complete | Where-Object event -eq 'base_camera_contract_footer'
if(!$footer.complete_cpu_values -or !$footer.frame_coherent -or $footer.value_mask -ne 511 -or $footer.finite_mask -ne 511 -or $footer.submitted_mask -ne 511){throw 'Complete group masks/correlation'}
foreach($record in $json | Where-Object event -eq 'base_camera_contract_sample') {
 for($m=0;$m -lt 3;$m++) {
  if($record.s[$m] -eq 0 -and $record.pvc[$m] -notmatch '^[0-9A-F]{128}$'){throw 'Invalid raw matrix encoding'}
  if($record.s[$m] -ne 0 -and $record.pvc[$m] -ne ''){throw 'Unavailable matrix must not serialize placeholder zero values'}
 }
}
foreach($record in $json | Where-Object event -eq 'base_camera_contract_footer') {
 if($record.byte_limit -ne 6144 -or $record.record_limit -ne 5 -or $record.wedge_cause -ne 'INCONCLUSIVE'){throw 'Evidence budget/verdict contract'}
 if($record.case -in @('optional_partial','culling_absent_pv','recorded_global_35048','metadata_export_unavailable','getter_failure','raw_nonfinite_signedzero','frame_changed','reused_frame_changed','reused_missing','source_alias','source_missing','owner_unavailable','owner_depth','identity_changed') -and $record.complete_cpu_values){throw 'Incomplete/nonfinite evidence promoted to complete'}
}
$partial=$json | Where-Object {$_.case -eq 'culling_absent_pv' -and $_.event -eq 'base_camera_contract_footer'}
if($partial.selected_mask -ne 219 -or $partial.seen_mask -ne 219 -or $partial.value_mask -ne 219 -or $partial.finite_mask -ne 219 -or $partial.missing_mask -ne 292){throw 'Absent C must preserve supported P/V only'}
$partialRows=@($json | Where-Object {$_.case -eq 'culling_absent_pv' -and $_.event -eq 'base_camera_contract_sample'})
if($partialRows.Count -ne 3 -or @($partialRows | Where-Object {$_.s[0] -ne 0 -or $_.s[1] -ne 0 -or $_.s[2] -ne 5 -or $_.pvc[2] -ne ''}).Count){throw 'Partial raw status/culling omission'}
$global=$json | Where-Object {$_.case -eq 'recorded_global_35048' -and $_.event -eq 'base_camera_contract_footer'}
if(!$global -or $global.selected_mask -ne 219 -or $global.value_mask -ne 219 -or $global.writer_dropped_before_footer -eq 0 -or $global.submitted_mask -eq 511){throw 'Global tail reservation/partial delivery truth'}
$json | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $out 'parsed-records.json') -Encoding utf8NoBOM
$result | Select-Object -Last 1
Write-Host "JSON PASS $($json.Count) records; evidence $out"
