[한국어](README.md) | [English](README.en.md) | [日本語](README.ja.md)

# UmaVR

Creator: [@TBluebox12](https://x.com/TBluebox12)  
Support: [buymeacoffee.com/vrshits](https://buymeacoffee.com/vrshits)

UmaVR is an unofficial Meta Quest/OpenXR VR mod for the DMM release of Umamusume: Pretty Derby. Validated Live scenes use binocular VR and physical 6DoF; all other screens remain the complete aspect-correct original game image on a flat panel. The game UI can be operated with VR controllers.

The Virtual Desktop VDXR route has been tested on hardware. This repository currently contains the **v0.1.0 release candidate**; no stable GitHub Release has been published yet.

## Documentation

- [Installation, updates and removal](docs/en/INSTALLATION.md)
- [Usage and controls](docs/en/USAGE.md)
- [Architecture and safety boundaries](docs/en/ARCHITECTURE.md)

## Highlights

- OpenXR stereo, physical HMD 6DoF and world scale in Live
- Complete original game screen as the front panel outside Live
- Grip-toggled auxiliary panel, controller ray and circular cursor in immersive mode
- Right-stick locomotion, left-stick 30° snap turn and complete hand-role swap
- Korean, English and Japanese settings UI for render, camera, scale, movement and supported VFX controls
- Installer/update design with full payload hashes, a package manifest, settings preservation and coexistence with other `externalDlls`

## Current support boundary

- Validated: Windows 11 x64, game 2.30.0, Unity 2022.3.62f2, Direct3D 11, Virtual Desktop VDXR 1.0.10 and the Localify 1.50.0 loader route
- Live supports immersive stereo, 6DoF, VFX and controllers.
- Home, Story, Race, Training and character preview remain PANEL because no safe stereo ownership route is available.
- SteamVR OpenXR and Meta Quest Link/Air Link are provisional and have not been tested in this project.

The current candidate requires an existing compatible `localify.dll` and its `config.json` `externalDlls` loader. The repository starts private, and no reusable GitHub credential is shipped; private Release auto-update therefore remains unavailable to ordinary users until a safe distribution decision is made.

This repository excludes original game files, Localify files, user settings, logs, rollback data, build outputs and credentials. Source is provided under the [MIT License](LICENSE); external components retain their own licenses.

> UmaVR is an unofficial fan project with no affiliation to the game developer or publisher. A legitimate installation of the game is required.
