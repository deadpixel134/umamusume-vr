namespace UmaVR.Configurator;

internal sealed class VrSettings
{
    public const int CurrentSchemaVersion = 9;
    public const float DefaultEyeRenderScale = 1.0f;
    public const float MinimumEyeRenderScale = 0.50f;
    public const float MaximumEyeRenderScale = 1.50f;
    public const float ExpensiveEyeRenderScale = 1.25f;
    public const float DefaultWorldScale = 1.0f;
    public const float MinimumWorldScale = 0.55f;
    public const float DefaultLocomotionSpeed = 1.0f;
    public const float MinimumLocomotionSpeed = 0.10f;
    public const float MaximumLocomotionSpeed = 5.0f;
    public const float DefaultSnapTurnAngleDegrees = 30.0f;
    public const float MinimumSnapTurnAngleDegrees = 15.0f;
    public const float MaximumSnapTurnAngleDegrees = 90.0f;

    public int SchemaVersion { get; set; } = CurrentSchemaVersion;

    public bool LiveCameraFollow { get; set; }

    public float WorldScale { get; set; } = DefaultWorldScale;

    public float EyeRenderScale { get; set; } = DefaultEyeRenderScale;

    public bool LocomotionEnabled { get; set; } = true;

    public float LocomotionSpeed { get; set; } = DefaultLocomotionSpeed;

    public bool SnapTurnEnabled { get; set; } = true;

    public float SnapTurnAngleDegrees { get; set; } = DefaultSnapTurnAngleDegrees;

    public bool NavigationHandsSwapped { get; set; }

    public bool PostProcessingEnabled { get; set; } = true;

    public bool BlurEnabled { get; set; } = true;

    public bool DepthOfFieldEnabled { get; set; } = true;

    public bool DiffusionEnabled { get; set; } = true;

    public bool BloomEnabled { get; set; } = true;

    public bool GlobalFogEnabled { get; set; } = true;

    public bool LensDistortionEnabled { get; set; } = true;

    public bool RadialBlurEnabled { get; set; } = true;

    public bool SunShaftsEnabled { get; set; } = true;
    public bool IndirectLightShaftsEnabled { get; set; } = true;
    public bool TransmittedLightEnabled { get; set; } = true;
    public bool DofDiffusionBloomOverlayEnabled { get; set; } = true;
    public bool TiltShiftEnabled { get; set; } = true;
    public bool FluctuationEnabled { get; set; } = true;
    public bool ChromaticAberrationEnabled { get; set; } = true;
    public bool ToneCurveEnabled { get; set; } = true;
    public bool ExposureEnabled { get; set; } = true;
    public bool ColorCorrectionEnabled { get; set; } = true;
    public bool ColorGradingEnabled { get; set; } = true;
    public bool BgBlurEnabled { get; set; } = true;
    public bool VortexEnabled { get; set; } = true;
    public bool FilmRollEnabled { get; set; } = true;
    public bool HatchingEnabled { get; set; } = true;
    public bool LetterBoxEnabled { get; set; } = true;
    public bool RainSplashEnabled { get; set; } = true;

    public static VrSettings CreateDefaults() => new();
}

internal sealed record VrSettingsValidation(VrSettings Settings, IReadOnlyList<string> Issues)
{
    public bool UsedFallback => Issues.Count != 0;
}

