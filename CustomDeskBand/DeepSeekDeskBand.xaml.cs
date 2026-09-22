using CSDeskBand;
using CustomDeskBand.Services;
using System;
using System.Globalization;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media;
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

        // 最近一次计算的余额比例（0~1），带宽变化时用于重算进度条填充宽度
        private double _ratio;

        // 进度条配色（参考电池电量百分比）：≤20% 红、21%~50% 黄、>50% 绿
        private static readonly SolidColorBrush LowBrush = CreateFrozenBrush(0xF4, 0x63, 0x43);
        private static readonly SolidColorBrush MidBrush = CreateFrozenBrush(0xF4, 0xC5, 0x42);
        private static readonly SolidColorBrush HighBrush = CreateFrozenBrush(0x4C, 0xCA, 0x50);

        private static SolidColorBrush CreateFrozenBrush(byte r, byte g, byte b)
        {
            var brush = new SolidColorBrush(Color.FromRgb(r, g, b));
            brush.Freeze();
            return brush;
        }

        public DeepSeekDeskBand()
        {
            InitializeComponent();

            Options.Title = "DeepSeek 余额";
            Options.MinHorizontalSize = new CSDeskBand.Size(90, 40);

            // 【试验】IsFixed：附带 DBIMF_NOGRIPPER 去掉带前 gripper 间隔（两带紧贴）；代价=带宽锁定不可拖拽
            Options.IsFixed = true;

            // 带宽变化时按最新比例重算进度条填充宽度
            ProgressTrack.SizeChanged += (s, e) =>
                ProgressFill.Width = Math.Max(0, ProgressTrack.ActualWidth * _ratio);

            SetSingle("加载中");

            try
            {
                _service = new DeepSeekService();
                _tracker = new BalanceTracker();
            }
            catch (Exception ex)
            {
                SetSingle("配置错误", ex.Message);
                return;
            }

            // 对齐到下一个1分钟整点 (:00 秒)
            var now = DateTime.Now;
            var nextMin = now.Date.AddHours(now.Hour).AddMinutes(now.Minute + 1);
            var initialDelay = nextMin - now;
            if (initialDelay <= TimeSpan.Zero) initialDelay = TimeSpan.FromMinutes(1);

            _timer = new DispatcherTimer { Interval = initialDelay };
            _timer.Tick += async (s, e) =>
            {
                if (_timer.Interval != TimeSpan.FromMinutes(1))
                    _timer.Interval = TimeSpan.FromMinutes(1);
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

                    // 第一行第二列：￥ 今日消耗 / 总余额（均向上取整）
                    BalanceLabel.Text = $"¥ {Math.Ceiling(state.ConsumedAmount):0} / {Math.Ceiling(cur):0}";

                    // 第二行：余额比例 = 当前余额 / 当日基准余额
                    double ratio = state.DailyBaseline > 0
                        ? (double)(cur / state.DailyBaseline)
                        : (cur > 0 ? 1 : 0);
                    ratio = Math.Max(0, Math.Min(1, ratio));
                    UpdateProgress(ratio);

                    RootGrid.ToolTip = $"总余额: ¥ {b.TotalBalance}\n" +
                                       $"赠送: ¥ {b.GrantedBalance}\n" +
                                       $"充值: ¥ {b.ToppedUpBalance}\n" +
                                       $"今日消耗: ¥ {state.ConsumedAmount:N1}\n" +
                                       $"余额比例: {ratio * 100:0.#}%";

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
                SetSingle("未知错误", ex.Message);
            }
        }

        /// <summary>
        /// 更新余额比例进度条（填充宽度 + 颜色，颜色参考电池电量百分比）
        /// </summary>
        private void UpdateProgress(double ratio)
        {
            _ratio = ratio;
            ProgressFill.Width = Math.Max(0, ProgressTrack.ActualWidth * _ratio);

            var percent = _ratio * 100;
            if (percent <= 20)
                ProgressFill.Fill = LowBrush;
            else if (percent <= 50)
                ProgressFill.Fill = MidBrush;
            else
                ProgressFill.Fill = HighBrush;
        }

        /// <summary>
        /// 显示非数值状态（加载中 / 错误提示），进度条归零
        /// </summary>
        private void SetSingle(string text, string tooltip = null)
        {
            BalanceLabel.Text = text;
            UpdateProgress(0);
            RootGrid.ToolTip = tooltip ?? text;
        }
    }
}
