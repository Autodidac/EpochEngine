@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
  echo Configuration must be Debug or Release.
  exit /b 2
)

where cl >nul 2>&1
if not errorlevel 1 goto :env_ready

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if exist "%%I\Common7\Tools\VsDevCmd.bat" call "%%I\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
  )
)
where cl >nul 2>&1
if not errorlevel 1 goto :env_ready

for %%P in (
  "%ProgramFiles%\Microsoft Visual Studio\2026\BuildTools"
  "%ProgramFiles%\Microsoft Visual Studio\2026\Community"
  "%ProgramFiles%\Microsoft Visual Studio\2026\Professional"
  "%ProgramFiles%\Microsoft Visual Studio\2026\Enterprise"
  "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
) do (
  if exist "%%~P\Common7\Tools\VsDevCmd.bat" (
    call "%%~P\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
    goto :env_ready
  )
)

echo ERROR: MSVC was not found.
exit /b 3

:env_ready
where cl >nul 2>&1 || exit /b 3
where lib >nul 2>&1 || exit /b 3
where link >nul 2>&1 || exit /b 3

set "OUT=%ROOT%\out\direct-msvc-%CONFIG%"
set "OBJS=%OUT%\obj-static"
set "OBJD=%OUT%\obj-shared"
if not exist "%OBJS%" mkdir "%OBJS%"
if not exist "%OBJD%" mkdir "%OBJD%"
del /q "%OBJS%\*.obj" "%OBJD%\*.obj" "%OUT%\*.lib" "%OUT%\*.dll" "%OUT%\*.exe" "%OUT%\*.pdb" 2>nul

set "COMMON=/nologo /std:c++latest /EHsc /W4 /permissive- /Zc:__cplusplus /I"%ROOT%\include" /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DWIN32_LEAN_AND_MEAN /DNOMINMAX"
if /I "%CONFIG%"=="Debug" (
  set "COMMON=%COMMON% /Od /Zi /MDd"
) else (
  set "COMMON=%COMMON% /O2 /DNDEBUG /MD"
)

set "SOURCES=application runtime runtime_host types"

echo [1/9] Static library
for %%S in (%SOURCES%) do cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\src\%%S.cpp" /Fo"%OBJS%\%%S.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\src\drivers\headless_driver.cpp" /Fo"%OBJS%\headless_driver.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\src\drivers\win32_driver.cpp" /Fo"%OBJS%\win32_driver.obj" || exit /b 1
lib /nologo /OUT:"%OUT%\EpochPlatformEngineStatic.lib" "%OBJS%\application.obj" "%OBJS%\runtime.obj" "%OBJS%\runtime_host.obj" "%OBJS%\types.obj" "%OBJS%\headless_driver.obj" "%OBJS%\win32_driver.obj" || exit /b 1

echo [2/9] Shared library
for %%S in (%SOURCES%) do cl %COMMON% /DEPOCH_PLATFORM_ENGINE_EXPORTS=1 /c "%ROOT%\src\%%S.cpp" /Fo"%OBJD%\%%S.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_EXPORTS=1 /c "%ROOT%\src\drivers\headless_driver.cpp" /Fo"%OBJD%\headless_driver.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_EXPORTS=1 /c "%ROOT%\src\drivers\win32_driver.cpp" /Fo"%OBJD%\win32_driver.obj" || exit /b 1
link /nologo /DLL /OUT:"%OUT%\EpochPlatformEngine.dll" /IMPLIB:"%OUT%\EpochPlatformEngine.lib" /PDB:"%OUT%\EpochPlatformEngine.pdb" "%OBJD%\application.obj" "%OBJD%\runtime.obj" "%OBJD%\runtime_host.obj" "%OBJD%\types.obj" "%OBJD%\headless_driver.obj" "%OBJD%\win32_driver.obj" user32.lib shell32.lib || exit /b 1

