using System.Diagnostics;

namespace UmaVR.Configurator;

internal sealed class MainForm : Form
{
    private readonly StartupResult _startup;
    private readonly UpdateService _updateService = new();
    private readonly TextBox _gameRoot = new() { Dock = DockStyle.Fill };
    private readonly CheckBox _liveCameraFollow = TaggedCheckBox("LiveCameraFollow");
    private readonly NumericUpDown _worldScale = Number(0.55m, decimal.MaxValue, 0.05m, 2);
    private readonly NumericUpDown _eyeRenderScale = Number(0.50m, 1.50m, 0.05m, 2);
    private readonly Label _eyeScaleWarning = new() { AutoSize = true, MaximumSize = new Size(600, 0) };
    private readonly CheckBox _postProcessingEnabled = TaggedCheckBox("PostProcessingEnabled");
    private readonly CheckBox _blurEnabled = TaggedCheckBox("BlurEnabled");
    private readonly CheckBox _depthOfFieldEnabled = TaggedCheckBox("DepthOfFieldEnabled");
    private readonly CheckBox _diffusionEnabled = TaggedCheckBox("DiffusionEnabled");
    private readonly CheckBox _bloomEnabled = TaggedCheckBox("BloomEnabled");
    private readonly CheckBox _globalFogEnabled = TaggedCheckBox("GlobalFogEnabled");
    private readonly CheckBox _lensDistortionEnabled = TaggedCheckBox("LensDistortionEnabled");
    private readonly CheckBox _radialBlurEnabled = TaggedCheckBox("RadialBlurEnabled");
    private readonly CheckBox _sunShaftsEnabled = TaggedCheckBox("SunShaftsEnabled");
    private readonly CheckBox _indirectLightShaftsEnabled = TaggedCheckBox("IndirectLightShaftsEnabled");
    private readonly CheckBox _transmittedLightEnabled = TaggedCheckBox("TransmittedLightEnabled");
    private readonly CheckBox _dofDiffusionBloomOverlayEnabled = TaggedCheckBox("DofDiffusionBloomOverlayEnabled");
    private readonly CheckBox _tiltShiftEnabled = TaggedCheckBox("TiltShiftEnabled");
    private readonly CheckBox _fluctuationEnabled = TaggedCheckBox("FluctuationEnabled");
    private readonly CheckBox _chromaticAberrationEnabled = TaggedCheckBox("ChromaticAberrationEnabled");
    private readonly CheckBox _toneCurveEnabled = TaggedCheckBox("ToneCurveEnabled");
    private readonly CheckBox _exposureEnabled = TaggedCheckBox("ExposureEnabled");
    private readonly CheckBox _colorCorrectionEnabled = TaggedCheckBox("ColorCorrectionEnabled");
    private readonly CheckBox _colorGradingEnabled = TaggedCheckBox("ColorGradingEnabled");
    private readonly CheckBox _bgBlurEnabled = TaggedCheckBox("BgBlurEnabled");
    private readonly CheckBox _vortexEnabled = TaggedCheckBox("VortexEnabled");
    private readonly Label _auraSupport = TaggedLabel("AuraUnsupported");
    private readonly CheckBox _filmRollEnabled = TaggedCheckBox("FilmRollEnabled");
    private readonly CheckBox _hatchingEnabled = TaggedCheckBox("HatchingEnabled");
    private readonly CheckBox _letterBoxEnabled = TaggedCheckBox("LetterBoxEnabled");
    private readonly CheckBox _rainSplashEnabled = TaggedCheckBox("RainSplashEnabled");
    private readonly CheckBox _locomotionEnabled = TaggedCheckBox("LocomotionEnabled");
    private readonly NumericUpDown _locomotionSpeed = Number(0.10m, 5.00m, 0.10m, 2);
    private readonly CheckBox _snapTurnEnabled = TaggedCheckBox("SnapTurnEnabled");
    private readonly NumericUpDown _snapTurnAngle = Number(15m, 90m, 5m, 0);
    private readonly CheckBox _navigationHandsSwapped = TaggedCheckBox("HandSwapEnabled");
    private readonly ToolStripStatusLabel _status = new();
    private readonly Button _checkUpdates;
    private readonly TabPage _cameraTab = new() { Tag = "TabCamera", AutoScroll = true };
    private readonly TabPage _renderTab = new() { Tag = "TabRender", AutoScroll = true };
    private readonly TabPage _controllerTab = new() { Tag = "TabController", AutoScroll = true };
    private bool _statusOverridden;

