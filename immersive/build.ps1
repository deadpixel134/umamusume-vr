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
& $zig cc -target x86_64-windows-gnu -shared -O2 -Wall -Wextra -Werror -DMH_STATIC `
  "-I$(Join-Path $minhook 'include')" "-I$(Join-Path $minhook 'src')" `
  "-I$openxrInclude" `
  (Join-Path $root "src\immersive.c") `
  (Join-Path $minhook "src\buffer.c") (Join-Path $minhook "src\hook.c") `
  (Join-Path $minhook "src\trampoline.c") (Join-Path $minhook "src\hde\hde64.c") `
  -lkernel32 -luser32 -ld3d11 -ldxgi -o $output
if ($LASTEXITCODE -ne 0) { throw "Zig build failed: $LASTEXITCODE" }
Copy-Item -LiteralPath $output -Destination (Join-Path $package "umavr_immersive.dll") -Force
$artifact = Get-Item -LiteralPath (Join-Path $package "umavr_immersive.dll")
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName).Hash
$manifest = [ordered]@{ schema=1; candidate_id="IMMERSIVE-001"; build_id=("IMMERSIVE-001-" + $hash.Substring(0,8)); artifact=$artifact.Name; artifact_size=$artifact.Length; artifact_sha256=$hash; target="x86_64-windows-gnu"; toolchain="Zig 0.16.0"; openxr_headers="KhronosGroup/OpenXR-SDK release-1.1.62"; log_path="%LOCALAPPDATA%\UmaVR\immersive-001.log" }
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package "BUILD_MANIFEST.json") -Encoding utf8NoBOM
$manifest
