[한국어](../ko/INSTALLATION.md) | [English](INSTALLATION.md) | [日本語](../ja/INSTALLATION.md)

# Installation, updates and removal

[Project home](../../README.en.md) · [Usage](USAGE.md) · [Architecture](ARCHITECTURE.md)

## Requirements

- Windows 11 x64 and a legitimate DMM installation of Umamusume
- A working PC OpenXR environment
- For this release, Localify 1.50.0 `localify.dll` and a valid root `config.json`. If Localify is not installed, first follow its [official installation guide](https://github.com/Kimjio/umamusume-localify).

## Install

1. Fully close `umamusume.exe` and the running game session.
2. When a stable Release is available, download `UmaVR-vX.Y.Z.zip` and its `.sha256` asset.
3. Extract outside the game folder and run `UmaVR.Installer.exe`.
4. Select the folder containing `umamusume.exe`, `GameAssembly.dll` and `UnityPlayer.dll`, then install.
5. If Localify is absent or incomplete, use **Open the official Localify installation guide** in the installer, finish that installation, and refresh the status.
6. Install UmaVR, open `vrmod/tools/UmaVR.Configurator.exe`, review settings, and start the game through DMM.

The installer verifies every payload hash before replacement, backs up owned files, merges UmaVR into the existing `externalDlls` array, and preserves existing settings and other entries.
UmaVR does not automatically download, install or replace Localify.

After extraction, the root contains one executable entry point, `UmaVR.Installer.exe`, plus `package-manifest.json`, `payload` and `licenses`. The installer and installed configurator are self-contained single-file applications and do not require a machine-wide .NET installation.

## Updates

The settings app checks stable Releases from `deadpixel134/umamusume-vr`. It accepts only the exact versioned ZIP and corresponding SHA-256, validates size/hash/package/version, and runs the installer only while the game is stopped. API, download or verification failure leaves the installed version unchanged.

The repository and Releases are public and update metadata/assets are accessed without authentication. No GitHub token or reusable credential is embedded.

## Remove or roll back

Close the game and run the installer from the package used. Removal touches only recorded product-owned files. Modified files are preserved with a warning, the original `config.json` is restored only from a matching verified backup, and user settings remain.
