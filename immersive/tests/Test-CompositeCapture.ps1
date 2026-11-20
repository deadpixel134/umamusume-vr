param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\composite-capture-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $out 'composite_capture_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'composite_capture_test.c') -o $exe
if($LASTEXITCODE){throw 'Production composite-capture fixture compile failed'}
$result=& $exe $out
$result | Set-Content -LiteralPath (Join-Path $out 'results.txt') -Encoding utf8NoBOM
if($LASTEXITCODE){$result | Select-Object -Last 2;throw "Composite-capture fixture failed; evidence $out"}
$sidecars=@(Get-ChildItem -LiteralPath $out -File -Filter 'original-composite-*.json')
if($sidecars.Count -ne 2){throw 'Exactly two successful disk fixture sidecars expected'}
foreach($file in $sidecars) {
 $json=Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
 if($file.Length -gt 4096 -or $json.schema -ne 1 -or $json.kind -ne 'original_final_composite' -or $json.own_files_verified -or !$json.image_write_complete -or $json.wedge_cause -ne 'INCONCLUSIVE'){throw 'Sidecar contract/false acceptance'}
 if($json.enqueue_present -ge $json.completion_present -or $json.capture_tick -ne 55 -or $json.main_observed_frame -ne 70 -or $json.own_files_expected.Count -ne 2){throw 'Copy-batch/witness correlation'}
 $image=Join-Path $out $json.image_file
 $bytes=[IO.File]::ReadAllBytes($image)
 if($bytes.Length -ne 70 -or [BitConverter]::ToUInt16($bytes,0) -ne 19778 -or [BitConverter]::ToUInt32($bytes,2) -ne 70 -or [BitConverter]::ToUInt32($bytes,10) -ne 54 -or [BitConverter]::ToInt32($bytes,18) -ne 2 -or [BitConverter]::ToInt32($bytes,22) -ne -2 -or [BitConverter]::ToUInt32($bytes,34) -ne 16){throw 'BMP header/size/row order'}
}
$gpuSource=Get-Content -LiteralPath (Join-Path $root 'src/base_producer_gpu.h') -Raw
$end=$gpuSource.LastIndexOf('ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp.fence);')
$arm=$gpuSource.LastIndexOf('bp_bc_arm(dev,ctx,backbuffer,&bp.c.pending,baseline_serial);')
if($end -lt 0 -or $arm -le $end){throw 'Optional copy must follow original product fenceEnd'}
if($gpuSource.IndexOf('bp_bc_poll(backbuffer,baseline_serial);') -ge $gpuSource.IndexOf('if(!bp.c.attempted || bp.c.phase==BP_RETIRED')){throw 'Optional drain must precede product early exits'}
$result | Select-Object -Last 1
Write-Host "PASS sidecars $($sidecars.Count), BMPs/correlation/strict JSON and integration order; evidence $out"
