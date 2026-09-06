using System.Globalization;

namespace UmaVR.Configurator;

internal enum UiLanguage
{
    Korean,
    English,
    Japanese
}

internal static class UiText
{
    private static readonly IReadOnlyDictionary<UiLanguage, IReadOnlyDictionary<string, string>> Resources =
        new Dictionary<UiLanguage, IReadOnlyDictionary<string, string>>
        {
            [UiLanguage.Korean] = Korean(),
            [UiLanguage.English] = English(),
            [UiLanguage.Japanese] = Japanese()
        };

    public static UiLanguage CurrentLanguage { get; private set; }

    public static void Initialize()
    {
        ValidateResources();
        CurrentLanguage = LoadLanguage();
    }

    public static string Get(string key) =>
        Resources[CurrentLanguage].TryGetValue(key, out string? value)
            ? value
            : throw new InvalidOperationException($"Missing UI text: {CurrentLanguage}/{key}");

    public static string Format(string key, params object[] arguments) =>
        string.Format(CultureInfo.CurrentCulture, Get(key), arguments);

    public static void SetLanguage(UiLanguage language)
    {
        CurrentLanguage = language;
        try
        {
            string directory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UmaVR");
            Directory.CreateDirectory(directory);
            File.WriteAllText(Path.Combine(directory, "ui-language.txt"), Code(language));
        }
        catch
        {
            // The in-process language switch remains valid if preference persistence fails.
        }
    }

    internal static void SetLanguageForValidation(UiLanguage language) => CurrentLanguage = language;

    private static UiLanguage LoadLanguage()
    {
        try
        {
            string path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UmaVR",
                "ui-language.txt");
            if (File.Exists(path))
            {
                return Parse(File.ReadAllText(path).Trim());
            }
        }
        catch
        {
            // Fall back to the Windows UI culture.
        }

