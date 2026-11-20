param(
    [string]$SdkRoot,
    [string]$NdkRoot,
    [ValidateSet('arm64-v8a', 'x86_64')]
    [string]$Abi = 'arm64-v8a',
    [switch]$BuildProbe
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path

function Find-InstalledDirectory([string]$Parent, [scriptblock]$Filter) {
    if (!$Parent -or !(Test-Path -LiteralPath $Parent -PathType Container)) { return $null }
    Get-ChildItem -LiteralPath $Parent -Directory | Where-Object $Filter |
        Sort-Object { [version]($_.Name -replace '^android-(\d+)$', '$1.0') } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (!$SdkRoot) {
    $SdkRoot = @($env:ANDROID_SDK_ROOT, $env:ANDROID_HOME,
        "$env:LOCALAPPDATA/Android/Sdk", "${env:ProgramFiles(x86)}/Android/android-sdk") |
        Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Container) } |
        Select-Object -First 1
}
if ($SdkRoot -and (Test-Path -LiteralPath $SdkRoot -PathType Container)) {
    $SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
}
if (!$NdkRoot) { $NdkRoot = $env:ANDROID_NDK_HOME }
if (!$NdkRoot -and $SdkRoot) {
    $NdkRoot = Find-InstalledDirectory (Join-Path $SdkRoot 'ndk') {
        $_.Name -match '^\d+(\.\d+){1,3}$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'build/cmake/android.toolchain.cmake') -PathType Leaf)
    }
}
$toolchain = if ($NdkRoot) { Join-Path $NdkRoot 'build/cmake/android.toolchain.cmake' }
$ndkReady = $toolchain -and (Test-Path -LiteralPath $toolchain -PathType Leaf)
$cmake = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$ninja = Get-Command ninja -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$java = Get-Command java -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$jdk = if ($env:JAVA_HOME) { Join-Path $env:JAVA_HOME 'bin/javac.exe' }
$javac = Get-Command javac -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$buildTools = if ($SdkRoot) {
    Find-InstalledDirectory (Join-Path $SdkRoot 'build-tools') {
        $_.Name -match '^\d+(\.\d+){1,3}$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'aapt2.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'zipalign.exe') -PathType Leaf)
    }
}
$platform = if ($SdkRoot) {
    Find-InstalledDirectory (Join-Path $SdkRoot 'platforms') {
        $_.Name -match '^android-\d+$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'android.jar') -PathType Leaf)
    }
}
$adb = $SdkRoot -and (Test-Path -LiteralPath (Join-Path $SdkRoot 'platform-tools/adb.exe') -PathType Leaf)
$emulator = $SdkRoot -and (Test-Path -LiteralPath (Join-Path $SdkRoot 'emulator/emulator.exe') -PathType Leaf)
$jdkReady = ($jdk -and (Test-Path -LiteralPath $jdk -PathType Leaf)) -or ($java -and $javac)

$report = [ordered]@{
    sdk_root = $SdkRoot
    ndk_root = $NdkRoot
    ndk_toolchain = [bool]$ndkReady
    cmake = $cmake.Source
    ninja = $ninja.Source
    android_platform = $platform
    build_tools = $buildTools
    jdk = [bool]$jdkReady
    adb = [bool]$adb
    emulator = [bool]$emulator
    probe_abi = $Abi
}
$probeStatus = if ($Abi -eq 'arm64-v8a') { 'arm64_module_probe' } else { 'x86_64_module_probe' }
$report[$probeStatus] = 'not requested'
if (!$BuildProbe) { $report | ConvertTo-Json; exit 0 }
if (!$ndkReady -or !$cmake -or !$ninja) {
    $report[$probeStatus] = 'blocked: requires an Android NDK toolchain, CMake and Ninja'
    $report | ConvertTo-Json
    exit 1
}

$toolchain = (Resolve-Path -LiteralPath $toolchain).Path
$output = Join-Path $repo ('Artifacts/Intermediate/android-module-probe/' + [guid]::NewGuid().ToString('N'))
$report.build_directory = $output
& $cmake.Source -S (Join-Path $repo 'Tests/platform/android') -B $output -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$($ninja.Source)" "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DANDROID_ABI=$Abi" '-DANDROID_PLATFORM=android-26' '-DCMAKE_BUILD_TYPE=Release' | Out-Host
if ($LASTEXITCODE -ne 0) {
    $report[$probeStatus] = 'failed: CMake configure'
    $report.exit_code = $LASTEXITCODE
    $report | ConvertTo-Json
    exit $report.exit_code
}
& $cmake.Source --build $output | Out-Host
if ($LASTEXITCODE -ne 0) {
    $report[$probeStatus] = 'failed: CMake build'
    $report.exit_code = $LASTEXITCODE
    $report | ConvertTo-Json
    exit $report.exit_code
}
$probe = Join-Path $output 'epoch_android_module_consumer'
$report[$probeStatus] = 'linked; device execution pending'
$report.probe_path = $probe
$report.probe_sha256 = (Get-FileHash -LiteralPath $probe).Hash
$report | ConvertTo-Json
