using CSDeskBand;
using CustomDeskBand.Services;
using System;
using System.Globalization;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Threading;

namespace CustomDeskBand
{
    [ComVisible(true)]
    [Guid("A1B2C3D4-E5F6-7890-ABCD-EF1234567890")]
    [CSDeskBandRegistration(Name = "DeepSeek 余额", ShowDeskBand = true)]
    public partial class DeepSeekDeskBand : CSDeskBand.Wpf.CSDeskBandWpf
    {
        private readonly DeepSeekService _service;
        private readonly BalanceTracker _tracker;
        private readonly DispatcherTimer _timer;

        public DeepSeekDeskBand()
        {
            InitializeComponent();

            Options.Title = "DeepSeek 余额";
            Options.MinHorizontalSize = new CSDeskBand.Size(56, 40);

            SetSingle("加载中", "...");

            try
            {
                _service = new DeepSeekService();
                _tracker = new BalanceTracker();
            }
            catch (Exception)
            {
                SetSingle("配置", "错误");
                return;
            }

            // 对齐到下一个5分钟整点 (:00, :05, :10, ...)
            var now = DateTime.Now;
            var next5Min = ((now.Minute / 5) + 1) * 5;
            var nextTick = now.Date.AddHours(now.Hour).AddMinutes(next5Min);
            var initialDelay = nextTick - now;
            if (initialDelay <= TimeSpan.Zero) initialDelay = TimeSpan.FromMinutes(5);

            _timer = new DispatcherTimer { Interval = initialDelay };
            _timer.Tick += async (s, e) =>
            {
                if (_timer.Interval != TimeSpan.FromMinutes(5))
                    _timer.Interval = TimeSpan.FromMinutes(5);
                await RefreshAsync();
            };
            _timer.Start();

            Loaded += async (s, e) =>
            {
                // 网络恢复时自动刷新
                NetworkChange.NetworkAvailabilityChanged += OnNetworkAvailabilityChanged;
                // 初始加载时立即查询
                await RefreshAsync();
            };
        }

        private void OnNetworkAvailabilityChanged(object sender, NetworkAvailabilityEventArgs e)
        {
            if (e.IsAvailable)
            {
                // NetworkChange 事件在后台线程触发，需封送到 UI 线程
                Dispatcher.InvokeAsync(async () => await RefreshAsync());
            }
        }

        private async System.Threading.Tasks.Task RefreshAsync()
        {
            if (_service == null) return;

            try
            {
                var result = await _service.GetBalanceAsync();

                if (result?.BalanceInfos != null && result.BalanceInfos.Length > 0)
                {
                    var b = result.BalanceInfos[0];
                    if (!decimal.TryParse(b.TotalBalance, NumberStyles.Number, CultureInfo.InvariantCulture, out var cur))
                    {
                        SetSingle($"¥ {b.TotalBalance}");
                        return;
                    }

                    var state = _tracker.Update(cur, b.Currency);

                    // 符号 + 数字分列显示
                    BalanceSymbol.Visibility = Visibility.Visible;
                    ConsumedSymbol.Visibility = Visibility.Visible;
                    // ConsumedLabel 可能被 SetSingle 折叠，需显式恢复
                    ConsumedLabel.Visibility = Visibility.Visible;
                    BalanceLabel.Text = $"{cur:N1}";
                    ConsumedLabel.Text = $"¥ {state.ConsumedAmount:N1}";
                    RootGrid.ToolTip = $"总余额: ¥ {b.TotalBalance}\n" +
                                       $"赠送: ¥ {b.GrantedBalance}\n" +
                                       $"充值: ¥ {b.ToppedUpBalance}\n" +
                                       $"今日消耗: ¥ {state.ConsumedAmount:N1}";

                }
                else
                {
                    SetSingle("无数据");
                }
            }
            catch (System.Net.Http.HttpRequestException)
            {
                SetSingle("无网络");
            }
            catch (System.Threading.Tasks.TaskCanceledException)
            {
                SetSingle("超时");
            }
            catch (Exception ex)
            {
                SetSingle("未知", "错误", ex.Message);
            }
        }

        private void SetSingle(string firstLine, string secondLine = null, string tooltip = null)
        {
            // 隐藏符号列，用标签显示状态文字（第二行可选）
            BalanceSymbol.Visibility = Visibility.Collapsed;
            ConsumedSymbol.Visibility = Visibility.Collapsed;
            BalanceLabel.Text = firstLine;
            if (secondLine != null)
            {
                ConsumedLabel.Visibility = Visibility.Visible;
                ConsumedLabel.Text = secondLine;
            }
            else
            {
                ConsumedLabel.Visibility = Visibility.Collapsed;
            }
            RootGrid.ToolTip = tooltip ?? (secondLine != null ? $"{firstLine}{secondLine}" : firstLine);
        }


    }
}
