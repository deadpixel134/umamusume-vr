using System.Runtime.InteropServices;

namespace UmaVR.Configurator;

internal sealed partial class MainForm
{
    private bool ReadContactNotice()
    {
        using Font bodyFont = new(Font.FontFamily, 10f);
        using Font boldFont = new(bodyFont, FontStyle.Bold);
        using Form notice = new()
        {
            Text = UiText.Get("ContactConsentTitle"),
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            AutoScaleMode = AutoScaleMode.Dpi,
            ClientSize = new Size(760, 640),
            MaximizeBox = false,
            MinimizeBox = false,
            ShowInTaskbar = false
        };
        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(24),
            ColumnCount = 1,
            RowCount = 3
        };
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        RichTextBox body = new()
        {
            Font = bodyFont,
            Dock = DockStyle.Fill,
            ReadOnly = true,
            DetectUrls = false,
            ScrollBars = RichTextBoxScrollBars.Vertical,
            BorderStyle = BorderStyle.None,
            BackColor = SystemColors.Control
        };
        // Localized notice text uses paired ** markers for emphasis.
        string[] fragments = UiText.Get("ContactConsentBody").Split("**", StringSplitOptions.None);
        for (int index = 0; index < fragments.Length; index++)
        {
            body.SelectionFont = index % 2 == 0 ? bodyFont : boldFont;
            body.AppendText(fragments[index]);
        }
        body.Select(0, 0);
        body.ScrollToCaret();
        layout.Controls.Add(body, 0, 0);
        Label readingHint = new()
        {
            Text = UiText.Get("ContactConsentReadToEnd"),
            AutoSize = true,
            Margin = new Padding(0, 12, 0, 8)
        };
        layout.Controls.Add(readingHint, 0, 1);
        FlowLayoutPanel actions = new()
        {
            AutoSize = true,
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false
        };
        Button decline = new()
        {
            Text = UiText.Get("ContactConsentDecline"),
            AutoSize = true,
            Padding = new Padding(12, 6, 12, 6),
            DialogResult = DialogResult.Cancel
        };
        Button agree = new()
        {
            Enabled = false,
            Text = UiText.Get("ContactConsentAgree"),
            AutoSize = true,
            Padding = new Padding(12, 6, 12, 6),
            DialogResult = DialogResult.OK
        };
        actions.Controls.Add(decline);
        actions.Controls.Add(agree);
        layout.Controls.Add(actions, 0, 2);
        notice.Controls.Add(layout);
        notice.CancelButton = decline;
        bool shown = false;
        void UpdateReadingState()
        {
            if (!shown || body.IsDisposed || agree.Enabled) return;
            if (!ContactNoticeAtEnd(body)) return;
            agree.Enabled = true;
            readingHint.Text = UiText.Get("ContactConsentReadComplete");
        }
        body.VScroll += (_, _) => body.BeginInvoke((Action)UpdateReadingState);
        body.SizeChanged += (_, _) => UpdateReadingState();
        notice.Shown += (_, _) =>
        {
            body.Select(0, 0);
            body.ScrollToCaret();
            shown = true;
            UpdateReadingState();
            decline.Focus();
        };
        return notice.ShowDialog(this) == DialogResult.OK && agree.Enabled;
    }

    private static bool ContactNoticeAtEnd(RichTextBox body)
    {
        if (!body.IsHandleCreated || body.ClientSize.Height <= 0 || body.TextLength == 0) return false;
        ContactScrollInfo info = new() { Size = (uint)Marshal.SizeOf<ContactScrollInfo>(), Mask = 7 };
        // RichEdit may stop one device pixel short of the standard inclusive range.
        if (GetScrollInfo(body.Handle, 1, ref info) && info.Page > 0 &&
            info.Position >= Math.Max(info.Minimum, (long)info.Maximum - info.Page))
            return true;
        // A control with no scroll bar can still show the entire notice.
        Point first = body.GetPositionFromCharIndex(0);
        Point last = body.GetPositionFromCharIndex(body.TextLength - 1);
        return first.Y >= 0 && last.Y >= 0 && last.Y + body.Font.Height <= body.ClientSize.Height;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ContactScrollInfo
    {
        public uint Size, Mask;
        public int Minimum, Maximum;
        public uint Page;
        public int Position, TrackPosition;
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetScrollInfo(IntPtr window, int bar, ref ContactScrollInfo info);
}
