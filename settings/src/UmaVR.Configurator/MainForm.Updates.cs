using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using UmaVR.Management;

namespace UmaVR.Configurator;

internal sealed partial class MainForm
{
    private readonly ToolStripStatusLabel _updateStatus = new();
    private readonly ToolStripStatusLabel _applyUpdate = new() { IsLink = true, Visible = false };
    private readonly ToolStripStatusLabel _releasePage = new() { IsLink = true, Visible = false };
    private CancellationTokenSource? _updateCancellation;
    private AvailableUpdate? _candidate;
    private string? _candidateRoot;
    private string? _savedValues;

    private string SnapshotValues() => JsonSerializer.Serialize(Read());

    private void ResetUpdate()
    {
        _updateCancellation?.Cancel();
        _candidate = null;
        _candidateRoot = null;
        _applyUpdate.Visible = _releasePage.Visible = false;
        _updateStatus.Text = "";
    }

    private void OpenRelease()
    {
        if (_candidate != null)
            Process.Start(new ProcessStartInfo(_candidate.Page) { UseShellExecute = true });
    }

    private async Task CheckForUpdatesAsync(bool manual)
    {
        if (_updateCancellation != null) return;
        ResetUpdate();
        using var request = new CancellationTokenSource();
        _updateCancellation = request;
        _checkUpdates.Enabled = false;
        try
        {
            string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(_gameRoot.Text));
            string? installed = null;
            string reason = UiText.Get("UpdateLayoutUnknown");
            try { installed = new InstallationEngine().Inspect(root, null).InstalledVersion; }
            catch (Exception ex) when (ex is IOException or JsonException or InstallationException or UnauthorizedAccessException) { }
            _updateStatus.Text = UiText.Get("UpdateChecking");
            var update = await _updateService.CheckAsync(installed ?? UpdateService.CurrentVersion, request.Token);
            request.Token.ThrowIfCancellationRequested();
            if (update == null)
            {
                _updateStatus.Text = UiText.Format("UpdateCurrent", installed ?? UpdateService.CurrentVersion);
                return;
            }
            _candidate = update;
            _candidateRoot = root;
            _releasePage.Visible = true;
            bool running = UpdateService.IsGameRunning();
            bool applicable = installed != null && update.ArchiveUrl.Length != 0 && !running;
            if (running) reason = UiText.Format("UpdateDeferredGameRunning", update.Version);
            _updateStatus.Text = UiText.Format("UpdateAvailable", update.Version);
            if (!applicable) _updateStatus.Text += " " + reason;
            _applyUpdate.Visible = applicable;
            ShowVersionNotice(root, update, manual);
        }
        catch (OperationCanceledException) when (request.IsCancellationRequested) { }
        catch (Exception e) { if (!request.IsCancellationRequested) _updateStatus.Text = UiText.Format("UpdateFailed", e.Message); }
        finally
        {
            _updateCancellation = null;
            if (!IsDisposed) _checkUpdates.Enabled = true;
        }
    }

    private void ShowVersionNotice(string root, AvailableUpdate update, bool manual)
    {
        string key = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(root.ToUpperInvariant())));
        string directory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UmaVR", "update-notices", key);
        string notice = Path.Combine(directory, update.Version + ".txt");
        if (!manual && File.Exists(notice)) return;
        string summary = ReleaseSummary(update.Body);
        var skip = new TaskDialogVerificationCheckBox(UiText.Get("SkipUpdate"));
        TaskDialog.ShowDialog(this, new TaskDialogPage
        {
            Caption = UiText.Get("AppTitle"), Heading = UiText.Format("UpdateAvailable", update.Version),
            Text = summary + Environment.NewLine + Environment.NewLine + _updateStatus.Text,
            Buttons = { TaskDialogButton.Close }, Verification = skip
        });
        try
        {
            Directory.CreateDirectory(directory);
            string temp = Path.Combine(directory, Guid.NewGuid().ToString("N") + ".tmp");
            File.WriteAllText(temp, skip.Checked ? "skipped" : "seen");
            File.Move(temp, notice, true);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
    }

    private static string ReleaseSummary(string? body)
    {
        if (string.IsNullOrWhiteSpace(body) || body.Length > 65536) return UiText.Get("SummaryMissing");
        var sections = new Dictionary<string, List<string>> { [""] = new() };
        string language = "";
        bool fenced = false;
        foreach (string raw in body.Split('\n'))
        {
            string line = raw.Trim();
            if (line.StartsWith("~~~") || line.StartsWith("\x60\x60\x60")) { fenced = !fenced; continue; }
            if (fenced || line.Length == 0 || line.StartsWith('|') || line.StartsWith("![")) continue;
            if (line.StartsWith('#'))
            {
                string header = line.Trim('#', ' ').ToLowerInvariant();
                string? code = header switch { "ko" or "한국어" or "korean" => "ko", "en" or "english" => "en", "ja" or "日本語" or "japanese" => "ja", _ => null };
                if (code != null) { language = code; sections.TryAdd(language, new()); }
                continue;
            }
            line = Regex.Replace(line, @"\[([^\]]*)\]\([^)]+\)", "$1");
            line = Regex.Replace(line, @"<[^>]+>|https?://\S+", "").Trim();
            if (line.Length > 0 && sections[language].Count < 5)
                sections[language].Add(line.Length <= 180 ? line : line[..(char.IsHighSurrogate(line[178]) ? 178 : 179)] + "…");
        }
        string locale = UiText.CurrentLanguage switch { UiLanguage.Korean => "ko", UiLanguage.Japanese => "ja", _ => "en" };
        foreach (string code in new[]{locale, "en", "ko"}.Concat(sections.Keys))
            if (sections.TryGetValue(code, out var lines) && lines.Count != 0) return string.Join(Environment.NewLine, lines);
        return UiText.Get("SummaryMissing");
    }

    private async Task ApplyUpdateAsync()
    {
        if (_updateCancellation != null || _candidate is not {} update || _candidateRoot is not {} root) return;
        if (MessageBox.Show(this, UiText.Format("ConfirmUpdate", update.Version, root), Text, MessageBoxButtons.YesNo) != DialogResult.Yes) return;
        using var request = new CancellationTokenSource();
        _updateCancellation = request;
        StagedUpdate? staged = null;
        try
        {
            if (UpdateService.IsGameRunning()) { _updateStatus.Text = UiText.Format("UpdateDeferredGameRunning", update.Version); return; }
            if (_savedValues != SnapshotValues())
            {
                var choice = MessageBox.Show(this, UiText.Get("SaveForUpdate"), Text, MessageBoxButtons.YesNoCancel);
                if (choice == DialogResult.Cancel) return;
                if (choice == DialogResult.Yes) SaveSettings();
            }
            Enabled = false;
            staged = await _updateService.StageAsync(update, root, new Progress<string>(s =>
            { if (!request.IsCancellationRequested && !IsDisposed) _updateStatus.Text = s; }), request.Token);
            request.Token.ThrowIfCancellationRequested();
            LaunchAutomaticUpdater(staged, root);
            staged = null;
        }
        catch (OperationCanceledException) when (request.IsCancellationRequested) { }
        catch (Exception ex)
        {
            if (!request.IsCancellationRequested)
                _updateStatus.Text = ex is InstallationException problem
                    ? UiText.Get("UpdateLayoutUnknown") + " " + string.Join(" ", problem.Arguments)
                    : UiText.Format("UpdateFailed", ex.Message);
        }
        finally
        {
            if (staged != null) UpdateService.Discard(staged);
            _updateCancellation = null;
            if (!IsDisposed) Enabled = true;
        }
    }
}
