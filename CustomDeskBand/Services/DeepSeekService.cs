using System;
using System.Configuration;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Threading.Tasks;
using System.Xml;
using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;

namespace CustomDeskBand.Services
{
    /// <summary>
    /// DeepSeek 账户余额查询结果
    /// </summary>
    public class DeepSeekBalanceInfo
    {
        public string Currency { get; set; }

        public string TotalBalance { get; set; }

        public string ToppedUpBalance { get; set; }

        public string GrantedBalance { get; set; }
    }

    public class DeepSeekBalanceResponse
    {
        public bool IsAvailable { get; set; }

        public DeepSeekBalanceInfo[] BalanceInfos { get; set; }
    }

    /// <summary>
    /// DeepSeek API 服务，用于查询账户余额
    /// </summary>
    public class DeepSeekService
    {
        /// <summary>
        /// 接口调用失败重试次数
        /// </summary>
        public const int APIRetryCount = 3;

        private static readonly HttpClient _client = new HttpClient();
        private readonly string _apiKey;
        private const string BalanceUrl = "https://api.deepseek.com/user/balance";
        private const string PlaceholderKey = "YOUR_API_KEY_HERE";

        public DeepSeekService()
        {
            _apiKey = ReadApiKey();
            if (string.IsNullOrEmpty(_apiKey))
            {
                throw new InvalidOperationException(
                    "未找到 DeepSeekApiKey，请检查 CustomDeskBand.dll.config");
            }
        }

        /// <summary>
        /// 读取 API Key，优先级：
        /// 1. 若 CustomDeskBand.dll.config 中配置了真实 Key（非 YOUR_API_KEY_HERE），
        ///    则使用该 Key 并持久化到 %AppData%\CustomDeskBand\apikey.json
        /// 2. 否则从持久化文件中读取
        /// </summary>
        private static string ReadApiKey()
        {
            var configKey = ReadApiKeyFromDllConfig();

            // 如果配置文件中有真实 Key，持久化并返回
            if (!string.IsNullOrEmpty(configKey) &&
                !string.Equals(configKey, PlaceholderKey, StringComparison.OrdinalIgnoreCase))
            {
                SaveApiKeyToStorage(configKey);
                return configKey;
            }

            // 否则从持久化存储中读取
            return ReadApiKeyFromStorage();
        }

        /// <summary>
        /// 从 DLL 同目录的 .dll.config 文件中读取 API Key
        /// </summary>
        private static string ReadApiKeyFromDllConfig()
        {
            try
            {
                var dllPath = Assembly.GetExecutingAssembly().Location;
                var config = ConfigurationManager.OpenExeConfiguration(dllPath);
                var key = config.AppSettings.Settings["DeepSeekApiKey"]?.Value;
                if (!string.IsNullOrEmpty(key)) return key;
            }
            catch { }

            try
            {
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
        /// 持久化 API Key 到 %AppData%\CustomDeskBand\apikey.json
        /// </summary>
        private static void SaveApiKeyToStorage(string apiKey)
        {
            try
            {
                var appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                var dir = Path.Combine(appData, "CustomDeskBand");
                Directory.CreateDirectory(dir);
                var filePath = Path.Combine(dir, "apikey.json");
                var json = JsonConvert.SerializeObject(new { apiKey }, Newtonsoft.Json.Formatting.Indented);
                File.WriteAllText(filePath, json);
            }
            catch
            {
                // 忽略保存错误
            }
        }

        /// <summary>
        /// 从 %AppData%\CustomDeskBand\apikey.json 读取持久化的 API Key
        /// </summary>
        private static string ReadApiKeyFromStorage()
        {
            try
            {
                var appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                var filePath = Path.Combine(appData, "CustomDeskBand", "apikey.json");
                if (File.Exists(filePath))
                {
                    var json = File.ReadAllText(filePath);
                    var obj = JsonConvert.DeserializeAnonymousType(json, new { apiKey = "" });
                    if (obj != null && !string.IsNullOrEmpty(obj.apiKey) &&
                        !string.Equals(obj.apiKey, PlaceholderKey, StringComparison.OrdinalIgnoreCase))
                    {
                        return obj.apiKey;
                    }
                }
            }
            catch { }

            return null;
        }

        /// <summary>
        /// 异步获取账户余额（带重试机制）
        /// </summary>
        public async Task<DeepSeekBalanceResponse> GetBalanceAsync()
        {
            return await GetBalanceWithRetryAsync(APIRetryCount);
        }

        /// <summary>
        /// 带重试机制的余额查询，采用指数退避策略
        /// </summary>
        private async Task<DeepSeekBalanceResponse> GetBalanceWithRetryAsync(int retryCount)
        {
            string lastErrorMsg = string.Empty;

            for (int i = 0; i < retryCount; i++)
            {
                try
                {
                    return await GetBalanceOnceAsync();
                }
                catch (Exception ex)
                {
                    lastErrorMsg = ex.Message;
                }

                if (i == retryCount - 1)
                    break;

                // 指数退避: 1s → 2s → 4s
                await Task.Delay((int)Math.Pow(2, i) * 1000);
            }

            throw new Exception($"查询 DeepSeek 余额失败（已重试 {retryCount} 次）: {lastErrorMsg}");
        }

        /// <summary>
        /// 单次余额查询
        /// </summary>
        private async Task<DeepSeekBalanceResponse> GetBalanceOnceAsync()
        {
            var request = new HttpRequestMessage(HttpMethod.Get, BalanceUrl);
            request.Headers.Add("Authorization", $"Bearer {_apiKey}");

            var response = await _client.SendAsync(request);
            response.EnsureSuccessStatusCode();

            var json = await response.Content.ReadAsStringAsync();
            var settings = new JsonSerializerSettings
            {
                ContractResolver = new DefaultContractResolver
                {
                    NamingStrategy = new SnakeCaseNamingStrategy()
                }
            };
            return JsonConvert.DeserializeObject<DeepSeekBalanceResponse>(json, settings);
        }
    }
}
