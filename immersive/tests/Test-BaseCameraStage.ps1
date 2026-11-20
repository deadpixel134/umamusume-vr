# Narrow reuse of the existing private product-stage fixture. No game/D3D tests.
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$actual=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$base=Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/97e7470c3aa34d468a43c0b3cc2b3126/case-1';$root=Join-Path $base 'game'
if($root-eq$actual-or!$root.StartsWith((Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/'),[StringComparison]::OrdinalIgnoreCase)){throw 'Private fixture boundary violation'}
function Digest($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$checks=[Collections.Generic.List[string]]::new();function Check($ok,$label){if(!$ok){throw "CHECK FAILED: $label"};$checks.Add($label)}
$oldLocal=$env:LOCALAPPDATA
try{
    $env:LOCALAPPDATA=Join-Path $base 'localappdata';$project=Join-Path $root 'vrmod/immersive';$logDir=Join-Path $env:LOCALAPPDATA 'UmaVR'
    $oldJournal=Join-Path $project 'end-frame-stage-4E156D9F.json';$old=Get-Content $oldJournal -Raw|ConvertFrom-Json -Depth 32
    $cap=Get-Content (Join-Path $old.run_path 'runtime-capture.json') -Raw|ConvertFrom-Json
    foreach($f in $cap.files){Copy-Item -LiteralPath (Join-Path $old.run_path ('runtime-'+$f.name)) -Destination $f.source -Force}
    if(Test-Path (Join-Path $project 'base-camera-stage-5A48F9F5.json')){throw 'Base fixture already staged; no automatic reset'}
    Copy-Item -LiteralPath (Join-Path $actual 'vrmod/immersive/fixed-base-camera-5A48F9F5') -Destination (Join-Path $project 'fixed-base-camera-5A48F9F5') -Recurse
    foreach($name in @('Stage-EndFrame.ps1','Collect-EndFrame.ps1')){
        $text=Get-Content (Join-Path $actual ('vrmod/immersive/tools/'+$name)) -Raw
        $text=$text.Replace('21076A2A6CE18238B8F9FBB3606E45A8B5A34E072EE669EC664F4E10E90F90C6',(Digest $oldJournal)).Replace('62AE26EA438FC830369E3627E031D1F4D0074254E6B0EA3163304CEE1C7CEA51',(Digest $old.config_path)).Replace('AADCD0CB0FD4115CCA3D94F434041112863541E87C628DF62F5D3B2568F009ED',(Digest (Join-Path $old.run_path 'runtime-capture.json'))).Replace('2DD2BFE60A2FF5F4FB3D350F5B87BC5D22826C116F462FDD22D6ABED96908BCC',(Digest (Join-Path $old.run_path 'runtime-immersive-001.log'))).Replace('staging/END-FRAME-4E156D9F-20261112T171319Z-3f56b9e97ac14f0da5e329144ec94196',('staging/'+$old.run_id))
        [IO.File]::WriteAllText((Join-Path $project ('tools/'+$name)),$text,[Text.UTF8Encoding]::new($false))
    }
    $stage=Join-Path $project 'tools/Stage-EndFrame.ps1';$collect=Join-Path $project 'tools/Collect-EndFrame.ps1'
    $baseline=Join-Path $logDir 'own-base-7-70-g1-0.bmp';[IO.File]::WriteAllText($baseline,'old partial bmp');$baselineHash=Digest $baseline
    $beforeConfig=Digest $old.config_path;$beforeReceipt=Digest $oldJournal
    $plan=&$stage -GameRoot $root -Revision BaseCamera5A -PlanOnly
    Check ($plan.image_baseline.Count-eq1-and(Digest $old.config_path)-ceq$beforeConfig-and!(Test-Path $plan.run_path)) 'read-only plan records existing BMP baseline'
    $package=Join-Path $project 'fixed-base-camera-5A48F9F5/package/umavr_immersive.dll';[IO.File]::AppendAllText($package,'tamper');$refused=$false;try{&$stage -GameRoot $root -Revision BaseCamera5A -PlanOnly|Out-Null}catch{$refused=$true};Check ($refused-and(Digest $old.config_path)-ceq$beforeConfig) 'fixed candidate tamper refuses without activation'
    Copy-Item -LiteralPath (Join-Path $actual 'vrmod/immersive/fixed-base-camera-5A48F9F5/package/umavr_immersive.dll') -Destination $package -Force
    $m=&$stage -GameRoot $root -Revision BaseCamera5A
    Check ($m.state-eq'Prepared'-and$m.log_build_id-eq'source-350D65953B5AFE82'-and(Digest $m.dll_path)-ceq'5A48F9F592DF82EF54E7027DB78800BCBBFFBC1DD6D1B87B248BAC597D31B974') 'new exact product revision stages separately'
    Check ((Digest $oldJournal)-ceq$beforeReceipt-and(Digest $baseline)-ceq$baselineHash-and$m.image_baseline.Count-eq1) 'old product receipt and BMP baseline unchanged'
    foreach($w in $m.watches){Check ((Digest (Join-Path $root $w.name))-ceq$w.sha256) ('preserved '+$w.name)}
    $startup=[ordered]@{schema=2;event='startup';pid=333;attach_id=3333;build_id='source-350D65953B5AFE82';seq=1}
    $witness=[ordered]@{schema=2;event='base_eye_write_witness';pid=333;attach_id=3333;build_id='source-350D65953B5AFE82';seq=2;generation=1;eye=0;image_saved=$true}
    [IO.File]::WriteAllText((Join-Path $logDir 'immersive-001.log'),(($startup|ConvertTo-Json -Compress)+"`n"+($witness|ConvertTo-Json -Compress)+"`n"))
    [IO.File]::WriteAllText((Join-Path $logDir 'own-base-333-3333-g1-0.bmp'),'BM partial');[IO.File]::WriteAllBytes((Join-Path $logDir 'own-base-444-4444-g9-1.bmp'),[byte[]]@())
    $c=&$collect -GameRoot $root -Revision BaseCamera5A;$result=Get-Content $c.capture -Raw|ConvertFrom-Json -Depth 32
    Check ($result.image_artifacts.Count-eq2-and@($result.image_artifacts|Where-Object session_matches_run).Count-eq1-and@($result.image_artifacts|Where-Object {$_.bytes-eq0}).Count-eq1-and@($result.image_artifacts|Where-Object header_minimum_bytes_present).Count-eq0) 'partial and zero-byte BMPs archived, unmatched session kept unattributed'
    Check ($result.image_baseline_observations.Count-eq1-and$result.image_baseline_observations[0].unchanged-and@($result.image_artifacts|Where-Object scene_image_reviewed).Count-eq0-and!$result.runtime_support_proven) 'baseline excluded from new evidence and file presence never accepts imagery'
    [IO.File]::AppendAllText((Join-Path $logDir 'own-base-333-3333-g1-0.bmp'),'changed');$refused=$false;try{&$collect -GameRoot $root -Revision BaseCamera5A|Out-Null}catch{$refused=$true};Check $refused 'changed image refuses overwrite or reseal'
    $out=Join-Path $actual 'vrmod/immersive/build/base-camera-stage-transition-result.json';[ordered]@{status='PASS';checks=$checks.ToArray();count=$checks.Count;actual_installation=$false;actual_game=$false;native_render_tests_repeated=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $out -Encoding utf8NoBOM
    [pscustomobject]@{status='PASS';checks=$checks.Count;report=$out}
}finally{$env:LOCALAPPDATA=$oldLocal}
