using System.Diagnostics;

namespace UmaVR.Configurator;

internal sealed record StartupResult(string? UpdatedVersion, string? UpdateError);

internal static class StartupTasks
{
    public static StartupResult Run(string[] args)
    {
        string? waitPidText = Value(args, "--wait-pid");
        if (int.TryParse(waitPidText, out int waitPid) && waitPid > 0)
        {
            try
            {
                using Process process = Process.GetProcessById(waitPid);
                process.WaitForExit(30_000);
            }
            catch (ArgumentException)
            {
                // The updater already exited.
            }
        }

        string? error = null;
        string? errorFile = Value(args, "--update-error-file");
        if (!string.IsNullOrWhiteSpace(errorFile))
        {
            try
            {
                error = File.ReadAllText(errorFile);
                File.Delete(errorFile);
            }
            catch
            {
                error = "The automatic update failed.";
            }
        }

        string? cleanup = Value(args, "--cleanup-update");
        if (!string.IsNullOrWhiteSpace(cleanup))
        {
            TryDeleteUpdateDirectory(cleanup);
        }
        return new StartupResult(Value(args, "--updated-version"), error);
    }

    private static string? Value(string[] args, string name)
    {
        for (int index = 0; index + 1 < args.Length; index++)
        {
            if (string.Equals(args[index], name, StringComparison.OrdinalIgnoreCase))
            {
                return args[index + 1];
            }
        }
        return null;
    }

    private static void TryDeleteUpdateDirectory(string path)
    {
        try
        {
            string updatesRoot = Path.GetFullPath(Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UmaVR",
                "updates")).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string target = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            if (target.StartsWith(updatesRoot, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch
        {
            // Cleanup is best effort and never blocks the settings application.
        }
    }
}
