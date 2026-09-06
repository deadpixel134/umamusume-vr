namespace UmaVR.Configurator;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        StartupResult startup = StartupTasks.Run(args);
        UiText.Initialize();
        if (args.Contains("--verify-localization", StringComparer.OrdinalIgnoreCase))
        {
            return;
        }

        if (args.Contains("--self-test", StringComparer.OrdinalIgnoreCase))
        {
            SettingsSelfTest.Run();
            return;
        }

        if (args.Contains("--verify-layout", StringComparer.OrdinalIgnoreCase))
        {
            ApplicationConfiguration.Initialize();
            MainForm.VerifyLocalizedLayout();
            return;
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm(startup));
    }
}
