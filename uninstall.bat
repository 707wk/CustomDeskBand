@echo off
:: DeepSeek DeskBand 卸载 - 右键以管理员身份运行
cd /d "%~dp0"
set DLL=%~dp0CustomDeskBand\bin\Debug\CustomDeskBand.dll
set REGASM=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\RegAsm.exe

taskkill /f /im explorer.exe >nul 2>&1
%REGASM% /u "%DLL%"
start explorer.exe

echo 已卸载
pause

