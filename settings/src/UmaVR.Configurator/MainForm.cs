using System.Diagnostics;

namespace UmaVR.Configurator;

internal sealed partial class MainForm : Form
{
    private readonly StartupResult _startup;
    private readonly UpdateService _updateService = new();
    private readonly TextBox _gameRoot = new() { Dock = DockStyle.Fill };
    private readonly CheckBox _liveCameraFollow = TaggedCheckBox("LiveCameraFollow");
    private readonly NumericUpDown _worldScale = Number(0.55m, decimal.MaxValue, 0.05m, 2);
    private readonly NumericUpDown _eyeRenderScale = Number(0.50m, 1.50m, 0.05m, 2);
    private readonly Label _eyeScaleWarning = new() { AutoSize = true, MaximumSize = new Size(600, 0) };
    // Bits 0 and 1 are reserved for the master and read-only Aura in the native catalog.
    private static readonly string[] HomeEffects = {
        "Blur", "DofDiffusionBloomOverlay", "DepthOfField", "Diffusion", "Bloom", "TiltShift",
        "RadialBlur", "BgBlur", "SunShafts", "IndirectLightShafts", "TransmittedLight", "GlobalFog",
        "ChromaticAberration", "ToneCurve", "Exposure", "ColorCorrection", "ColorGrading", "Fluctuation",
        "LensDistortion", "Vortex", "FilmRoll", "Hatching", "LetterBox", "RainSplash"
    };
    private readonly CheckBox[] _homeAllow = HomeEffects.Select(_ => TaggedCheckBox("SceneDefault")).ToArray();
    private int _homeReserved;
    private readonly TabPage _homeTab = new() { Tag = "HomeEffects", AutoScroll = true };
    private readonly CheckBox _homePost = TaggedCheckBox("HomePost");
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
    private readonly NumericUpDown _locomotionSpeed = Number(0.00m, decimal.MaxValue, 0.10m, 2);
    private readonly CheckBox _locomotionScaleCompensationEnabled =
        TaggedCheckBox("LocomotionScaleCompensationEnabled");
    private readonly CheckBox _snapTurnEnabled = TaggedCheckBox("SnapTurnEnabled");
    private readonly NumericUpDown _snapTurnAngle = Number(15m, 90m, 5m, 0);
    private readonly ComboBox _movementSticks = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox _grabHand = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly ComboBox _grabButton = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly NumericUpDown _movementDeadzone = Number(0, 0.90m, 0.05m, 2);
    private readonly CheckBox _showHands = TaggedCheckBox("ShowHands");
    private readonly CheckBox _contactEnabled = TaggedCheckBox("Enabled");
    private readonly CheckBox _contactMotion = TaggedCheckBox("Enabled");
    private readonly NumericUpDown _contactStrength = Number(0m, 100m, 5m, 0);
    private readonly NumericUpDown _contactTilt = Number(0m, 100m, 5m, 0);
    private readonly NumericUpDown _contactDuration = Number(0.5m, 2m, 0.05m, 2);
    private readonly NumericUpDown _handRadius = Number(10, 50, 1, 0);
    private readonly NumericUpDown _contactHold = Number(0, 250, 5, 0);
    private readonly NumericUpDown _contactReach = Number(50m, 200m, 5m, 0);
    private readonly CheckBox _contactHaptics = TaggedCheckBox("Enabled");
    private readonly NumericUpDown _hapticStrength = Number(0m, 100m, 1m, 0);
    private readonly NumericUpDown _hapticDuration = Number(0m, 250m, 5m, 0);
    private readonly CheckBox _showContact = TaggedCheckBox("ShowContact");
    private readonly ComboBox _panelHands = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 280 };
    private readonly ToolStripStatusLabel _status = new();
    private readonly Button _checkUpdates;
    private readonly TabPage _spatialTab = new() { Tag = "TabSpatial", AutoScroll = true };
    private readonly TabPage _renderTab = new() { Tag = "TabRender", AutoScroll = true };
    private readonly TabPage _controllerTab = new() { Tag = "TabInput", AutoScroll = true };
    private readonly TabPage _panelTab = new() { Tag = "TabPanel", AutoScroll = true };
    private readonly TabPage _contactTab = new() { Tag = "Contact", AutoScroll = true };
    private bool _statusOverridden;

    public MainForm(StartupResult startup)
    {
        _startup = startup;
        _checkUpdates = TaggedButton("CheckUpdates", CheckUpdates);
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(760, 500);
        Size area = Screen.FromPoint(Cursor.Position).WorkingArea.Size;
        Size = new Size(Math.Min(1180, area.Width), Math.Min(900, area.Height));
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
        BuildSpatialTab();
        BuildRenderTab();
        BuildControllerTab();
        _panelHands.SelectedIndexChanged += (_, _) => UpdateStickChoices();
        BuildPanelTab();
        BuildContactTab();
        tabs.TabPages.AddRange([_renderTab, _homeTab, _spatialTab, _panelTab, _controllerTab, _contactTab]);
        tabs.Selecting += (_, args) =>
        {
            if (args.TabPage == _contactTab && !ConfirmContactConsent()) args.Cancel = true;
        };
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
        statusStrip.Items.AddRange([_updateStatus, _applyUpdate, _releasePage]);
        _applyUpdate.Click += async (_, _) => await ApplyUpdateAsync();
        _releasePage.Click += (_, _) => Run("StatusReady", OpenRelease);
        _gameRoot.TextChanged += (_, _) => ResetUpdate();
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
            _updateCancellation?.Cancel();
            _updateService.Dispose();
        }
        base.Dispose(disposing);
    }

    private void BuildSpatialTab()
    {
        TableLayoutPanel grid = Grid();
        AddRow(grid, "WorldScale", _worldScale);
        AddNote(grid, "SpatialHelp");
        AddNote(grid, "CameraSupport");
        AddSection(grid, "CameraControlsGroup");
        AddRow(grid, "Live", _liveCameraFollow);
        _spatialTab.Controls.Add(grid);
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
        TableLayoutPanel home = Grid();
        AddRow(home, "HomeEffects", _homePost);
        AddNote(home, "HomeEffectHelp");
        for (int i = 0; i < HomeEffects.Length; i++)
            AddRow(home, HomeEffects[i], _homeAllow[i]);
        _homeTab.Controls.Add(home);
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
        AddRow(grid, "SwapMovementSticks", _movementSticks);
        AddRow(grid, "Locomotion", _locomotionEnabled);
        AddRow(grid, "LocomotionSpeed", _locomotionSpeed);
        AddRow(grid, "LocomotionScaleCompensation", _locomotionScaleCompensationEnabled);
        AddRow(grid, "SnapTurn", _snapTurnEnabled);
        AddRow(grid, "SnapAngle", _snapTurnAngle);
        AddRow(grid, "MovementDeadzone", _movementDeadzone);
        AddSection(grid, "GrabGroup");
        AddRow(grid, "GrabUsesTrigger", _grabButton);
        AddRow(grid, "GrabTracksPanelHand", _grabHand);
        AddRow(grid, "Hands", _showHands);
        AddRow(grid, "ControlsGuide", TaggedButton("ControlsGuide", ShowControlsGuide));
        _controllerTab.Controls.Add(grid);
    }

    private static void SetChoices(ComboBox box, string first, string second)
    {
        int previous = Math.Max(0, box.SelectedIndex);
        box.Items.Clear();
        box.Items.AddRange([UiText.Get(first), UiText.Get(second)]);
        box.SelectedIndex = previous;
    }

    private void UpdateStickChoices()
    {
        bool swapped = _panelHands.SelectedIndex == 1;
        SetChoices(_movementSticks, swapped ? "MoveLeft" : "MoveRight", swapped ? "MoveRight" : "MoveLeft");
    }

    private void BuildPanelTab()
    {
        TableLayoutPanel grid = Grid();
        AddRow(grid, "HandSwap", _panelHands);
        AddNote(grid, "PanelHandsHelp");
        AddNote(grid, "PanelActions");
        _panelTab.Controls.Add(grid);
    }

    private void BuildContactTab()
    {
        TableLayoutPanel grid = Grid();
        AddRow(grid, "ContactEnabled", _contactEnabled);
        AddRow(grid, "Contact", _showContact);
        AddRow(grid, "ContactHandRadiusMm", _handRadius);
        AddRow(grid, "ContactHoldMs", _contactHold);
        AddRow(grid, "ContactReach", _contactReach);
        AddSection(grid, "ResponseGroup");
        AddRow(grid, "ContactMotion", _contactMotion);
        AddRow(grid, "ContactStrength", _contactStrength);
        AddRow(grid, "ContactTilt", _contactTilt);
        AddRow(grid, "ContactDuration", _contactDuration);
        AddSection(grid, "FeedbackGroup");
        AddRow(grid, "ContactHaptics", _contactHaptics);
        AddRow(grid, "HapticStrength", _hapticStrength);
        AddRow(grid, "HapticDuration", _hapticDuration);
        _contactTab.Controls.Add(grid);
        void RefreshAvailability()
        {
            bool enabled = _contactEnabled.Checked;
            _contactHold.Enabled = _handRadius.Enabled = _showContact.Enabled = _contactReach.Enabled = _contactMotion.Enabled = _contactHaptics.Enabled = enabled;
            _contactStrength.Enabled = _contactTilt.Enabled = _contactDuration.Enabled = enabled && _contactMotion.Checked;
            _hapticStrength.Enabled = _hapticDuration.Enabled = enabled && _contactHaptics.Checked;
        }
        _contactEnabled.CheckedChanged += (_, _) => RefreshAvailability();
        _contactMotion.CheckedChanged += (_, _) => RefreshAvailability();
        _contactHaptics.CheckedChanged += (_, _) => RefreshAvailability();
        RefreshAvailability();
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
                var tabs = (TabControl)form._renderTab.Parent!;
                foreach (TabPage page in tabs.TabPages)
                {
                    // The consent dialog requires an explicit user action; inspect each grid directly.
                    VerifyGridCells(page, language);
                }
                var values = VrSettings.CreateDefaults();
                values.HomeVfxDisabled = (1 << 25) | (1 << 3) | 2;
                form.Apply(values);
                if (form._homeAllow[23].Checked || form._homeAllow[1].Checked || !form._homeAllow[0].Checked ||
                    form.Read().HomeVfxDisabled != values.HomeVfxDisabled) throw new InvalidOperationException("Home VFX mask round-trip");
                foreach (bool swapped in new[] { false, true })
                {
                    values.NavigationHandsSwapped = swapped;
                    form.Apply(values);
                    foreach (bool swapSticks in new[] { false, true })
                    {
                        form._movementSticks.SelectedIndex = swapSticks ? 1 : 0;
                        if (form._movementSticks.Text != UiText.Get(swapped != swapSticks ? "MoveLeft" : "MoveRight") ||
                            form.Read().SwapMovementSticks != swapSticks)
                            throw new InvalidOperationException("Physical stick assignment mismatch");
                    }
                    form.ApplyLanguage(languageChanged: true);
                    if (form.Read().NavigationHandsSwapped != swapped ||
                        form._panelHands.Text != UiText.Get(swapped ? "PanelRight" : "PanelLeft"))
                        throw new InvalidOperationException("Panel hand selection round-trip");
                }
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
        int handChoice = Math.Max(0, _panelHands.SelectedIndex);
        _panelHands.Items.Clear();
        _panelHands.Items.AddRange([UiText.Get("PanelLeft"), UiText.Get("PanelRight")]);
        _panelHands.SelectedIndex = handChoice;
        UpdateStickChoices();
        SetChoices(_grabButton, "GripChoice", "TriggerChoice");
        SetChoices(_grabHand, "PointerChoice", "PanelChoice");
        UpdateTaggedText(this);
        _applyUpdate.Text = UiText.Get("ApplyUpdate");
        _releasePage.Text = UiText.Get("ReleasePage");
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
        _savedValues = SnapshotValues();
        if (loaded.UsedFallback)
        {
            _status.Text = UiText.Format("InvalidValues", string.Join(", ", loaded.Issues));
            _statusOverridden = true;
        }
    }

    private void SaveSettings()
    {
        SettingsStore.SaveToGameRoot(_gameRoot.Text, Read());
        _savedValues = SnapshotValues();
    }

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

    private void LaunchAutomaticUpdater(StagedUpdate update, string game)
    {
        ProcessStartInfo start = new(update.InstallerPath)
        {
            UseShellExecute = true,
            WorkingDirectory = update.PackageRoot
        };
        start.ArgumentList.Add("--auto-update");
        start.ArgumentList.Add("--game-root");
        start.ArgumentList.Add(game);
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
        LocomotionScaleCompensationEnabled = _locomotionScaleCompensationEnabled.Checked,
        SnapTurnEnabled = _snapTurnEnabled.Checked,
        SnapTurnAngleDegrees = (float)_snapTurnAngle.Value,
        SwapMovementSticks = _movementSticks.SelectedIndex == 1,
        GrabTracksPanelHand = _grabHand.SelectedIndex == 1,
        GrabUsesTrigger = _grabButton.SelectedIndex == 1,
        MovementDeadzone = (float)_movementDeadzone.Value,
        ShowControllerHands = _showHands.Checked,
        ContactEnabled = _contactEnabled.Checked,
        ContactMotion = _contactMotion.Checked,
        ContactStrength = (float)_contactStrength.Value,
        ContactTilt = (float)_contactTilt.Value,
        ContactDuration = (float)_contactDuration.Value,
        ContactHandRadiusMm = (float)_handRadius.Value,
        ContactHoldMs = (float)_contactHold.Value,
        ContactReach = (float)_contactReach.Value,
        ContactHaptics = _contactHaptics.Checked,
        HapticStrength = (float)_hapticStrength.Value,
        HapticDuration = (float)_hapticDuration.Value,
        ShowContactLocations = _showContact.Checked,
        NavigationHandsSwapped = _panelHands.SelectedIndex == 1,
        HomeVfxDisabled = _homeAllow.Select((box, i) => box.Checked ? 0 : 1 << (i + 2)).Aggregate(_homeReserved, (mask, bit) => mask | bit),
        HomePostProcessingEnabled = _homePost.Checked,
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
        _locomotionScaleCompensationEnabled.Checked = settings.LocomotionScaleCompensationEnabled;
        _snapTurnEnabled.Checked = settings.SnapTurnEnabled;
        _snapTurnAngle.Value = Math.Clamp((decimal)settings.SnapTurnAngleDegrees,
            _snapTurnAngle.Minimum, _snapTurnAngle.Maximum);
        _movementSticks.SelectedIndex = settings.SwapMovementSticks ? 1 : 0;
        _grabHand.SelectedIndex = settings.GrabTracksPanelHand ? 1 : 0;
        _grabButton.SelectedIndex = settings.GrabUsesTrigger ? 1 : 0;
        _movementDeadzone.Value = (decimal)settings.MovementDeadzone;
        _showHands.Checked = settings.ShowControllerHands;
        _contactEnabled.Checked = settings.ContactEnabled;
        _contactMotion.Checked = settings.ContactMotion;
        _contactStrength.Value = (decimal)settings.ContactStrength;
        _contactTilt.Value = (decimal)settings.ContactTilt;
        _contactDuration.Value = (decimal)settings.ContactDuration;
        _handRadius.Value = (decimal)settings.ContactHandRadiusMm;
        _contactHold.Value = (decimal)settings.ContactHoldMs;
        _contactReach.Value = (decimal)settings.ContactReach;
        _contactHaptics.Checked = settings.ContactHaptics;
        _hapticStrength.Value = (decimal)settings.HapticStrength;
        _hapticDuration.Value = (decimal)settings.HapticDuration;
        _showContact.Checked = settings.ShowContactLocations;
        _panelHands.SelectedIndex = settings.NavigationHandsSwapped ? 1 : 0;
        _homeReserved = settings.HomeVfxDisabled & 3;
        for (int i = 0; i < _homeAllow.Length; i++)
            _homeAllow[i].Checked = (settings.HomeVfxDisabled & (1 << (i + 2))) == 0;
        _homePost.Checked = settings.HomePostProcessingEnabled;
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
        var label = TaggedLabel(labelKey);
        if (UiText.Has(labelKey + "HelpText"))
        {
            var description = TaggedLabel(labelKey + "HelpText");
            description.MaximumSize = new Size(350, 0);
            description.ForeColor = SystemColors.GrayText;
            label.Margin = new Padding(3, 6, 3, 0);
            description.Margin = new Padding(3, 0, 3, 5);
            var stack = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false };
            stack.Controls.Add(label);
            stack.Controls.Add(description);
            grid.Controls.Add(stack, 0, row);
        }
        else grid.Controls.Add(label, 0, row);
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
