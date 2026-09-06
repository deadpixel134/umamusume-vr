[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
$vrmodRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build'))
$packageRoot = [IO.Path]::GetFullPath((Join-Path $buildRoot "UmaVR-v$Version"))
$allowedPrefix = $buildRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not (($packageRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar).StartsWith(
    $allowedPrefix, [StringComparison]::OrdinalIgnoreCase))) {
    throw 'Package output escaped the installer build directory.'
}
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

$runtimeRoot = Join-Path $vrmodRoot 'immersive\package'
$runtimeManifestPath = Join-Path $runtimeRoot 'BUILD_MANIFEST.json'
$runtimeManifest = Get-Content -LiteralPath $runtimeManifestPath -Raw | ConvertFrom-Json
$runtimeDll = Join-Path $runtimeRoot 'umavr_immersive.dll'
if ((Get-FileHash -LiteralPath $runtimeDll -Algorithm SHA256).Hash -ne
    [string]$runtimeManifest.artifact_sha256) {
    throw 'Accepted immersive package does not match BUILD_MANIFEST.json.'
}

$configProject = Join-Path $vrmodRoot 'settings\src\UmaVR.Configurator\UmaVR.Configurator.csproj'
$installerProject = Join-Path $PSScriptRoot 'src\UmaVR.Installer\UmaVR.Installer.csproj'
$configPublish = Join-Path $buildRoot 'configurator-publish'
$installerPublish = Join-Path $buildRoot 'installer-publish'
foreach ($path in @($configPublish, $installerPublish)) {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}

dotnet publish $configProject -c Release -r win-x64 --self-contained true `
    -p:Version=$Version -p:DebugType=None -p:DebugSymbols=false -o $configPublish
if ($LASTEXITCODE) { throw "Configurator publish failed: $LASTEXITCODE" }
& (Join-Path $configPublish 'UmaVR.Configurator.exe') --self-test
if ($LASTEXITCODE) { throw "Configurator self-test failed: $LASTEXITCODE" }
& (Join-Path $configPublish 'UmaVR.Configurator.exe') --verify-localization
if ($LASTEXITCODE) { throw "Configurator localization verification failed: $LASTEXITCODE" }
& (Join-Path $configPublish 'UmaVR.Configurator.exe') --verify-layout
if ($LASTEXITCODE) { throw "Configurator layout verification failed: $LASTEXITCODE" }

dotnet publish $installerProject -c Release -r win-x64 --self-contained true `
    -p:Version=$Version -p:DebugType=None -p:DebugSymbols=false -o $installerPublish
if ($LASTEXITCODE) { throw "Installer publish failed: $LASTEXITCODE" }
& (Join-Path $installerPublish 'UmaVR.Installer.exe') --self-test
if ($LASTEXITCODE) { throw "Installer self-test failed: $LASTEXITCODE" }
& (Join-Path $installerPublish 'UmaVR.Installer.exe') --verify-localization
if ($LASTEXITCODE) { throw "Installer localization verification failed: $LASTEXITCODE" }

$payload = Join-Path $packageRoot 'payload'
$runtimeDestination = Join-Path $payload 'UmaVR\immersive'
$settingsDestination = Join-Path $payload 'vrmod\config'
$toolsDestination = Join-Path $payload 'vrmod\tools'
New-Item -ItemType Directory -Path $runtimeDestination,$settingsDestination,$toolsDestination -Force | Out-Null
Copy-Item -LiteralPath $runtimeDll -Destination (Join-Path $runtimeDestination 'umavr_immersive.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\default-settings.json') `
    -Destination (Join-Path $settingsDestination 'settings.json')
Get-ChildItem -LiteralPath $configPublish -File | Where-Object Extension -ne '.pdb' |
    Copy-Item -Destination $toolsDestination
Get-ChildItem -LiteralPath $installerPublish -File | Where-Object Extension -ne '.pdb' |
    Copy-Item -Destination $packageRoot
$noticeFiles = [ordered]@{
    'LICENSE.txt' = Join-Path $vrmodRoot 'LICENSE'
    'THIRD_PARTY_NOTICES.txt' = Join-Path $vrmodRoot 'THIRD_PARTY_NOTICES.txt'
    'DOTNET_LICENSE.txt' = Join-Path $PSScriptRoot 'assets\dotnet\LICENSE.txt'
    'DOTNET_THIRD_PARTY_NOTICES.txt' = Join-Path $PSScriptRoot 'assets\dotnet\ThirdPartyNotices.txt'
}
foreach ($entry in $noticeFiles.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Required license notice is missing: $($entry.Value)"
    }
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $packageRoot $entry.Key)
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $payload "vrmod\$($entry.Key)")
}

$files = foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($payload, $file.FullName).Replace('\','/')
    [ordered]@{
        path = $relative
        sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        preserveExisting = $relative -eq 'vrmod/config/settings.json'
        preserveOnUninstall = $relative -eq 'vrmod/config/settings.json'
    }
}
$manifest = [ordered]@{
    schemaVersion = 1
    version = $Version
    loader = 'localify-external-dll'
    localifyPolicy = 'require-compatible-existing'
    files = @($files)
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath `
    (Join-Path $packageRoot 'package-manifest.json') -Encoding utf8NoBOM

& (Join-Path $packageRoot 'UmaVR.Installer.exe') --verify-package
if ($LASTEXITCODE) { throw "Packaged installer verification failed: $LASTEXITCODE" }

$archive = Join-Path $buildRoot "UmaVR-v$Version.zip"
$checksum = $archive + '.sha256'
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
if (Test-Path -LiteralPath $checksum) { Remove-Item -LiteralPath $checksum -Force }
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $archive
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath $checksum -Encoding ascii

[pscustomobject]@{
    version = $Version
    archive = $archive
    sha256 = $hash
    checksum = $checksum
    runtimeBuild = [string]$runtimeManifest.build_id
    repository = 'deadpixel134/umamusume-vr'
}
