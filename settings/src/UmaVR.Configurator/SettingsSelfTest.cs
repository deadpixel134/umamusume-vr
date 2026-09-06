using System.Text.Json;

namespace UmaVR.Configurator;

internal static class SettingsSelfTest
{
    public static void Run()
    {
        string root = Path.Combine(Path.GetTempPath(), "umavr-settings-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            File.WriteAllBytes(Path.Combine(root, "umamusume.exe"), new byte[] { 0x4D, 0x5A });
            VrSettings requested = new()
            {
                LiveCameraFollow = true,
                WorldScale = 8.0f,
                EyeRenderScale = 0.75f,
                LocomotionEnabled = false,
                LocomotionSpeed = 1.75f,
                SnapTurnEnabled = false,
                SnapTurnAngleDegrees = 45.0f,
                NavigationHandsSwapped = true,
                PostProcessingEnabled = false,
                BlurEnabled = false,
                DepthOfFieldEnabled = false,
                DiffusionEnabled = false,
                BloomEnabled = false,
                GlobalFogEnabled = false,
                LensDistortionEnabled = false,
                RadialBlurEnabled = false,
                SunShaftsEnabled = false,
                IndirectLightShaftsEnabled = false,
                TransmittedLightEnabled = false,
                DofDiffusionBloomOverlayEnabled = false,
                TiltShiftEnabled = false,
                FluctuationEnabled = false,
                ChromaticAberrationEnabled = false,
                ToneCurveEnabled = false,
                ExposureEnabled = false,
                ColorCorrectionEnabled = false,
                ColorGradingEnabled = false,
                BgBlurEnabled = false,
                VortexEnabled = false,
                FilmRollEnabled = false,
                HatchingEnabled = false,
                LetterBoxEnabled = false,
                RainSplashEnabled = false
            };
            SettingsStore.SaveToGameRoot(root, requested);
            string path = SettingsStore.SettingsPath(root);
            if (!File.Exists(path)) throw new InvalidOperationException("settings file missing");

            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
            JsonElement json = document.RootElement;
            if (json.GetProperty("schemaVersion").GetInt32() != VrSettings.CurrentSchemaVersion ||
                !json.GetProperty("liveCameraFollow").GetBoolean() ||
                Math.Abs(json.GetProperty("worldScale").GetSingle() - 8.0f) > 0.0001f ||
                Math.Abs(json.GetProperty("eyeRenderScale").GetSingle() - 0.75f) > 0.0001f ||
                json.GetProperty("locomotionEnabled").GetBoolean() ||
                Math.Abs(json.GetProperty("locomotionSpeed").GetSingle() - 1.75f) > 0.0001f ||
                json.GetProperty("snapTurnEnabled").GetBoolean() ||
                Math.Abs(json.GetProperty("snapTurnAngleDegrees").GetSingle() - 45.0f) > 0.0001f ||
                !json.GetProperty("navigationHandsSwapped").GetBoolean() ||
                json.GetProperty("postProcessingEnabled").GetBoolean() ||
                json.GetProperty("blurEnabled").GetBoolean() ||
                json.GetProperty("depthOfFieldEnabled").GetBoolean() ||
                json.GetProperty("diffusionEnabled").GetBoolean() ||
                json.GetProperty("bloomEnabled").GetBoolean() ||
                json.GetProperty("globalFogEnabled").GetBoolean() ||
                json.GetProperty("lensDistortionEnabled").GetBoolean() ||
                json.GetProperty("radialBlurEnabled").GetBoolean() ||
                json.GetProperty("sunShaftsEnabled").GetBoolean() ||
                json.GetProperty("dofDiffusionBloomOverlayEnabled").GetBoolean() ||
                json.GetProperty("bgBlurEnabled").GetBoolean() ||
                json.GetProperty("rainSplashEnabled").GetBoolean())
            {
                throw new InvalidOperationException("serialized settings mismatch");
            }

            VrSettingsValidation roundTrip = SettingsStore.LoadFromGameRoot(root);
            if (roundTrip.UsedFallback || !roundTrip.Settings.LiveCameraFollow ||
                Math.Abs(roundTrip.Settings.WorldScale - 8.0f) > 0.0001f ||
                Math.Abs(roundTrip.Settings.EyeRenderScale - 0.75f) > 0.0001f ||
                roundTrip.Settings.LocomotionEnabled ||
                Math.Abs(roundTrip.Settings.LocomotionSpeed - 1.75f) > 0.0001f ||
                roundTrip.Settings.SnapTurnEnabled ||
                Math.Abs(roundTrip.Settings.SnapTurnAngleDegrees - 45.0f) > 0.0001f ||
                !roundTrip.Settings.NavigationHandsSwapped ||
                roundTrip.Settings.PostProcessingEnabled ||
                roundTrip.Settings.BlurEnabled || roundTrip.Settings.DepthOfFieldEnabled ||
                roundTrip.Settings.DiffusionEnabled || roundTrip.Settings.BloomEnabled ||
                roundTrip.Settings.GlobalFogEnabled || roundTrip.Settings.LensDistortionEnabled ||
                roundTrip.Settings.RadialBlurEnabled || AnyNewEffectsEnabled(roundTrip.Settings))
            {
                throw new InvalidOperationException("round-trip mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":8,\"liveCameraFollow\":true,\"worldScale\":8,\"eyeRenderScale\":9," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":99," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":5," +
                "\"navigationHandsSwapped\":true,\"postProcessingEnabled\":false,\"blurEnabled\":false," +
                "\"depthOfFieldEnabled\":false,\"diffusionEnabled\":false," +
                "\"bloomEnabled\":false,\"globalFogEnabled\":false," +
                "\"lensDistortionEnabled\":false,\"radialBlurEnabled\":false}");
            VrSettingsValidation invalid = SettingsStore.LoadFromGameRoot(root);
            if (!invalid.UsedFallback || !invalid.Settings.LiveCameraFollow ||
                Math.Abs(invalid.Settings.WorldScale - 8.0f) > 0.0001f ||
                Math.Abs(invalid.Settings.EyeRenderScale - VrSettings.DefaultEyeRenderScale) > 0.0001f ||
                Math.Abs(invalid.Settings.LocomotionSpeed - VrSettings.DefaultLocomotionSpeed) > 0.0001f ||
                Math.Abs(invalid.Settings.SnapTurnAngleDegrees - VrSettings.DefaultSnapTurnAngleDegrees) > 0.0001f ||
                !invalid.Settings.NavigationHandsSwapped || invalid.Settings.PostProcessingEnabled ||
                invalid.Settings.BlurEnabled ||
                invalid.Settings.DepthOfFieldEnabled || invalid.Settings.DiffusionEnabled ||
                invalid.Settings.BloomEnabled || invalid.Settings.GlobalFogEnabled ||
                invalid.Settings.LensDistortionEnabled || invalid.Settings.RadialBlurEnabled)
            {
                throw new InvalidOperationException("invalid-value fallback mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":8,\"worldScale\":0.50,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30}");
            VrSettingsValidation belowMinimumWorldScale = SettingsStore.LoadFromGameRoot(root);
            if (!belowMinimumWorldScale.UsedFallback ||
                Math.Abs(belowMinimumWorldScale.Settings.WorldScale -
                    VrSettings.DefaultWorldScale) > 0.0001f)
            {
                throw new InvalidOperationException("world-scale lower-bound fallback mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":1,\"liveCameraFollow\":true,\"eyeRenderScale\":0.75}");
            VrSettingsValidation migrated = SettingsStore.LoadFromGameRoot(root);
            if (migrated.UsedFallback || migrated.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migrated.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                !migrated.Settings.LocomotionEnabled ||
                Math.Abs(migrated.Settings.LocomotionSpeed - VrSettings.DefaultLocomotionSpeed) > 0.0001f ||
                !migrated.Settings.SnapTurnEnabled ||
                Math.Abs(migrated.Settings.SnapTurnAngleDegrees - VrSettings.DefaultSnapTurnAngleDegrees) > 0.0001f ||
                migrated.Settings.NavigationHandsSwapped || !migrated.Settings.PostProcessingEnabled ||
                !migrated.Settings.BlurEnabled ||
                !migrated.Settings.DepthOfFieldEnabled || !migrated.Settings.DiffusionEnabled ||
                !migrated.Settings.BloomEnabled || !SelectiveEffectsEnabled(migrated.Settings))
            {
                throw new InvalidOperationException("schema v1 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":2,\"liveCameraFollow\":false,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30}");
            VrSettingsValidation migratedV2 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV2.UsedFallback || migratedV2.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV2.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                migratedV2.Settings.NavigationHandsSwapped || !migratedV2.Settings.PostProcessingEnabled ||
                !migratedV2.Settings.BlurEnabled ||
                !migratedV2.Settings.DepthOfFieldEnabled || !migratedV2.Settings.DiffusionEnabled ||
                !migratedV2.Settings.BloomEnabled || !SelectiveEffectsEnabled(migratedV2.Settings))
            {
                throw new InvalidOperationException("schema v2 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":3,\"liveCameraFollow\":false,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30," +
                "\"navigationHandsSwapped\":false}");
            VrSettingsValidation migratedV3 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV3.UsedFallback || migratedV3.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV3.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                !migratedV3.Settings.PostProcessingEnabled || !migratedV3.Settings.BlurEnabled ||
                !migratedV3.Settings.DepthOfFieldEnabled ||
                !migratedV3.Settings.DiffusionEnabled || !migratedV3.Settings.BloomEnabled ||
                !SelectiveEffectsEnabled(migratedV3.Settings))
            {
                throw new InvalidOperationException("schema v3 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":4,\"liveCameraFollow\":false,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30," +
                "\"navigationHandsSwapped\":false,\"blurEnabled\":false," +
                "\"depthOfFieldEnabled\":false}");
            VrSettingsValidation migratedV4 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV4.UsedFallback || migratedV4.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV4.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                !migratedV4.Settings.PostProcessingEnabled || migratedV4.Settings.BlurEnabled ||
                migratedV4.Settings.DepthOfFieldEnabled ||
                !migratedV4.Settings.DiffusionEnabled || !migratedV4.Settings.BloomEnabled ||
                migratedV4.Settings.BgBlurEnabled || !LegacyNewEffectsEnabled(migratedV4.Settings))
            {
                throw new InvalidOperationException("schema v4 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":5,\"liveCameraFollow\":false,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30," +
                "\"navigationHandsSwapped\":false,\"blurEnabled\":false," +
                "\"depthOfFieldEnabled\":false,\"diffusionEnabled\":false," +
                "\"bloomEnabled\":false}");
            VrSettingsValidation migratedV5 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV5.UsedFallback || migratedV5.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV5.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                !migratedV5.Settings.PostProcessingEnabled || migratedV5.Settings.BlurEnabled ||
                migratedV5.Settings.DepthOfFieldEnabled || migratedV5.Settings.DiffusionEnabled ||
                migratedV5.Settings.BloomEnabled || migratedV5.Settings.BgBlurEnabled ||
                !LegacyNewEffectsEnabled(migratedV5.Settings))
            {
                throw new InvalidOperationException("schema v5 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":6,\"liveCameraFollow\":false,\"eyeRenderScale\":1," +
                "\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30," +
                "\"navigationHandsSwapped\":false,\"postProcessingEnabled\":false," +
                "\"blurEnabled\":false,\"depthOfFieldEnabled\":false," +
                "\"diffusionEnabled\":false,\"bloomEnabled\":false}");
            VrSettingsValidation migratedV6 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV6.UsedFallback || migratedV6.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV6.Settings.WorldScale - VrSettings.DefaultWorldScale) > 0.0001f ||
                migratedV6.Settings.PostProcessingEnabled || migratedV6.Settings.BlurEnabled ||
                migratedV6.Settings.DepthOfFieldEnabled || migratedV6.Settings.DiffusionEnabled ||
                migratedV6.Settings.BloomEnabled || migratedV6.Settings.BgBlurEnabled ||
                !LegacyNewEffectsEnabled(migratedV6.Settings))
            {
                throw new InvalidOperationException("schema v6 migration mismatch");
            }

            File.WriteAllText(path,
                "{\"schemaVersion\":7,\"liveCameraFollow\":false,\"worldScale\":2," +
                "\"eyeRenderScale\":1,\"locomotionEnabled\":true,\"locomotionSpeed\":1," +
                "\"snapTurnEnabled\":true,\"snapTurnAngleDegrees\":30," +
                "\"postProcessingEnabled\":true,\"blurEnabled\":true," +
                "\"depthOfFieldEnabled\":true,\"diffusionEnabled\":true," +
                "\"bloomEnabled\":true}");
            VrSettingsValidation migratedV7 = SettingsStore.LoadFromGameRoot(root);
            if (migratedV7.UsedFallback ||
                migratedV7.Settings.SchemaVersion != VrSettings.CurrentSchemaVersion ||
                Math.Abs(migratedV7.Settings.WorldScale - 2.0f) > 0.0001f ||
                !SelectiveEffectsEnabled(migratedV7.Settings))
            {
                throw new InvalidOperationException("schema v7 migration mismatch");
            }

            SettingsStore.SaveToGameRoot(root, requested);
            SettingsStore.SaveToGameRoot(root, new VrSettings());
            if (!File.Exists(path + ".bak")) throw new InvalidOperationException("atomic backup missing");
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    private static bool SelectiveEffectsEnabled(VrSettings settings) =>
        settings.GlobalFogEnabled && settings.LensDistortionEnabled && settings.RadialBlurEnabled &&
        !AnyNewEffectsDisabled(settings);

    private static bool AnyNewEffectsEnabled(VrSettings settings) =>
        settings.SunShaftsEnabled || settings.IndirectLightShaftsEnabled ||
        settings.TransmittedLightEnabled || settings.DofDiffusionBloomOverlayEnabled ||
        settings.TiltShiftEnabled || settings.FluctuationEnabled ||
        settings.ChromaticAberrationEnabled || settings.ToneCurveEnabled ||
        settings.ExposureEnabled || settings.ColorCorrectionEnabled ||
        settings.ColorGradingEnabled || settings.BgBlurEnabled || settings.VortexEnabled ||
        settings.FilmRollEnabled || settings.HatchingEnabled ||
        settings.LetterBoxEnabled || settings.RainSplashEnabled;

    private static bool AnyNewEffectsDisabled(VrSettings settings) =>
        !settings.SunShaftsEnabled || !settings.IndirectLightShaftsEnabled ||
        !settings.TransmittedLightEnabled || !settings.DofDiffusionBloomOverlayEnabled ||
        !settings.TiltShiftEnabled || !settings.FluctuationEnabled ||
        !settings.ChromaticAberrationEnabled || !settings.ToneCurveEnabled ||
        !settings.ExposureEnabled || !settings.ColorCorrectionEnabled ||
        !settings.ColorGradingEnabled || !settings.BgBlurEnabled || !settings.VortexEnabled ||
        !settings.FilmRollEnabled || !settings.HatchingEnabled ||
        !settings.LetterBoxEnabled || !settings.RainSplashEnabled;

    private static bool LegacyNewEffectsEnabled(VrSettings settings) =>
        settings.GlobalFogEnabled && settings.LensDistortionEnabled && settings.RadialBlurEnabled &&
        settings.SunShaftsEnabled && settings.IndirectLightShaftsEnabled &&
        settings.TransmittedLightEnabled && settings.DofDiffusionBloomOverlayEnabled &&
        settings.TiltShiftEnabled && settings.FluctuationEnabled &&
        settings.ChromaticAberrationEnabled && settings.ToneCurveEnabled &&
        settings.ExposureEnabled && settings.ColorCorrectionEnabled &&
        settings.ColorGradingEnabled && settings.VortexEnabled &&
        settings.FilmRollEnabled && settings.HatchingEnabled && settings.LetterBoxEnabled &&
        settings.RainSplashEnabled;
}
