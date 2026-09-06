[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$settingsRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $settingsRoot "src\UmaVR.Configurator\UmaVR.Configurator.csproj"
dotnet build $project -c Release
if ($LASTEXITCODE) { throw "configurator build failed: $LASTEXITCODE" }
$exe = Join-Path $settingsRoot "src\UmaVR.Configurator\bin\Release\net10.0-windows\UmaVR.Configurator.exe"
& $exe --self-test
if ($LASTEXITCODE) { throw "configurator self-test failed: $LASTEXITCODE" }
& $exe --verify-localization
if ($LASTEXITCODE) { throw "configurator localization verification failed: $LASTEXITCODE" }
& $exe --verify-layout
if ($LASTEXITCODE) { throw "configurator localized layout verification failed: $LASTEXITCODE" }
$source = Get-Content -LiteralPath (Join-Path $settingsRoot "src\UmaVR.Configurator\MainForm.cs") -Raw
$uiText = Get-Content -LiteralPath (Join-Path $settingsRoot "src\UmaVR.Configurator\UiText.cs") -Raw
$buildScript = Get-Content -LiteralPath (Join-Path $settingsRoot "build.ps1") -Raw
if (-not $buildScript.Contains('candidate_id = "SETTINGS-013"') -or
    -not $buildScript.Contains('build_id = "SETTINGS-013-$($payloadHash.Substring(0,8))"')) {
    throw "SETTINGS-013 package identity missing"
}
if ($source.Contains('STORY Camera Follow') -or $source.Contains('RACE Camera Follow')) {
    throw "unsupported Camera Follow phantom control found"
}
if (-not $uiText.Contains('Live에서 게임 카메라 위치·각도 따라가기')) {
    throw "Live Camera Follow control missing"
}
if (-not $source.Contains('_worldScale') -or
    -not $source.Contains('WorldScale = (float)_worldScale.Value') -or
    -not $source.Contains('Number(0.55m, decimal.MaxValue, 0.05m, 2)') -or
    -not $uiText.Contains('체감 세계 크기 (권장 0.55–4.00, 기본 1.00; 상한 없음)')) {
    throw "POSE-003 minimum-bounded perceived world scale control missing"
}
if (-not $uiText.Contains('Primary / Secondary controller 손 역할 전체 바꾸기') -or
    -not $uiText.Contains('왼손 pointer ray·Trigger·X/Y·이동') -or
    -not $uiText.Contains('오른손 panel Grip·Snap Turn')) {
    throw "implemented full controller role-swap control missing"
}
if (-not $source.Contains('Number(0.00m, decimal.MaxValue, 0.10m, 2)') -or
    -not $source.Contains('_locomotionScaleCompensationEnabled') -or
    -not $source.Contains('LocomotionScaleCompensationEnabled = _locomotionScaleCompensationEnabled.Checked') -or
    -not $uiText.Contains('월드 스케일에 맞춰 이동 속도 자동 보정') -or
    -not $uiText.Contains('기본 5.00') -or
    -not $uiText.Contains('상한 없음')) {
    throw "SETTINGS-013 unbounded locomotion compensation controls missing"
}
if ($source.Contains('Smooth Turn 사용') -or $uiText.Contains('Smooth Turn 사용')) {
    throw "unsupported Smooth Turn phantom control found"
}
foreach ($token in @('_postProcessingEnabled', '_blurEnabled', '_depthOfFieldEnabled', '_diffusionEnabled', '_bloomEnabled',
        '_globalFogEnabled', '_lensDistortionEnabled', '_radialBlurEnabled',
        '_sunShaftsEnabled', '_indirectLightShaftsEnabled', '_transmittedLightEnabled',
        '_dofDiffusionBloomOverlayEnabled', '_tiltShiftEnabled', '_fluctuationEnabled',
        '_chromaticAberrationEnabled', '_toneCurveEnabled', '_exposureEnabled',
        '_colorCorrectionEnabled', '_colorGradingEnabled', '_bgBlurEnabled',
        '_vortexEnabled', '_filmRollEnabled', '_hatchingEnabled',
        '_letterBoxEnabled', '_rainSplashEnabled',
        'PostProcessingEnabled = _postProcessingEnabled.Checked',
        'BlurEnabled = _blurEnabled.Checked', 'DepthOfFieldEnabled = _depthOfFieldEnabled.Checked',
        'DiffusionEnabled = _diffusionEnabled.Checked', 'BloomEnabled = _bloomEnabled.Checked',
        'GlobalFogEnabled = _globalFogEnabled.Checked',
        'LensDistortionEnabled = _lensDistortionEnabled.Checked',
        'RadialBlurEnabled = _radialBlurEnabled.Checked',
        'SunShaftsEnabled = _sunShaftsEnabled.Checked',
        'DofDiffusionBloomOverlayEnabled = _dofDiffusionBloomOverlayEnabled.Checked',
        'BgBlurEnabled = _bgBlurEnabled.Checked', '_auraSupport = TaggedLabel("AuraUnsupported")',
        'RainSplashEnabled = _rainSplashEnabled.Checked')) {
    if (-not $source.Contains($token)) { throw "proved VFX control missing: $token" }
}
if ($source.Contains('_auraEnabled') -or $source.Contains('AuraEnabled =') -or
    $uiText.Contains('["AuraEnabled"]')) {
    throw "unsafe Aura interactive control found"
}
foreach ($token in @('전체 효과 사용 (OFF: 잔여 흐림 포함 전체 억제)',
        '게임에서 지정한 Blur 사용', '게임에서 지정한 Depth of Field 사용',
        '게임에서 지정한 Diffusion 사용', '게임에서 지정한 Bloom 사용',
        '게임에서 지정한 Global Fog 사용', '게임에서 지정한 Lens Distortion 사용',
        '게임에서 지정한 Radial Blur 사용', '게임에서 지정한 Sun Shafts 사용',
        '게임에서 지정한 composite overlay slot 전체 사용',
        '게임에서 지정한 Background Blur 사용', '개별 끄기 미지원',
        '게임에서 지정한 Rain Splash 사용')) {
    if (-not $uiText.Contains($token)) { throw "VFX authored-state localization missing: $token" }
}
foreach ($token in @('이 방법만 Live에서 완전 제거가 검증되었습니다',
        'OFF: focus 일부 억제', '잔여 흐림이 남을 수 있습니다',
        '전체 최종 합성 효과 제거를 뜻하지는 않습니다',
        'This is the only method verified to remove it completely in Live',
        'residual blur may remain',
        '完全除去が確認済みなのはこの方法だけです',
        '残留ブラーが残ることがあります')) {
    if (-not $uiText.Contains($token)) { throw "post-processing support boundary text missing: $token" }
}
foreach ($token in @('AddNote(grid, "PostProcessingSupport")',
        'AddSection(grid, "VfxFocusGroup")', 'AddNote(grid, "VfxFocusSupport")',
        'AddSection(grid, "VfxSupportedGroup")', 'AddSection(grid, "VfxUnsupportedGroup")')) {
    if (-not $source.Contains($token)) { throw "post-processing information hierarchy missing: $token" }
}
$masterIndex = $source.IndexOf('AddRow(grid, "PostProcessing", _postProcessingEnabled)')
$focusIndex = $source.IndexOf('AddSection(grid, "VfxFocusGroup")')
$supportedIndex = $source.IndexOf('AddSection(grid, "VfxSupportedGroup")')
$unsupportedIndex = $source.IndexOf('AddSection(grid, "VfxUnsupportedGroup")')
$auraIndex = $source.IndexOf('AddRow(grid, "Aura", _auraSupport)')
if ($masterIndex -lt 0 -or $focusIndex -le $masterIndex -or $supportedIndex -le $focusIndex -or
    $unsupportedIndex -le $supportedIndex -or $auraIndex -le $unsupportedIndex) {
    throw "post-processing support groups are not ordered from master to unsupported"
}
foreach ($token in @('UiLanguage.Korean', 'UiLanguage.English', 'UiLanguage.Japanese',
        'ui-language.txt', 'ValidateResources')) {
    if (-not $uiText.Contains($token)) { throw "localization contract missing: $token" }
}
if (-not $source.Contains('LanguageButton("한국어"') -or
    -not $source.Contains('LanguageButton("English"') -or
    -not $source.Contains('LanguageButton("日本語"')) {
    throw "language switch controls missing"
}
if (-not $source.Contains('control is CheckBox') -or
    -not $source.Contains('? AnchorStyles.Left')) {
    throw "auto-sized checkbox text must not be horizontally stretched or clipped"
}
if (-not $source.Contains('EnsureCheckBoxTextFits(this)') -or
    -not $source.Contains('checkBox.GetPreferredSize(Size.Empty)') -or
    -not $source.Contains('checkBox.MinimumSize = new Size(preferred.Width + 16, preferred.Height)')) {
    throw "localized checkbox text must enforce measured trailing render room"
}
if (-not $source.Contains('AddNote(grid, "CameraSupport")') -or
    $source.Contains('grid.Controls.Add(support, 0, 0)') -or
    -not $source.Contains('VerifyLocalizedLayout()') -or
    -not $source.Contains('Overlapping layout cell')) {
    throw "Camera Follow overlap regression guard missing"
}
[pscustomobject]@{ build = "PASS"; selftest = "PASS"; localization = "ko/en/ja"; layout = "PASS"; phantom_controls = "NONE" }
