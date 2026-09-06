# IMMERSIVE-001

First coherent fail-open `Live` stereo candidate for Umamusume 2.30.0.

- Preserves the accepted `PANEL-002` isolated-device OpenXR session, final-composite capture, and viewer-locked panel fallback.
- Resolves Unity/IL2CPP only from a proven attached window-thread callback; no Unity runtime calls occur on the bootstrap worker.
- Selects a `Live` source by the observed component contract `LiveTimelineCamera + LiveImageEffect + MultiCameraFinalComposite`, not by camera name alone.
- Does not clone the runtime Live owner (`FAIL-014`). It requires the selected original owner to contain `LiveImageEffect`, `MultiCameraFinalComposite`, Gallop `CameraData`, and URP additional-camera data, then synchronously renders that authored owner once per eye with a temporary game-device `RenderTexture`, OpenXR per-eye FOV/IPD, the owner's near/far clip range, and a local eye offset. Target/aspect/transform/projection are restored before returning to the game so the PC path retains its authored state. HMD pose composition is intentionally outside this milestone candidate.
- Copies both eye textures into a persistent keyed-mutex shared pair on Present without waiting. The isolated XR device consumes only a fresh, atomically published source generation; invalid, stale, incomplete, or retired output immediately uses the accepted panel.

Missing or incorrect authored effects remain blocking `Immersive stereo` evidence; a missing owner contract or failed sequential render preparation fails open to the accepted panel rather than publishing a partial eye image.

Build with `build.ps1`; run `tests/Test-Local.ps1`. Use `tools/Run-Test.ps1` only through the canonical Runtime Gate request.
