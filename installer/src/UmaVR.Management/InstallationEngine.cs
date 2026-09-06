using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace UmaVR.Management;

public sealed class InstallationEngine
{
    private const int SupportedSchemaVersion = 1;
    private const string StateRelativePath = "vrmod/install-state.json";
    private const string ProductLoader = "localify-external-dll";
    private const string ProductDllRelativePath = "UmaVR/immersive/umavr_immersive.dll";
    private const string BootstrapConfigRelativePath = "config.json";
    private static readonly string[] RequiredProductFiles =
    {
        ProductDllRelativePath,
        "vrmod/config/settings.json",
        "vrmod/tools/UmaVR.Configurator.exe",
        "vrmod/LICENSE.txt",
        "vrmod/THIRD_PARTY_NOTICES.txt",
        "vrmod/DOTNET_LICENSE.txt",
        "vrmod/DOTNET_THIRD_PARTY_NOTICES.txt",
    };
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true
    };

    public static string FindInitialGameRoot()
    {
        DirectoryInfo? current = new(AppContext.BaseDirectory);
        for (int depth = 0; depth < 7 && current is not null; depth++, current = current.Parent)
        {
            if (IsGameRoot(current.FullName))
            {
                return current.FullName;
            }
        }
        return Environment.CurrentDirectory;
    }

    public static string? FindPackageRoot()
    {
        DirectoryInfo? current = new(AppContext.BaseDirectory);
        for (int depth = 0; depth < 4 && current is not null; depth++, current = current.Parent)
        {
            if (File.Exists(Path.Combine(current.FullName, "package-manifest.json")) &&
                Directory.Exists(Path.Combine(current.FullName, "payload")))
            {
                return current.FullName;
            }
        }
        return null;
    }

    public InstallationStatus Inspect(string gameRoot, string? packageRoot)
    {
        string game = Path.GetFullPath(gameRoot);
        bool validGame = IsGameRoot(game);
        string? installedVersion = null;
        bool hasPreviousVersion = false;
        if (validGame)
        {
            string statePath = ResolveContainedPath(game, StateRelativePath);
            if (File.Exists(statePath))
            {
                InstallState state = ReadState(statePath);
                installedVersion = state.Version;
                hasPreviousVersion = !string.IsNullOrWhiteSpace(state.PreviousStateBackup);
            }
        }

        string? packageVersion = null;
        bool packageAvailable = false;
        if (!string.IsNullOrWhiteSpace(packageRoot))
        {
            string package = Path.GetFullPath(packageRoot);
            string manifestPath = Path.Combine(package, "package-manifest.json");
            if (File.Exists(manifestPath) && Directory.Exists(Path.Combine(package, "payload")))
            {
                packageVersion = ReadManifest(manifestPath).Version;
                packageAvailable = true;
            }
        }

        return new InstallationStatus
        {
            GameRoot = game,
            IsGameRoot = validGame,
            Localify = validGame ? DetectLocalify(game) : LocalifyStatus.Absent,
            InstalledVersion = installedVersion,
            HasPreviousVersion = hasPreviousVersion,
            PackageVersion = packageVersion,
            PackageAvailable = packageAvailable
        };
    }

    public InstallationResult Install(
        string gameRoot,
        string packageRoot,
        IProgress<InstallationProgress>? progress = null)
    {
        string game = RequireGameRoot(gameRoot);
        EnsureGameStopped();
        RequireSupportedLoader(game);
        string package = Path.GetFullPath(packageRoot);
        PackageManifest manifest = ReadManifest(Path.Combine(package, "package-manifest.json"));
        string payload = Path.Combine(package, "payload");
        if (!Directory.Exists(payload))
        {
            throw new InstallationException("PackagePayloadMissing", payload);
        }
        ValidatePackagePayload(manifest, payload);

        LocalifyStatus localify = DetectLocalify(game);
        string timestamp = DateTime.Now.ToString("yyyyMMdd-HHmmss-fff");
        string backupRelative = $"vrmod/rollback/product-install-{manifest.Version}-{timestamp}";
        string backupRoot = ResolveContainedPath(game, backupRelative);
        string statePath = ResolveContainedPath(game, StateRelativePath);
        string? previousStateBackup = null;
        string? previousVersion = null;
        if (File.Exists(statePath))
        {
            InstallState previousState = ReadState(statePath);
            previousVersion = previousState.Version;
            previousStateBackup = "_previous-install-state.json";
            Directory.CreateDirectory(backupRoot);
            File.Copy(statePath, Path.Combine(backupRoot, previousStateBackup), overwrite: true);
        }

        List<InstallStateFile> completed = new();
        List<InstallStateFile> stateFiles = new();
        try
        {
            for (int index = 0; index < manifest.Files.Count; index++)
            {
                PackageFile file = manifest.Files[index];
                Report(progress, index, manifest.Files.Count, "Installing", file.Path);
                string source = ResolveContainedPath(payload, file.Path);
                string destination = ResolveContainedPath(game, file.Path);
                if (!File.Exists(source))
                {
                    throw new InstallationException("PackageFileMissing", file.Path);
                }
                string sourceHash = FileSha256(source);
                if (!string.Equals(sourceHash, file.Sha256, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InstallationException("PackageHashMismatch", file.Path);
                }

                bool priorFile = File.Exists(destination);
                if (file.PreserveExisting && priorFile)
                {
                    stateFiles.Add(new InstallStateFile
                    {
                        Path = file.Path,
                        Action = "preserved",
                        PriorFile = true,
                        PreserveOnUninstall = file.PreserveOnUninstall
                    });
                    continue;
                }

                string? fileBackupRelative = null;
                if (priorFile)
                {
                    string backup = ResolveContainedPath(backupRoot, file.Path);
                    Directory.CreateDirectory(Path.GetDirectoryName(backup)!);
                    File.Copy(destination, backup, overwrite: true);
                    fileBackupRelative = file.Path;
                }
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                File.Copy(source, destination, overwrite: true);
                string installedHash = FileSha256(destination);
                if (!string.Equals(installedHash, sourceHash, StringComparison.Ordinal))
                {
                    throw new InstallationException("InstalledHashMismatch", file.Path);
                }
                InstallStateFile entry = new()
                {
                    Path = file.Path,
                    Action = "installed",
                    InstalledHash = installedHash,
                    PriorFile = priorFile,
                    BackupRelative = fileBackupRelative,
                    PreserveOnUninstall = file.PreserveOnUninstall
                };
                completed.Add(entry);
                stateFiles.Add(entry);
            }

            InstallStateFile bootstrap = InstallBootstrapConfig(game, backupRoot);
            completed.Add(bootstrap);
            stateFiles.Add(bootstrap);

            InstallState state = new()
            {
                SchemaVersion = SupportedSchemaVersion,
                Version = manifest.Version,
                InstalledUtc = DateTimeOffset.UtcNow.ToString("o"),
                LocalifyStatus = localify.ToString(),
                BackupRoot = backupRelative,
                PreviousStateBackup = previousStateBackup,
                Files = stateFiles
            };
            WriteJsonAtomic(statePath, state);
            Report(progress, manifest.Files.Count, manifest.Files.Count, "Complete", null);
            return new InstallationResult
            {
                Version = manifest.Version,
                Localify = localify,
                RestoredPreviousVersion = false,
                RestoredVersion = previousVersion
            };
        }
        catch
        {
            for (int index = completed.Count - 1; index >= 0; index--)
            {
                InstallStateFile entry = completed[index];
                string destination = ResolveContainedPath(game, entry.Path);
                if (entry.PriorFile && entry.BackupRelative is not null)
                {
                    string backup = ResolveContainedPath(backupRoot, entry.BackupRelative);
                    if (File.Exists(backup))
                    {
                        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                        File.Copy(backup, destination, overwrite: true);
                    }
                }
                else if (File.Exists(destination))
                {
                    File.Delete(destination);
                }
            }
            throw;
        }
    }

    public InstallationResult Uninstall(
        string gameRoot,
        IProgress<InstallationProgress>? progress = null)
    {
        string game = RequireGameRoot(gameRoot);
        EnsureGameStopped();
        string statePath = ResolveContainedPath(game, StateRelativePath);
        if (!File.Exists(statePath))
        {
            throw new InstallationException("InstallStateMissing");
        }
        InstallState state = ReadState(statePath);
        string backupRoot = ResolveContainedPath(game, state.BackupRoot);
        List<string> warnings = new();

        for (int index = 0; index < state.Files.Count; index++)
        {
            InstallStateFile entry = state.Files[index];
            Report(progress, index, state.Files.Count, "Uninstalling", entry.Path);
            if (!string.Equals(entry.Action, "installed", StringComparison.Ordinal) ||
                entry.PreserveOnUninstall)
            {
                continue;
            }
            string destination = ResolveContainedPath(game, entry.Path);
            if (!File.Exists(destination))
            {
                warnings.Add($"Missing:{entry.Path}");
                continue;
            }
            if (entry.InstalledHash is null ||
                !string.Equals(FileSha256(destination), entry.InstalledHash, StringComparison.OrdinalIgnoreCase))
            {
                warnings.Add($"Modified:{entry.Path}");
                continue;
            }

            if (entry.PriorFile && entry.BackupRelative is not null)
            {
                string backup = ResolveContainedPath(backupRoot, entry.BackupRelative);
                if (!File.Exists(backup))
                {
                    warnings.Add($"BackupMissing:{entry.Path}");
                    continue;
                }
                File.Copy(backup, destination, overwrite: true);
            }
            else
            {
                File.Delete(destination);
            }
        }

        bool restoredPrevious = false;
        string? restoredVersion = null;
        if (warnings.Count == 0)
        {
            if (!string.IsNullOrWhiteSpace(state.PreviousStateBackup))
            {
                string previousStatePath = ResolveContainedPath(backupRoot, state.PreviousStateBackup);
                if (!File.Exists(previousStatePath))
                {
                    throw new InstallationException("PreviousStateMissing");
                }
                InstallState previousState = ReadState(previousStatePath);
                File.Copy(previousStatePath, statePath, overwrite: true);
                restoredPrevious = true;
                restoredVersion = previousState.Version;
            }
            else
            {
                File.Delete(statePath);
            }
        }
        Report(progress, state.Files.Count, state.Files.Count, "Complete", null);
        return new InstallationResult
        {
            Version = state.Version,
            Localify = ParseLocalify(state.LocalifyStatus),
            RestoredPreviousVersion = restoredPrevious,
            RestoredVersion = restoredVersion,
            Warnings = warnings
        };
    }

    public static LocalifyStatus DetectLocalify(string gameRoot)
    {
        bool loader = File.Exists(Path.Combine(gameRoot, "localify.dll"));
        bool config = File.Exists(Path.Combine(gameRoot, BootstrapConfigRelativePath));
        if (loader && config)
        {
            return LocalifyStatus.Installed;
        }
        if (loader || config)
        {
            return LocalifyStatus.Partial;
        }
        return LocalifyStatus.Absent;
    }

    public static bool IsGameRoot(string path) =>
        File.Exists(Path.Combine(path, "umamusume.exe")) &&
        File.Exists(Path.Combine(path, "GameAssembly.dll")) &&
        File.Exists(Path.Combine(path, "UnityPlayer.dll"));

    private static PackageManifest ReadManifest(string path)
    {
        if (!File.Exists(path))
        {
            throw new InstallationException("PackageManifestMissing", path);
        }
        PackageManifest manifest = JsonSerializer.Deserialize<PackageManifest>(
            File.ReadAllText(path), JsonOptions) ??
            throw new InstallationException("PackageManifestInvalid");
        if (manifest.SchemaVersion != SupportedSchemaVersion)
        {
            throw new InstallationException("PackageSchemaUnsupported", manifest.SchemaVersion);
        }
        if (string.IsNullOrWhiteSpace(manifest.Version) || manifest.Files.Count == 0)
        {
            throw new InstallationException("PackageManifestInvalid");
        }
        return manifest;
    }

    private static void ValidatePackagePayload(PackageManifest manifest, string payload)
    {
        Dictionary<string, PackageFile> files = new(StringComparer.OrdinalIgnoreCase);
        foreach (PackageFile file in manifest.Files)
        {
            string normalized = file.Path.Replace('\\', '/');
            if (string.IsNullOrWhiteSpace(normalized) || !files.TryAdd(normalized, file))
            {
                throw new InstallationException("PackageDuplicatePath", file.Path);
            }

            string source = ResolveContainedPath(payload, normalized);
            if (!File.Exists(source))
            {
                throw new InstallationException("PackageFileMissing", normalized);
            }
            if (string.IsNullOrWhiteSpace(file.Sha256) ||
                !string.Equals(FileSha256(source), file.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                throw new InstallationException("PackageHashMismatch", normalized);
            }
        }

        if (!string.Equals(manifest.Loader, ProductLoader, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        foreach (string required in RequiredProductFiles)
        {
            if (!files.ContainsKey(required))
            {
                throw new InstallationException("PackageRequiredFileMissing", required);
            }
        }

        PackageFile settings = files["vrmod/config/settings.json"];
        if (!settings.PreserveExisting || !settings.PreserveOnUninstall ||
            files.ContainsKey("localify.dll") || files.ContainsKey(BootstrapConfigRelativePath))
        {
            throw new InstallationException("PackagePolicyInvalid");
        }
    }

    private static InstallState ReadState(string path)
    {
        InstallState state = JsonSerializer.Deserialize<InstallState>(
            File.ReadAllText(path), JsonOptions) ??
            throw new InstallationException("InstallStateInvalid");
        if (state.SchemaVersion != SupportedSchemaVersion)
        {
            throw new InstallationException("InstallStateSchemaUnsupported", state.SchemaVersion);
        }
        return state;
    }

    private static string RequireGameRoot(string path)
    {
        string game = Path.GetFullPath(path);
        if (!IsGameRoot(game))
        {
            throw new InstallationException("GameRootInvalid");
        }
        return game;
    }

    private static void EnsureGameStopped()
    {
        Process[] processes = Process.GetProcessesByName("umamusume");
        try
        {
            if (processes.Length != 0)
            {
                throw new InstallationException("GameRunning");
            }
        }
        finally
        {
            foreach (Process process in processes)
            {
                process.Dispose();
            }
        }
    }

    private static string ResolveContainedPath(string root, string relativePath)
    {
        string normalized = relativePath.Replace('/', Path.DirectorySeparatorChar);
        if (Path.IsPathRooted(normalized) ||
            normalized.Split(Path.DirectorySeparatorChar).Any(segment => segment == ".."))
        {
            throw new InstallationException("UnsafePath", relativePath);
        }
        if (string.Equals(normalized, "localify.dll", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(normalized, "umamusume.exe.local", StringComparison.OrdinalIgnoreCase) ||
            normalized.StartsWith("umamusume.exe.local" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
        {
            throw new InstallationException("ProtectedLocalifyPath", relativePath);
        }
        string rootFull = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string candidate = Path.GetFullPath(Path.Combine(rootFull, normalized));
        if (!candidate.StartsWith(rootFull, StringComparison.OrdinalIgnoreCase))
        {
            throw new InstallationException("UnsafePath", relativePath);
        }
        return candidate;
    }

    private static string FileSha256(string path)
    {
        using FileStream stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static void RequireSupportedLoader(string gameRoot)
    {
        if (!File.Exists(Path.Combine(gameRoot, "localify.dll")) ||
            !File.Exists(Path.Combine(gameRoot, BootstrapConfigRelativePath)))
        {
            throw new InstallationException("LocalifyRequired");
        }
    }

    private static InstallStateFile InstallBootstrapConfig(string gameRoot, string backupRoot)
    {
        string configPath = ResolveContainedPath(gameRoot, BootstrapConfigRelativePath);
        string backupPath = ResolveContainedPath(backupRoot, BootstrapConfigRelativePath);
        Directory.CreateDirectory(Path.GetDirectoryName(backupPath)!);
        File.Copy(configPath, backupPath, overwrite: true);

        JsonObject root = JsonNode.Parse(File.ReadAllText(configPath)) as JsonObject ??
            throw new InstallationException("BootstrapConfigInvalid");
        JsonArray externalDlls;
        if (root["externalDlls"] is null)
        {
            externalDlls = new JsonArray();
            root["externalDlls"] = externalDlls;
        }
        else if (root["externalDlls"] is JsonArray array)
        {
            externalDlls = array;
        }
        else
        {
            throw new InstallationException("BootstrapConfigInvalid");
        }

        string productDll = ResolveContainedPath(gameRoot, ProductDllRelativePath);
        for (int index = externalDlls.Count - 1; index >= 0; index--)
        {
            if (externalDlls[index] is JsonValue value &&
                value.TryGetValue(out string? existing) &&
                !string.IsNullOrWhiteSpace(existing) &&
                PathsEqual(existing, productDll))
            {
                externalDlls.RemoveAt(index);
            }
        }
        externalDlls.Add(productDll);

        string temporary = configPath + $".UmaVR.{Guid.NewGuid():N}.tmp";
        try
        {
            File.WriteAllText(temporary, root.ToJsonString(new JsonSerializerOptions
            {
                WriteIndented = true
            }) + Environment.NewLine);
            using (FileStream stream = new(temporary, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            {
                stream.Flush(flushToDisk: true);
            }
            File.Move(temporary, configPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }

        return new InstallStateFile
        {
            Path = BootstrapConfigRelativePath,
            Action = "installed",
            InstalledHash = FileSha256(configPath),
            PriorFile = true,
            BackupRelative = BootstrapConfigRelativePath,
            PreserveOnUninstall = false
        };
    }

    private static bool PathsEqual(string first, string second)
    {
        try
        {
            return string.Equals(
                Path.GetFullPath(first),
                Path.GetFullPath(second),
                StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private static void WriteJsonAtomic(string path, InstallState state)
    {
        string directory = Path.GetDirectoryName(path)!;
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(directory, $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        string replaceBackup = path + ".replace-backup";
        try
        {
            File.WriteAllText(temporary, JsonSerializer.Serialize(state, JsonOptions) + Environment.NewLine);
            using (FileStream stream = new(temporary, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            {
                stream.Flush(flushToDisk: true);
            }
            if (File.Exists(path))
            {
                File.Replace(temporary, path, replaceBackup, ignoreMetadataErrors: true);
                File.Delete(replaceBackup);
            }
            else
            {
                File.Move(temporary, path);
            }
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
            if (File.Exists(replaceBackup)) File.Delete(replaceBackup);
        }
    }

    private static void Report(
        IProgress<InstallationProgress>? progress,
        int current,
        int total,
        string stage,
        string? path) =>
        progress?.Report(new InstallationProgress
        {
            Current = current,
            Total = total,
            Stage = stage,
            Path = path
        });

    private static LocalifyStatus ParseLocalify(string value) =>
        Enum.TryParse(value, ignoreCase: true, out LocalifyStatus result)
            ? result
            : LocalifyStatus.Absent;
}
