# CustomDeskBand

Windows 10 任务栏 DeskBand 工具，在任务栏上直接显示 **电池电量**。

> ⚠️ 仅支持 **Windows 10**，需要 .NET Framework 4.7.2。

## 功能

| DeskBand | 说明 |
|----------|------|
| **电池电量** | 显示电池电量百分比及图标，插电/充电/低电量自动变色，每 30 秒刷新 |

### 电池电量

- 绿色：充电中或已接通电源
- 黄色：电量 21%–50%
- 红色：电量 ≤ 20%
- 无电池（台式机）显示 🔌 图标

## 环境要求

- **操作系统**：Windows 10（仅限此版本）
- **运行时**：.NET Framework 4.7.2
- **权限**：安装/卸载需要管理员权限

## 快速开始

### 1. 下载

从 [Releases](../../releases) 下载 `CustomDeskBand.zip` 并解压。

### 2. 安装

右键以**管理员身份**运行解压目录中的 `install.bat`。

### 3. 启用

右键任务栏空白处 → **工具栏** → 勾选 **电池电量**。

## 卸载

右键以**管理员身份**运行 `uninstall.bat`。

## 项目结构

```
CustomDeskBand/
├── CustomDeskBand.csproj        # 项目文件 (.NET Framework 4.7.2)
├── BatteryDeskBand.xaml/.cs    # 电池电量 DeskBand
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

## 从源码编译

用 Visual Studio 2022 打开 `CustomDeskBand.slnx`，**Release** 模式编译即可。

编译产物在 `bin\Release\net472\`，其中运行 `install.bat` 即可完成安装。

