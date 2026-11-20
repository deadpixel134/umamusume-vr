param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\home-source-output-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$source=Get-Content -LiteralPath (Join-Path $root 'src/immersive.c') -Raw
$math=[Collections.Generic.List[string]]::new()
foreach($name in @('quat_normalized','quat_multiply','quat_rotate','quat_to_yaw_pitch_roll','quat_from_yaw_pitch_roll')) {
 $match=[regex]::Match($source,'(?ms)^static (?:Quat|Vec3|void) '+$name+'\(.*?^\}')
 if(!$match.Success){throw "Production math function missing $name"}
 $math.Add($match.Value)
}
$math -join "`n" | Set-Content -LiteralPath (Join-Path $out 'source_follow_quaternion.inc') -Encoding utf8NoBOM
$shaders=[Collections.Generic.List[string]]::new()
foreach($name in @('g_eye_copy_vs_src','g_ps_src','g_panel_ps_src')) {
 $match=[regex]::Match($source,'(?ms)^static const char\* '+$name+' =\r?\n(?:[^\r\n]*\r?\n)*?[^\r\n]*";')
 if(!$match.Success){throw "Production shader missing $name"}
 $shaders.Add($match.Value)
}
$shaders -join "`n" | Set-Content -LiteralPath (Join-Path $out 'eye_color_shaders.inc') -Encoding utf8NoBOM
$draw=[Collections.Generic.List[string]]::new()
foreach($name in @('draw_cached_eye_pair_to_projection','copy_fresh_eye_pair_to_projection')) {
 $match=[regex]::Match($source,'(?ms)^static BOOL '+$name+'\([^\r\n]*\) \{.*?^\}')
 if(!$match.Success){throw "Actual cached draw function missing $name"}
 if($name -eq 'draw_cached_eye_pair_to_projection' -and $match.Value -match '\bbp\.c\b|open_eye_pair_on_probe_device|refresh_flat_cache_for_generation|IDXGIKeyedMutex|game_pair_telemetry'){throw 'Cache draw must use XR-owned cache only'}
 $draw.Add($match.Value)
}
$draw -join "`n" | Set-Content -LiteralPath (Join-Path $out 'cached_eye_draw.inc') -Encoding utf8NoBOM
$producer=Get-Content -LiteralPath (Join-Path $root 'src/base_producer.h') -Raw
if($producer.IndexOf('if(bp.c.phase==BP_ACTIVE && !bp_source_follow(frame)) goto done;') -gt $producer.IndexOf('(void)bp_observe(&bp.c,&bp.c.identity,frame)')){throw 'Source pose must be verified before freshness'}
foreach($test in @('source_follow','consumer_lease','cached_eye_draw','eye_color')) {
 $exe=Join-Path $out ($test+'_test.exe')
 $extra=if($test -eq 'eye_color'){@('-ld3d11','-ldxgi')}else{@()}
 & $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$out" (Join-Path $PSScriptRoot ($test+'_test.c')) @extra -o $exe
 if($LASTEXITCODE){throw "Fixture compile failed: $test"}
 $result=& $exe
 $result | Set-Content -LiteralPath (Join-Path $out ($test+'-results.txt')) -Encoding utf8NoBOM
 if($LASTEXITCODE){$result | Select-Object -Last 2;throw "Fixture failed: $test; evidence $out"}
 $result | Select-Object -Last 1
}
Write-Host "Focused Home source/output PASS; no Unity/OpenXR/HMD executed; evidence $out"
