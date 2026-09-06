using System.Text.Json;
using System.Text.Json.Nodes;
using UmaVR.Management;

namespace UmaVR.Installer;

internal static class InstallerSelfTest
{
    private static readonly string[] RequiredPayload =
    {
        "UmaVR/immersive/umavr_immersive.dll",
        "vrmod/config/settings.json",
        "vrmod/tools/UmaVR.Configurator.exe",
        "vrmod/tools/UmaVR.Configurator.dll",
        "vrmod/tools/UmaVR.Configurator.deps.json",
        "vrmod/tools/UmaVR.Configurator.runtimeconfig.json",
        "vrmod/tools/UmaVR.Management.dll",
        "vrmod/LICENSE.txt",
        "vrmod/THIRD_PARTY_NOTICES.txt",
        "vrmod/DOTNET_LICENSE.txt",
        "vrmod/DOTNET_THIRD_PARTY_NOTICES.txt"
    };

    public static void Run()
    {
        Assert(Uri.TryCreate(InstallerText.LocalifyRepository, UriKind.Absolute, out Uri? localify) &&
            localify.Scheme == Uri.UriSchemeHttps &&
            string.Equals(localify.Host, "github.com", StringComparison.OrdinalIgnoreCase) &&
            localify.AbsolutePath == "/Kimjio/umamusume-localify",
            "official Localify installation URL");
        Assert(!InstallerText.NeedsLocalifyGuide(LocalifyStatus.Installed),
            "installed Localify hides guidance");
        Assert(InstallerText.NeedsLocalifyGuide(LocalifyStatus.Partial),
            "partial Localify shows guidance");
        Assert(InstallerText.NeedsLocalifyGuide(LocalifyStatus.Absent),
            "absent Localify shows guidance");
        Assert(ReleaseUpdatePolicy.IsNewer("0.1.0", "v0.1.1"), "version comparison");
        Assert(!ReleaseUpdatePolicy.IsNewer("0.1.0", "v0.1.0"), "equal version comparison");
        Assert(ReleaseUpdatePolicy.ParseSha256(
            new string('A', 64) + "  UmaVR-v0.1.1.zip", "UmaVR-v0.1.1.zip") ==
            new string('A', 64), "checksum parsing");

        string baseRoot = Path.Combine(Path.GetTempPath(), "UmaVR.Installer.Tests");
        string runRoot = Path.Combine(baseRoot, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(runRoot);
        try
        {
            string package = CreatePackage(runRoot);
            string game = CreateGame(runRoot, "game", withLoader: true);
            string originalConfig = File.ReadAllText(Path.Combine(game, "config.json"));
            string existingSettings = Path.Combine(game, "vrmod", "config", "settings.json");
            Directory.CreateDirectory(Path.GetDirectoryName(existingSettings)!);
            File.WriteAllText(existingSettings, "user-settings-sentinel");

            InstallationEngine engine = new();
            InstallationResult installed = engine.Install(game, package);
            Assert(installed.Version == "0.1.0", "installed version");
            Assert(File.ReadAllText(existingSettings) == "user-settings-sentinel", "settings preservation");
            Assert(File.Exists(Path.Combine(game, "UmaVR", "immersive", "umavr_immersive.dll")),
                "runtime install");
            JsonObject config = JsonNode.Parse(File.ReadAllText(Path.Combine(game, "config.json")))!
                .AsObject();
            JsonArray external = config["externalDlls"]!.AsArray();
            Assert(external.Count == 2, "external DLL merge count");
            Assert(external.Any(item => string.Equals(item?.GetValue<string>(), "other-mod.dll",
                StringComparison.Ordinal)), "existing external DLL preservation");
            Assert(external.Any(item => item?.GetValue<string>().EndsWith(
                "UmaVR\\immersive\\umavr_immersive.dll", StringComparison.OrdinalIgnoreCase) == true),
                "UmaVR bootstrap registration");

            InstallationResult removed = engine.Uninstall(game);
            Assert(removed.Warnings.Count == 0, "clean uninstall warnings");
            Assert(!File.Exists(Path.Combine(game, "UmaVR", "immersive", "umavr_immersive.dll")),
                "runtime uninstall");
            Assert(File.ReadAllText(existingSettings) == "user-settings-sentinel", "settings survive uninstall");
            Assert(File.ReadAllText(Path.Combine(game, "config.json")) == originalConfig,
                "byte-exact bootstrap rollback");

            string noLoader = CreateGame(runRoot, "no-loader", withLoader: false);
            ExpectCode(() => engine.Install(noLoader, package), "LocalifyRequired");
            Assert(!Directory.Exists(Path.Combine(noLoader, "UmaVR")), "loader failure leaves game unchanged");

            string badGame = CreateGame(runRoot, "bad-package", withLoader: true);
            File.AppendAllText(Path.Combine(package, "payload", RequiredPayload[0]), "tampered");
            ExpectCode(() => engine.Install(badGame, package), "PackageHashMismatch");
            Assert(!Directory.Exists(Path.Combine(badGame, "UmaVR")), "hash failure leaves game unchanged");
        }
        finally
        {
            string allowed = Path.GetFullPath(baseRoot).TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            string target = Path.GetFullPath(runRoot).TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            if (target.StartsWith(allowed, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(runRoot))
            {
                Directory.Delete(runRoot, recursive: true);
            }
        }
    }

    private static string CreateGame(string root, string name, bool withLoader)
    {
        string game = Path.Combine(root, name);
        Directory.CreateDirectory(game);
        foreach (string file in new[] { "umamusume.exe", "GameAssembly.dll", "UnityPlayer.dll" })
        {
            File.WriteAllText(Path.Combine(game, file), file);
        }
        File.WriteAllText(Path.Combine(game, "config.json"),
            "{\n  \"existing\": true,\n  \"externalDlls\": [\"other-mod.dll\"]\n}\n");
        if (withLoader)
        {
            File.WriteAllText(Path.Combine(game, "localify.dll"), "localify");
        }
        return game;
    }

    private static string CreatePackage(string root)
    {
        string package = Path.Combine(root, "package");
        string payload = Path.Combine(package, "payload");
        Directory.CreateDirectory(payload);
        PackageManifest manifest = new()
        {
            SchemaVersion = 1,
            Version = "0.1.0",
            Loader = "localify-external-dll",
            LocalifyPolicy = "require-compatible-existing",
        };
        foreach (string relative in RequiredPayload)
        {
            string path = Path.Combine(payload, relative.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, relative);
            manifest.Files.Add(new PackageFile
            {
                Path = relative,
                Sha256 = ReleaseUpdatePolicy.FileSha256(path),
                PreserveExisting = relative == "vrmod/config/settings.json",
                PreserveOnUninstall = relative == "vrmod/config/settings.json"
            });
        }
        File.WriteAllText(Path.Combine(package, "package-manifest.json"),
            JsonSerializer.Serialize(manifest, new JsonSerializerOptions
            {
                PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
                WriteIndented = true
            }));
        return package;
    }

    private static void ExpectCode(Action action, string code)
    {
        try
        {
            action();
            throw new InvalidOperationException($"Expected installation error: {code}");
        }
        catch (InstallationException exception) when (exception.Code == code)
        {
        }
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"Installer self-test failed: {message}");
        }
    }
}
