using System.Diagnostics;
using UmaVR.Management;

namespace UmaVR.Installer;

internal static class AutoUpdateRunner
{
    public static bool IsRequested(string[] args) =>
        args.Contains("--auto-update", StringComparer.OrdinalIgnoreCase);

    public static int Run(string[] args)
    {
        string gameRoot = Required(args, "--game-root");
        string packageRoot = Required(args, "--package-root");
        string cleanupRoot = Required(args, "--cleanup-update");
        int waitPid = int.Parse(Required(args, "--wait-pid"));
        WaitForProcess(waitPid);

        string logPath = UpdateLogPath();
        try
        {
            AppendLog(logPath, "Automatic update started.");
            InstallationResult result = new InstallationEngine().Install(gameRoot, packageRoot);
            AppendLog(logPath, $"Automatic update installed v{result.Version}.");
            LaunchConfigurator(
                gameRoot,
                cleanupRoot,
                "--updated-version",
                result.Version);
            return 0;
        }
        catch (Exception exception)
        {
            AppendLog(logPath, $"Automatic update failed: {exception.GetType().Name}: {exception.Message}");
            string errorFile = Path.Combine(cleanupRoot, "update-error.txt");
            try
            {
                File.WriteAllText(errorFile, exception.Message);
                LaunchConfigurator(gameRoot, cleanupRoot, "--update-error-file", errorFile);
            }
            catch (Exception relaunchException)
            {
                AppendLog(logPath, $"Configurator relaunch failed: {relaunchException.Message}");
            }
            return 1;
        }
    }

    private static void LaunchConfigurator(
        string gameRoot,
        string cleanupRoot,
        string resultArgument,
        string resultValue)
    {
        string configurator = Path.Combine(
            Path.GetFullPath(gameRoot),
            "vrmod",
            "tools",
            "UmaVR.Configurator.exe");
        if (!File.Exists(configurator))
        {
            throw new FileNotFoundException("The installed configurator is missing.", configurator);
        }

        ProcessStartInfo start = new(configurator) { UseShellExecute = true };
        start.ArgumentList.Add("--cleanup-update");
        start.ArgumentList.Add(cleanupRoot);
        start.ArgumentList.Add("--wait-pid");
        start.ArgumentList.Add(Environment.ProcessId.ToString());
        start.ArgumentList.Add(resultArgument);
        start.ArgumentList.Add(resultValue);
        _ = Process.Start(start) ?? throw new InvalidOperationException(
            "The updated configurator could not be started.");
    }

    private static void WaitForProcess(int processId)
    {
        if (processId <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(processId));
        }
        try
        {
            using Process process = Process.GetProcessById(processId);
            if (!process.WaitForExit(30_000))
            {
                throw new TimeoutException("The settings application did not exit in time.");
            }
        }
        catch (ArgumentException)
        {
            // The settings application already exited.
        }
    }

    private static string Required(string[] args, string name)
    {
        for (int index = 0; index + 1 < args.Length; index++)
        {
            if (string.Equals(args[index], name, StringComparison.OrdinalIgnoreCase))
            {
                return args[index + 1];
            }
        }
        throw new ArgumentException($"Missing automatic update argument: {name}");
    }

    private static string UpdateLogPath()
    {
        string directory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "UmaVR");
        Directory.CreateDirectory(directory);
        return Path.Combine(directory, "update.log");
    }

    private static void AppendLog(string path, string message) =>
        File.AppendAllText(path, $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}");
}
