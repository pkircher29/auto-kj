@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_windows.ps1" -Generator "Visual Studio 17 2022" -Config Release %*
exit /b %ERRORLEVEL%