        return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName switch
        {
            "ja" => UiLanguage.Japanese,
            "en" => UiLanguage.English,
            _ => UiLanguage.Korean
        };
    }

    private static UiLanguage Parse(string value) => value.ToLowerInvariant() switch
    {
        "ko" => UiLanguage.Korean,
        "en" => UiLanguage.English,
        "ja" => UiLanguage.Japanese,
        _ => UiLanguage.Korean
    };

    private static string Code(UiLanguage language) => language switch
    {
        UiLanguage.Korean => "ko",
        UiLanguage.English => "en",
        UiLanguage.Japanese => "ja",
        _ => "ko"
    };

    private static void ValidateResources()
    {
        HashSet<string> reference = Resources[UiLanguage.Korean].Keys.ToHashSet(StringComparer.Ordinal);
        foreach ((UiLanguage language, IReadOnlyDictionary<string, string> resource) in Resources)
        {
            if (!reference.SetEquals(resource.Keys) || resource.Any(pair => string.IsNullOrWhiteSpace(pair.Value)))
            {
                throw new InvalidOperationException($"Invalid localization resource: {language}");
            }
        }
    }

    private static Dictionary<string, string> Korean() => new()
    {
        ["AppTitle"] = "UmaVR 설정",
        ["GameFolder"] = "게임 폴더",
        ["Browse"] = "찾기...",
        ["TabCamera"] = "Camera Follow",
        ["TabRender"] = "렌더 품질",
        ["TabController"] = "컨트롤러",
        ["CameraSupport"] = "현재 runtime에서 지원·검증된 category는 Live뿐입니다. STORY/RACE control은 안전한 runtime 경로가 없어 표시하지 않습니다.",
        ["CameraControlsGroup"] = "Live 카메라 및 공간 설정",
        ["Live"] = "LIVE",
        ["LiveCameraFollow"] = "Live에서 게임 카메라 위치·각도 따라가기",
        ["WorldScale"] = "체감 세계 크기 (권장 0.55–4.00, 기본 1.00; 상한 없음)",
        ["EyeRenderScale"] = "Eye render scale",
        ["PostProcessing"] = "전체 Post-processing",
        ["PostProcessingEnabled"] = "전체 효과 사용 (OFF: 잔여 흐림 포함 전체 억제)",
        ["PostProcessingSupport"] = "흐림을 확실히 제거하려면 전체 Post-processing을 끄세요. 이 방법만 Live에서 완전 제거가 검증되었습니다.",
        ["VfxFocusGroup"] = "흐림 및 포커스 — 부분 제어",
        ["VfxFocusSupport"] = "아래 항목은 안전하게 개별 억제되지만, 전체 Post-processing이 켜져 있으면 잔여 흐림이 남을 수 있습니다.",
        ["VfxSupportedGroup"] = "기타 개별 지원 효과",
        ["VfxUnsupportedGroup"] = "개별 제어 비지원",
        ["Blur"] = "Blur Optimized",
        ["BlurEnabled"] = "게임에서 지정한 Blur 사용",
        ["DepthOfField"] = "Depth of Field",
        ["DepthOfFieldEnabled"] = "게임에서 지정한 Depth of Field 사용 (OFF: focus 일부 억제)",
        ["Diffusion"] = "Diffusion",
        ["DiffusionEnabled"] = "게임에서 지정한 Diffusion 사용",
        ["Bloom"] = "Bloom",
        ["BloomEnabled"] = "게임에서 지정한 Bloom 사용",
        ["GlobalFog"] = "Global Fog",
        ["GlobalFogEnabled"] = "게임에서 지정한 Global Fog 사용",
        ["LensDistortion"] = "Lens Distortion",
        ["LensDistortionEnabled"] = "게임에서 지정한 Lens Distortion 사용",
        ["RadialBlur"] = "Radial Blur",
        ["RadialBlurEnabled"] = "게임에서 지정한 Radial Blur 사용",
        ["SunShafts"] = "Sun Shafts",
        ["SunShaftsEnabled"] = "게임에서 지정한 Sun Shafts 사용",
        ["IndirectLightShafts"] = "Indirect Light Shafts",
        ["IndirectLightShaftsEnabled"] = "게임에서 지정한 Indirect Light Shafts 사용",
        ["TransmittedLight"] = "Transmitted Light",
        ["TransmittedLightEnabled"] = "게임에서 지정한 Transmitted Light 사용",
        ["DofDiffusionBloomOverlay"] = "DoF / Diffusion / Bloom Overlay (전체 slot)",
        ["DofDiffusionBloomOverlayEnabled"] = "게임에서 지정한 composite overlay slot 전체 사용",
        ["TiltShift"] = "Tilt Shift",
        ["TiltShiftEnabled"] = "게임에서 지정한 Tilt Shift 사용",
        ["Fluctuation"] = "Fluctuation",
        ["FluctuationEnabled"] = "게임에서 지정한 Fluctuation 사용",
        ["ChromaticAberration"] = "Chromatic Aberration",
        ["ChromaticAberrationEnabled"] = "게임에서 지정한 Chromatic Aberration 사용",
        ["ToneCurve"] = "Tone Curve",
        ["ToneCurveEnabled"] = "게임에서 지정한 Tone Curve 사용",
        ["Exposure"] = "Exposure",
        ["ExposureEnabled"] = "게임에서 지정한 Exposure 사용",
        ["ColorCorrection"] = "Color Correction",
        ["ColorCorrectionEnabled"] = "게임에서 지정한 Color Correction 사용",
        ["ColorGrading"] = "Color Grading",
        ["ColorGradingEnabled"] = "게임에서 지정한 Color Grading 사용",
        ["BgBlur"] = "Background Blur",
        ["BgBlurEnabled"] = "게임에서 지정한 Background Blur 사용",
        ["Vortex"] = "Vortex",
        ["VortexEnabled"] = "게임에서 지정한 Vortex 사용",
        ["Aura"] = "Aura",
        ["AuraUnsupported"] = "개별 끄기 미지원 — 전체 Post-processing master로만 억제 가능",
        ["FilmRoll"] = "Film Roll",
        ["FilmRollEnabled"] = "게임에서 지정한 Film Roll 사용",
        ["Hatching"] = "Hatching",
        ["HatchingEnabled"] = "게임에서 지정한 Hatching 사용",
        ["LetterBox"] = "Letter Box",
        ["LetterBoxEnabled"] = "게임에서 지정한 Letter Box 사용",
        ["RainSplash"] = "Rain Splash",
        ["RainSplashEnabled"] = "게임에서 지정한 Rain Splash 사용",
        ["VfxDescription"] = "지원 항목의 ON은 게임이 지정한 상태를 보존하고, OFF는 해당 authored field만 억제합니다. 전체 최종 합성 효과 제거를 뜻하지는 않습니다.",
        ["Locomotion"] = "Locomotion",
        ["LocomotionEnabled"] = "오른쪽 스틱 이동 사용",
        ["LocomotionSpeed"] = "이동 기준 속도 (m/s, 기본 5.00; 상한 없음)",
        ["LocomotionScaleCompensation"] = "월드 스케일 보정",
        ["LocomotionScaleCompensationEnabled"] = "월드 스케일에 맞춰 이동 속도 자동 보정 (스케일 2.00에서 5.00)",
        ["SnapTurn"] = "Snap Turn",
        ["SnapTurnEnabled"] = "왼쪽 스틱 Snap Turn 사용",
        ["SnapAngle"] = "Snap 각도 (도)",
        ["HandSwap"] = "손 역할 전환",
        ["HandSwapEnabled"] = "Primary / Secondary controller 손 역할 전체 바꾸기",
        ["ControllerDescription"] = "자동 보정은 입력한 기준 속도를 월드 스케일에 비례시킵니다(2.00에서 5.00 기준). 끄면 입력값을 그대로 사용합니다. 기본값은 오른손 pointer ray·Trigger·A/B·이동, 왼손 auxiliary panel Grip·Snap Turn입니다. 손 역할 전환을 켜면 왼손 pointer ray·Trigger·X/Y·이동, 오른손 panel Grip·Snap Turn으로 primary/secondary 역할 전체가 함께 바뀝니다. Smooth Turn은 구현되지 않아 표시하지 않습니다.",
        ["Save"] = "저장",
        ["Reload"] = "다시 불러오기",
        ["Defaults"] = "기본값",
        ["Export"] = "내보내기...",
        ["Import"] = "가져오기...",
        ["CheckUpdates"] = "업데이트 확인",
        ["StatusReady"] = "준비 완료",
        ["StatusLoaded"] = "설정을 불러왔습니다.",
        ["StatusSaved"] = "저장했습니다.",
        ["StatusReloaded"] = "다시 불러왔습니다.",
        ["StatusDefaults"] = "기본값을 화면에 적용했습니다. 저장 버튼을 눌러 확정하세요.",
        ["StatusImported"] = "가져온 설정을 화면에 적용했습니다. 저장 버튼을 눌러 확정하세요.",
        ["StatusExported"] = "설정을 내보냈습니다.",
        ["StatusLanguageChanged"] = "표시 언어를 변경했습니다.",
        ["UpdateChecking"] = "GitHub에서 업데이트를 확인하는 중...",
        ["UpdateCurrent"] = "현재 v{0}이 최신 버전입니다.",
        ["UpdateAvailable"] = "v{0} 업데이트를 찾았습니다.",
        ["UpdateDownloading"] = "v{0} 업데이트를 다운로드하는 중...",
        ["UpdateVerifying"] = "다운로드한 업데이트를 검증하는 중...",
        ["UpdateInstalling"] = "v{0} 업데이트를 설치합니다. 설정 프로그램을 다시 시작합니다...",
        ["UpdateDeferredGameRunning"] = "v{0} 업데이트가 있지만 게임 실행 중에는 설치할 수 없습니다. 게임 종료 후 다시 확인하세요.",
        ["UpdateCompleted"] = "v{0} 자동 업데이트를 완료했습니다.",
        ["UpdateFailed"] = "자동 업데이트 실패: {0}",
        ["UpdateLauncherFailed"] = "자동 업데이트 설치 프로그램을 시작하지 못했습니다.",
        ["InvalidValues"] = "잘못된 값은 안전한 기본값으로 대체했습니다: {0}",
        ["EyeScaleNormal"] = "권장 해상도 대비 약 {0}% pixel load",
        ["EyeScaleWarning"] = "주의: 권장 해상도 대비 약 {0}% pixel load입니다.",
        ["SelectGameFolder"] = "Umamusume 게임 폴더를 선택하세요.",
        ["JsonOpenFilter"] = "JSON 설정 (*.json)|*.json|모든 파일 (*.*)|*.*",
        ["JsonSaveFilter"] = "JSON 설정 (*.json)|*.json",
        ["InvalidSettingsFile"] = "설정 파일에 유효하지 않은 값이 있습니다: {0}",
        ["GameExeMissing"] = "선택한 폴더에서 umamusume.exe를 찾을 수 없습니다.",
        ["GameRunning"] = "게임을 종료한 뒤 설정을 저장하세요.",
        ["InvalidSettingsToSave"] = "저장할 수 없는 설정입니다: {0}",
        ["SettingsPathNoParent"] = "설정 파일의 상위 폴더가 없습니다.",
        ["ErrorPrefix"] = "오류: {0}"
    };

    private static Dictionary<string, string> English() => new()
    {
        ["AppTitle"] = "UmaVR Settings",
        ["GameFolder"] = "Game folder",
        ["Browse"] = "Browse...",
        ["TabCamera"] = "Camera Follow",
        ["TabRender"] = "Render quality",
        ["TabController"] = "Controllers",
        ["CameraSupport"] = "Live is the only runtime-supported and validated category. STORY/RACE controls are hidden because no safe runtime path exists.",
        ["CameraControlsGroup"] = "Live camera and spatial settings",
        ["Live"] = "LIVE",
        ["LiveCameraFollow"] = "Follow the game camera position and rotation in Live",
        ["WorldScale"] = "Perceived world scale (recommended 0.55–4.00, default 1.00; no maximum)",
        ["EyeRenderScale"] = "Eye render scale",
        ["PostProcessing"] = "All post-processing",
        ["PostProcessingEnabled"] = "Enable all effects (OFF: suppresses residual blur too)",
        ["PostProcessingSupport"] = "Turn off all post-processing for reliable blur removal. This is the only method verified to remove it completely in Live.",
        ["VfxFocusGroup"] = "Blur and focus — partial control",
        ["VfxFocusSupport"] = "These effects can be suppressed safely, but residual blur may remain while all post-processing is enabled.",
        ["VfxSupportedGroup"] = "Other individually supported effects",
        ["VfxUnsupportedGroup"] = "Individual control unsupported",
        ["Blur"] = "Blur Optimized",
        ["BlurEnabled"] = "Use the game-authored Blur state",
        ["DepthOfField"] = "Depth of Field",
        ["DepthOfFieldEnabled"] = "Use the game-authored Depth of Field state (OFF: partial focus suppression)",
        ["Diffusion"] = "Diffusion",
        ["DiffusionEnabled"] = "Use the game-authored Diffusion state",
        ["Bloom"] = "Bloom",
        ["BloomEnabled"] = "Use the game-authored Bloom state",
        ["GlobalFog"] = "Global Fog",
        ["GlobalFogEnabled"] = "Use the game-authored Global Fog state",
        ["LensDistortion"] = "Lens Distortion",
        ["LensDistortionEnabled"] = "Use the game-authored Lens Distortion state",
        ["RadialBlur"] = "Radial Blur",
        ["RadialBlurEnabled"] = "Use the game-authored Radial Blur state",
        ["SunShafts"] = "Sun Shafts",
        ["SunShaftsEnabled"] = "Use the game-authored Sun Shafts state",
        ["IndirectLightShafts"] = "Indirect Light Shafts",
        ["IndirectLightShaftsEnabled"] = "Use the game-authored Indirect Light Shafts state",
        ["TransmittedLight"] = "Transmitted Light",
        ["TransmittedLightEnabled"] = "Use the game-authored Transmitted Light state",
        ["DofDiffusionBloomOverlay"] = "DoF / Diffusion / Bloom Overlay (whole slot)",
        ["DofDiffusionBloomOverlayEnabled"] = "Use the whole game-authored composite overlay slot",
        ["TiltShift"] = "Tilt Shift",
        ["TiltShiftEnabled"] = "Use the game-authored Tilt Shift state",
        ["Fluctuation"] = "Fluctuation",
        ["FluctuationEnabled"] = "Use the game-authored Fluctuation state",
        ["ChromaticAberration"] = "Chromatic Aberration",
        ["ChromaticAberrationEnabled"] = "Use the game-authored Chromatic Aberration state",
        ["ToneCurve"] = "Tone Curve",
        ["ToneCurveEnabled"] = "Use the game-authored Tone Curve state",
        ["Exposure"] = "Exposure",
        ["ExposureEnabled"] = "Use the game-authored Exposure state",
        ["ColorCorrection"] = "Color Correction",
        ["ColorCorrectionEnabled"] = "Use the game-authored Color Correction state",
        ["ColorGrading"] = "Color Grading",
        ["ColorGradingEnabled"] = "Use the game-authored Color Grading state",
        ["BgBlur"] = "Background Blur",
        ["BgBlurEnabled"] = "Use the game-authored Background Blur state",
        ["Vortex"] = "Vortex",
        ["VortexEnabled"] = "Use the game-authored Vortex state",
        ["Aura"] = "Aura",
        ["AuraUnsupported"] = "Individual disable unsupported — use the Post-processing master",
        ["FilmRoll"] = "Film Roll",
        ["FilmRollEnabled"] = "Use the game-authored Film Roll state",
        ["Hatching"] = "Hatching",
        ["HatchingEnabled"] = "Use the game-authored Hatching state",
        ["LetterBox"] = "Letter Box",
        ["LetterBoxEnabled"] = "Use the game-authored Letter Box state",
        ["RainSplash"] = "Rain Splash",
        ["RainSplashEnabled"] = "Use the game-authored Rain Splash state",
        ["VfxDescription"] = "ON preserves the game-authored state; OFF suppresses only that supported authored field. It does not mean the complete final composite effect is removed.",
        ["Locomotion"] = "Locomotion",
        ["LocomotionEnabled"] = "Use the right stick for movement",
        ["LocomotionSpeed"] = "Reference movement speed (m/s, default 5.00; no maximum)",
        ["LocomotionScaleCompensation"] = "World-scale compensation",
        ["LocomotionScaleCompensationEnabled"] = "Automatically scale movement speed with world scale (5.00 at scale 2.00)",
        ["SnapTurn"] = "Snap Turn",
        ["SnapTurnEnabled"] = "Use the left stick for Snap Turn",
        ["SnapAngle"] = "Snap angle (degrees)",
        ["HandSwap"] = "Hand roles",
        ["HandSwapEnabled"] = "Swap all Primary / Secondary controller roles",
        ["ControllerDescription"] = "Automatic compensation scales the reference speed proportionally with world scale (5.00 at 2.00); OFF uses the entered speed directly. Default: right pointer ray, Trigger, A/B and locomotion; left auxiliary-panel Grip and Snap Turn. Swap assigns pointer ray, Trigger, X/Y and locomotion to the left hand, and panel Grip and Snap Turn to the right. Smooth Turn is not implemented and is not shown.",
        ["Save"] = "Save",
        ["Reload"] = "Reload",
        ["Defaults"] = "Defaults",
        ["Export"] = "Export...",
        ["Import"] = "Import...",
        ["CheckUpdates"] = "Check for updates",
        ["StatusReady"] = "Ready",
        ["StatusLoaded"] = "Settings loaded.",
        ["StatusSaved"] = "Settings saved.",
        ["StatusReloaded"] = "Settings reloaded.",
        ["StatusDefaults"] = "Defaults loaded. Click Save to apply them.",
        ["StatusImported"] = "Settings imported. Click Save to apply them to the game folder.",
        ["StatusExported"] = "Settings exported.",
        ["StatusLanguageChanged"] = "Display language changed.",
        ["UpdateChecking"] = "Checking GitHub for updates...",
        ["UpdateCurrent"] = "v{0} is up to date.",
        ["UpdateAvailable"] = "Update v{0} is available.",
        ["UpdateDownloading"] = "Downloading update v{0}...",
        ["UpdateVerifying"] = "Verifying the downloaded update...",
        ["UpdateInstalling"] = "Installing update v{0}. The settings app will restart...",
        ["UpdateDeferredGameRunning"] = "Update v{0} is available, but it cannot be installed while the game is running. Close the game and check again.",
        ["UpdateCompleted"] = "Automatic update to v{0} completed.",
        ["UpdateFailed"] = "Automatic update failed: {0}",
        ["UpdateLauncherFailed"] = "The automatic update installer could not be started.",
        ["InvalidValues"] = "Invalid values were replaced with safe defaults: {0}",
        ["EyeScaleNormal"] = "About {0}% pixel load relative to the recommended resolution",
        ["EyeScaleWarning"] = "Caution: about {0}% pixel load relative to the recommended resolution.",
        ["SelectGameFolder"] = "Select the Umamusume game folder.",
        ["JsonOpenFilter"] = "JSON settings (*.json)|*.json|All files (*.*)|*.*",
        ["JsonSaveFilter"] = "JSON settings (*.json)|*.json",
        ["InvalidSettingsFile"] = "The settings file contains invalid values: {0}",
        ["GameExeMissing"] = "umamusume.exe was not found in the selected folder.",
        ["GameRunning"] = "Close the game before saving settings.",
        ["InvalidSettingsToSave"] = "These settings cannot be saved: {0}",
        ["SettingsPathNoParent"] = "The settings path has no parent folder.",
        ["ErrorPrefix"] = "Error: {0}"
    };

    private static Dictionary<string, string> Japanese() => new()
    {
        ["AppTitle"] = "UmaVR 設定",
        ["GameFolder"] = "ゲームフォルダー",
        ["Browse"] = "参照...",
        ["TabCamera"] = "カメラ追従",
        ["TabRender"] = "描画品質",
        ["TabController"] = "コントローラー",
        ["CameraSupport"] = "現在ランタイムで対応・検証済みのカテゴリはライブのみです。安全な経路がないためSTORY/RACE設定は表示しません。",
        ["CameraControlsGroup"] = "ライブカメラと空間設定",
        ["Live"] = "ライブ",
        ["LiveCameraFollow"] = "ライブでゲームカメラの位置・回転に追従",
        ["WorldScale"] = "体感ワールドスケール (推奨 0.55–4.00、既定 1.00、上限なし)",
        ["EyeRenderScale"] = "片目レンダー倍率",
        ["PostProcessing"] = "ポストプロセス全体",
        ["PostProcessingEnabled"] = "全エフェクトを使用 (OFF: 残留ブラーも含めて抑制)",
        ["PostProcessingSupport"] = "ブラーを確実に除去するにはポストプロセス全体をオフにしてください。ライブで完全除去が確認済みなのはこの方法だけです。",
        ["VfxFocusGroup"] = "ブラーとフォーカス — 部分制御",
        ["VfxFocusSupport"] = "以下は安全に個別抑制できますが、ポストプロセス全体がオンの場合は残留ブラーが残ることがあります。",
        ["VfxSupportedGroup"] = "その他の個別対応エフェクト",
        ["VfxUnsupportedGroup"] = "個別制御未対応",
        ["Blur"] = "ブラー最適化",
        ["BlurEnabled"] = "ゲーム指定のブラーを使用",
        ["DepthOfField"] = "被写界深度",
        ["DepthOfFieldEnabled"] = "ゲーム指定の被写界深度を使用 (OFF: フォーカスを部分抑制)",
        ["Diffusion"] = "ディフュージョン",
        ["DiffusionEnabled"] = "ゲーム指定のディフュージョンを使用",
        ["Bloom"] = "ブルーム",
        ["BloomEnabled"] = "ゲーム指定のブルームを使用",
        ["GlobalFog"] = "グローバルフォグ",
        ["GlobalFogEnabled"] = "ゲーム指定のグローバルフォグを使用",
        ["LensDistortion"] = "レンズディストーション",
        ["LensDistortionEnabled"] = "ゲーム指定のレンズディストーションを使用",
        ["RadialBlur"] = "ラジアルブラー",
        ["RadialBlurEnabled"] = "ゲーム指定のラジアルブラーを使用",
        ["SunShafts"] = "サンシャフト",
        ["SunShaftsEnabled"] = "ゲーム指定のサンシャフトを使用",
        ["IndirectLightShafts"] = "間接光シャフト",
        ["IndirectLightShaftsEnabled"] = "ゲーム指定の間接光シャフトを使用",
        ["TransmittedLight"] = "透過光",
        ["TransmittedLightEnabled"] = "ゲーム指定の透過光を使用",
        ["DofDiffusionBloomOverlay"] = "DoF / Diffusion / Bloom Overlay (スロット全体)",
        ["DofDiffusionBloomOverlayEnabled"] = "ゲーム指定の複合オーバーレイスロット全体を使用",
        ["TiltShift"] = "チルトシフト",
        ["TiltShiftEnabled"] = "ゲーム指定のチルトシフトを使用",
        ["Fluctuation"] = "フラクチュエーション",
        ["FluctuationEnabled"] = "ゲーム指定のフラクチュエーションを使用",
        ["ChromaticAberration"] = "色収差",
        ["ChromaticAberrationEnabled"] = "ゲーム指定の色収差を使用",
        ["ToneCurve"] = "トーンカーブ",
        ["ToneCurveEnabled"] = "ゲーム指定のトーンカーブを使用",
        ["Exposure"] = "露出",
        ["ExposureEnabled"] = "ゲーム指定の露出を使用",
        ["ColorCorrection"] = "カラー補正",
        ["ColorCorrectionEnabled"] = "ゲーム指定のカラー補正を使用",
        ["ColorGrading"] = "カラーグレーディング",
        ["ColorGradingEnabled"] = "ゲーム指定のカラーグレーディングを使用",
        ["BgBlur"] = "背景ブラー",
        ["BgBlurEnabled"] = "ゲーム指定の背景ブラーを使用",
        ["Vortex"] = "ボルテックス",
        ["VortexEnabled"] = "ゲーム指定のボルテックスを使用",
        ["Aura"] = "オーラ",
        ["AuraUnsupported"] = "個別オフは未対応 — ポストプロセス全体設定でのみ抑制可能",
        ["FilmRoll"] = "フィルムロール",
        ["FilmRollEnabled"] = "ゲーム指定のフィルムロールを使用",
        ["Hatching"] = "ハッチング",
        ["HatchingEnabled"] = "ゲーム指定のハッチングを使用",
        ["LetterBox"] = "レターボックス",
        ["LetterBoxEnabled"] = "ゲーム指定のレターボックスを使用",
        ["RainSplash"] = "雨しぶき",
        ["RainSplashEnabled"] = "ゲーム指定の雨しぶきを使用",
        ["VfxDescription"] = "対応項目のONはゲーム指定状態を維持し、OFFは該当するauthored fieldのみ抑制します。最終合成エフェクト全体の除去を意味しません。",
        ["Locomotion"] = "移動",
        ["LocomotionEnabled"] = "右スティックで移動",
        ["LocomotionSpeed"] = "基準移動速度 (m/s、既定 5.00、上限なし)",
        ["LocomotionScaleCompensation"] = "ワールドスケール補正",
        ["LocomotionScaleCompensationEnabled"] = "ワールドスケールに合わせて移動速度を自動補正 (2.00で5.00)",
        ["SnapTurn"] = "スナップターン",
        ["SnapTurnEnabled"] = "左スティックでスナップターン",
        ["SnapAngle"] = "スナップ角度 (度)",
        ["HandSwap"] = "手の役割",
        ["HandSwapEnabled"] = "Primary / Secondaryコントローラーの全役割を交換",
        ["ControllerDescription"] = "自動補正は基準速度をワールドスケールに比例させます (2.00で5.00)。OFFでは入力値をそのまま使用します。初期設定では右手がポインター・Trigger・A/B・移動、左手が補助パネルのGrip・スナップターンを担当します。交換すると左手がポインター・Trigger・X/Y・移動、右手がパネルGrip・スナップターンを担当します。未実装のSmooth Turnは表示しません。",
        ["Save"] = "保存",
        ["Reload"] = "再読み込み",
        ["Defaults"] = "初期値",
        ["Export"] = "エクスポート...",
        ["Import"] = "インポート...",
        ["CheckUpdates"] = "更新を確認",
        ["StatusReady"] = "準備完了",
        ["StatusLoaded"] = "設定を読み込みました。",
        ["StatusSaved"] = "設定を保存しました。",
        ["StatusReloaded"] = "設定を再読み込みしました。",
        ["StatusDefaults"] = "初期値を読み込みました。適用するには保存を押してください。",
        ["StatusImported"] = "設定をインポートしました。ゲームフォルダーに適用するには保存を押してください。",
        ["StatusExported"] = "設定をエクスポートしました。",
        ["StatusLanguageChanged"] = "表示言語を変更しました。",
        ["UpdateChecking"] = "GitHubで更新を確認しています...",
        ["UpdateCurrent"] = "現在のv{0}は最新です。",
        ["UpdateAvailable"] = "v{0}の更新があります。",
        ["UpdateDownloading"] = "v{0}をダウンロードしています...",
        ["UpdateVerifying"] = "ダウンロードした更新を検証しています...",
        ["UpdateInstalling"] = "v{0}をインストールします。設定アプリを再起動します...",
        ["UpdateDeferredGameRunning"] = "v{0}の更新がありますが、ゲーム実行中はインストールできません。ゲーム終了後に再確認してください。",
        ["UpdateCompleted"] = "v{0}への自動更新が完了しました。",
        ["UpdateFailed"] = "自動更新に失敗しました: {0}",
        ["UpdateLauncherFailed"] = "自動更新インストーラーを起動できませんでした。",
        ["InvalidValues"] = "無効な値を安全な初期値に置き換えました: {0}",
        ["EyeScaleNormal"] = "推奨解像度比 約{0}%のピクセル負荷",
        ["EyeScaleWarning"] = "注意: 推奨解像度比 約{0}%のピクセル負荷です。",
        ["SelectGameFolder"] = "Umamusumeのゲームフォルダーを選択してください。",
        ["JsonOpenFilter"] = "JSON設定 (*.json)|*.json|すべてのファイル (*.*)|*.*",
        ["JsonSaveFilter"] = "JSON設定 (*.json)|*.json",
        ["InvalidSettingsFile"] = "設定ファイルに無効な値があります: {0}",
        ["GameExeMissing"] = "選択したフォルダーにumamusume.exeが見つかりません。",
        ["GameRunning"] = "ゲームを終了してから設定を保存してください。",
        ["InvalidSettingsToSave"] = "この設定は保存できません: {0}",
        ["SettingsPathNoParent"] = "設定パスに親フォルダーがありません。",
        ["ErrorPrefix"] = "エラー: {0}"
    };
}
