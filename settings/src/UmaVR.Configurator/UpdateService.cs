using System.Diagnostics;
using System.IO.Compression;
using System.Net.Http.Json;
using System.Reflection;
using System.Text.Json.Serialization;
using UmaVR.Management;

namespace UmaVR.Configurator;

internal sealed class UpdateService : IDisposable
{
    private const string RepositoryApi =
        "https://api.github.com/repos/deadpixel134/umamusume-vr";
    private const long MaximumAssetBytes = 512L * 1024 * 1024;
    private readonly HttpClient _http = new()
    {
        Timeout = TimeSpan.FromMinutes(5)
    };

    public UpdateService()
    {
        _http.DefaultRequestHeaders.UserAgent.ParseAdd("UmaVR-Configurator/0.1.1");
        _http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
    }

    public static string CurrentVersion =>
        Assembly.GetExecutingAssembly().GetName().Version?.ToString(3) ?? "0.0.0";

    public async Task<AvailableUpdate?> CheckAsync(CancellationToken cancellationToken)
    {
        GithubRelease release = await _http.GetFromJsonAsync<GithubRelease>(
            $"{RepositoryApi}/releases/latest",
            cancellationToken) ?? throw new InvalidDataException("GitHub returned an empty release.");
        if (release.Draft || release.Prerelease ||
            !ReleaseUpdatePolicy.IsNewer(CurrentVersion, release.TagName))
        {
            return null;
        }

        Version version = ReleaseUpdatePolicy.ParseVersion(release.TagName);
        string normalizedVersion = version.ToString(3);
        string archiveName = $"UmaVR-v{normalizedVersion}.zip";
        string checksumName = archiveName + ".sha256";
        GithubAsset archive = release.Assets.SingleOrDefault(asset =>
            string.Equals(asset.Name, archiveName, StringComparison.Ordinal)) ??
            throw new InvalidDataException($"Release asset is missing: {archiveName}");
        GithubAsset? checksum = release.Assets.SingleOrDefault(asset =>
            string.Equals(asset.Name, checksumName, StringComparison.Ordinal));
        if (checksum is null && string.IsNullOrWhiteSpace(archive.Digest))
        {
            throw new InvalidDataException($"Release checksum is missing: {checksumName}");
        }
        if (archive.Size <= 0 || archive.Size > MaximumAssetBytes)
        {
            throw new InvalidDataException("The release archive size is invalid.");
        }

        return new AvailableUpdate(
            normalizedVersion,
            archiveName,
            archive.BrowserDownloadUrl,
            archive.Digest,
            checksum?.BrowserDownloadUrl);
    }

