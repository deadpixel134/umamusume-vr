param([string]$ZigPath='',[string]$GameRoot='')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GameRoot){$GameRoot=Join-Path $root '..\..'}
$workspace=(Resolve-Path -LiteralPath $GameRoot).Path
if(!$ZigPath){$ZigPath=Join-Path $workspace '.tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe'}
$out=Join-Path $root ('build\binding-preflight-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $out 'binding_preflight_test.exe'
& $ZigPath cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror "-I$workspace" (Join-Path $PSScriptRoot 'binding_preflight_test.c') -o $exe
if($LASTEXITCODE){throw 'Production binding preflight fixture compile failed'}
$lines=@(& $exe)
$code=$LASTEXITCODE
$lines | Set-Content -LiteralPath (Join-Path $out 'results.txt') -Encoding utf8NoBOM
if($code){throw "Production binding preflight fixture failed; evidence $out"}
$json=@($lines | Where-Object { $_.StartsWith('RECORD ') } | ForEach-Object { $_.Substring(7) })
foreach($line in $json){$null=$line|ConvertFrom-Json -ErrorAction Stop}
$json | Set-Content -LiteralPath (Join-Path $out 'records.jsonl') -Encoding utf8NoBOM
$lines | Where-Object { !$_.StartsWith('RECORD ') }
Write-Host "JSON parser PASS records=$($json.Count); focused evidence: $out"
