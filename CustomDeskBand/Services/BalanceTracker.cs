using System;
using System.IO;
using Newtonsoft.Json;

namespace CustomDeskBand.Services
{
    /// <summary>
    /// 每日余额追踪状态
    /// </summary>
    public class BalanceTrackerState
    {
        public string Date { get; set; }

        public decimal DailyBaseline { get; set; }

        public decimal ConsumedAmount { get; set; }

        public decimal LastBalance { get; set; }

        public string Currency { get; set; }
    }

    /// <summary>
    /// 每日余额基准追踪器
    /// 以每天首次获取的余额为基准，计算当日消耗金额。
    /// 若检测到充值（余额反增），则自动调整基准，使消耗统计不受充值影响。
    /// 状态持久化到 %AppData%\CustomDeskBand\balance_tracker.json
    /// </summary>
    public class BalanceTracker
    {
        private readonly string _stateFilePath;
        private BalanceTrackerState _state;

        public BalanceTrackerState State => _state;

        public BalanceTracker()
        {
            var appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            var dir = Path.Combine(appData, "CustomDeskBand");
            Directory.CreateDirectory(dir);
            _stateFilePath = Path.Combine(dir, "balance_tracker.json");
            _state = LoadState();
        }

        /// <summary>
        /// 更新余额并返回最新状态
        /// </summary>
        /// <param name="currentBalance">当前 API 返回的总余额（decimal）</param>
        /// <param name="currency">货币单位</param>
        /// <returns>更新后的状态</returns>
        public BalanceTrackerState Update(decimal currentBalance, string currency)
        {
            var today = DateTime.Today.ToString("yyyy-MM-dd");

            // 新的一天：重置基准
            if (_state.Date != today)
            {
                _state.Date = today;
                _state.DailyBaseline = currentBalance;
                _state.ConsumedAmount = 0;
                _state.LastBalance = currentBalance;
                _state.Currency = currency;
            }
            else
            {
                // 当天：检测充值（当前余额 > 上次余额）
                if (currentBalance > _state.LastBalance)
                {
                    // 充值了：将基准上调充值金额，保持消耗统计准确
                    // newBaseline = currentBalance + consumedAmount
                    // 等价于：newBaseline = oldBaseline + rechargeAmount
                    _state.DailyBaseline = currentBalance + _state.ConsumedAmount;
                }

                // 计算消耗金额
                _state.ConsumedAmount = _state.DailyBaseline - currentBalance;
                if (_state.ConsumedAmount < 0)
                    _state.ConsumedAmount = 0;

                _state.LastBalance = currentBalance;
                _state.Currency = currency;
            }

            SaveState(_state);
            return _state;
        }

        private BalanceTrackerState LoadState()
        {
            try
            {
                if (File.Exists(_stateFilePath))
                {
                    var json = File.ReadAllText(_stateFilePath);
                    return JsonConvert.DeserializeObject<BalanceTrackerState>(json)
                           ?? CreateDefaultState();
                }
            }
            catch
            {
                // 文件损坏则重置
            }

            return CreateDefaultState();
        }

        private void SaveState(BalanceTrackerState state)
        {
            try
            {
                var json = JsonConvert.SerializeObject(state, Formatting.Indented);
                File.WriteAllText(_stateFilePath, json);
            }
            catch
            {
                // 忽略保存错误，不影响主流程
            }
        }

        private static BalanceTrackerState CreateDefaultState()
        {
            return new BalanceTrackerState
            {
                Date = "",
                DailyBaseline = 0,
                ConsumedAmount = 0,
                LastBalance = 0,
                Currency = "CNY"
            };
        }
    }
}
