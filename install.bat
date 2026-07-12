@echo off
:: DeepSeek DeskBand 安装 - 右键以管理员身份运行
cd /d "%~dp0"
set "DLL=%~dp0CustomDeskBand\bin\Debug\CustomDeskBand.dll"
set "REGASM=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\RegAsm.exe"

echo [1/3] 关闭资源管理器...
taskkill /f /im explorer.exe >nul 2>&1

echo [2/3] 注册 DeskBand...
"%REGASM%" /u /silent "%DLL%" >nul 2>&1
"%REGASM%" /codebase "%DLL%"

echo [3/3] 重启资源管理器...
start explorer.exe

echo.
echo 完成！右键任务栏 - 工具栏 - 勾选 DeepSeek 余额
pause

