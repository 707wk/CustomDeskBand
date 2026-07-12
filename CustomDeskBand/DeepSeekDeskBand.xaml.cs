using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using CSDeskBand;
using CustomDeskBand.Services;

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

            try
            {
                _service = new DeepSeekService();
                _tracker = new BalanceTracker();
            }
            catch (Exception ex)
            {
                SetSingle("配置错误");
                return;
            }

            _timer = new DispatcherTimer { Interval = TimeSpan.FromMinutes(5) };
            _timer.Tick += async (s, e) => await RefreshAsync();
            _timer.Start();

            Loaded += async (s, e) => await RefreshAsync();
        }

        private async System.Threading.Tasks.Task RefreshAsync()
        {
            if (_service == null) return;

            try
            {
                SetSingle("查询中...");

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
                    Grid.SetColumnSpan(BalanceLabel, 1);
                    BalanceLabel.HorizontalAlignment = HorizontalAlignment.Right;
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
                SetSingle(ex.Message);
            }
        }

        private void SetSingle(string text)
        {
            BalanceSymbol.Visibility = Visibility.Collapsed;
            ConsumedSymbol.Visibility = Visibility.Collapsed;
            BalanceLabel.HorizontalAlignment = HorizontalAlignment.Center;
            BalanceLabel.Text = text;
            Grid.SetColumnSpan(BalanceLabel, 2);
            ConsumedLabel.Text = "";
            RootGrid.ToolTip = text;
        }


    }
}
