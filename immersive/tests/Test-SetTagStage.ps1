# One fixed-revision transition/archive check; no lookup/render suite execution.
param([ValidateSet('SetTag41','CompareTag87','BindingPreflight9F','NativeMonoF6','ProjectionObserve0B')][string]$Revision='SetTag41')
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$actual=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$base=Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/97e7470c3aa34d468a43c0b3cc2b3126/case-1';$root=Join-Path $base 'game'
if($root-eq$actual-or!$root.StartsWith((Join-Path $actual 'vrmod/discovery_survey/build/code-transition-tests/'),[StringComparison]::OrdinalIgnoreCase)){throw 'Private fixture boundary violation'}
function Digest($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$checks=[Collections.Generic.List[string]]::new();function Check($ok,$label){if(!$ok){throw "CHECK FAILED: $label"};$checks.Add($label)}
$oldLocal=$env:LOCALAPPDATA
try{
    $env:LOCALAPPDATA=Join-Path $base 'localappdata';$project=Join-Path $root 'vrmod/immersive';$logDir=Join-Path $env:LOCALAPPDATA 'UmaVR'
    $priorName='base-camera-stage-5A48F9F5.json';$newName='set-tag-stage-41E8196D.json';$fixedName='fixed-set-tag-observability-41E8196D';$sourceBuild='source-537C0128E2C6452B';$expectedHash='41E8196DAB85007054B638C54CF99DCDECA60DB5382AC4C96527F86A154C753A';$expectedType='BOUNDARY_OBSERVABILITY';$reportName='set-tag-stage-transition-result.json'
    $pins=@('7717B847425F613FBB4F557612E3D9D3FCD8D5DFA5C2599E9AC8B6B181DC8DBF','7B4BB99FADF0896095C5958CC3486301C7C49941EACC1EAE755D216420B4B3D8','EDA11498AF3FF0F709B24CBC3E0B0AFF35B0F4268D6818C87019313D0FA893A8','43BE0764B9BB5373E9633FC0314FB620277A043D492A3DC0A8CD6D62A484FBCA','staging/BASE-CAMERA-5A48F9F5-20261113T023543Z-6dda36d7a4504d318dd4babf558e4416')
    if($Revision-eq'CompareTag87'){$priorName='set-tag-stage-41E8196D.json';$newName='compare-tag-stage-870F2F43.json';$fixedName='fixed-compare-tag-870F2F43';$sourceBuild='source-4EB5CD5058683EE7';$expectedHash='870F2F43665CFA2AFFE48E16FAC6ECA33C3861CF9E43CFB233FD21835D27CF3F';$expectedType='INTEGRATION';$reportName='compare-tag-stage-transition-result.json';$pins=@('EE776E5373AB9DE01107287D316467D25DCE7C77CFB2AC47B0F56AE3A1C45B86','7B66A221965C3CF9D93BB5B88CA70866276757E3EEE22558A8CC763793BE6B70','9127CBCA992BC0FCCE404EB2A7A7EB5E0D3F00A44411C87DAA472FB05BF4376C','7C4E2C555E0C85E6371FBB078CB5F7E01AC5638539F1770ADA5C49B152CE35CF','staging/SET-TAG-41E8196D-20261113T041007Z-3d92c8b7679d45a6aa7d3ecebae92075')}
    if($Revision-eq'BindingPreflight9F'){$priorName='compare-tag-stage-870F2F43.json';$newName='binding-preflight-stage-9FA562E1.json';$fixedName='fixed-binding-preflight-9FA562E1';$sourceBuild='source-6B93CFCDDAD1384C';$expectedHash='9FA562E10F5D48299D0B71F1A3307B4651F51C895FCA2209DD0BA469E489AA9F';$expectedType='BOUNDARY_OBSERVABILITY';$reportName='binding-preflight-stage-transition-result.json';$pins=@('2C4F024554E4C01726A4C1792533F49317654B0D130FB3A826E65E12C19ACD9D','8516C9CB8179C73D3A65A3665CBA704F9757A47B67BB9B22988A58DDB68E757A','8368C6F4AD54034AA16F6DD34968120E5259955998260B587DD3E18A7F141605','11A7D1D1A6EF4A32233BE1205D62B2384781B170F97CAF8054EFA4077A9CDF0B','staging/COMPARE-TAG-870F2F43-20261113T045111Z-ebc7d54921384211aee20ee8b6b2b433')}
    if($Revision-eq'NativeMonoF6'){$priorName='binding-preflight-stage-9FA562E1.json';$newName='native-mono-stage-F6F6CF9D.json';$fixedName='fixed-native-mono-F6F6CF9D';$sourceBuild='source-8BCF05E8EA50DB4D';$expectedHash='F6F6CF9D947552756F93B3D290C8C73C6647D5A214992A1758F23425F3439F47';$expectedType='INTEGRATION';$reportName='native-mono-stage-transition-result.json';$pins=@('E823929709654B6AD8150C0CBD1A1671CED4380A5C95B607507E32B01B252852','CCA184FF1723057DF764A1C5362A32354B8FDEA598852A64F3D319B719E9D78C','B9C006AC2E24DE081DC6BDBB6DCC6F8F3ABF7EDC6BFD8183040994E192FC24D2','96ABFD4A5097B64E017818033F22880ECBC9ACF64174FEF6A6CDF09A2FB2063E','staging/BINDING-PREFLIGHT-9FA562E1-20261113T064010Z-88fa313e719d4f77a3a2ff6fec36b1fa')}
    if($Revision-eq'ProjectionObserve0B'){$priorName='native-mono-stage-F6F6CF9D.json';$newName='projection-observe-stage-0BA10D99.json';$fixedName='fixed-projection-observe-0BA10D99';$sourceBuild='source-7ED79C1852BCCF4B';$expectedHash='0BA10D998FAD79B499612BE6CB2300745F756273C7A384CFEBFC7CCDB0B2D411';$expectedType='BOUNDARY_OBSERVABILITY';$reportName='projection-observe-stage-transition-result.json';$pins=@('9C85C445AC2EBFE16491580B83924B4CAD39E63EC337DA3E41C816B7FAC9FB30','B1265ED778EDA7963A5F8061294F84A88D93448134A1020EAF55789309EEC62D','3B61F1EFA0A8A759CB5ADF6AA2F8436A33A43C3F33E61ECC288E903037552B1B','042E94B2F11F3D6BE13E7CB274B76FB27EEEA34D904D67435C19913756FED300','staging/NATIVE-MONO-F6F6CF9D-20261113T074507Z-9fdbbb5d0f8b45cfa22a7f59ff327f6b')}
    $priorPath=Join-Path $project $priorName;$prior=Get-Content $priorPath -Raw|ConvertFrom-Json -Depth 32;$priorHash=Digest $priorPath
    $cap=Get-Content (Join-Path $prior.run_path 'runtime-capture.json') -Raw|ConvertFrom-Json -Depth 32
    foreach($f in $cap.files){Copy-Item -LiteralPath (Join-Path $prior.run_path ('runtime-'+$f.name)) -Destination $f.source -Force}
    foreach($image in $cap.image_artifacts){Copy-Item -LiteralPath $image.archive -Destination $image.source -Force}
    if(Test-Path (Join-Path $project $newName)){throw 'Fixture already staged; no reset'}
    Copy-Item -LiteralPath (Join-Path $actual ('vrmod/immersive/'+$fixedName)) -Destination (Join-Path $project $fixedName) -Recurse
    foreach($name in @('Stage-EndFrame.ps1','Collect-EndFrame.ps1')){
        $text=Get-Content (Join-Path $actual ('vrmod/immersive/tools/'+$name)) -Raw
        $text=$text.Replace($pins[0],$priorHash).Replace($pins[1],(Digest $prior.config_path)).Replace($pins[2],(Digest (Join-Path $prior.run_path 'runtime-capture.json'))).Replace($pins[3],(Digest (Join-Path $prior.run_path 'runtime-immersive-001.log'))).Replace($pins[4],('staging/'+$prior.run_id))
        [IO.File]::WriteAllText((Join-Path $project ('tools/'+$name)),$text,[Text.UTF8Encoding]::new($false))
    }
    $stage=Join-Path $project 'tools/Stage-EndFrame.ps1';$collect=Join-Path $project 'tools/Collect-EndFrame.ps1'
    $beforeConfig=Digest $prior.config_path;$plan=&$stage -GameRoot $root -Revision $Revision -PlanOnly
    Check ((Digest $prior.config_path)-ceq$beforeConfig-and!(Test-Path $plan.run_path)-and$plan.test_type-eq$expectedType) 'read-only fixed revision plan'
    $m=&$stage -GameRoot $root -Revision $Revision
    Check ($m.state-eq'Prepared'-and$m.log_build_id-eq$sourceBuild-and(Digest $m.dll_path)-ceq$expectedHash) 'exact revision separately staged'
    $cfg=Get-Content $m.config_path -Raw|ConvertFrom-Json;Check (@($cfg.externalDlls).Count-eq1-and$cfg.externalDlls[0]-ceq$m.dll_path-and(Digest $priorPath)-ceq$priorHash) 'single active observer product and prior receipt preserved'
    foreach($w in $m.watches){Check ((Digest (Join-Path $root $w.name))-ceq$w.sha256) ('preserved '+$w.name)}
    Check (@($m.logs|Where-Object {Test-Path -LiteralPath $_.path}).Count-eq0) 'all fresh telemetry paths absent'
    $rows=@([ordered]@{schema=2;event='startup';pid=555;attach_id=5555;build_id=$sourceBuild;seq=1},[ordered]@{schema=2;event='revision_attribution_fixture';pid=555;attach_id=5555;build_id=$sourceBuild;seq=2})
    [IO.File]::WriteAllText((Join-Path $logDir 'immersive-001.log'),((@($rows|ForEach-Object{$_|ConvertTo-Json -Compress})-join"`n")+"`n"))
    $c=&$collect -GameRoot $root -Revision $Revision;$captured=Get-Content $c.capture -Raw|ConvertFrom-Json -Depth 32
    Check ($captured.sessions.Count-eq1-and$captured.sessions[0].matches_expected_build-and$captured.sessions[0].records-eq2-and$captured.image_artifacts.Count-eq0-and$captured.image_baseline_observations.Count-eq$m.image_baseline.Count) 'observer log attribution and existing BMP exclusion'
    $again=&$collect -GameRoot $root -Revision $Revision;Check ($again.capture_sha256-ceq$c.capture_sha256) 'unchanged capture remains immutable'
    $out=Join-Path $actual ('vrmod/immersive/build/'+$reportName);[ordered]@{status='PASS';revision=$Revision;checks=$checks.ToArray();count=$checks.Count;actual_game=$false;actual_installation=$false;lookup_or_render_tests_repeated=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $out -Encoding utf8NoBOM
    [pscustomobject]@{status='PASS';checks=$checks.Count;report=$out}
}finally{$env:LOCALAPPDATA=$oldLocal}
