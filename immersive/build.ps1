param([string]$ZigPath = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = (Resolve-Path (Join-Path $root "..\..")).Path
if ([string]::IsNullOrWhiteSpace($ZigPath)) { $ZigPath = Join-Path $workspace ".tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe" }
$zig = (Resolve-Path -LiteralPath $ZigPath).Path
$minhook = Join-Path $workspace ".tmp\umamusume-localify\deps\minhook"
if (-not (Test-Path -LiteralPath (Join-Path $minhook "include\MinHook.h"))) { throw "Pinned MinHook submodule is missing." }
$openxrInclude = Join-Path $workspace ".tmp\openxr-sdk\include"
if (-not (Test-Path -LiteralPath (Join-Path $openxrInclude "openxr\openxr.h"))) { throw "Vendored OpenXR headers are missing." }
$build = Join-Path $root "build"; $package = Join-Path $root "package"
New-Item -ItemType Directory -Path $build,$package -Force | Out-Null
$cache = Join-Path $workspace ".tmp\zig-cache\immersive"; New-Item -ItemType Directory -Path "$cache\global","$cache\local" -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = "$cache\global"; $env:ZIG_LOCAL_CACHE_DIR = "$cache\local"
$output = Join-Path $build "umavr_immersive.dll"
$sourceIdentity = (@('immersive.c','telemetry.h','telemetry_frame.h','request_dispatch.h','end_frame_bridge.h','base_producer_state.h','base_producer.h','base_producer_gpu.h','base_producer_lookup.h','base_producer_ownership.h','base_binding_preflight.h','base_binding_preflight_log.h','base_producer_mono.h','base_projection_types.h','base_projection_normal_log.h','base_projection_observe.h','base_producer_fov.h') | ForEach-Object {
    (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root "src\$_")).Hash
}) -join ''
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_keyed_mutex.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_background_policy.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_background.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_camera_contract_observe.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_composite_capture.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_consumer_lease.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_source_follow.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_eye_output.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_source_capability.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_story_source.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_generation_rearm.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_transport.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/base_story_observe.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/story_native_read.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/story_native_flow.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/story_body_draw.h')).Hash
$sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'src/home_panel_visibility.h')).Hash
foreach ($extra in 'camera_follow.h','live_render_scale.h','vr_scale_menu.h','home_hand_contact.h','home_touch_reaction.h','home_chest_surface.h') {
    $sourceIdentity += (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root "src/$extra")).Hash
}
$sourceHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($sourceIdentity)))
$buildDefine = '-DUMAVR_BUILD_ID="source-' + $sourceHash.Substring(0,16) + '"'
& $zig cc -target x86_64-windows-gnu -shared -O2 -Wall -Wextra -Werror -DMH_STATIC `
  $buildDefine `
  "-I$(Join-Path $minhook 'include')" "-I$(Join-Path $minhook 'src')" `
  "-I$openxrInclude" `
  (Join-Path $root "src\immersive.c") `
  (Join-Path $minhook "src\buffer.c") (Join-Path $minhook "src\hook.c") `
  (Join-Path $minhook "src\trampoline.c") (Join-Path $minhook "src\hde\hde64.c") `
  -lkernel32 -luser32 -lgdi32 -ld3d11 -ldxgi -lbcrypt -o $output
if ($LASTEXITCODE -ne 0) { throw "Zig build failed: $LASTEXITCODE" }
Copy-Item -LiteralPath $output -Destination (Join-Path $package "umavr_immersive.dll") -Force
$artifact = Get-Item -LiteralPath (Join-Path $package "umavr_immersive.dll")
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName).Hash
$manifest = [ordered]@{ schema=1; candidate_id="IMMERSIVE-001"; build_id=("IMMERSIVE-001-" + $hash.Substring(0,8)); artifact=$artifact.Name; artifact_size=$artifact.Length; artifact_sha256=$hash; target="x86_64-windows-gnu"; toolchain="Zig 0.16.0"; openxr_headers="KhronosGroup/OpenXR-SDK release-1.1.62"; log_path="%LOCALAPPDATA%\UmaVR\immersive-001.log" }
$manifest.source_build_id = "source-" + $sourceHash.Substring(0,16)
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package "BUILD_MANIFEST.json") -Encoding utf8NoBOM
$manifest


