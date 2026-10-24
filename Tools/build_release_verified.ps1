# Build the source located next to this script, never a previously extracted release.
[CmdletBinding()]
param(
    [ValidateRange(1, 32)][int]$Jobs = 2,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$versionFile = Join-Path $root 'Engine\modules\epoch.version.ixx'
$resourceFile = Join-Path $root 'Engine\resource\WinResource.rc'
$engineFile = Join-Path $root 'Engine\src\ai\ai.engine.cpp'
$solution = Join-Path $root 'Engine.sln'
. (Join-Path $root 'Tools\build\epoch_package_layout.ps1')
$edition = Get-EpochSourceEdition -RepoRoot $root
$binaryRoot = Join-Path $root ('build\bin\windows\{0}\x64\Release' -f $edition.Directory)
$probeFile = Join-Path $root 'Engine\src\build\build.cpp_feature_probe.hpp'
$perfHeaderFile = Join-Path $root 'Engine\src\editor\editor.frame_performance.hpp'
$itemsFile = Join-Path $root 'Engine\Engine.vcxitems'
$editorProjectFile = Join-Path $root 'Engine\examples\EpochEditor\EpochEditor.vcxproj'
$editorIcon = Join-Path $root 'Engine\resource\icon.ico'
$buildDocs = Join-Path $root 'Engine\docs\build'

foreach ($file in @($versionFile, $resourceFile, $editorProjectFile, $editorIcon, $engineFile, $solution, $probeFile, $perfHeaderFile, $itemsFile)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing source file: $file" }
}
# epoch.version.ixx is the sole authority for development version metadata.
# Derive the verification target from its literal fallback macros.  A stale
# release-script revision must never block an otherwise correct source tree.
$versionSource = Get-Content -LiteralPath $versionFile -Raw
$versionFields = @{}
foreach ($component in @('MAJOR', 'MINOR', 'REVISION')) {
    $pattern = "(?m)^\s*#\s*define\s+EPOCH_VERSION_${component}_VALUE\s+(\d+)\s*$"
    $definitions = [regex]::Matches($versionSource, $pattern)
    if ($definitions.Count -ne 1) {
        throw "Expected one literal EPOCH_VERSION_${component}_VALUE fallback in $versionFile."
    }
    $versionFields[$component] = [int]$definitions[0].Groups[1].Value
}
$expectedVersion = '{0}.{1}.{2}' -f $versionFields['MAJOR'], $versionFields['MINOR'], $versionFields['REVISION']
$expectedRevision = $versionFields['REVISION']
# MSVC-safe editor frame timer environment lookup. Prevent C4996 from
# breaking the strict Release build again; do not relax warning policy.
$perfHeaderSource = Get-Content -LiteralPath $perfHeaderFile -Raw
if ($perfHeaderSource -notmatch '#if defined\(_MSC_VER\)' -or
    $perfHeaderSource -notmatch '_dupenv_s\s*\(' -or
    $perfHeaderSource -notmatch 'std::free\s*\(value\)') {
    throw "Missing MSVC-safe editor frame timer environment lookup: $perfHeaderFile"
}
# The feature-probe header is first-party SOURCE, not a generated build artifact.
# A prior ZIP accidentally removed Engine/src/build alongside actual build outputs.
$probeSource = Get-Content -LiteralPath $probeFile -Raw
foreach ($feature in @('has_expected', 'has_stacktrace', 'has_std_execution', 'has_contracts', 'has_static_reflection')) {
    if ($probeSource -notmatch [regex]::Escape($feature)) {
        throw "Incomplete first-party feature probe: $feature is missing in $probeFile"
    }
}
if (-not (Test-Path -LiteralPath $buildDocs -PathType Container)) {
    throw "Source packaging omitted first-party build documentation: $buildDocs"
}
# Resolve every directly registered source/header under the Engine shared-items project.
# Fail early instead of waiting for MSVC's C1083 deep into a full rebuild.
$itemsSource = Get-Content -LiteralPath $itemsFile -Raw
$sharedDir = Split-Path -Parent $itemsFile
$prefix = '$(MSBuildThisFileDirectory)'
$refs = [regex]::Matches($itemsSource, '<(?:ClInclude|ClCompile|ResourceCompile)\s+Include="([^"]+)"')
foreach ($ref in $refs) {
    $include = $ref.Groups[1].Value
    if (-not $include.StartsWith($prefix, [StringComparison]::Ordinal)) { continue }
    $relative = $include.Substring($prefix.Length)
    $absolute = Join-Path $sharedDir $relative
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) {
        throw "Incomplete source package; MSBuild references a missing file: $absolute"
    }
}
# rc.exe is not reliably UTF-8-BOM safe. The script is intentionally plain ASCII.
# This must be checked on the disk bytes, BEFORE text decoding hides the BOM.
$resourceBytes = [IO.File]::ReadAllBytes($resourceFile)
if ($resourceBytes.Length -eq 0) { throw "Empty Windows resource script: $resourceFile" }
foreach ($value in $resourceBytes) {
    if ($value -gt 127) {
        throw "Windows resource script must be ASCII without a BOM: $resourceFile"
    }
}
# The editor owns its executable icon/version resources; the engine library does not.
if ($itemsSource -match '(?s)<ResourceCompile\s+Include="[^"]*WinResource\.rc"') {
    throw 'WinResource.rc must not be compiled by the shared Engine.vcxitems static library.'
}
$editorProjectSource = Get-Content -LiteralPath $editorProjectFile -Raw
if ($editorProjectSource -notmatch '<ResourceCompile\s+Include="\.\.\\\.\.\\resource\\WinResource\.rc"') {
    throw "Missing editor executable resource compile registration in $editorProjectFile"
}
if ($editorProjectSource -notmatch '\$\(MSBuildProjectDirectory\)\\\.\.\\\.\.\\resource') {
    throw "Editor resource compiler lacks icon/header include directory: $editorProjectFile"
}
$iconBytes = [IO.File]::ReadAllBytes($editorIcon)
if ($iconBytes.Length -lt 6 -or $iconBytes[0] -ne 0 -or $iconBytes[1] -ne 0 -or
    $iconBytes[2] -ne 1 -or $iconBytes[3] -ne 0) {
    throw "Invalid editor icon file: $editorIcon"
}
$resourceSource = Get-Content -LiteralPath $resourceFile -Raw
$engineSource = Get-Content -LiteralPath $engineFile -Raw
if ($versionSource -notmatch "(?m)^\s*#\s*define\s+EPOCH_VERSION_REVISION_VALUE\s+$expectedRevision\s*$") {
    throw "Wrong source revision in $versionFile (expected $expectedVersion)."
}
# Use one expected source version for all RC checks. Never carry forward an
# older product-version literal when generating a new source release.
$expectedResourceTuple = [regex]::Escape(($expectedVersion.Replace('.', ',') + ',0'))
$expectedResourceText = [regex]::Escape($expectedVersion)
if ($resourceSource -notmatch ('FILEVERSION\s+' + $expectedResourceTuple) -or
    $resourceSource -notmatch ('PRODUCTVERSION\s+' + $expectedResourceTuple) -or
    $resourceSource -notmatch ('VALUE\s+"FileVersion",\s*"' + $expectedResourceText + '"') -or
    $resourceSource -notmatch ('VALUE\s+"ProductVersion",\s*"' + $expectedResourceText + '"')) {
    throw "Windows resource version does not match $expectedVersion."
}
foreach ($mode in @('off','low','high')) {
    $safe = 'choice\.compare\("' + $mode + '"\)\s*==\s*0'
    $unsafe = 'choice\s*==\s*"' + $mode + '"'
    if ($engineSource -notmatch $safe -or $engineSource -match $unsafe) {
        throw "Unsafe thinking parser for '$mode' in $engineFile."
    }
}
Write-Host "Verified EpochEngine $expectedVersion source: $root"
Write-Host 'Configuration: Release | Platform: x64'
Write-Host 'Thinking parser: verified (std::string_view::compare)'
if ($SkipBuild) { exit 0 }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
        Select-Object -First 1
}
if (-not $msbuild) {
    $command = Get-Command 'MSBuild.exe' -ErrorAction SilentlyContinue
    if ($command) { $msbuild = $command.Source }
}
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw 'MSBuild.exe not found. Install the Visual Studio C++ build workload.'
}
Write-Host "MSBuild: $msbuild"
# Keep the unabridged compiler output in a predictable, copyable text log.
# FileLogger runs alongside normal console output; no PowerShell stderr piping
# or output buffering can alter the native MSBuild exit code.
$logDirectory = Join-Path $root 'build\logs\msvc'
[IO.Directory]::CreateDirectory($logDirectory) | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$buildLog = Join-Path $logDirectory "build_release_verified-$timestamp.log"
$latestLog = Join-Path $logDirectory 'build_release_verified-latest.log'
$errorLog = Join-Path $logDirectory 'build_release_errors-latest.txt'
Write-Host "Full build log: $buildLog"
& $msbuild $solution '/t:Rebuild' '/p:Configuration=Release' '/p:Platform=x64' "/m:$Jobs" '/verbosity:minimal' '/nologo' '/fl' "/flp:logfile=$buildLog;verbosity=normal;encoding=UTF-8"
$buildExitCode = $LASTEXITCODE
if (Test-Path -LiteralPath $buildLog -PathType Leaf) {
    Copy-Item -LiteralPath $buildLog -Destination $latestLog -Force
}
if ($buildExitCode -ne 0) {
    $errors = @()
    if (Test-Path -LiteralPath $buildLog -PathType Leaf) {
        $errors = @(Get-Content -LiteralPath $buildLog |
            Where-Object { $_ -match '(?i)(?:^|:)\s*(?:fatal\s+)?error\s+(C\d+|MSB\d+|LNK\d+|RC\d+|[A-Z]+\d+)' } |
            Select-Object -Unique -First 40)
        if ($errors.Count -eq 0) {
            $errors = @(Get-Content -LiteralPath $buildLog -Tail 60)
        }
    }
    if ($errors.Count -eq 0) { $errors = @("MSBuild exited $buildExitCode without a readable diagnostic log.") }
    $errors | Set-Content -LiteralPath $errorLog -Encoding UTF8
    Write-Host ''
    Write-Host '========== FIRST BUILD ERRORS (copy this section) ==========' -ForegroundColor Red
    $errors | ForEach-Object { Write-Host $_ }
    Write-Host '============================================================'
    Write-Host "Full log: $latestLog"
    Write-Host "Error summary: $errorLog"
    Write-Host "Copy errors: Get-Content -LiteralPath '$errorLog' -Raw | Set-Clipboard"
    throw "Release x64 build failed (exit $buildExitCode). First errors and full log were saved above."
}
Write-Host "Full build log: $latestLog"
Write-Host "Copy log: Get-Content -LiteralPath '$latestLog' -Raw | Set-Clipboard"
foreach ($output in @((Join-Path $binaryRoot 'EpochEngine.lib'), (Join-Path $binaryRoot 'EpochEditor.exe'))) {
    $path = $output
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Build reported success but expected output is missing: $path"
    }
}
Write-Host "Build succeeded: $expectedVersion Release x64"
