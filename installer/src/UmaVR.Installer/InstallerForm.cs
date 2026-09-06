using System.Diagnostics;
using UmaVR.Management;

namespace UmaVR.Installer;

internal sealed class InstallerForm : Form
{
    private readonly InstallationEngine _engine = new();
    private readonly string? _packageRoot = InstallationEngine.FindPackageRoot();
    private readonly TextBox _gameRoot = new() { Dock = DockStyle.Fill };
    private readonly Label _packageVersion = ValueLabel();
    private readonly Label _installedVersion = ValueLabel();
    private readonly Label _localifyStatus = ValueLabel();
    private readonly ProgressBar _progress = new() { Dock = DockStyle.Fill, Minimum = 0, Maximum = 100 };
    private readonly Label _status = new() { Dock = DockStyle.Fill, AutoSize = true };
    private readonly TextBox _log = new()
    {
        Dock = DockStyle.Fill,
        Multiline = true,
        ReadOnly = true,
        ScrollBars = ScrollBars.Vertical,
        BackColor = SystemColors.Window
    };
    private readonly Button _install = TaggedButton("Install");
    private readonly Button _uninstall = TaggedButton("Uninstall");
    private readonly Button _openSettings = TaggedButton("OpenSettings");
    private readonly Button _refresh = TaggedButton("Refresh");
    private InstallationStatus? _currentStatus;
    private bool _busy;

    public InstallerForm()
    {
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(820, 640);
        ClientSize = new Size(960, 720);
        AutoScaleMode = AutoScaleMode.Dpi;

        TableLayoutPanel root = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 8,
            Padding = new Padding(16)
        };
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        Controls.Add(root);

        Label heading = new()
        {
            Tag = "Heading",
            AutoSize = true,
            Font = new Font(Font.FontFamily, 16f, FontStyle.Bold),
            Margin = new Padding(0, 0, 0, 3)
        };
        root.Controls.Add(heading, 0, 0);

