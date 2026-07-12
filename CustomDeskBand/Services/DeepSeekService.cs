using System;
using System.Configuration;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Threading.Tasks;
using System.Xml;
using Newtonsoft.Json;

namespace CustomDeskBand.Services
{
    /// <summary>
    /// DeepSeek 账户余额查询结果
    /// </summary>
    public class DeepSeekBalanceInfo
    {
        [JsonProperty("currency")]
        public string Currency { get; set; }

        [JsonProperty("total_balance")]
        public string TotalBalance { get; set; }

        [JsonProperty("topped_up_balance")]
        public string ToppedUpBalance { get; set; }

        [JsonProperty("granted_balance")]
        public string GrantedBalance { get; set; }
    }

    public class DeepSeekBalanceResponse
    {
        [JsonProperty("is_available")]
        public bool IsAvailable { get; set; }

        [JsonProperty("balance_infos")]
        public DeepSeekBalanceInfo[] BalanceInfos { get; set; }
    }

    /// <summary>
    /// DeepSeek API 服务，用于查询账户余额
    /// </summary>
    public class DeepSeekService
    {
        private static readonly HttpClient _client = new HttpClient();
        private readonly string _apiKey;
        private const string BalanceUrl = "https://api.deepseek.com/user/balance";

        public DeepSeekService()
        {
            _apiKey = ReadApiKeyFromConfig();
            if (string.IsNullOrEmpty(_apiKey))
            {
                throw new InvalidOperationException(
                    "未找到 DeepSeekApiKey，请检查 CustomDeskBand.dll.config");
            }
        }

        /// <summary>
        /// 从 DLL 同目录的 .dll.config 文件中读取 API Key
        /// 不能直接用 ConfigurationManager，因为 DLL 被 explorer.exe 加载时
        /// 默认配置路径是 explorer.exe.config 而非我们的配置文件
        /// </summary>
        private static string ReadApiKeyFromConfig()
        {
            try
            {
                // 方式1：用 OpenExeConfiguration 指定 DLL 路径
                var dllPath = Assembly.GetExecutingAssembly().Location;
                var config = ConfigurationManager.OpenExeConfiguration(dllPath);
                var key = config.AppSettings.Settings["DeepSeekApiKey"]?.Value;
                if (!string.IsNullOrEmpty(key)) return key;
            }
            catch { }

            try
            {
                // 方式2：直接读取 XML 文件（兜底）
                var dllPath = Assembly.GetExecutingAssembly().Location;
                var configPath = dllPath + ".config";
                if (File.Exists(configPath))
                {
                    var doc = new XmlDocument();
                    doc.Load(configPath);
                    var node = doc.SelectSingleNode("//appSettings/add[@key='DeepSeekApiKey']");
                    return node?.Attributes?["value"]?.Value;
                }
            }
            catch { }

            return null;
        }

        /// <summary>
        /// 异步获取账户余额
        /// </summary>
        public async Task<DeepSeekBalanceResponse> GetBalanceAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, BalanceUrl);
            request.Headers.Add("Authorization", $"Bearer {_apiKey}");

            var response = await _client.SendAsync(request);
            response.EnsureSuccessStatusCode();

            var json = await response.Content.ReadAsStringAsync();
            return JsonConvert.DeserializeObject<DeepSeekBalanceResponse>(json);
        }
    }
}
