@echo off
setlocal
set "ROOT=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%Tools\build_release_verified.ps1" %*
exit /b %ERRORLEVEL%
