# Corresponding source for releases

For each distributed binary release, link from the GitHub release page to the
complete Corresponding Source for that build. Match the package version,
source tag, build inputs, and build and installation scripts. Publish any
required materials that are absent from the tag archive alongside the binary.

The package embeds the versioned source and release links in its notices.
Third-party components retain their own terms and attribution.

For the native DLL, preserve the exact MinHook/HDE checkout and OpenXR
headers used by immersive/build.ps1. Record the dependency revisions with
the release, and make the required dependency sources and notices available
from the release page. Localify's loader is obtained and installed separately.
