[한국어](../ko/UPDATE.md) | [English](../en/UPDATE.md) | [日本語](../ja/UPDATE.md)

# Updates

## Update · v0.1.1

Close the game and open `vrmod/tools/UmaVR.Configurator.exe`. The v0.1.1 configurator checks for a newer public release at startup. Select **Check for updates** to check again. When an update is available, it verifies the ZIP and launches the installer automatically.

## Manual update

1. Under **Assets**, download `UmaVR-vX.Y.Z.zip` and the matching `.zip.sha256` from the same release.
2. To update manually, extract the new ZIP from [Releases](https://github.com/deadpixel134/umamusume-vr/releases) outside the game directory. Run its `UmaVR.Installer.exe` and select your existing game folder.

## If the update stops

If the configurator reports that the game is running, close the game and select **Check for updates** again. Check the configurator status for download or verification errors. If the installer started, check the last error in `%LOCALAPPDATA%\UmaVR\update.log`.

## Development build v0.1.12

In this build, select **Apply update** after a newer version is found. If the installer asks for manual review because of an unfamiliar layout or modified files, check the indicated paths and files before continuing.

[Download v0.1.1](https://github.com/deadpixel134/umamusume-vr/releases/tag/v0.1.1) · [Install, update or remove](INSTALLATION.md) · [UmaVR](../../README.md)
