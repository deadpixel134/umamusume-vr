[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$project = Join-Path $PSScriptRoot "src\UmaVR.Configurator\UmaVR.Configurator.csproj"
$output = Join-Path $PSScriptRoot "build\publish"
dotnet publish $project -c Release -r win-x64 --self-contained false -o $output
if ($LASTEXITCODE) { throw "dotnet publish failed: $LASTEXITCODE" }
& (Join-Path $output "UmaVR.Configurator.exe") --self-test
if ($LASTEXITCODE) { throw "configurator self-test failed: $LASTEXITCODE" }
& (Join-Path $output "UmaVR.Configurator.exe") --verify-localization
if ($LASTEXITCODE) { throw "configurator localization verification failed: $LASTEXITCODE" }
& (Join-Path $output "UmaVR.Configurator.exe") --verify-layout
if ($LASTEXITCODE) { throw "configurator localized layout verification failed: $LASTEXITCODE" }
$artifact = Get-Item -LiteralPath (Join-Path $output "UmaVR.Configurator.exe")
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName).Hash
$payload = Get-Item -LiteralPath (Join-Path $output "UmaVR.Configurator.dll")
$payloadHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $payload.FullName).Hash
$runtimeManifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot "..\immersive\package\BUILD_MANIFEST.json") -Raw | ConvertFrom-Json
$manifest = [ordered]@{
    schema = 1
    candidate_id = "SETTINGS-013"
    build_id = "SETTINGS-013-$($payloadHash.Substring(0,8))"
    artifact = $artifact.Name
    artifact_size = $artifact.Length
    artifact_sha256 = $hash
    payload_artifact = $payload.Name
    payload_size = $payload.Length
    payload_sha256 = $payloadHash
    settings_schema = 10
    runtime_build_id = $runtimeManifest.build_id
    runtime_artifact_sha256 = $runtimeManifest.artifact_sha256
    settings_path = "vrmod/config/settings.json"
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "BUILD_MANIFEST.json") -Encoding utf8NoBOM
$manifest
