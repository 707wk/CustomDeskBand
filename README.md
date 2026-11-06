# CustomDeskBand

Windows 10 任务栏电池电量工具，提供两种实现：

| 方案 | 显示位置 | 说明 |
|------|----------|------|
| **CustomDeskBand** | 通知区域左侧的工具栏区域 | DeskBand，需要 .NET Framework 4.7.2 |
| **Windhawk mod** | 时钟左侧、托盘图标右侧 | 注入 explorer，位置更精确 |

> ⚠️ 两种方案都只支持 **Windows 10**.

## 功能

| 组件 | 说明 |
|------|------|
| **DeskBand 电池电量** | 工具栏形式，显示电池电量百分比及图标，插电/充电/低电量自动变色，每 30 秒刷新 |
| **Windhawk mod** | 显示在时钟左侧，信息与 DeskBand 相同，鼠标悬停可查看详细状态 |

### 电池电量（两种方案通用）

- 绿色：充电中或已接通电源
- 黄色：电量 21%–50%
- 红色：电量 ≤ 20%
- 无电池（台式机）显示 🔌 图标

## 环境要求

- **操作系统**：Windows 10（仅限此版本）
- **DeskBand**：.NET Framework 4.7.2，安装/卸载需要管理员权限
- **Windhawk mod**：Windhawk 1.6 或更高版本

## 快速开始（DeskBand）

### 1. 下载

从 [Releases](../../releases) 下载 `CustomDeskBand.zip` 并解压。

### 2. 安装

右键以**管理员身份**运行解压目录中的 `install.bat`。

### 3. 启用

右键任务栏空白处 → **工具栏** → 勾选 **电池电量**。

## 快速开始（Windhawk mod）

源码位于 [WindhawkMods/taskbar-battery-left-of-clock.wh.cpp](WindhawkMods/taskbar-battery-left-of-clock.wh.cpp)。

1. 打开 Windhawk，点击 **创建新 mod**（Create a new mod）。
2. 把 `.wh.cpp` 的内容粘贴到编辑器，点击 **编译 mod**。
3. 编译成功后点击 **安装 mod**。

安装后按 Windhawk 的提示操作;如果电池没有立即出现,重启一次 explorer 或
在 Windhawk 中重新加载该 mod。mod 会占用 `TrayClockWClass` 左侧的空间,显示电池电量和百分比,
任务按钮区会相应缩小。

> 如果同时启用了 DeskBand 的"电池电量"工具栏，会看到两个电池。可以右键任务栏 → **工具栏** → 取消勾选 **电池电量**。

## 卸载

### DeskBand

右键以**管理员身份**运行 `uninstall.bat`。

### Windhawk mod

在 Windhawk 中禁用或卸载 **任务栏电池电量(时钟左侧)**。

## 项目结构

```
CustomDeskBand/
├── CustomDeskBand.csproj        # 项目文件 (.NET Framework 4.7.2)
├── BatteryDeskBand.xaml/.cs    # 电池电量 DeskBand
├── Properties/
│   └── AssemblyInfo.cs         # 程序集信息
├── install.bat                 # 安装脚本
└── uninstall.bat               # 卸载脚本

WindhawkMods/
└── taskbar-battery-left-of-clock.wh.cpp   # 时钟左侧电池 mod
```

## 依赖

| 包 | 版本 | 用途 |
|---|------|------|
| [CSDeskBand](https://github.com/dsafa/CSDeskBand) | 2.1.0 | DeskBand 基础框架 |
| [CSDeskBand.Wpf](https://github.com/dsafa/CSDeskBand) | 2.1.0 | WPF DeskBand 支持 |
| [Windhawk](https://windhawk.net/) | 1.6+ | Windhawk mod 运行平台 |

## 从源码编译

### DeskBand

用 Visual Studio 2022 打开 `CustomDeskBand.slnx`，**Release** 模式编译即可。

编译产物在 `bin\Release\net472\`，其中运行 `install.bat` 即可完成安装。

### Windhawk mod

不需要单独编译。把 `WindhawkMods/taskbar-battery-left-of-clock.wh.cpp` 粘贴到 Windhawk 的
mod 编辑器中，由 Windhawk 在本机编译安装。