        TableLayoutPanel languageAndDescription = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 2
        };
        languageAndDescription.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        languageAndDescription.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        languageAndDescription.Controls.Add(new Label
        {
            Tag = "Description",
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            ForeColor = SystemColors.GrayText
        }, 0, 0);
        FlowLayoutPanel languages = new()
        {
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };
        languages.Controls.Add(LanguageButton("한국어", InstallerLanguage.Korean));
        languages.Controls.Add(LanguageButton("English", InstallerLanguage.English));
        languages.Controls.Add(LanguageButton("日本語", InstallerLanguage.Japanese));
        languageAndDescription.Controls.Add(languages, 1, 0);
        root.Controls.Add(languageAndDescription, 0, 1);

        TableLayoutPanel folderRow = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 3,
            Margin = new Padding(0, 12, 0, 8)
        };
        folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        folderRow.Controls.Add(new Label
        {
            Tag = "GameFolder",
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 7, 10, 0)
        }, 0, 0);
        folderRow.Controls.Add(_gameRoot, 1, 0);
        Button browse = TaggedButton("Browse");
        browse.Click += Browse;
        folderRow.Controls.Add(browse, 2, 0);
        root.Controls.Add(folderRow, 0, 2);

        GroupBox information = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            Padding = new Padding(12),
            Text = string.Empty
        };
        TableLayoutPanel informationGrid = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 2
        };
        informationGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 35));
        informationGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 65));
        AddInfo(informationGrid, "PackageVersion", _packageVersion);
        AddInfo(informationGrid, "InstalledVersion", _installedVersion);
        AddInfo(informationGrid, "LocalifyStatus", _localifyStatus);
        information.Controls.Add(informationGrid);
        root.Controls.Add(information, 0, 3);

        root.Controls.Add(_progress, 0, 4);
        _status.Margin = new Padding(0, 6, 0, 6);
        root.Controls.Add(_status, 0, 5);
        root.Controls.Add(_log, 0, 6);

        FlowLayoutPanel actions = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Padding = new Padding(0, 10, 0, 0)
        };
        _install.Click += Install;
        _uninstall.Click += Uninstall;
        _openSettings.Click += OpenSettings;
        _refresh.Click += (_, _) => RefreshStatus();
        actions.Controls.Add(_install);
        actions.Controls.Add(_uninstall);
        actions.Controls.Add(_openSettings);
        actions.Controls.Add(_refresh);
        root.Controls.Add(actions, 0, 7);

        _gameRoot.Text = InstallationEngine.FindInitialGameRoot();
        ApplyLanguage(languageChanged: false);
        Shown += (_, _) => RefreshStatus();
    }

    private void ApplyLanguage(bool languageChanged)
    {
        Text = InstallerText.Get("AppTitle");
        UpdateTaggedText(this);
        if (languageChanged)
        {
            SetStatus("LanguageChanged");
        }
        RefreshStatusLabels();
    }

    private void ChangeLanguage(InstallerLanguage language)
    {
        InstallerText.SetLanguage(language);
        ApplyLanguage(languageChanged: true);
    }

    private void Browse(object? sender, EventArgs args)
    {
        using FolderBrowserDialog dialog = new()
        {
            Description = InstallerText.Get("GameRootInvalid"),
            SelectedPath = _gameRoot.Text,
            UseDescriptionForTitle = true
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _gameRoot.Text = dialog.SelectedPath;
            RefreshStatus();
        }
    }

    private void RefreshStatus(bool setReady = true)
    {
        if (_busy)
        {
            return;
        }
        try
        {
            _currentStatus = _engine.Inspect(_gameRoot.Text, _packageRoot);
            RefreshStatusLabels();
            if (setReady)
            {
                SetStatus("Ready");
            }
        }
        catch (Exception exception)
        {
            _currentStatus = null;
            string message = InstallerText.ExceptionMessage(exception);
            _status.Text = InstallerText.Format("InspectFailed", message);
            AppendLog(_status.Text);
            UpdateButtons();
        }
    }

    private void RefreshStatusLabels()
    {
        InstallationStatus? status = _currentStatus;
        _packageVersion.Text = status?.PackageVersion ?? InstallerText.Get("NotAvailable");
        _installedVersion.Text = !string.IsNullOrWhiteSpace(status?.InstalledVersion)
            ? status.InstalledVersion
            : InstallerText.Get("NotInstalled");
        _localifyStatus.Text = status is { IsGameRoot: true }
            ? InstallerText.Localify(status.Localify)
            : InstallerText.Get("InvalidGameRoot");
        UpdateButtons();
    }

    private void UpdateButtons()
    {
        InstallationStatus? status = _currentStatus;
        bool valid = status is { IsGameRoot: true };
        bool installed = !string.IsNullOrWhiteSpace(status?.InstalledVersion);
        _install.Enabled = !_busy && valid && status is { PackageAvailable: true };
        _uninstall.Enabled = !_busy && valid && installed;
        _openSettings.Enabled = !_busy && valid &&
            File.Exists(Path.Combine(status!.GameRoot, "vrmod", "tools", "UmaVR.Configurator.exe"));
        _refresh.Enabled = !_busy;
        _gameRoot.Enabled = !_busy;

        string installKey = !installed
            ? "Install"
            : string.Equals(status!.InstalledVersion, status.PackageVersion, StringComparison.OrdinalIgnoreCase)
                ? "Reinstall"
                : "Update";
        _install.Text = InstallerText.Get(installKey);
        _uninstall.Text = InstallerText.Get(status?.HasPreviousVersion == true ? "Rollback" : "Uninstall");
    }

    private async void Install(object? sender, EventArgs args)
    {
        if (_busy || _packageRoot is null)
        {
            SetStatus("Busy");
            return;
        }
        await RunOperation(async progress =>
        {
            InstallationResult result = await Task.Run(() =>
                _engine.Install(_gameRoot.Text, _packageRoot, progress));
            string message = InstallerText.Format(
                "InstallComplete",
                result.Version,
                InstallerText.Localify(result.Localify));
            SetStatusText(message);
            AppendLog(message);
        });
    }

    private async void Uninstall(object? sender, EventArgs args)
    {
        if (_busy || _currentStatus?.InstalledVersion is null)
        {
            SetStatus("Busy");
            return;
        }
        string confirmation = InstallerText.Get(
            _currentStatus.HasPreviousVersion ? "ConfirmRollback" : "ConfirmUninstall");
        if (MessageBox.Show(
                this,
                confirmation,
                InstallerText.Get("ConfirmTitle"),
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2) != DialogResult.Yes)
        {
            return;
        }

        await RunOperation(async progress =>
        {
            InstallationResult result = await Task.Run(() =>
                _engine.Uninstall(_gameRoot.Text, progress));
            string message;
            if (result.Warnings.Count != 0)
            {
                message = InstallerText.Get("UninstallWarnings");
                foreach (string warning in result.Warnings)
                {
                    AppendLog(InstallerText.Warning(warning));
                }
            }
            else if (result.RestoredPreviousVersion)
            {
                message = InstallerText.Format(
                    "RollbackComplete",
                    result.Version,
                    result.RestoredVersion ?? "?");
            }
            else
            {
                message = InstallerText.Format("UninstallComplete", result.Version);
            }
            SetStatusText(message);
            AppendLog(message);
        });
    }

    private async Task RunOperation(Func<IProgress<InstallationProgress>, Task> operation)
    {
        _busy = true;
        _progress.Value = 0;
        UpdateButtons();
        Progress<InstallationProgress> progress = new(UpdateProgress);
        try
        {
            await operation(progress);
        }
        catch (Exception exception)
        {
            string message = InstallerText.ExceptionMessage(exception);
            SetStatusText(InstallerText.Format("ErrorPrefix", message));
            AppendLog(_status.Text);
            MessageBox.Show(
                this,
                message,
                InstallerText.Get("AppTitle"),
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        finally
        {
            _busy = false;
            RefreshStatus(setReady: false);
        }
    }

    private void UpdateProgress(InstallationProgress progress)
    {
        int total = Math.Max(progress.Total, 1);
        int current = Math.Clamp(progress.Current, 0, total);
        _progress.Value = Math.Clamp(current * 100 / total, 0, 100);
        if (progress.Stage == "Complete")
        {
            _progress.Value = 100;
            SetStatus("ProgressComplete");
            return;
        }
        string key = progress.Stage == "Uninstalling" ? "Uninstalling" : "Installing";
        _status.Text = InstallerText.Format(
            key,
            Math.Min(current + 1, total),
            total,
            progress.Path ?? string.Empty);
    }

    private void OpenSettings(object? sender, EventArgs args)
    {
        string path = Path.Combine(
            Path.GetFullPath(_gameRoot.Text),
            "vrmod",
            "tools",
            "UmaVR.Configurator.exe");
        if (!File.Exists(path))
        {
            SetStatus("SettingsMissing");
            return;
        }
        try
        {
            Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Exception exception)
        {
            SetStatusText(InstallerText.Format("ErrorPrefix", exception.Message));
        }
    }

    private void SetStatus(string key) => SetStatusText(InstallerText.Get(key));

    private void SetStatusText(string value) => _status.Text = value;

    private void AppendLog(string value)
    {
        _log.AppendText($"[{DateTime.Now:HH:mm:ss}] {value}{Environment.NewLine}");
    }

    private Button LanguageButton(string text, InstallerLanguage language)
    {
        Button button = new() { Text = text, AutoSize = true };
        button.Click += (_, _) => ChangeLanguage(language);
        return button;
    }

    private static void AddInfo(TableLayoutPanel grid, string key, Label value)
    {
        int row = grid.RowCount++;
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        grid.Controls.Add(new Label
        {
            Tag = key,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 5, 12, 5)
        }, 0, row);
        value.Margin = new Padding(0, 5, 0, 5);
        grid.Controls.Add(value, 1, row);
    }

    private static Label ValueLabel() => new()
    {
        AutoSize = true,
        Anchor = AnchorStyles.Left
    };

    private static Button TaggedButton(string key) => new()
    {
        Tag = key,
        AutoSize = true
    };

    private static void UpdateTaggedText(Control root)
    {
        if (root.Tag is string key)
        {
            root.Text = InstallerText.Get(key);
        }
        foreach (Control child in root.Controls)
        {
            UpdateTaggedText(child);
        }
    }
}
