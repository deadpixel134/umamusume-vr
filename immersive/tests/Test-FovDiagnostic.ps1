param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspace=(Resolve-Path (Join-Path $root '../..')).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp/zig-0.16.0/zig-x86_64-windows-0.16.0/zig.exe'}
$out=Join-Path $root ('build/fov-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $out 'fov_diagnostic_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'fov_diagnostic_test.c') -o $exe
if($LASTEXITCODE){throw 'FOV adapter fixture compile failed'}
$result=& $exe
$result | Set-Content (Join-Path $out 'results.txt')
if($LASTEXITCODE){throw "FOV adapter fixture failed: $result"}
$result | Where-Object {$_ -match '^\{'} | ForEach-Object {$_ | ConvertFrom-Json | Out-Null}
$result
Write-Host "Focused FOV evidence: $out"
