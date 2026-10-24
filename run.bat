@echo off
setlocal

set "ROOT=%~dp0"
set "EDITION=development"
findstr /x /c:"#define EPOCH_SOURCE_EDITION_PUBLIC 1" "%ROOT%Engine\include\epoch.edition.hpp" >nul && set "EDITION=public"
set "BIN=%ROOT%build\bin\windows\%EDITION%\x64\Release"
set "EXE=%BIN%\EpochEditor.exe"

if "%EPOCH_BACKEND%"=="" set "EPOCH_BACKEND=opengl"

if not exist "%EXE%" (
    echo Missing Release executable:
    echo   %EXE%
    echo Build Engine.sln Release x64 first, then run this file again.
    exit /b 1
)

pushd "%BIN%" >nul
"%EXE%" --backend "%EPOCH_BACKEND%" --window-mode standalone %*
set "EPOCH_EXIT=%ERRORLEVEL%"
popd >nul

exit /b %EPOCH_EXIT%
