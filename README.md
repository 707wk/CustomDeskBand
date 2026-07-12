# CustomDeskBand

Windows 10 任务栏 DeskBand 工具集，在任务栏上直接显示 **DeepSeek 余额** 和 **电池电量**。

> ⚠️ 仅支持 **Windows 10**，需要 .NET Framework 4.7.2。

## 功能

| DeskBand | 说明 |
|----------|------|
| **DeepSeek 余额** | 显示 DeepSeek API 账户余额及当日消耗金额，每 5 分钟自动刷新 |
| **电池电量** | 显示电池电量百分比及图标，插电/充电/低电量自动变色，每 30 秒刷新 |

### DeepSeek 余额

- 余额与当日消耗分两行显示
- 鼠标悬停查看总余额、赠送余额、充值余额详情
- 自动记录每日基准余额，充值后智能校正消耗统计
- API Key 与余额状态分别持久化至 `%AppData%\CustomDeskBand\` 下的 `apikey.json` 和 `balance_tracker.json`

### 电池电量

- 绿色：充电中或已接通电源
- 黄色：电量 21%–50%
- 红色：电量 ≤ 20%
- 无电池（台式机）显示 🔌 图标

## 截图

![screenshot](assets/screenshot.png)

## 环境要求

- **操作系统**：Windows 10（仅限此版本）
- **运行时**：.NET Framework 4.7.2
- **权限**：安装/卸载需要管理员权限

## 快速开始

### 1. 下载

从 [Releases](../../releases) 下载 `CustomDeskBand.zip` 并解压。

### 2. 配置 API Key

编辑解压目录中的 `CustomDeskBand.dll.config`，将 `YOUR_API_KEY_HERE` 替换为你的 DeepSeek API Key：

```xml
<add key="DeepSeekApiKey" value="sk-xxxxxxxxxxxxxxxxxxxxxxxx" />
```

> 在 [DeepSeek 开放平台](https://platform.deepseek.com/api_keys) 获取 API Key。
>
> **首次配置后**，API Key 会自动保存到 `%AppData%\CustomDeskBand\apikey.json`，之后更新 DLL 版本无需重新配置。

### 3. 安装

右键以**管理员身份**运行解压目录中的 `install.bat`。

### 4. 启用

右键任务栏空白处 → **工具栏** → 勾选 **DeepSeek 余额** / **电池电量**。

## 卸载

右键以**管理员身份**运行 `uninstall.bat`。

## 项目结构

```
CustomDeskBand/
├── CustomDeskBand.csproj        # 项目文件 (.NET Framework 4.7.2)
├── App.config                  # 配置文件 (API Key)
├── DeepSeekDeskBand.xaml/.cs   # DeepSeek 余额 DeskBand
├── BatteryDeskBand.xaml/.cs    # 电池电量 DeskBand
├── Services/
│   ├── DeepSeekService.cs      # DeepSeek API 服务
│   └── BalanceTracker.cs       # 余额日耗追踪器
├── Properties/
│   └── AssemblyInfo.cs         # 程序集信息
├── install.bat                 # 安装脚本
└── uninstall.bat               # 卸载脚本
```

## 依赖

| 包 | 版本 | 用途 |
|---|------|------|
| [CSDeskBand](https://github.com/dsafa/CSDeskBand) | 2.1.0 | DeskBand 基础框架 |
| [CSDeskBand.Wpf](https://github.com/dsafa/CSDeskBand) | 2.1.0 | WPF DeskBand 支持 |
| [Newtonsoft.Json](https://www.newtonsoft.com/json) | 13.0.3 | JSON 序列化 |

## 从源码编译

用 Visual Studio 2022 打开 `CustomDeskBand.slnx`，**Release** 模式编译即可。

编译产物在 `bin\Release\net472\`，之后编辑其中的 `CustomDeskBand.dll.config` 填入 API Key，再运行 `install.bat`。

