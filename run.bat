@echo off
setlocal

set "ROOT=%~dp0"
set "BIN=%ROOT%x64\Debug"
set "EXE=%BIN%\ConsoleApplication1.exe"

if "%EPOCH_BACKEND%"=="" set "EPOCH_BACKEND=opengl"

if not exist "%EXE%" (
    echo Missing debug executable:
    echo   %EXE%
    echo Build Engine.sln Debug x64 first, then run this file again.
    exit /b 1
)

pushd "%BIN%" >nul
"%EXE%" --backend "%EPOCH_BACKEND%" --window-mode standalone %*
set "EPOCH_EXIT=%ERRORLEVEL%"
popd >nul

exit /b %EPOCH_EXIT%
