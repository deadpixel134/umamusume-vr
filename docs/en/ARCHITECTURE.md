# Architecture and safety boundaries

- `immersive/src`: native OpenXR/D3D11 runtime loaded through Localify `externalDlls`.
- `settings/src`: validated schema-9 settings and verified GitHub Release staging.
- `installer/src/UmaVR.Management`: payload integrity, path containment, backup, rollback and process guards.
- `installer/src/UmaVR.Installer`: localized UI and the automatic-update handoff.

Updates are staged outside the installation and are never applied before asset SHA-256 and package/version verification. No reusable repository credential is shipped. The repository uses a deny-by-default allowlist so game files, Localify, user data, logs and generated packages cannot be added by default.