internal static class VrSettingsValidator
{
    public static VrSettingsValidation Validate(VrSettings? source)
    {
        List<string> issues = new();
        if (source is null)
        {
            issues.Add("settings:null");
            return new(VrSettings.CreateDefaults(), issues);
        }

        if (source.SchemaVersion is not 1 and not 2 and not 3 and not 4 and not 5 and not 6 and not 7 and not 8 and not VrSettings.CurrentSchemaVersion)
        {
            issues.Add($"schemaVersion:unsupported:{source.SchemaVersion}");
            return new(VrSettings.CreateDefaults(), issues);
        }

        float locomotionSpeed = source.LocomotionSpeed;
        if (!float.IsFinite(locomotionSpeed) ||
            locomotionSpeed < VrSettings.MinimumLocomotionSpeed ||
            locomotionSpeed > VrSettings.MaximumLocomotionSpeed)
        {
            issues.Add("locomotionSpeed:out-of-range");
            locomotionSpeed = VrSettings.DefaultLocomotionSpeed;
        }

        float snapAngle = source.SnapTurnAngleDegrees;
        if (!float.IsFinite(snapAngle) ||
            snapAngle < VrSettings.MinimumSnapTurnAngleDegrees ||
            snapAngle > VrSettings.MaximumSnapTurnAngleDegrees)
        {
            issues.Add("snapTurnAngleDegrees:out-of-range");
            snapAngle = VrSettings.DefaultSnapTurnAngleDegrees;
        }

        float eyeScale = source.EyeRenderScale;
        if (!float.IsFinite(eyeScale) ||
            eyeScale < VrSettings.MinimumEyeRenderScale ||
            eyeScale > VrSettings.MaximumEyeRenderScale)
        {
            issues.Add("eyeRenderScale:out-of-range");
            eyeScale = VrSettings.DefaultEyeRenderScale;
        }

        float worldScale = source.SchemaVersion < 7 ? VrSettings.DefaultWorldScale : source.WorldScale;
        if (!float.IsFinite(worldScale) ||
            worldScale < VrSettings.MinimumWorldScale)
        {
            issues.Add("worldScale:out-of-range");
            worldScale = VrSettings.DefaultWorldScale;
        }

        return new(
            new VrSettings
            {
                SchemaVersion = VrSettings.CurrentSchemaVersion,
                LiveCameraFollow = source.LiveCameraFollow,
                WorldScale = worldScale,
                EyeRenderScale = eyeScale,
                LocomotionEnabled = source.LocomotionEnabled,
                LocomotionSpeed = locomotionSpeed,
                SnapTurnEnabled = source.SnapTurnEnabled,
                SnapTurnAngleDegrees = snapAngle,
                NavigationHandsSwapped = source.NavigationHandsSwapped,
                PostProcessingEnabled = source.SchemaVersion < 6 || source.PostProcessingEnabled,
                BlurEnabled = source.SchemaVersion < 4 || source.BlurEnabled,
                DepthOfFieldEnabled = source.SchemaVersion < 4 || source.DepthOfFieldEnabled,
                DiffusionEnabled = source.SchemaVersion < 5 || source.DiffusionEnabled,
                BloomEnabled = source.SchemaVersion < 5 || source.BloomEnabled,
                GlobalFogEnabled = source.SchemaVersion < 8 || source.GlobalFogEnabled,
                LensDistortionEnabled = source.SchemaVersion < 8 || source.LensDistortionEnabled,
                RadialBlurEnabled = source.SchemaVersion < 8 || source.RadialBlurEnabled,
                SunShaftsEnabled = source.SchemaVersion < 9 || source.SunShaftsEnabled,
                IndirectLightShaftsEnabled = source.SchemaVersion < 9 || source.IndirectLightShaftsEnabled,
                TransmittedLightEnabled = source.SchemaVersion < 9 || source.TransmittedLightEnabled,
                DofDiffusionBloomOverlayEnabled = source.SchemaVersion < 9 || source.DofDiffusionBloomOverlayEnabled,
                TiltShiftEnabled = source.SchemaVersion < 9 || source.TiltShiftEnabled,
                FluctuationEnabled = source.SchemaVersion < 9 || source.FluctuationEnabled,
                ChromaticAberrationEnabled = source.SchemaVersion < 9 || source.ChromaticAberrationEnabled,
                ToneCurveEnabled = source.SchemaVersion < 9 || source.ToneCurveEnabled,
                ExposureEnabled = source.SchemaVersion < 9 || source.ExposureEnabled,
                ColorCorrectionEnabled = source.SchemaVersion < 9 || source.ColorCorrectionEnabled,
                ColorGradingEnabled = source.SchemaVersion < 9 || source.ColorGradingEnabled,
                BgBlurEnabled = source.SchemaVersion < 9 ?
                    (source.SchemaVersion < 4 || source.BlurEnabled) : source.BgBlurEnabled,
                VortexEnabled = source.SchemaVersion < 9 || source.VortexEnabled,
                FilmRollEnabled = source.SchemaVersion < 9 || source.FilmRollEnabled,
                HatchingEnabled = source.SchemaVersion < 9 || source.HatchingEnabled,
                LetterBoxEnabled = source.SchemaVersion < 9 || source.LetterBoxEnabled,
                RainSplashEnabled = source.SchemaVersion < 9 || source.RainSplashEnabled
            },
            issues);
    }
}
