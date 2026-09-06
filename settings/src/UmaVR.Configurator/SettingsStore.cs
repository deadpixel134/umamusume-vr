using System.Diagnostics;
using System.Text.Json;

namespace UmaVR.Configurator;

internal static class SettingsStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        WriteIndented = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true
    };

    public static string FindInitialGameRoot()
    {
        DirectoryInfo? current = new(AppContext.BaseDirectory);
        for (int depth = 0; depth < 6 && current is not null; depth++, current = current.Parent)
        {
            if (File.Exists(Path.Combine(current.FullName, "umamusume.exe")))
            {
                return current.FullName;
            }
        }

        return Environment.CurrentDirectory;
    }

    public static VrSettingsValidation LoadFromGameRoot(string gameRoot)
    {
        string path = SettingsPath(gameRoot);
        if (!File.Exists(path))
        {
            return new(VrSettings.CreateDefaults(), Array.Empty<string>());
        }

        try
        {
            VrSettings? parsed = JsonSerializer.Deserialize<VrSettings>(
                File.ReadAllText(path), JsonOptions);
            return VrSettingsValidator.Validate(parsed);
        }
        catch (JsonException)
        {
            return new(VrSettings.CreateDefaults(), new[] { "settings:malformed-json" });
        }
    }

    public static VrSettings LoadFile(string path)
    {
        VrSettings? parsed = JsonSerializer.Deserialize<VrSettings>(
            File.ReadAllText(path), JsonOptions);
        VrSettingsValidation validation = VrSettingsValidator.Validate(parsed);
        if (validation.UsedFallback)
        {
            throw new InvalidDataException(
                UiText.Format("InvalidSettingsFile", string.Join(", ", validation.Issues)));
        }

        return validation.Settings;
    }

    public static void SaveToGameRoot(string gameRoot, VrSettings settings)
    {
        EnsureGameStopped();
        string normalizedRoot = Path.GetFullPath(gameRoot);
        if (!File.Exists(Path.Combine(normalizedRoot, "umamusume.exe")))
        {
            throw new DirectoryNotFoundException(UiText.Get("GameExeMissing"));
        }

        SaveAtomic(SettingsPath(normalizedRoot), settings, createBackup: true);
    }

    public static void Export(string path, VrSettings settings) =>
        SaveAtomic(path, settings, createBackup: false);

    public static string SettingsPath(string gameRoot) =>
        Path.Combine(Path.GetFullPath(gameRoot), "vrmod", "config", "settings.json");

    private static void EnsureGameStopped()
    {
        using Process current = Process.GetCurrentProcess();
        if (Process.GetProcessesByName("umamusume").Any(process => process.Id != current.Id))
        {
            throw new InvalidOperationException(UiText.Get("GameRunning"));
        }
    }

    private static void SaveAtomic(string path, VrSettings settings, bool createBackup)
    {
        VrSettingsValidation validation = VrSettingsValidator.Validate(settings);
        if (validation.UsedFallback)
        {
            throw new InvalidDataException(
                UiText.Format("InvalidSettingsToSave", string.Join(", ", validation.Issues)));
        }

        string fullPath = Path.GetFullPath(path);
        string directory = Path.GetDirectoryName(fullPath) ??
            throw new InvalidOperationException(UiText.Get("SettingsPathNoParent"));
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(
            directory,
            $".{Path.GetFileName(fullPath)}.{Guid.NewGuid():N}.tmp");
        string backup = fullPath + ".bak";
        try
        {
            File.WriteAllText(
                temporary,
                JsonSerializer.Serialize(validation.Settings, JsonOptions) + Environment.NewLine);
            using (FileStream stream = new(
                temporary,
                FileMode.Open,
                FileAccess.ReadWrite,
                FileShare.None))
            {
                stream.Flush(flushToDisk: true);
            }

            if (File.Exists(fullPath))
            {
                File.Replace(temporary, fullPath, createBackup ? backup : null, true);
            }
            else
            {
                File.Move(temporary, fullPath);
            }
        }
        finally
        {
            if (File.Exists(temporary))
            {
                File.Delete(temporary);
            }
        }
    }
}
