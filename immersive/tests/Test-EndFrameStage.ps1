# One reuse of the completed private transition fixture, no product/game tests.
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$actual=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$base=Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/97e7470c3aa34d468a43c0b3cc2b3126/case-1'
$root=Join-Path $base 'game';if($root-eq$actual-or!$root.StartsWith((Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/'),[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture boundary violation'}
$checks=[Collections.Generic.List[string]]::new();$oldLocal=$env:LOCALAPPDATA
function Get-StageTestDigest($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Check($ok,$name){if(!$ok){throw "CHECK FAILED: $name"};$checks.Add($name)}
try{
    $env:LOCALAPPDATA=Join-Path $base 'localappdata'
    $project=Join-Path $root 'vrmod/immersive';$tools=Join-Path $project 'tools';$helpers=Join-Path $root 'vrmod/discovery_survey/tools/selective-obs'
    New-Item -ItemType Directory -Path $tools,$helpers,(Join-Path $project 'build'),(Join-Path $root 'docs/agent') -Force|Out-Null
    Get-ChildItem (Join-Path $base 'tools') -File|Copy-Item -Destination $helpers
    Copy-Item -LiteralPath (Join-Path $actual 'docs/agent/RUNTIME_GATE.json') -Destination (Join-Path $root 'docs/agent/RUNTIME_GATE.json')
    if(!(Test-Path (Join-Path $project 'fixed-end-frame-4E156D9F'))){Copy-Item -LiteralPath (Join-Path $actual 'vrmod/immersive/fixed-end-frame-4E156D9F') -Destination (Join-Path $project 'fixed-end-frame-4E156D9F') -Recurse}
    Copy-Item -LiteralPath (Join-Path $actual 'vrmod/immersive/build/target-binding-callback-preflight.json') -Destination (Join-Path $project 'build/target-binding-callback-preflight.json')
    $sourceReceipt=Join-Path $root 'UmaVR/discovery_survey/method-flow-cached-obs-install-manifest.json';$source=Get-Content $sourceReceipt -Raw|ConvertFrom-Json
    $sourceRun=Split-Path $source.backup_path;$cap=Get-Content (Join-Path $sourceRun 'log-capture.json') -Raw|ConvertFrom-Json
    foreach($name in @('Stage-EndFrame.ps1','Collect-EndFrame.ps1')){
        $text=Get-Content (Join-Path $actual ('vrmod/immersive/tools/'+$name)) -Raw
        $text=$text.Replace('20261111T084549Z-bcc6261e3b2742be8b444d446dbd6020',$source.run_id).Replace('618ACC5DA3E4A3BDE6138EF12A5023F54BB26345BAE7B125A13B5D6FC2C0D6DE',(Get-StageTestDigest $sourceReceipt)).Replace('E1CE7632DFA121F16D5849AB637B4F8363EE4633B97FCE2DBA4963C35B288729',(Get-StageTestDigest $source.config_path)).Replace('1840080DA695A82CFB4DB4495AC726FEC826D79C68E3BF50BFEFC45C91E4FB30',(Get-StageTestDigest (Join-Path $sourceRun 'log-capture.json'))).Replace('768299D95D1F3B506CDD6D296864B6A0F23E42AACBB16825B4292A57D860BC34',$cap.source_sha256)
        [IO.File]::WriteAllText((Join-Path $tools $name),$text,[Text.UTF8Encoding]::new($false))
    }
    $stage=Join-Path $tools 'Stage-EndFrame.ps1';$collect=Join-Path $tools 'Collect-EndFrame.ps1';$journal=Join-Path $project 'end-frame-stage-4E156D9F.json'
    if(Test-Path -LiteralPath $journal){throw 'Fixture already staged; no reset/reuse'}
    $beforeConfig=[IO.File]::ReadAllBytes($source.config_path);$beforeConfigHash=Get-StageTestDigest $source.config_path
    $package=Join-Path $project 'fixed-end-frame-4E156D9F/package/umavr_immersive.dll'
    [IO.File]::AppendAllText($package,'tamper');$refused=$false;try{&$stage -GameRoot $root -PlanOnly|Out-Null}catch{$refused=$true};Check ($refused-and!(Test-Path $journal)-and(Get-StageTestDigest $source.config_path)-ceq$beforeConfigHash) 'foreign artifact refuses before mutation'
    Copy-Item -LiteralPath (Join-Path $actual 'vrmod/immersive/fixed-end-frame-4E156D9F/package/umavr_immersive.dll') -Destination $package -Force
    [IO.File]::AppendAllText($source.config_path,' ');$refused=$false;try{&$stage -GameRoot $root -PlanOnly|Out-Null}catch{$refused=$true};Check ($refused-and!(Test-Path $journal)) 'foreign live config refuses without journal'
    [IO.File]::WriteAllBytes($source.config_path,$beforeConfig)
    $legacy=@('immersive-001.log','immersive-001.previous.log','immersive-001.health.previous.log');foreach($name in $legacy){[IO.File]::WriteAllText((Join-Path $env:LOCALAPPDATA ('UmaVR/'+$name)),('fixture old stream '+$name))}
    $m=&$stage -GameRoot $root
    Check ($m.state-eq'Prepared'-and(Get-StageTestDigest $m.dll_path)-ceq'4E156D9F8EEF6E1654143DD2F7FDD62AA83456953578234F94210996BB3D8423') 'exact product DLL staged in separate path'
    $new=Get-Content $m.config_path -Raw|ConvertFrom-Json;$old=[Text.Encoding]::UTF8.GetString($beforeConfig).TrimStart([char]0xfeff)|ConvertFrom-Json
    Check (@($new.externalDlls).Count-eq1-and$new.externalDlls[0]-ceq$m.dll_path) 'one active product provider'
    $new.externalDlls=$null;$old.externalDlls=$null;Check (($new|ConvertTo-Json -Compress)-ceq($old|ConvertTo-Json -Compress)) 'all unrelated config values preserved'
    foreach($w in $m.watches){Check ((Get-StageTestDigest (Join-Path $root $w.name))-ceq$w.sha256) ('unchanged '+$w.name)}
    Check (@($m.logs|Where-Object {Test-Path -LiteralPath $_.path}).Count-eq0) 'all six product telemetry streams start absent'
    foreach($log in $m.logs|Where-Object existed){Check ((Get-StageTestDigest $log.preserved_path)-ceq$log.sha256-and(Get-StageTestDigest $log.snapshot)-ceq$log.sha256) ('old telemetry preserved '+$log.name)}
    Check ((Get-StageTestDigest $sourceReceipt)-ceq$m.source_receipt_sha256-and(Get-StageTestDigest (Join-Path $root 'docs/agent/RUNTIME_GATE.json'))-ceq$m.gate_sha256) 'cached receipt and Gate preserved'
    $logDir=Join-Path $env:LOCALAPPDATA 'UmaVR'
    foreach($pair in @(@('immersive-001.log',111,1111),@('immersive-001.previous.log',222,2222))){$r=[ordered]@{schema=2;event='startup';pid=$pair[1];attach_id=$pair[2];build_id='source-86AC54841BEB3FD8';seq=1};[IO.File]::WriteAllText((Join-Path $logDir $pair[0]),(($r|ConvertTo-Json -Compress)+"`n"))}
    [IO.File]::WriteAllText((Join-Path $logDir 'immersive-001.health.log'),'{broken crash fragment')
    $c=&$collect -GameRoot $root
    Check ($c.sessions.Count-eq2-and$c.parse_failures.Count-eq1-and$c.configuration_retained) 'two sessions and malformed raw crash fragment retained without fabricated acceptance'
    [IO.File]::AppendAllText((Join-Path $logDir 'immersive-001.log'),'tamper');$refused=$false;try{&$collect -GameRoot $root|Out-Null}catch{$refused=$true};Check $refused 'changed telemetry refuses reseal'
    $report=Join-Path $actual 'vrmod/immersive/build/end-frame-stage-transition-result.json';[ordered]@{status='PASS';checks=$checks.ToArray();count=$checks.Count;fixture_root=$root;actual_game=$false;actual_installation=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $report -Encoding utf8NoBOM
    [pscustomobject]@{status='PASS';checks=$checks.Count;report=$report}
}finally{$env:LOCALAPPDATA=$oldLocal}
