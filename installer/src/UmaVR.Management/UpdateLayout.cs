using System.Text.Json.Nodes;

namespace UmaVR.Management;

public sealed partial class InstallationEngine
{
    // This contract covers the Localify external-DLL layout and framework-dependent tool files.
    // Other loaders and settings locations need a version-specific migration.
    private static readonly StringComparer LayoutComparer = StringComparer.OrdinalIgnoreCase;

    public void VerifyAutomaticUpdate(string game, string package)
    {
        var manifest = ReadManifest(Path.Combine(package, "package-manifest.json"));
        InspectUpdateLayout(RequireGameRoot(game), manifest, automatic: true);
    }

    private static List<InstallStateFile> InspectUpdateLayout(string game, PackageManifest manifest, bool automatic)
    {
        var retired = new List<InstallStateFile>();
        var statePath = ResolveContainedPath(game, StateRelativePath);
        RequireOrdinaryPath(game, StateRelativePath);
        if (automatic && manifest.UpdateLayoutVersion != 1)
            throw Manual("package-manifest.json");
        if (!File.Exists(statePath))
        {
            if (automatic) throw Manual(StateRelativePath);
            return retired;
        }

        var state = ReadState(statePath);
        if (!LayoutComparer.Equals(manifest.Loader, ProductLoader) ||
            !LayoutComparer.Equals(manifest.LocalifyPolicy, "require-compatible-existing") ||
            state.Files is null || state.Files.Count == 0)
            throw Manual(StateRelativePath);
        if (automatic && !ReleaseUpdatePolicy.IsNewer(state.Version, manifest.Version))
            throw Manual(state.Version);

        var target = new Dictionary<string, PackageFile>(LayoutComparer);
        foreach (var file in manifest.Files)
        {
            string path = Canonical(file.Path);
            if (!(RequiredProductFiles.Contains(path, LayoutComparer) ||
                LayoutComparer.Equals(path, "vrmod/CONTROLS.txt") || IsRetirableTool(path)) ||
                !target.TryAdd(path, file)) throw Manual(path);
            RequireOrdinaryPath(game, path);
        }
        if (RequiredProductFiles.Any(path => !target.ContainsKey(path)) ||
            !target["vrmod/config/settings.json"].PreserveExisting ||
            !target["vrmod/config/settings.json"].PreserveOnUninstall)
            throw Manual("package-manifest.json");

        var previous = new HashSet<string>(LayoutComparer);
        foreach (var entry in state.Files)
        {
            string path = Canonical(entry.Path);
            RequireOrdinaryPath(game, path);
            if (!previous.Add(path)) throw Manual(path);
            string existing = ResolveContainedPath(game, path);
            if (entry.Action == "retired")
            {
                if (!IsRetirableTool(path) || File.Exists(existing) || Directory.Exists(existing)) throw Manual(path);
                continue;
            }
            if (LayoutComparer.Equals(path, "vrmod/config/settings.json") && entry.PreserveOnUninstall &&
                entry.Action is "installed" or "preserved") continue;
            bool obsolete = !target.ContainsKey(path) && !LayoutComparer.Equals(path, BootstrapConfigRelativePath);
            if (entry.Action != "installed" || entry.PreserveOnUninstall ||
                (obsolete && (!IsRetirableTool(path) || entry.PriorFile)) ||
                !File.Exists(existing) || !LayoutComparer.Equals(FileSha256(existing), entry.InstalledHash))
                throw Manual(path);
            if (obsolete)
                retired.Add(new InstallStateFile { Path = path, Action = "retired", PriorFile = true,
                    BackupRelative = path, InstalledHash = entry.InstalledHash });
        }
        foreach (string path in new[]{ ProductDllRelativePath, BootstrapConfigRelativePath, "vrmod/config/settings.json" })
            if (!previous.Contains(path)) throw Manual(path);
        foreach (var pair in target)
        {
            var dest = ResolveContainedPath(game, pair.Key);
            if (Directory.Exists(dest) || (!previous.Contains(pair.Key) && File.Exists(dest)))
                throw Manual(pair.Key);
        }

        var config = JsonNode.Parse(File.ReadAllText(ResolveContainedPath(game, BootstrapConfigRelativePath)));
        string runtime = ResolveContainedPath(game, ProductDllRelativePath);
        if (config?["externalDlls"] is not JsonArray dlls || !dlls.Any(node =>
            node is JsonValue value && value.TryGetValue<string>(out var registered) && PathsEqual(registered, runtime)))
            throw Manual(BootstrapConfigRelativePath);
        foreach (var obsolete in retired)
        {
            string path = ResolveContainedPath(game, obsolete.Path);
            if (dlls.OfType<JsonValue>().Any(value => value.TryGetValue<string>(out var name) &&
                !string.IsNullOrWhiteSpace(name) && PathsEqual(Path.Combine(game, name), path)))
                throw Manual(obsolete.Path);
        }
        return retired;
    }

    private static string Canonical(string path)
    {
        string normalized = path.Replace('\\', '/');
        if (normalized.Split('/').Any(part => part.Length == 0 || part is "." or ".." ||
            part.EndsWith('.') || part.EndsWith(' ') || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0))
            throw Manual(path);
        return normalized;
    }

    private static bool IsRetirableTool(string path) =>
        Canonical(path).StartsWith("vrmod/tools/", StringComparison.OrdinalIgnoreCase) &&
        (path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
         path.EndsWith(".pdb", StringComparison.OrdinalIgnoreCase) ||
         path.EndsWith(".deps.json", StringComparison.OrdinalIgnoreCase) ||
         path.EndsWith(".runtimeconfig.json", StringComparison.OrdinalIgnoreCase));

    private static void RequireOrdinaryPath(string root, string relative)
    {
        string current = root;
        foreach (var part in Canonical(relative).Split('/'))
        {
            current = Path.Combine(current, part);
            try
            {
                if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw Manual(relative);
            }
            catch (FileNotFoundException) { }
            catch (DirectoryNotFoundException) { }
        }
    }

    private static InstallationException Manual(string path) => new("UpdateLayoutManual", path);

    private static IDisposable LockInstallation(string game)
    {
        string key = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(
            System.Text.Encoding.UTF8.GetBytes(Path.TrimEndingDirectorySeparator(game).ToUpperInvariant())));
        var mutex = new Mutex(false, "Local\\UmaVR-Install-" + key);
        try
        {
            if (!mutex.WaitOne(0)) throw Manual(StateRelativePath);
            return new MutexLease(mutex);
        }
        catch (AbandonedMutexException) { mutex.ReleaseMutex(); mutex.Dispose(); throw Manual(StateRelativePath); }
        catch { mutex.Dispose(); throw; }
    }

    private sealed class MutexLease(Mutex mutex) : IDisposable
    {
        public void Dispose() { mutex.ReleaseMutex(); mutex.Dispose(); }
    }
}
