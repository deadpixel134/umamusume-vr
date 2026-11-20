param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspace=(Resolve-Path (Join-Path $root '..\..')).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\base-producer-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $out 'base_producer_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'base_producer_test.c') -o $exe
if($LASTEXITCODE){throw 'Producer policy fixture compile failed'}
& $exe | Tee-Object -FilePath (Join-Path $out 'results.txt')
if($LASTEXITCODE){throw 'Producer policy fixture failed'}
Write-Host "Focused producer policy evidence: $out"
