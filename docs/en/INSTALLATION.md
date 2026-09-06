[한국어](../ko/INSTALLATION.md) | [English](INSTALLATION.md) | [日本語](../ja/INSTALLATION.md)

# Installation, updates and removal

[Project home](../../README.en.md) · [Usage](USAGE.md) · [Architecture](ARCHITECTURE.md)

## Requirements

- Windows 11 x64 and a legitimate DMM installation of Umamusume
- A working PC OpenXR environment
- For this release candidate, Localify 1.50.0 `localify.dll` and a valid root `config.json`

## Install

1. Fully close `umamusume.exe` and the running game session.
2. When a stable Release is available, download `UmaVR-vX.Y.Z.zip` and its `.sha256` asset.
3. Extract outside the game folder and run `UmaVR.Installer.exe`.
4. Select the folder containing `umamusume.exe`, `GameAssembly.dll` and `UnityPlayer.dll`, then install.
5. Open `vrmod/tools/UmaVR.Configurator.exe`, review settings, and start the game through DMM.

The installer verifies every payload hash before replacement, backs up owned files, merges UmaVR into the existing `externalDlls` array, and preserves existing settings and other entries.

## Updates

The settings app checks stable Releases from `deadpixel134/umamusume-vr`. It accepts only the exact versioned ZIP and corresponding SHA-256, validates size/hash/package/version, and runs the installer only while the game is stopped. API, download or verification failure leaves the installed version unchanged.

The repository initially remains private and no GitHub credential is embedded. Automatic updates for ordinary users therefore remain unavailable until a safe distribution route is approved.

## Remove or roll back

Close the game and run the installer from the package used. Removal touches only recorded product-owned files. Modified files are preserved with a warning, the original `config.json` is restored only from a matching verified backup, and user settings remain.
