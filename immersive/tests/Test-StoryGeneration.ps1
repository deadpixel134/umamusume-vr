param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspace=(Resolve-Path (Join-Path $root '..\..')).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\story-generation-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$gpu=Get-Content -LiteralPath (Join-Path $root 'src/base_producer_gpu.h') -Raw
$reclaim=[regex]::Match($gpu,'(?ms)^static bool bp_reclaim_transport\(void\) \{.*?^\}')
if(!$reclaim.Success){throw 'Actual transport reclaim adapter missing'}
$reclaim.Value | Set-Content -LiteralPath (Join-Path $out 'transport_reclaim.inc') -Encoding utf8NoBOM
$witness=[regex]::Match($gpu,'(?ms)^ if\(!bp_witness_export_generation\) bp_witness_export_generation=bp\.c\.pending\.generation;.*?^ if\(export_image\) \+\+bp_witness_export_attempts;')
if(!$witness.Success){throw 'Actual witness export budget missing'}
$witnessFixture="static uint64_t bp_witness_export_generation;`nstatic unsigned bp_witness_export_attempts;`nstatic bool fixture_export(uint64_t generation) {`n bp.c.pending.generation=generation;`n"+$witness.Value+"`n return export_image;`n}"
$witnessFixture | Set-Content -LiteralPath (Join-Path $out 'witness_export.inc') -Encoding utf8NoBOM
$producer=Get-Content -LiteralPath (Join-Path $root 'src/base_producer.h') -Raw
$context=[regex]::Match($producer,'(?ms)^static unsigned bp_context_records,.*?^static void bp_context_event\(.*?^\}')
if(!$context.Success){throw 'Actual context process budget missing'}
$context.Value | Set-Content -LiteralPath (Join-Path $out 'context_budget.inc') -Encoding utf8NoBOM
foreach($test in @('story_source','generation_transport','context_budget')) {
 $exe=Join-Path $out ($test+'_test.exe')
 & $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$out" (Join-Path $PSScriptRoot ($test+'_test.c')) -o $exe
 if($LASTEXITCODE){throw "Fixture compile failed: $test"}
 $result=& $exe
 $result | Set-Content -LiteralPath (Join-Path $out ($test+'-results.txt')) -Encoding utf8NoBOM
 if($LASTEXITCODE){throw "Fixture failed: $test; evidence $out"}
 $result | Where-Object {$_ -match '^\{'} | ForEach-Object {$_ | ConvertFrom-Json | Out-Null}
 $result | Select-Object -Last 1
}
Write-Host "Story actual-helper PASS; no Unity/GPU/HMD executed; evidence $out"
