param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\native-mono-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $out 'native_mono_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$workspace" (Join-Path $PSScriptRoot 'native_mono_test.c') -o $exe
if($LASTEXITCODE){throw 'Production native/mono fixture compile failed'}
& $exe | Tee-Object -FilePath (Join-Path $out 'results.txt')
if($LASTEXITCODE){throw "Production native/mono fixture failed; evidence $out"}
Write-Host "Focused native/mono evidence: $out"
