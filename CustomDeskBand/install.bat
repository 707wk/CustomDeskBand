@echo off
chcp 65001 >nul
:: CustomDeskBand 安装 - 右键以管理员身份运行
cd /d "%~dp0"
set "DLL=%~dp0CustomDeskBand.dll"
set "REGASM=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\RegAsm.exe"

echo [1/4] 关闭资源管理器...
taskkill /f /im explorer.exe >nul 2>&1

echo [2/4] 注册 DeskBand...
"%REGASM%" /u /silent "%DLL%" >nul 2>&1
"%REGASM%" /codebase "%DLL%" > "%TEMP%\regasm.log" 2>&1
if %errorlevel% equ 0 (
    echo     注册成功
) else (
    type "%TEMP%\regasm.log"
    echo     注册失败，请以管理员身份运行
    pause
    exit /b 1
)
del "%TEMP%\regasm.log" >nul 2>&1

echo [3/4] 重启资源管理器...
start explorer.exe

echo [4/4] 清理 Windows 计划任务（轮询等待 + 删除）...
:: explorer 启动后会异步创建 CreateExplorerShellUnelevatedTask
:: 轮询：不断尝试删除，直到某次成功删除后退出
for /l %%i in (1,1,15) do (
    timeout /t 2 /nobreak >nul
    schtasks /delete /tn "CreateExplorerShellUnelevatedTask" /f >nul 2>&1
    if not errorlevel 1 goto :done
)
:done

echo.
echo 完成！右键任务栏 - 工具栏 - 勾选 DeepSeek 余额
pause
