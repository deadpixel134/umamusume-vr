namespace UmaVR.Management;

public enum LocalifyStatus
{
    Absent,
    Partial,
    Installed
}

public sealed class PackageManifest
{
    public int SchemaVersion { get; set; }

    public string Version { get; set; } = string.Empty;

    public string Loader { get; set; } = string.Empty;

    public string LocalifyPolicy { get; set; } = string.Empty;

    public List<PackageFile> Files { get; set; } = new();
}

public sealed class PackageFile
{
    public string Path { get; set; } = string.Empty;

    public string Sha256 { get; set; } = string.Empty;

    public bool PreserveExisting { get; set; }

    public bool PreserveOnUninstall { get; set; }
}

public sealed class InstallState
{
    public int SchemaVersion { get; set; }

    public string Version { get; set; } = string.Empty;

    public string InstalledUtc { get; set; } = string.Empty;

    public string LocalifyStatus { get; set; } = string.Empty;

    public string BackupRoot { get; set; } = string.Empty;

    public string? PreviousStateBackup { get; set; }

    public List<InstallStateFile> Files { get; set; } = new();
}

public sealed class InstallStateFile
{
    public string Path { get; set; } = string.Empty;

    public string Action { get; set; } = string.Empty;

    public string? InstalledHash { get; set; }

    public bool PriorFile { get; set; }

    public string? BackupRelative { get; set; }

    public bool PreserveOnUninstall { get; set; }
}

public sealed class InstallationStatus
{
    public string GameRoot { get; set; } = string.Empty;

    public bool IsGameRoot { get; set; }

    public LocalifyStatus Localify { get; set; }

    public string? InstalledVersion { get; set; }

    public bool HasPreviousVersion { get; set; }

    public string? PackageVersion { get; set; }

    public bool PackageAvailable { get; set; }
}

public sealed class InstallationProgress
{
    public int Current { get; set; }

    public int Total { get; set; }

    public string Stage { get; set; } = string.Empty;

    public string? Path { get; set; }
}

public sealed class InstallationResult
{
    public string Version { get; set; } = string.Empty;

    public LocalifyStatus Localify { get; set; }

    public bool RestoredPreviousVersion { get; set; }

    public string? RestoredVersion { get; set; }

    public IReadOnlyList<string> Warnings { get; set; } = Array.Empty<string>();
}

public sealed class InstallationException : Exception
{
    public InstallationException(string code, params object[] arguments)
        : base(code)
    {
        Code = code;
        Arguments = arguments;
    }

    public string Code { get; }

    public IReadOnlyList<object> Arguments { get; }
}
