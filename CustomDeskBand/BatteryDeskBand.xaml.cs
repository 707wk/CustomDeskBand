using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using CSDeskBand;

namespace CustomDeskBand
{
    [ComVisible(true)]
    [Guid("B1C2D3E4-F5A6-7890-BCDE-F12345678901")]
    [CSDeskBandRegistration(Name = "电池电量", ShowDeskBand = true)]
    public partial class BatteryDeskBand : CSDeskBand.Wpf.CSDeskBandWpf
    {
        private readonly DispatcherTimer _timer;

        [StructLayout(LayoutKind.Sequential)]
        private struct SYSTEM_POWER_STATUS
        {
            public byte ACLineStatus;
            public byte BatteryFlag;
            public byte BatteryLifePercent;
            public byte Reserved;
            public int BatteryLifeTime;
            public int BatteryFullLifeTime;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetSystemPowerStatus(out SYSTEM_POWER_STATUS lpSystemPowerStatus);

        public BatteryDeskBand()
        {
            InitializeComponent();

            Options.Title = "电池电量";
            Options.MinHorizontalSize = new CSDeskBand.Size(width: 62, 40);

            // 对齐到下一个30秒整点 (:00, :30)
            var now = DateTime.Now;
            var next30Sec = ((now.Second / 30) + 1) * 30;
            var nextTick = now.Date.AddHours(now.Hour).AddMinutes(now.Minute).AddSeconds(next30Sec);
            var initialDelay = nextTick - now;
            if (initialDelay <= TimeSpan.Zero) initialDelay = TimeSpan.FromSeconds(30);

            _timer = new DispatcherTimer { Interval = initialDelay };
            _timer.Tick += (s, e) =>
            {
                if (_timer.Interval != TimeSpan.FromSeconds(30))
                    _timer.Interval = TimeSpan.FromSeconds(30);
                RefreshBattery();
            };
            _timer.Start();

            Loaded += (s, e) => RefreshBattery();
            Microsoft.Win32.SystemEvents.PowerModeChanged += OnPowerModeChanged;
        }

        private void OnPowerModeChanged(object sender, Microsoft.Win32.PowerModeChangedEventArgs e)
        {
            Dispatcher.BeginInvoke((Action)RefreshBattery);
        }

        private void RefreshBattery()
        {
            try
            {
                if (!GetSystemPowerStatus(out var status))
                {
                    SetUnknown();
                    return;
                }

                // 无电池（台式机）
                if (status.BatteryFlag == 128)
                {
                    PercentLabel.Text = "🔌";
                    RootGrid.ToolTip = "无电池";
                    BatteryFill.Width = 0;
                    return;
                }

                bool isCharging = (status.BatteryFlag & 8) != 0;
                bool isAcOnline = status.ACLineStatus == 1;
                int percent = status.BatteryLifePercent;

                // 充电或插电 → 绿色图标；纯电池 → 按电量着色
                bool isPowered = isCharging || isAcOnline;
                var green = new SolidColorBrush(Color.FromRgb(0x4C, 0xCA, 0x50));

                if (percent > 100)
                {
                    SetUnknown();
                    return;
                }

                PercentLabel.Text = $"{percent}%";
                BatteryFill.Width = 17.0 * percent / 100.0;

                // 仅调整电量填充颜色
                if (isPowered)
                {
                    BatteryFill.Fill = green;
                }
                else if (percent <= 20)
                {
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(0xF4, 0x63, 0x43));
                }
                else if (percent <= 50)
                {
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(0xF4, 0xC5, 0x42));
                }
                else
                {
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(0xFF, 0xFF, 0xFF));
                }

                // ToolTip
                string stateText = isCharging ? "充电中" : (isAcOnline ? "已接通电源" : "电池供电");
                string timeText = "";
                if (status.BatteryLifeTime >= 0)
                {
                    var ts = TimeSpan.FromSeconds(status.BatteryLifeTime);
                    timeText = ts.Hours > 0
                        ? $"\n剩余: {ts.Hours} 小时 {ts.Minutes} 分钟"
                        : $"\n剩余: {ts.Minutes} 分钟";
                }
                RootGrid.ToolTip = $"电量: {percent}%\n状态: {stateText}{timeText}";
            }
            catch
            {
                SetUnknown();
            }
        }

        private void SetUnknown()
        {
            PercentLabel.Text = "--%";
            RootGrid.ToolTip = "无法获取电池状态";
            BatteryFill.Width = 0;
        }
    }
}
