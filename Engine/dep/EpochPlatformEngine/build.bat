@echo off
call "%~dp0scripts\build_msvc.bat" %*
exit /b %errorlevel%
