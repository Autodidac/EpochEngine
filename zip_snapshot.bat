@echo off
setlocal EnableExtensions

set "OUTZIP=%~1"
if "%OUTZIP%"=="" set "OUTZIP=project_snapshot.zip"

set "ROOT=%CD%"
set "OUTZIP_FULL=%OUTZIP%"
if not "%OUTZIP:~1,1%"==":" set "OUTZIP_FULL=%ROOT%\%OUTZIP%"

if exist "%OUTZIP_FULL%" del /q "%OUTZIP_FULL%" >nul 2>&1

echo ROOT        = "%ROOT%"
echo OUTZIP_FULL = "%OUTZIP_FULL%"

powershell -NoProfile -ExecutionPolicy Bypass ^
  "$ErrorActionPreference='Stop';" ^
  "$root    = '%ROOT%';" ^
  "$zipPath = '%OUTZIP_FULL%';" ^
  "" ^
  "$skipDirs = @(" ^
  "  '.git/','.vs/','Debug/','Release/','x64/','x86/','out/'," ^
  "  'build/','build_clang/','build_gcc/','built/'," ^
  "  'vcpkg_installed/','bin/','Bin/','backup/','app1/'," ^
  "  'updater/Cpp20_Ultimate_Project_Updater/'," ^
  "  'Engine/bin/','Engine/Bin/','Engine/docs/api/'," ^
  "  'Engine/examples/ConsoleApplication1/atlas_dump/'" ^
  ");" ^
  "" ^
  "$skipFiles = @(" ^
  "  '*.d','*.slo','*.lo','*.o','*.obj'," ^
  "  '*.gch','*.pch'," ^
  "  '*.so','*.dylib','*.dll'," ^
  "  '*.mod','*.smod'," ^
  "  '*.lai','*.la','*.a','*.lib'," ^
  "  '*.exe','*.out','*.app'," ^
  "  '*.ninja_log','ninja.zip'," ^
  "  '*.pdb','*.idb','*.log','*.tlog'," ^
  "  '*.cmake','*.bin'," ^
  "  'Cpp20_Ultimate_Project_Updater.exe','update.exe'," ^
  "  '*.zip'" ^
  ");" ^
  "" ^
  "function IsInSkipDir([string]$full) {" ^
  "  $rel = $full.Substring($root.Length).TrimStart('\','/');" ^
  "  $rel = $rel -replace '\\','/';" ^
  "  foreach ($d in $skipDirs) {" ^
  "    if ($rel.StartsWith($d, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }" ^
  "  }" ^
  "  return $false" ^
  "}" ^
  "" ^
  "function MatchesPatterns([string]$name, [string[]]$patterns) {" ^
  "  foreach ($p in $patterns) { if ($name -like $p) { return $true } }" ^
  "  return $false" ^
  "}" ^
  "" ^
  "$all = Get-ChildItem -LiteralPath $root -Recurse -File -Force;" ^
  "$include = @();" ^
  "foreach ($f in $all) {" ^
  "  if ($f.FullName -eq $zipPath) { continue }" ^
  "  if (IsInSkipDir $f.FullName) { continue }" ^
  "  if (MatchesPatterns $f.Name $skipFiles) { continue }" ^
  "  $include += $f.FullName" ^
  "}" ^
  "if ($include.Count -eq 0) { throw 'Nothing to zip after filters.' }" ^
  "" ^
  "Compress-Archive -LiteralPath $include -DestinationPath $zipPath -Force -CompressionLevel Fastest;" ^
  "$fi = Get-Item -LiteralPath $zipPath;" ^
  "Write-Host ('DONE. Included=' + $include.Count + ' ZipSize=' + $fi.Length + ' bytes');" ^
  "exit 0"

if errorlevel 1 (
    echo ERROR: Zip failed.
    exit /b 1
)

for %%A in ("%OUTZIP_FULL%") do echo ZIP SIZE: %%~zA bytes
echo Created OK: "%OUTZIP_FULL%"

endlocal
exit /b 0
