@echo off
setlocal

set "ROOT=%~dp0"
set "BIN=%ROOT%x64\Release"
set "EXE=%BIN%\EpochEditor.exe"

if not exist "%EXE%" (
    echo Missing debug executable:
    echo   %EXE%
    echo Build Engine.sln Debug x64 first, then run this file again.
    exit /b 1
)

pushd "%BIN%" >nul
"%EXE%" --backend auto --window-mode parented %*
set "EPOCH_EXIT=%ERRORLEVEL%"
popd >nul

exit /b %EPOCH_EXIT%
