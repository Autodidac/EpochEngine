@echo off
setlocal
rem Explicit throughput experiment: optimized Release executable, no intentional frame limiter.
rem For everyday AI sessions use run.bat (pacing stays under the user setting).
call "%~dp0run.bat" --frame-limit unlimited %*
exit /b %ERRORLEVEL%
