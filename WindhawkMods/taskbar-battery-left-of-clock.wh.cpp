// ==WindhawkMod==
// @id              taskbar-battery-left-of-clock
// @name            任务栏电池电量(时钟左侧)
// @description     在 Windows 10 任务栏时钟左侧显示电池电量
// @version         1.2
// @author          wk950
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lgdi32 -ladvapi32 -lcomctl32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- meterWidth: 64
  $name: 电池区域宽度
  $description: 96 DPI 下的宽度, 范围 40-120. 实际宽度会按系统 DPI 缩放.
- refreshInterval: 30
  $name: 刷新间隔(秒)
  $description: 定时刷新电池状态的间隔, 范围 5-300.
*/
// ==/WindhawkModSettings==

// ==WindhawkModReadme==
/*
# 任务栏电池电量(时钟左侧)

在 Windows 10 任务栏的时钟左侧、托盘图标右侧显示电池电量. 效果与
[Taskbar User Tile](https://windhawk.net/mods/taskbar-user-tile) 类似,
只是内容放在时钟左边.

## 工作原理

* hook `ClockButton::CalculateMinimumSize`, 让任务栏按"原宽度 + 电池宽度"
  布局时钟, 从而在时钟左侧预留出空间.
* 在时钟窗口的 `WM_WINDOWPOSCHANGING` 中把宽度改回原值, 并让右边界保持不变,
  预留出的空间由本 mod 创建的电池窗口占用.
* 电池窗口是任务栏 (`Shell_TrayWnd`) 的子窗口, 与 `TrayNotifyWnd` 并列.
  这样通知区域重新布局时, 电池窗口不会跟着托盘移动而闪烁. 使用分层窗口
  颜色键作为背景, 因此可以与任务栏背景融合.

## 限制

* 只支持 Windows 10 的经典任务栏. Windows 11 的 XAML 任务栏不支持.
* 只处理主显示器的任务栏; 垂直任务栏不显示电池窗口.
* 如果同时启用了 CustomDeskBand 的"电池电量"工具栏, 会显示两个电池.
  可以右键任务栏 -> 工具栏 -> 取消勾选"电池电量".

## 安装

1. 打开 Windhawk, 点击"创建新 mod"(Create a new mod).
2. 把本文件的内容粘贴到编辑器, 点击"编译 mod".
3. 编译成功后点击"安装 mod".
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cwchar>

namespace {

constexpr wchar_t kMeterClass[] = L"WhTaskbarBatteryLeftOfClock";
constexpr UINT_PTR kRefreshTimerId = 1;
constexpr COLORREF kColorKey = RGB(1, 2, 3);
constexpr int kDefaultMeterWidth = 64;
constexpr int kDefaultRefreshSeconds = 30;

// Power setting GUIDs, defined locally to avoid extra import libraries.
const GUID kAcDcPowerSourceGuid = {
    0x5D3E9A59, 0xE9D5, 0x4B00, {0xA6, 0xBD, 0xFF, 0x34, 0xFF, 0x51, 0x65, 0x48}};
const GUID kBatteryPercentageRemainingGuid = {
    0xA7AD8041, 0xB45A, 0x4CAE, {0x87, 0xA3, 0xEE, 0xCB, 0xB4, 0x68, 0xA9, 0xE1}};

std::atomic<bool> g_stopping{false};
std::atomic<bool> g_hooksReady{false};
std::atomic<bool> g_slotAllocated{false};
std::atomic<HWND> g_taskbar{nullptr};
std::atomic<HWND> g_tray{nullptr};
std::atomic<HWND> g_clock{nullptr};
std::atomic<HWND> g_meter{nullptr};
std::atomic<int> g_nativeClockWidth{0};
std::atomic<int> g_reservedSlot{0};

int g_meterWidth = kDefaultMeterWidth;
int g_refreshInterval = kDefaultRefreshSeconds;
UINT g_setupMsg = 0;
UINT g_cleanupMsg = 0;
HANDLE g_watchThread = nullptr;
HFONT g_font = nullptr;
UINT g_fontDpi = 0;
HWND g_tooltip = nullptr;
HPOWERNOTIFY g_powerNotifyAcDc = nullptr;
HPOWERNOTIFY g_powerNotifyBattery = nullptr;
wchar_t g_tooltipText[256] = {};

struct MeterState {
    bool valid;
    bool hasBattery;
    bool charging;
    bool pluggedIn;
    int percent;
    int secondsRemaining;
    COLORREF textColor;
    COLORREF fillColor;
    wchar_t text[16];
};

MeterState g_state = {};

using ClockButtonCalculateMinimumSizeProc = SIZE*(*)(void*, SIZE*);
ClockButtonCalculateMinimumSizeProc g_clockButtonCalculateMinimumSize = nullptr;

LRESULT CALLBACK MeterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ClockSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData);
LRESULT CALLBACK TraySubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData);

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

UINT DpiOf(HWND hWnd) {
    using GetDpiForWindowProc = UINT(WINAPI*)(HWND);
    static GetDpiForWindowProc getDpiForWindow = []() {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<GetDpiForWindowProc>(
                            GetProcAddress(user32, "GetDpiForWindow"))
                      : nullptr;
    }();
    UINT dpi = getDpiForWindow ? getDpiForWindow(hWnd) : 0;
    return dpi ? dpi : 96;
}

bool ReadDwordSetting(HKEY root, PCWSTR subKey, PCWSTR valueName, DWORD* value) {
    DWORD size = sizeof(DWORD);
    return RegGetValueW(root, subKey, valueName, RRF_RT_REG_DWORD, nullptr, value, &size) ==
           ERROR_SUCCESS;
}

bool IsLightTaskbar() {
    DWORD colorPrevalence = 0;
    if (ReadDwordSetting(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                         L"ColorPrevalence", &colorPrevalence) &&
        colorPrevalence) {
        DWORD accentColor = 0;
        if (ReadDwordSetting(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM",
                             L"AccentColor", &accentColor)) {
            // AccentColor is stored as 0xAABBGGRR.
            int red = accentColor & 0xFF;
            int green = (accentColor >> 8) & 0xFF;
            int blue = (accentColor >> 16) & 0xFF;
            int luma = (red * 299 + green * 587 + blue * 114) / 1000;
            return luma > 160;
        }
    }

    DWORD systemUsesLightTheme = 0;
    if (ReadDwordSetting(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                         L"SystemUsesLightTheme", &systemUsesLightTheme)) {
        return systemUsesLightTheme != 0;
    }

    return false;
}

HFONT GetMeterFont(UINT dpi) {
    if (g_font && g_fontDpi == dpi) {
        return g_font;
    }

    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }

    // Match the DeskBand: FontSize = StatusFontSize + 2, which is 14px at 96 DPI.
    LOGFONTW logFont = {};
    logFont.lfHeight = -MulDiv(14, static_cast<int>(dpi), 96);
    logFont.lfWeight = FW_NORMAL;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfQuality = CLEARTYPE_QUALITY;
    logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    lstrcpynW(logFont.lfFaceName, L"Segoe UI", LF_FACESIZE);

    g_font = CreateFontIndirectW(&logFont);
    g_fontDpi = dpi;
    return g_font;
}

COLORREF BatteryFillColor(int percent, bool powered, bool lightTaskbar) {
    if (powered) {
        return lightTaskbar ? RGB(0x0F, 0x7B, 0x0F) : RGB(0x4C, 0xCA, 0x50);
    }

    if (percent <= 20) {
        return lightTaskbar ? RGB(0xC0, 0x2B, 0x1E) : RGB(0xF4, 0x63, 0x43);
    }

    if (percent <= 50) {
        return lightTaskbar ? RGB(0xB0, 0x7A, 0x00) : RGB(0xF4, 0xC5, 0x42);
    }

    return lightTaskbar ? RGB(0x1A, 0x1A, 0x1A) : RGB(0xFF, 0xFF, 0xFF);
}

void UpdateTooltip() {
    HWND meter = g_meter.load();
    if (!IsWindow(g_tooltip) || !IsWindow(meter)) {
        return;
    }

    if (!g_state.valid) {
        wcscpy(g_tooltipText, L"无法获取电池状态");
    } else if (!g_state.hasBattery) {
        wcscpy(g_tooltipText, L"无电池");
    } else {
        const wchar_t* statusText = g_state.charging
                                        ? L"充电中"
                                        : (g_state.pluggedIn ? L"已接通电源" : L"电池供电");
        if (g_state.secondsRemaining >= 0) {
            int hours = g_state.secondsRemaining / 3600;
            int minutes = (g_state.secondsRemaining % 3600) / 60;
            if (hours > 0) {
                swprintf(g_tooltipText, ARRAYSIZE(g_tooltipText),
                         L"电量: %d%%\n状态: %s\n剩余: %d 小时 %d 分钟", g_state.percent,
                         statusText, hours, minutes);
            } else {
                swprintf(g_tooltipText, ARRAYSIZE(g_tooltipText), L"电量: %d%%\n状态: %s\n剩余: %d 分钟",
                         g_state.percent, statusText, minutes);
            }
        } else {
            swprintf(g_tooltipText, ARRAYSIZE(g_tooltipText), L"电量: %d%%\n状态: %s", g_state.percent,
                     statusText);
        }
    }

    TOOLINFOW toolInfo = {};
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.uFlags = TTF_IDISHWND;
    toolInfo.hwnd = meter;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(meter);
    toolInfo.lpszText = g_tooltipText;
    SendMessageW(g_tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&toolInfo));
}

void UpdateBattery() {
    SYSTEM_POWER_STATUS status = {};
    bool lightTaskbar = IsLightTaskbar();
    MeterState state = {};
    state.textColor = lightTaskbar ? RGB(0x1A, 0x1A, 0x1A) : RGB(0xFF, 0xFF, 0xFF);
    state.fillColor = state.textColor;
    wcscpy(state.text, L"--%");
    state.valid = GetSystemPowerStatus(&status) != FALSE;

    if (state.valid) {
        if (status.BatteryFlag == 128) {
            state.hasBattery = false;
            wcscpy(state.text, L"AC");
        } else if (status.BatteryLifePercent > 100) {
            state.valid = false;
        } else {
            state.hasBattery = true;
            state.charging = (status.BatteryFlag & 8) != 0;
            state.pluggedIn = status.ACLineStatus == 1;
            state.percent = status.BatteryLifePercent;
            state.secondsRemaining = status.BatteryLifeTime;
            swprintf(state.text, ARRAYSIZE(state.text), L"%d%%", state.percent);
            state.fillColor =
                BatteryFillColor(state.percent, state.charging || state.pluggedIn, lightTaskbar);
        }
    }

    g_state = state;
    UpdateTooltip();

    HWND meter = g_meter.load();
    if (IsWindow(meter)) {
        InvalidateRect(meter, nullptr, TRUE);
    }
}

void DrawBatteryIcon(HDC dc, int x, int y, int width, int height, UINT dpi, COLORREF borderColor,
                     COLORREF fillColor, int percent, bool hasBattery) {
    int capWidth = ScaleForDpi(3, dpi);
    int bodyWidth = width - capWidth;
    int padding = ScaleForDpi(2, dpi);
    int borderWidth = (std::max)(1, ScaleForDpi(1, dpi));

    HPEN borderPen = CreatePen(PS_SOLID, borderWidth, borderColor);
    HGDIOBJ oldPen = SelectObject(dc, borderPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, x, y, x + bodyWidth, y + height, ScaleForDpi(4, dpi), ScaleForDpi(4, dpi));
    SelectObject(dc, oldBrush);

    HBRUSH capBrush = CreateSolidBrush(borderColor);
    oldBrush = SelectObject(dc, capBrush);
    RoundRect(dc, x + bodyWidth, y + height * 3 / 12, x + width, y + height * 9 / 12,
              ScaleForDpi(2, dpi), ScaleForDpi(2, dpi));
    SelectObject(dc, oldBrush);
    DeleteObject(capBrush);

    if (hasBattery && percent > 0) {
        int innerWidth = (bodyWidth - 2 * padding) * percent / 100;
        if (innerWidth > 0) {
            HBRUSH fillBrush = CreateSolidBrush(fillColor);
            RECT fillRect = {x + padding, y + padding, x + padding + innerWidth,
                             y + height - padding};
            FillRect(dc, &fillRect, fillBrush);
            DeleteObject(fillBrush);
        }
    }

    SelectObject(dc, oldPen);
    DeleteObject(borderPen);
}

void DrawMeterContent(HWND hWnd, HDC dc, const RECT& clientRect) {
    UINT dpi = DpiOf(hWnd);
    int iconWidth = ScaleForDpi(24, dpi);
    int iconHeight = ScaleForDpi(12, dpi);
    int gap = ScaleForDpi(4, dpi);

    HFONT font = GetMeterFont(dpi);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    int oldBkMode = SetBkMode(dc, OPAQUE);
    COLORREF oldBkColor = SetBkColor(dc, kColorKey);
    COLORREF oldTextColor = SetTextColor(dc, g_state.textColor);

    SIZE textSize = {};
    GetTextExtentPoint32W(dc, g_state.text, static_cast<int>(wcslen(g_state.text)), &textSize);

    int contentWidth = iconWidth + gap + textSize.cx;
    int startX = (clientRect.right - contentWidth) / 2;
    if (startX < 0) {
        startX = 0;
    }
    int iconTop = (clientRect.bottom - iconHeight) / 2;

    DrawBatteryIcon(dc, startX, iconTop, iconWidth, iconHeight, dpi, g_state.textColor,
                    g_state.fillColor, g_state.percent, g_state.hasBattery);

    RECT textRect = {startX + iconWidth + gap, 0, clientRect.right, clientRect.bottom};
    DrawTextW(dc, g_state.text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(dc, oldTextColor);
    SetBkColor(dc, oldBkColor);
    SetBkMode(dc, oldBkMode);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
}

void PaintMeter(HWND hWnd) {
    PAINTSTRUCT paintStruct = {};
    HDC dc = BeginPaint(hWnd, &paintStruct);
    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);

    if (clientRect.right > 0 && clientRect.bottom > 0) {
        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = clientRect.right;
        bitmapInfo.bmiHeader.biHeight = -clientRect.bottom;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 24;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC memoryDc = bitmap ? CreateCompatibleDC(dc) : nullptr;
        if (memoryDc) {
            HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

            HBRUSH backgroundBrush = CreateSolidBrush(kColorKey);
            FillRect(memoryDc, &clientRect, backgroundBrush);
            DeleteObject(backgroundBrush);

            DrawMeterContent(hWnd, memoryDc, clientRect);

            BitBlt(dc, 0, 0, clientRect.right, clientRect.bottom, memoryDc, 0, 0, SRCCOPY);

            SelectObject(memoryDc, oldBitmap);
            DeleteDC(memoryDc);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
    }

    EndPaint(hWnd, &paintStruct);
}

void CreateTooltip(HWND meter) {
    g_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!g_tooltip) {
        Wh_Log(L"Battery meter: failed to create tooltip (%lu).", GetLastError());
        return;
    }

    TOOLINFOW toolInfo = {};
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.hwnd = meter;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(meter);
    toolInfo.lpszText = g_tooltipText;
    SendMessageW(g_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&toolInfo));
    SendMessageW(g_tooltip, TTM_SETMAXTIPWIDTH, 0, 300);
}

void ReleasePowerNotifications() {
    if (g_powerNotifyAcDc) {
        UnregisterPowerSettingNotification(g_powerNotifyAcDc);
        g_powerNotifyAcDc = nullptr;
    }
    if (g_powerNotifyBattery) {
        UnregisterPowerSettingNotification(g_powerNotifyBattery);
        g_powerNotifyBattery = nullptr;
    }
}

void ReleaseTooltip() {
    if (g_tooltip) {
        HWND tooltip = g_tooltip;
        g_tooltip = nullptr;
        DestroyWindow(tooltip);
    }
}

void DestroyMeterWindow() {
    ReleasePowerNotifications();
    ReleaseTooltip();

    HWND meter = g_meter.exchange(nullptr);
    if (IsWindow(meter)) {
        DestroyWindow(meter);
    }
}

void EnsureMeterWindow() {
    if (IsWindow(g_meter.load())) {
        return;
    }

    HWND taskbar = g_taskbar.load();
    if (!IsWindow(taskbar)) {
        return;
    }

    // Parent the meter to the taskbar rather than to TrayNotifyWnd. When the
    // notification area is re-laid out (for example when a tray icon is
    // added), a child of TrayNotifyWnd is moved together with its parent for a
    // frame before PositionMeter gets a chance to move it back. A taskbar child
    // keeps its position while the tray is being resized.
    HWND meter = CreateWindowExW(WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED, kMeterClass, L"",
                                 WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, taskbar, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
    if (!meter) {
        Wh_Log(L"Battery meter: failed to create meter window (%lu).", GetLastError());
        return;
    }

    SetLayeredWindowAttributes(meter, kColorKey, 0, LWA_COLORKEY);
    g_meter.store(meter);
    SetTimer(meter, kRefreshTimerId, static_cast<UINT>(g_refreshInterval) * 1000, nullptr);
    CreateTooltip(meter);
    g_powerNotifyAcDc = RegisterPowerSettingNotification(meter, &kAcDcPowerSourceGuid,
                                                        DEVICE_NOTIFY_WINDOW_HANDLE);
    g_powerNotifyBattery = RegisterPowerSettingNotification(
        meter, &kBatteryPercentageRemainingGuid, DEVICE_NOTIFY_WINDOW_HANDLE);
    UpdateBattery();
}

void RelayoutTaskbar() {
    HWND taskbar = g_taskbar.load();
    if (!IsWindow(taskbar)) {
        return;
    }

    RECT clientRect = {};
    GetClientRect(taskbar, &clientRect);
    SendMessageW(taskbar, WM_SIZE, SIZE_RESTORED, MAKELPARAM(clientRect.right, clientRect.bottom));
    InvalidateRect(taskbar, nullptr, TRUE);
}

bool TryGetMeterPlacement(RECT* clockRect, int* slot) {
    if (g_stopping.load() || !g_slotAllocated.load()) {
        return false;
    }

    HWND clock = g_clock.load();
    int reserved = g_reservedSlot.load();
    RECT rect = {};
    if (!IsWindow(clock) || !IsWindowVisible(clock) || reserved <= 0 ||
        !GetWindowRect(clock, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) {
        return false;
    }

    *clockRect = rect;
    *slot = reserved;
    return true;
}

void PositionMeter() {
    HWND meter = g_meter.load();
    if (!IsWindow(meter)) {
        return;
    }

    RECT clockRect = {};
    int slot = 0;
    if (!TryGetMeterPlacement(&clockRect, &slot)) {
        ShowWindow(meter, SW_HIDE);
        return;
    }

    // SetWindowPos uses parent client coordinates for child windows.
    HWND parent = GetParent(meter);
    POINT topLeft = {clockRect.left, clockRect.top};
    MapWindowPoints(HWND_DESKTOP, parent, &topLeft, 1);
    SetWindowPos(meter, HWND_TOP, topLeft.x - slot, topLeft.y, slot,
                 clockRect.bottom - clockRect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(meter, nullptr, TRUE);
}

LRESULT CALLBACK MeterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT:
            PaintMeter(hWnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            if (wParam == kRefreshTimerId) {
                UpdateBattery();
            }
            return 0;
        case WM_POWERBROADCAST:
            UpdateBattery();
            return TRUE;
        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
            UpdateBattery();
            return 0;
        case WM_NCDESTROY:
            if (g_meter.load() == hWnd) {
                ReleasePowerNotifications();
                ReleaseTooltip();
                g_meter.store(nullptr);
            }
            return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

SIZE* ClockButtonCalculateMinimumSizeHook(void* object, SIZE* result) {
    SIZE* answer = g_clockButtonCalculateMinimumSize(object, result);
    HWND clock = g_clock.load();

    if (!answer || !object || !clock || g_stopping.load()) {
        return answer;
    }

    int slot = ScaleForDpi(g_meterWidth, DpiOf(clock));
    if (answer->cx <= 0 || slot <= 0) {
        return answer;
    }

    // The window handle is stored right after the first pointer sized field of
    // the ClockButton object. If the object can't be matched to our clock,
    // still record the first value so that a single taskbar setup keeps working.
    HWND objectWindow = *reinterpret_cast<HWND*>(reinterpret_cast<BYTE*>(object) + sizeof(void*));
    bool belongsToOurClock =
        objectWindow == clock ||
        reinterpret_cast<void*>(GetWindowLongPtrW(clock, GWLP_USERDATA)) == object;
    if (belongsToOurClock || g_nativeClockWidth.load() == 0) {
        g_nativeClockWidth.store(answer->cx);
        g_reservedSlot.store(slot);
    }

    answer->cx += slot;
    return answer;
}

LRESULT CALLBACK ClockSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                               DWORD_PTR dwRefData) {
    if (uMsg == g_setupMsg) {
        if (g_stopping.load()) {
            return 0;
        }

        if (wParam == 0) {
            EnsureMeterWindow();
            RelayoutTaskbar();
        }
        PositionMeter();
        return 0;
    }

    if (uMsg == g_cleanupMsg) {
        g_slotAllocated.store(false);
        DestroyMeterWindow();
        RelayoutTaskbar();
        return 0;
    }

    if (uMsg == WM_WINDOWPOSCHANGING) {
        WINDOWPOS* windowPos = reinterpret_cast<WINDOWPOS*>(lParam);
        if (!g_stopping.load() && windowPos && !(windowPos->flags & SWP_NOSIZE)) {
            int nativeWidth = g_nativeClockWidth.load();
            int slot = g_reservedSlot.load();
            if (nativeWidth > 0 && slot > 0 && windowPos->cx == nativeWidth + slot) {
                // Give the reserved part back to the meter window while keeping
                // the right edge of the clock where explorer placed it.
                int x = windowPos->x;
                if (windowPos->flags & SWP_NOMOVE) {
                    // x is undefined when SWP_NOMOVE is set, so start from the
                    // current position instead.
                    RECT currentRect = {};
                    if (GetWindowRect(hWnd, &currentRect)) {
                        POINT currentOrigin = {currentRect.left, currentRect.top};
                        HWND parent = GetParent(hWnd);
                        MapWindowPoints(HWND_DESKTOP, parent, &currentOrigin, 1);
                        x = currentOrigin.x;
                    }
                }
                windowPos->x = x + slot;
                windowPos->cx = nativeWidth;
                // Clear the flags so Windows applies the modified values even
                // if explorer only asked to resize the clock.
                windowPos->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
                g_slotAllocated.store(true);
            } else if (windowPos->cx != nativeWidth) {
                g_slotAllocated.store(false);
            }
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    if (uMsg == WM_WINDOWPOSCHANGED || uMsg == WM_SHOWWINDOW || uMsg == WM_SETTINGCHANGE ||
        uMsg == WM_THEMECHANGED || uMsg == WM_DPICHANGED) {
        PositionMeter();
    }

    if (uMsg == WM_NCDESTROY) {
        g_clock.store(nullptr);
        g_slotAllocated.store(false);
        g_nativeClockWidth.store(0);
        g_reservedSlot.store(0);
    }

    return result;
}

LRESULT CALLBACK TraySubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                              DWORD_PTR dwRefData) {
    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    if (uMsg == WM_SIZE || uMsg == WM_WINDOWPOSCHANGED || uMsg == WM_SETTINGCHANGE ||
        uMsg == WM_THEMECHANGED || uMsg == WM_DPICHANGED) {
        // Explorer has finished its own layout, put the meter in place again.
        PositionMeter();
    }

    if (uMsg == WM_NCDESTROY) {
        g_tray.store(nullptr);
    }

    return result;
}

BOOL CALLBACK FindClockProc(HWND hWnd, LPARAM lParam) {
    wchar_t className[64] = {};
    if (GetClassNameW(hWnd, className, ARRAYSIZE(className)) &&
        wcscmp(className, L"TrayClockWClass") == 0) {
        *reinterpret_cast<HWND*>(lParam) = hWnd;
        return FALSE;
    }
    return TRUE;
}

bool IsVerticalTaskbar(HWND tray) {
    RECT clientRect = {};
    GetClientRect(tray, &clientRect);
    return clientRect.right < clientRect.bottom;
}

void DiscoverTaskbar() {
    HWND clock = g_clock.load();
    if (IsWindow(clock)) {
        SendMessageTimeoutW(clock, g_setupMsg, 1, 0, SMTO_ABORTIFHUNG, 1000, nullptr);
        return;
    }

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!IsWindow(taskbar)) {
        return;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(taskbar, &processId);
    if (processId != GetCurrentProcessId()) {
        return;
    }

    HWND tray = FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    if (!IsWindow(tray) || IsVerticalTaskbar(tray)) {
        return;
    }

    HWND clockWindow = nullptr;
    EnumChildWindows(tray, FindClockProc, reinterpret_cast<LPARAM>(&clockWindow));
    if (!IsWindow(clockWindow)) {
        return;
    }

    if (!IsWindow(g_tray.load())) {
        if (!WindhawkUtils::SetWindowSubclassFromAnyThread(tray, TraySubclass, 0)) {
            Wh_Log(L"Battery meter: failed to subclass TrayNotifyWnd.");
            return;
        }
        g_tray.store(tray);
    }

    g_taskbar.store(taskbar);
    g_clock.store(clockWindow);

    if (!WindhawkUtils::SetWindowSubclassFromAnyThread(clockWindow, ClockSubclass, 0)) {
        Wh_Log(L"Battery meter: failed to subclass TrayClockWClass.");
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(tray, TraySubclass);
        g_taskbar.store(nullptr);
        g_tray.store(nullptr);
        g_clock.store(nullptr);
        return;
    }

    Wh_Log(L"Battery meter: attached to taskbar clock %p.", clockWindow);
    SendMessageTimeoutW(clockWindow, g_setupMsg, 0, 0, SMTO_ABORTIFHUNG, 2000, nullptr);
}

DWORD WINAPI WatchThread(LPVOID) {
    while (!g_stopping.load()) {
        if (g_hooksReady.load()) {
            DiscoverTaskbar();
        }

        for (int i = 0; i < 20 && !g_stopping.load(); i++) {
            Sleep(100);
        }
    }
    return 0;
}

bool InitSymbolHooks() {
    const WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {L"public: struct tagSIZE __cdecl ClockButton::CalculateMinimumSize(struct tagSIZE)"},
            &g_clockButtonCalculateMinimumSize,
            ClockButtonCalculateMinimumSizeHook,
            false,
        },
    };

    return WindhawkUtils::HookSymbols(GetModuleHandleW(nullptr), hooks, ARRAYSIZE(hooks));
}

void DetachTaskbar() {
    HWND clock = g_clock.load();
    if (IsWindow(clock)) {
        SendMessageTimeoutW(clock, g_cleanupMsg, 0, 0, SMTO_ABORTIFHUNG, 2000, nullptr);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(clock, ClockSubclass);
    }

    HWND tray = g_tray.load();
    if (IsWindow(tray)) {
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(tray, TraySubclass);
    }

    g_clock.store(nullptr);
    g_tray.store(nullptr);
    g_taskbar.store(nullptr);
}

}  // namespace

BOOL Wh_ModInit() {
    g_stopping.store(false);

    int meterWidthSetting = Wh_GetIntSetting(L"meterWidth");
    g_meterWidth = meterWidthSetting > 0 ? std::clamp(meterWidthSetting, 40, 120)
                                         : kDefaultMeterWidth;
    int refreshSetting = Wh_GetIntSetting(L"refreshInterval");
    g_refreshInterval = refreshSetting > 0 ? std::clamp(refreshSetting, 5, 300)
                                           : kDefaultRefreshSeconds;

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = MeterProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kMeterClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&windowClass)) {
        Wh_Log(L"Battery meter: failed to register window class (%lu).", GetLastError());
        return FALSE;
    }

    g_setupMsg = RegisterWindowMessageW(L"WhTaskbarBatteryLeftOfClock.Setup");
    g_cleanupMsg = RegisterWindowMessageW(L"WhTaskbarBatteryLeftOfClock.Cleanup");
    if (!g_setupMsg || !g_cleanupMsg) {
        Wh_Log(L"Battery meter: failed to register messages.");
        UnregisterClassW(kMeterClass, GetModuleHandleW(nullptr));
        return FALSE;
    }

    if (!InitSymbolHooks()) {
        Wh_Log(L"Battery meter: failed to hook ClockButton::CalculateMinimumSize.");
        UnregisterClassW(kMeterClass, GetModuleHandleW(nullptr));
        return FALSE;
    }

    g_hooksReady.store(true);
    g_watchThread = CreateThread(nullptr, 0, WatchThread, nullptr, 0, nullptr);
    if (!g_watchThread) {
        Wh_Log(L"Battery meter: failed to create watch thread.");
        UnregisterClassW(kMeterClass, GetModuleHandleW(nullptr));
        return FALSE;
    }

    return TRUE;
}

BOOL Wh_ModSettingsChanged(BOOL* reload) {
    *reload = TRUE;
    return TRUE;
}

void Wh_ModBeforeUninit() {
    g_stopping.store(true);
    DetachTaskbar();
}

void Wh_ModUninit() {
    g_stopping.store(true);
    DetachTaskbar();

    if (g_watchThread) {
        WaitForSingleObject(g_watchThread, 5000);
        CloseHandle(g_watchThread);
        g_watchThread = nullptr;
    }

    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }

    UnregisterClassW(kMeterClass, GetModuleHandleW(nullptr));
}