    public async Task<StagedUpdate> StageAsync(
        AvailableUpdate update,
        string gameRoot,
        IProgress<string>? progress,
        CancellationToken cancellationToken)
    {
        string updatesRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "UmaVR",
            "updates");
        Directory.CreateDirectory(updatesRoot);
        string stagingRoot = Path.Combine(
            updatesRoot,
            $"v{update.Version}-{Guid.NewGuid():N}");
        Directory.CreateDirectory(stagingRoot);
        try
        {
            string archivePath = Path.Combine(stagingRoot, update.ArchiveName);
            progress?.Report(UiText.Format("UpdateDownloading", update.Version));
            await DownloadFileAsync(
                update.ArchiveUrl,
                archivePath,
                cancellationToken);

            string expectedHash;
            if (!string.IsNullOrWhiteSpace(update.ChecksumUrl))
            {
                string checksumText = await _http.GetStringAsync(
                    update.ChecksumUrl,
                    cancellationToken);
                expectedHash = ReleaseUpdatePolicy.ParseSha256(
                    checksumText,
                    update.ArchiveName);
            }
            else
            {
                expectedHash = ReleaseUpdatePolicy.ParseSha256(
                    update.Digest ?? string.Empty,
                    update.ArchiveName);
            }

            progress?.Report(UiText.Get("UpdateVerifying"));
            string packageRoot = Path.Combine(stagingRoot, "package");
            await Task.Run(
                () => VerifyAndExtractPackage(
                    archivePath,
                    expectedHash,
                    packageRoot,
                    gameRoot,
                    update.Version,
                    cancellationToken),
                cancellationToken);
            string installer = Path.Combine(packageRoot, "UmaVR.Installer.exe");
            return new StagedUpdate(update.Version, stagingRoot, packageRoot, installer);
        }
        catch
        {
            TryDeleteContainedUpdate(stagingRoot, updatesRoot);
            throw;
        }
    }

    public static bool IsGameRunning()
    {
        Process[] processes = Process.GetProcessesByName("umamusume");
        try
        {
            return processes.Length != 0;
        }
        finally
        {
            foreach (Process process in processes)
            {
                process.Dispose();
            }
        }
    }

    public void Dispose() => _http.Dispose();

    private async Task DownloadFileAsync(
        string url,
        string destination,
        CancellationToken cancellationToken)
    {
        using HttpResponseMessage response = await _http.GetAsync(
            url,
            HttpCompletionOption.ResponseHeadersRead,
            cancellationToken);
        response.EnsureSuccessStatusCode();
        if (response.Content.Headers.ContentLength is long length &&
            (length <= 0 || length > MaximumAssetBytes))
        {
            throw new InvalidDataException("The downloaded update size is invalid.");
        }

        await using Stream source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using FileStream destinationStream = new(
            destination,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None,
            128 * 1024,
            FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[128 * 1024];
        long total = 0;
        while (true)
        {
            int read = await source.ReadAsync(buffer, cancellationToken);
            if (read == 0)
            {
                break;
            }
            total += read;
            if (total > MaximumAssetBytes)
            {
                throw new InvalidDataException("The downloaded update exceeded the size limit.");
            }
            await destinationStream.WriteAsync(buffer.AsMemory(0, read), cancellationToken);
        }
        await destinationStream.FlushAsync(cancellationToken);
        destinationStream.Flush(flushToDisk: true);
    }

    private static void VerifyAndExtractPackage(
        string archivePath,
        string expectedHash,
        string packageRoot,
        string gameRoot,
        string version,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string actualHash = ReleaseUpdatePolicy.FileSha256(archivePath);
        if (!string.Equals(expectedHash, actualHash, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("The downloaded update SHA-256 does not match.");
        }

        cancellationToken.ThrowIfCancellationRequested();
        ZipFile.ExtractToDirectory(archivePath, packageRoot);
        string installer = Path.Combine(packageRoot, "UmaVR.Installer.exe");
        if (!File.Exists(installer))
        {
            throw new FileNotFoundException("The update installer is missing.", installer);
        }

        cancellationToken.ThrowIfCancellationRequested();
        InstallationStatus status = new InstallationEngine().Inspect(gameRoot, packageRoot);
        if (!status.IsGameRoot || !status.PackageAvailable ||
            !string.Equals(status.PackageVersion, version, StringComparison.Ordinal))
        {
            throw new InvalidDataException("The downloaded package manifest is invalid.");
        }
    }

    private static void TryDeleteContainedUpdate(string path, string updatesRoot)
    {
        try
        {
            string root = Path.GetFullPath(updatesRoot).TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            string target = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            if (target.StartsWith(root, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch
        {
            // A later run may clean an abandoned staging directory.
        }
    }

    private sealed class GithubRelease
    {
        [JsonPropertyName("tag_name")]
        public string TagName { get; set; } = string.Empty;

        [JsonPropertyName("draft")]
        public bool Draft { get; set; }

        [JsonPropertyName("prerelease")]
        public bool Prerelease { get; set; }

        [JsonPropertyName("assets")]
        public List<GithubAsset> Assets { get; set; } = new();
    }

    private sealed class GithubAsset
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = string.Empty;

        [JsonPropertyName("browser_download_url")]
        public string BrowserDownloadUrl { get; set; } = string.Empty;

        [JsonPropertyName("size")]
        public long Size { get; set; }

        [JsonPropertyName("digest")]
        public string? Digest { get; set; }
    }
}

internal sealed record AvailableUpdate(
    string Version,
    string ArchiveName,
    string ArchiveUrl,
    string? Digest,
    string? ChecksumUrl);

internal sealed record StagedUpdate(
    string Version,
    string StagingRoot,
    string PackageRoot,
    string InstallerPath);