echo [3/9] Internal entrypoint archives
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\src\entrypoint\entrypoint.cpp" /Fo"%OBJS%\entrypoint.obj" || exit /b 1
lib /nologo /OUT:"%OUT%\EpochPlatformEntrypointConsoleStatic.lib" "%OBJS%\entrypoint.obj" || exit /b 1
cl %COMMON% /c "%ROOT%\src\entrypoint\entrypoint.cpp" /Fo"%OBJD%\entrypoint.obj" || exit /b 1
lib /nologo /OUT:"%OUT%\EpochPlatformEntrypointConsoleShared.lib" "%OBJD%\entrypoint.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /DEPOCH_PLATFORM_WINDOWED_ENTRYPOINT=1 /c "%ROOT%\src\entrypoint\entrypoint.cpp" /Fo"%OBJS%\entrypoint_windowed.obj" || exit /b 1
lib /nologo /OUT:"%OUT%\EpochPlatformEntrypointWindowedStatic.lib" "%OBJS%\entrypoint_windowed.obj" || exit /b 1
cl %COMMON% /DEPOCH_PLATFORM_WINDOWED_ENTRYPOINT=1 /c "%ROOT%\src\entrypoint\entrypoint.cpp" /Fo"%OBJD%\entrypoint_windowed.obj" || exit /b 1
lib /nologo /OUT:"%OUT%\EpochPlatformEntrypointWindowedShared.lib" "%OBJD%\entrypoint_windowed.obj" || exit /b 1

echo [4/9] Static validation example
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\examples\basic_app.cpp" /Fo"%OBJS%\basic_app.obj" || exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /OUT:"%OUT%\EpochPlatformExampleStatic.exe" "%OBJS%\basic_app.obj" "%OUT%\EpochPlatformEntrypointConsoleStatic.lib" "%OUT%\EpochPlatformEngineStatic.lib" user32.lib shell32.lib || exit /b 1

echo [5/9] Shared validation example
cl %COMMON% /c "%ROOT%\examples\basic_app.cpp" /Fo"%OBJD%\basic_app.obj" || exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /OUT:"%OUT%\EpochPlatformExampleShared.exe" "%OBJD%\basic_app.obj" "%OUT%\EpochPlatformEntrypointConsoleShared.lib" "%OUT%\EpochPlatformEngine.lib" user32.lib shell32.lib || exit /b 1

echo [6/9] Static release example
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\examples\release_app.cpp" /Fo"%OBJS%\release_app.obj" || exit /b 1
link /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\EpochPlatformReleaseExampleStatic.exe" "%OBJS%\release_app.obj" "%OUT%\EpochPlatformEntrypointWindowedStatic.lib" "%OUT%\EpochPlatformEngineStatic.lib" user32.lib shell32.lib || exit /b 1

echo [7/9] Shared release example
cl %COMMON% /c "%ROOT%\examples\release_app.cpp" /Fo"%OBJD%\release_app.obj" || exit /b 1
link /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\EpochPlatformReleaseExampleShared.exe" "%OBJD%\release_app.obj" "%OUT%\EpochPlatformEntrypointWindowedShared.lib" "%OUT%\EpochPlatformEngine.lib" user32.lib shell32.lib || exit /b 1

echo [8/9] Tests
cl %COMMON% /DEPOCH_PLATFORM_ENGINE_STATIC=1 /c "%ROOT%\tests\platform_tests.cpp" /Fo"%OBJS%\platform_tests.obj" || exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /OUT:"%OUT%\EpochPlatformEngineTests.exe" "%OBJS%\platform_tests.obj" "%OUT%\EpochPlatformEngineStatic.lib" user32.lib shell32.lib || exit /b 1

echo [9/9] Validation
"%OUT%\EpochPlatformEngineTests.exe" || exit /b 1
"%OUT%\EpochPlatformExampleStatic.exe" || exit /b 1
"%OUT%\EpochPlatformExampleShared.exe" || exit /b 1

echo Built: %OUT%
exit /b 0
