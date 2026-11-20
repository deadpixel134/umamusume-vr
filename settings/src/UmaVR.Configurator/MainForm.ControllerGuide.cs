namespace UmaVR.Configurator;

internal sealed partial class MainForm
{
    private bool ConfirmContactConsent()
    {
        string directory = Path.Combine(Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData), "UmaVR");
        string path = Path.Combine(directory, "contact-consent.txt");
        const string token = "uma-contact-v1";
        try
        {
            if (File.Exists(path) && File.ReadAllBytes(path).AsSpan().SequenceEqual("uma-contact-v1"u8))
                return true;
            if (!ReadContactNotice())
                return false;
            Directory.CreateDirectory(directory);
            string temp = Path.Combine(directory, Guid.NewGuid().ToString("N") + ".tmp");
            try
            {
                File.WriteAllText(temp, token);
                File.Move(temp, path, overwrite: true);
            }
            finally { File.Delete(temp); }
            return true;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, ex.Message, UiText.Get("ConsentTitle"), MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
    }

    private void ShowControlsGuide()
    {
        using var dialog = new Form {
            Text = UiText.Get("ControlsGuide"), StartPosition = FormStartPosition.CenterParent,
            ClientSize = new Size(Math.Min(800, ClientSize.Width), Math.Min(760, ClientSize.Height)),
            MinimizeBox = false, ShowInTaskbar = false
        };
        using Stream stream = typeof(MainForm).Assembly.GetManifestResourceStream("Quest_03.png")!;
        using Image diagram = Image.FromStream(stream);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2, ColumnCount = 1, Padding = new Padding(16) };
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 49));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 51));
        layout.Controls.Add(new PictureBox { Image = diagram, SizeMode = PictureBoxSizeMode.Zoom, Dock = DockStyle.Fill }, 0, 0);
        bool swapped = _panelHands.SelectedIndex == 1;
        bool leftMoves = swapped != (_movementSticks.SelectedIndex == 1);
        string hand(bool left) => UiText.Get(left ? "LeftHand" : "RightHand");
        var lines = new List<string> {
            UiText.Format("GuideHands", hand(!swapped), hand(swapped)),
            UiText.Get("GuideClicks"), UiText.Get("PanelActions"),
            UiText.Format("GuideMove", _locomotionEnabled.Checked ? hand(leftMoves) : UiText.Get("GuideOff"),
                _snapTurnEnabled.Checked ? hand(!leftMoves) : UiText.Get("GuideOff")),
            UiText.Format("GuideGrab", (_grabButton.SelectedIndex == 1) ? "Trigger" : "Grip", hand((_grabHand.SelectedIndex == 1) ? !swapped : swapped))
        };
        layout.Controls.Add(new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
            ScrollBars = ScrollBars.Vertical, BackColor = SystemColors.Window, Text = string.Join(Environment.NewLine + Environment.NewLine, lines) }, 0, 1);
        dialog.Controls.Add(layout);
        dialog.ShowDialog(this);
    }
}