    public MainForm(StartupResult startup)
    {
        _startup = startup;
        _checkUpdates = TaggedButton("CheckUpdates", CheckUpdates);
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(760, 500);
        ClientSize = new Size(920, 620);
        AutoScaleMode = AutoScaleMode.Dpi;

        TableLayoutPanel root = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 4,
            Padding = new Padding(12)
        };
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        Controls.Add(root);

        TableLayoutPanel gameRow = new() { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 3 };
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        gameRow.Controls.Add(TaggedLabel("GameFolder"), 0, 0);
        gameRow.Controls.Add(_gameRoot, 1, 0);
        gameRow.Controls.Add(TaggedButton("Browse", BrowseRoot), 2, 0);
        root.Controls.Add(gameRow, 0, 0);

        TabControl tabs = new() { Dock = DockStyle.Fill };
        BuildCameraTab();
        BuildRenderTab();
        BuildControllerTab();
        tabs.TabPages.Add(_cameraTab);
        tabs.TabPages.Add(_renderTab);
        tabs.TabPages.Add(_controllerTab);
        root.Controls.Add(tabs, 0, 1);

        TableLayoutPanel footer = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 2,
            Padding = new Padding(0, 8, 0, 4)
        };
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        FlowLayoutPanel languages = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };
        languages.Controls.Add(LanguageButton("한국어", UiLanguage.Korean));
        languages.Controls.Add(LanguageButton("English", UiLanguage.English));
        languages.Controls.Add(LanguageButton("日本語", UiLanguage.Japanese));
        footer.Controls.Add(languages, 0, 0);

        FlowLayoutPanel actions = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false
        };
        actions.Controls.Add(TaggedButton("Save", () => Run("StatusSaved", SaveSettings)));
        actions.Controls.Add(TaggedButton("Reload", () => Run("StatusReloaded", LoadSettings)));
        actions.Controls.Add(TaggedButton("Defaults", () =>
        {
            Apply(VrSettings.CreateDefaults());
            _status.Text = UiText.Get("StatusDefaults");
        }));
        actions.Controls.Add(TaggedButton("Export", Export));
        actions.Controls.Add(TaggedButton("Import", Import));
        actions.Controls.Add(_checkUpdates);
        footer.Controls.Add(actions, 1, 0);
        root.Controls.Add(footer, 0, 2);

        StatusStrip statusStrip = new();
        statusStrip.Items.Add(_status);
        root.Controls.Add(statusStrip, 0, 3);

        _gameRoot.Text = SettingsStore.FindInitialGameRoot();
        _eyeRenderScale.ValueChanged += (_, _) => UpdateEyeScaleWarning();
        ApplyLanguage(languageChanged: false);
        Load += (_, _) => Run("StatusLoaded", LoadSettings);
        Shown += async (_, _) => await HandleStartupAsync();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _updateService.Dispose();
        }
        base.Dispose(disposing);
    }

    private void BuildCameraTab()
    {
        TableLayoutPanel grid = Grid();
        AddNote(grid, "CameraSupport");
        AddSection(grid, "CameraControlsGroup");
        AddRow(grid, "Live", _liveCameraFollow);
        AddRow(grid, "WorldScale", _worldScale);
        _cameraTab.Controls.Add(grid);
    }

    private void BuildRenderTab()
    {
        TableLayoutPanel grid = Grid();
        FlowLayoutPanel scalePanel = new()
        {
            AutoSize = true,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false
        };
        scalePanel.Controls.Add(_eyeRenderScale);
        scalePanel.Controls.Add(_eyeScaleWarning);
        AddRow(grid, "EyeRenderScale", scalePanel);

        AddNote(grid, "PostProcessingSupport");
        AddRow(grid, "PostProcessing", _postProcessingEnabled);

        AddSection(grid, "VfxFocusGroup");
        AddRow(grid, "Blur", _blurEnabled);
        AddRow(grid, "DepthOfField", _depthOfFieldEnabled);
        AddRow(grid, "RadialBlur", _radialBlurEnabled);
        AddRow(grid, "TiltShift", _tiltShiftEnabled);
        AddRow(grid, "BgBlur", _bgBlurEnabled);
        AddNote(grid, "VfxFocusSupport");

        AddSection(grid, "VfxSupportedGroup");
        AddRow(grid, "DofDiffusionBloomOverlay", _dofDiffusionBloomOverlayEnabled);
        AddRow(grid, "Diffusion", _diffusionEnabled);
        AddRow(grid, "Bloom", _bloomEnabled);
        AddRow(grid, "GlobalFog", _globalFogEnabled);
        AddRow(grid, "LensDistortion", _lensDistortionEnabled);
        AddRow(grid, "SunShafts", _sunShaftsEnabled);
        AddRow(grid, "IndirectLightShafts", _indirectLightShaftsEnabled);
        AddRow(grid, "TransmittedLight", _transmittedLightEnabled);
        AddRow(grid, "Fluctuation", _fluctuationEnabled);
        AddRow(grid, "ChromaticAberration", _chromaticAberrationEnabled);
        AddRow(grid, "ToneCurve", _toneCurveEnabled);
        AddRow(grid, "Exposure", _exposureEnabled);
        AddRow(grid, "ColorCorrection", _colorCorrectionEnabled);
        AddRow(grid, "ColorGrading", _colorGradingEnabled);
        AddRow(grid, "Vortex", _vortexEnabled);
        AddRow(grid, "FilmRoll", _filmRollEnabled);
        AddRow(grid, "Hatching", _hatchingEnabled);
        AddRow(grid, "LetterBox", _letterBoxEnabled);
        AddRow(grid, "RainSplash", _rainSplashEnabled);

        AddSection(grid, "VfxUnsupportedGroup");
        AddRow(grid, "Aura", _auraSupport);
        AddNote(grid, "VfxDescription");
        _renderTab.Controls.Add(grid);
    }

    private void BuildControllerTab()
    {
        TableLayoutPanel grid = Grid();
        AddRow(grid, "Locomotion", _locomotionEnabled);
        AddRow(grid, "LocomotionSpeed", _locomotionSpeed);
        AddRow(grid, "SnapTurn", _snapTurnEnabled);
        AddRow(grid, "SnapAngle", _snapTurnAngle);
        AddRow(grid, "HandSwap", _navigationHandsSwapped);
        AddNote(grid, "ControllerDescription");
        _controllerTab.Controls.Add(grid);
    }

    internal static void VerifyLocalizedLayout()
    {
        UiLanguage original = UiText.CurrentLanguage;
        try
        {
            foreach (UiLanguage language in Enum.GetValues<UiLanguage>())
            {
                UiText.SetLanguageForValidation(language);
                using MainForm form = new(new StartupResult(null, null));
                form.ClientSize = new Size(760, 500);
                form.CreateControl();
                form.PerformLayout();
                VerifyGridCells(form._cameraTab, language);
                VerifyGridCells(form._renderTab, language);
                VerifyGridCells(form._controllerTab, language);
            }
        }
        finally
        {
            UiText.SetLanguageForValidation(original);
        }
    }

    private static void VerifyGridCells(TabPage tab, UiLanguage language)
    {
        tab.CreateControl();
        tab.PerformLayout();
        TableLayoutPanel grid = tab.Controls.OfType<TableLayoutPanel>().Single();
        grid.PerformLayout();
        Dictionary<(int Row, int Column), Control> occupied = new();
        foreach (Control control in grid.Controls)
        {
            int firstRow = grid.GetRow(control);
            int firstColumn = grid.GetColumn(control);
            for (int row = firstRow; row < firstRow + grid.GetRowSpan(control); ++row)
            {
                for (int column = firstColumn; column < firstColumn + grid.GetColumnSpan(control); ++column)
                {
                    if (occupied.TryGetValue((row, column), out Control? other))
                    {
                        throw new InvalidOperationException(
                            $"Overlapping layout cell: {language}/{tab.Tag}/row {row}/column {column}: {other.Tag} and {control.Tag}");
                    }
                    occupied[(row, column)] = control;
                }
            }
        }
    }

    private void ChangeLanguage(UiLanguage language)
    {
        UiText.SetLanguage(language);
        ApplyLanguage(languageChanged: true);
    }

    private void ApplyLanguage(bool languageChanged)
    {
        Text = UiText.Get("AppTitle");
        UpdateTaggedText(this);
        EnsureCheckBoxTextFits(this);
        UpdateEyeScaleWarning();
        _status.Text = UiText.Get(languageChanged ? "StatusLanguageChanged" : "StatusReady");
    }

    private static void UpdateTaggedText(Control root)
    {
        if (root.Tag is string key)
        {
            root.Text = UiText.Get(key);
        }
        foreach (Control child in root.Controls)
        {
            UpdateTaggedText(child);
        }
    }

    private static void EnsureCheckBoxTextFits(Control root)
    {
        if (root is CheckBox checkBox)
        {
            // Recompute after every language change. MinimumSize makes the extra
            // trailing render room part of the layout contract instead of relying
            // on Padding being incorporated into WinForms' AutoSize result.
            checkBox.MinimumSize = Size.Empty;
            Size preferred = checkBox.GetPreferredSize(Size.Empty);
            checkBox.MinimumSize = new Size(preferred.Width + 16, preferred.Height);
        }
        foreach (Control child in root.Controls)
        {
            EnsureCheckBoxTextFits(child);
        }
    }

    private void BrowseRoot()
    {
        using FolderBrowserDialog dialog = new()
        {
            Description = UiText.Get("SelectGameFolder"),
            SelectedPath = _gameRoot.Text,
            UseDescriptionForTitle = true
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _gameRoot.Text = dialog.SelectedPath;
            Run("StatusLoaded", LoadSettings);
        }
    }

    private void LoadSettings()
    {
        VrSettingsValidation loaded = SettingsStore.LoadFromGameRoot(_gameRoot.Text);
        Apply(loaded.Settings);
        if (loaded.UsedFallback)
        {
            _status.Text = UiText.Format("InvalidValues", string.Join(", ", loaded.Issues));
            _statusOverridden = true;
        }
    }

    private void SaveSettings() => SettingsStore.SaveToGameRoot(_gameRoot.Text, Read());

    private async Task HandleStartupAsync()
    {
        if (!string.IsNullOrWhiteSpace(_startup.UpdateError))
        {
            _status.Text = UiText.Format("UpdateFailed", _startup.UpdateError);
            return;
        }
        if (!string.IsNullOrWhiteSpace(_startup.UpdatedVersion))
        {
            _status.Text = UiText.Format("UpdateCompleted", _startup.UpdatedVersion);
            return;
        }
        await CheckForUpdatesAsync(manual: false);
    }

    private async void CheckUpdates() => await CheckForUpdatesAsync(manual: true);

    private async Task CheckForUpdatesAsync(bool manual)
    {
        if (!_checkUpdates.Enabled)
        {
            return;
        }

        _checkUpdates.Enabled = false;
        try
        {
            _status.Text = UiText.Get("UpdateChecking");
            AvailableUpdate? update = await _updateService.CheckAsync(CancellationToken.None);
            if (update is null)
            {
                _status.Text = UiText.Format("UpdateCurrent", UpdateService.CurrentVersion);
                return;
            }
            if (UpdateService.IsGameRunning())
            {
                _status.Text = UiText.Format("UpdateDeferredGameRunning", update.Version);
                if (manual)
                {
                    MessageBox.Show(this, _status.Text, UiText.Get("AppTitle"),
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                return;
            }

            _status.Text = UiText.Format("UpdateAvailable", update.Version);
            Progress<string> progress = new(message => _status.Text = message);
            StagedUpdate staged = await _updateService.StageAsync(
                update, _gameRoot.Text, progress, CancellationToken.None);
            _status.Text = UiText.Format("UpdateInstalling", update.Version);
            LaunchAutomaticUpdater(staged);
        }
        catch (Exception exception)
        {
            _status.Text = UiText.Format("UpdateFailed", exception.Message);
            if (manual)
            {
                MessageBox.Show(this, _status.Text, UiText.Get("AppTitle"),
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        finally
        {
            if (!IsDisposed)
            {
                _checkUpdates.Enabled = true;
            }
        }
    }

    private void LaunchAutomaticUpdater(StagedUpdate update)
    {
        ProcessStartInfo start = new(update.InstallerPath)
        {
            UseShellExecute = true,
            WorkingDirectory = update.PackageRoot
        };
        start.ArgumentList.Add("--auto-update");
        start.ArgumentList.Add("--game-root");
        start.ArgumentList.Add(Path.GetFullPath(_gameRoot.Text));
        start.ArgumentList.Add("--package-root");
        start.ArgumentList.Add(update.PackageRoot);
        start.ArgumentList.Add("--cleanup-update");
        start.ArgumentList.Add(update.StagingRoot);
        start.ArgumentList.Add("--wait-pid");
        start.ArgumentList.Add(Environment.ProcessId.ToString());
        _ = Process.Start(start) ?? throw new InvalidOperationException(
            UiText.Get("UpdateLauncherFailed"));
        BeginInvoke(Close);
    }

    private void Import()
    {
        using OpenFileDialog dialog = new() { Filter = UiText.Get("JsonOpenFilter") };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            Apply(SettingsStore.LoadFile(dialog.FileName));
            _status.Text = UiText.Get("StatusImported");
            _statusOverridden = true;
        }
    }

    private void Export()
    {
        using SaveFileDialog dialog = new()
        {
            Filter = UiText.Get("JsonSaveFilter"),
            FileName = "umavr-settings.json"
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            SettingsStore.Export(dialog.FileName, Read());
            _status.Text = UiText.Get("StatusExported");
            _statusOverridden = true;
        }
    }

    private VrSettings Read() => new()
    {
        SchemaVersion = VrSettings.CurrentSchemaVersion,
        LiveCameraFollow = _liveCameraFollow.Checked,
        WorldScale = (float)_worldScale.Value,
        EyeRenderScale = (float)_eyeRenderScale.Value,
        LocomotionEnabled = _locomotionEnabled.Checked,
        LocomotionSpeed = (float)_locomotionSpeed.Value,
        SnapTurnEnabled = _snapTurnEnabled.Checked,
        SnapTurnAngleDegrees = (float)_snapTurnAngle.Value,
        NavigationHandsSwapped = _navigationHandsSwapped.Checked,
        PostProcessingEnabled = _postProcessingEnabled.Checked,
        BlurEnabled = _blurEnabled.Checked,
        DepthOfFieldEnabled = _depthOfFieldEnabled.Checked,
        DiffusionEnabled = _diffusionEnabled.Checked,
        BloomEnabled = _bloomEnabled.Checked,
        GlobalFogEnabled = _globalFogEnabled.Checked,
        LensDistortionEnabled = _lensDistortionEnabled.Checked,
        RadialBlurEnabled = _radialBlurEnabled.Checked,
        SunShaftsEnabled = _sunShaftsEnabled.Checked,
        IndirectLightShaftsEnabled = _indirectLightShaftsEnabled.Checked,
        TransmittedLightEnabled = _transmittedLightEnabled.Checked,
        DofDiffusionBloomOverlayEnabled = _dofDiffusionBloomOverlayEnabled.Checked,
        TiltShiftEnabled = _tiltShiftEnabled.Checked,
        FluctuationEnabled = _fluctuationEnabled.Checked,
        ChromaticAberrationEnabled = _chromaticAberrationEnabled.Checked,
        ToneCurveEnabled = _toneCurveEnabled.Checked,
        ExposureEnabled = _exposureEnabled.Checked,
        ColorCorrectionEnabled = _colorCorrectionEnabled.Checked,
        ColorGradingEnabled = _colorGradingEnabled.Checked,
        BgBlurEnabled = _bgBlurEnabled.Checked,
        VortexEnabled = _vortexEnabled.Checked,
        FilmRollEnabled = _filmRollEnabled.Checked,
        HatchingEnabled = _hatchingEnabled.Checked,
        LetterBoxEnabled = _letterBoxEnabled.Checked,
        RainSplashEnabled = _rainSplashEnabled.Checked
    };

    private void Apply(VrSettings settings)
    {
        _liveCameraFollow.Checked = settings.LiveCameraFollow;
        _worldScale.Value = Math.Clamp((decimal)settings.WorldScale,
            _worldScale.Minimum, _worldScale.Maximum);
        _eyeRenderScale.Value = Math.Clamp((decimal)settings.EyeRenderScale,
            _eyeRenderScale.Minimum, _eyeRenderScale.Maximum);
        _locomotionEnabled.Checked = settings.LocomotionEnabled;
        _locomotionSpeed.Value = Math.Clamp((decimal)settings.LocomotionSpeed,
            _locomotionSpeed.Minimum, _locomotionSpeed.Maximum);
        _snapTurnEnabled.Checked = settings.SnapTurnEnabled;
        _snapTurnAngle.Value = Math.Clamp((decimal)settings.SnapTurnAngleDegrees,
            _snapTurnAngle.Minimum, _snapTurnAngle.Maximum);
        _navigationHandsSwapped.Checked = settings.NavigationHandsSwapped;
        _postProcessingEnabled.Checked = settings.PostProcessingEnabled;
        _blurEnabled.Checked = settings.BlurEnabled;
        _depthOfFieldEnabled.Checked = settings.DepthOfFieldEnabled;
        _diffusionEnabled.Checked = settings.DiffusionEnabled;
        _bloomEnabled.Checked = settings.BloomEnabled;
        _globalFogEnabled.Checked = settings.GlobalFogEnabled;
        _lensDistortionEnabled.Checked = settings.LensDistortionEnabled;
        _radialBlurEnabled.Checked = settings.RadialBlurEnabled;
        _sunShaftsEnabled.Checked = settings.SunShaftsEnabled;
        _indirectLightShaftsEnabled.Checked = settings.IndirectLightShaftsEnabled;
        _transmittedLightEnabled.Checked = settings.TransmittedLightEnabled;
        _dofDiffusionBloomOverlayEnabled.Checked = settings.DofDiffusionBloomOverlayEnabled;
        _tiltShiftEnabled.Checked = settings.TiltShiftEnabled;
        _fluctuationEnabled.Checked = settings.FluctuationEnabled;
        _chromaticAberrationEnabled.Checked = settings.ChromaticAberrationEnabled;
        _toneCurveEnabled.Checked = settings.ToneCurveEnabled;
        _exposureEnabled.Checked = settings.ExposureEnabled;
        _colorCorrectionEnabled.Checked = settings.ColorCorrectionEnabled;
        _colorGradingEnabled.Checked = settings.ColorGradingEnabled;
        _bgBlurEnabled.Checked = settings.BgBlurEnabled;
        _vortexEnabled.Checked = settings.VortexEnabled;
        _filmRollEnabled.Checked = settings.FilmRollEnabled;
        _hatchingEnabled.Checked = settings.HatchingEnabled;
        _letterBoxEnabled.Checked = settings.LetterBoxEnabled;
        _rainSplashEnabled.Checked = settings.RainSplashEnabled;
        UpdateEyeScaleWarning();
    }

    private void UpdateEyeScaleWarning()
    {
        decimal scale = _eyeRenderScale.Value;
        int pixelLoadPercent = decimal.ToInt32(decimal.Round(scale * scale * 100m));
        bool expensive = scale > (decimal)VrSettings.ExpensiveEyeRenderScale;
        _eyeScaleWarning.Text = UiText.Format(
            expensive ? "EyeScaleWarning" : "EyeScaleNormal",
            pixelLoadPercent);
        _eyeScaleWarning.ForeColor = expensive ? Color.DarkOrange : SystemColors.ControlText;
    }

    private void Run(string successKey, Action action)
    {
        try
        {
            _statusOverridden = false;
            action();
            if (!_statusOverridden)
            {
                _status.Text = UiText.Get(successKey);
            }
        }
        catch (Exception exception)
        {
            string message = UiText.Format("ErrorPrefix", exception.Message);
            _status.Text = message;
            MessageBox.Show(this, message, UiText.Get("AppTitle"),
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private static TableLayoutPanel Grid()
    {
        TableLayoutPanel grid = new()
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = 2,
            Padding = new Padding(16),
            GrowStyle = TableLayoutPanelGrowStyle.AddRows
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58));
        return grid;
    }

    private static void AddRow(TableLayoutPanel grid, string labelKey, Control control)
    {
        int row = grid.RowCount++;
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        grid.Controls.Add(TaggedLabel(labelKey), 0, row);
        control.Margin = new Padding(3, 4, 3, 4);
        control.Anchor = control is CheckBox
            ? AnchorStyles.Left
            : AnchorStyles.Left | AnchorStyles.Right;
        grid.Controls.Add(control, 1, row);
    }

    private static void AddSection(TableLayoutPanel grid, string textKey)
    {
        Label section = TaggedLabel(textKey);
        section.Font = new Font(section.Font, FontStyle.Bold);
        section.Margin = new Padding(3, 16, 3, 6);
        AddSpanningControl(grid, section);
    }

    private static void AddNote(TableLayoutPanel grid, string textKey)
    {
        Label note = TaggedLabel(textKey);
        note.MaximumSize = new Size(760, 0);
        note.ForeColor = SystemColors.GrayText;
        AddSpanningControl(grid, note);
    }

    private static void AddSpanningControl(TableLayoutPanel grid, Control control)
    {
        int row = grid.RowCount++;
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        grid.Controls.Add(control, 0, row);
        grid.SetColumnSpan(control, 2);
    }

    private static Label TaggedLabel(string key) => new()
    {
        Tag = key,
        AutoSize = true,
        Margin = new Padding(3, 8, 12, 8),
        Anchor = AnchorStyles.Left
    };

    private static CheckBox TaggedCheckBox(string key) => new()
    {
        Tag = key,
        AutoSize = true
    };

    private static Button TaggedButton(string key, Action action)
    {
        Button button = new() { Tag = key, AutoSize = true };
        button.Click += (_, _) => action();
        return button;
    }

    private Button LanguageButton(string text, UiLanguage language)
    {
        Button button = new() { Text = text, AutoSize = true };
        button.Click += (_, _) => ChangeLanguage(language);
        return button;
    }

    private static NumericUpDown Number(decimal min, decimal max, decimal increment, int decimals) => new()
    {
        Minimum = min,
        Maximum = max,
        Increment = increment,
        DecimalPlaces = decimals,
        Width = 180
    };
}
