#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$PackageReceipt,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$')][string]$Serial,
    [ValidateRange(1,65535)][int]$AdbServerPort = 5037,
    [switch]$Inspect,
    [switch]$PhysicalDevice
)

$ErrorActionPreference = 'Stop'
$emulatorSerial = $Serial -cmatch '^emulator-[0-9]{4,5}$'
if ([bool]$PhysicalDevice -eq $emulatorSerial) {
    throw 'Select an emulator serial, or explicitly select a physical device serial with -PhysicalDevice.'
}
$receiptFile = Get-Item -LiteralPath $PackageReceipt
$receipt = Get-Content -LiteralPath $receiptFile.FullName -Raw | ConvertFrom-Json
if ($receipt.apk_sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
    $receipt.application_id -notmatch '^[a-zA-Z][a-zA-Z0-9_]*(\.[a-zA-Z][a-zA-Z0-9_]*)+$' -or
    $receipt.activity -cne 'org.epochengine.runtime.EpochActivity' -or
    $receipt.abi -cnotin @('x86_64','arm64-v8a')) {
    throw 'The receipt does not describe an Epoch Android debug package.'
}

$apk = Get-Item -LiteralPath $receipt.apk
$adbName = if ($IsWindows) { 'adb.exe' } else { 'adb' }
$adb = Get-Item -LiteralPath (Join-Path (Resolve-Path -LiteralPath $SdkRoot).Path "platform-tools/$adbName")
$expectedHash = $receipt.apk_sha256.ToUpperInvariant()
if ($apk.PSIsContainer -or ($apk.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $apk.Extension -ine '.apk' -or
    (Get-FileHash -LiteralPath $apk.FullName).Hash -ne $expectedHash) {
    throw 'The APK does not match the package receipt.'
}

function Invoke-DeviceAdb {
    param([Parameter(Mandatory)][string[]]$Arguments, [int]$TimeoutSeconds = 8)

    $start = [Diagnostics.ProcessStartInfo]::new($adb.FullName)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($value in @('-H', '127.0.0.1', '-P', [string]$AdbServerPort, '-s', $Serial) + $Arguments) {
        $start.ArgumentList.Add($value)
    }

    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill()
            throw "ADB $($Arguments[0]) exceeded its deadline for $Serial."
        }
        $output = $stdout.GetAwaiter().GetResult().Trim()
        $diagnostic = $stderr.GetAwaiter().GetResult().Trim()
        if ($process.ExitCode -ne 0) {
            throw "ADB $($Arguments[0]) failed for $Serial ($($process.ExitCode)). $diagnostic $output"
        }
        return $output
    } finally {
        $process.Dispose()
    }
}

function Get-InstalledPackageHash {
    # Android's package path comes from the selected guest. Accept exactly one
    # regular base package and pass only a shell-safe path back to the guest.
    $packages = Invoke-DeviceAdb -Arguments @('shell','pm','list','packages',$receipt.application_id)
    if (($packages -split '\r?\n') -cnotcontains "package:$($receipt.application_id)") {
        return $null
    }
    $packagePath = Invoke-DeviceAdb -Arguments @('shell','pm','path',$receipt.application_id)
    if ($packagePath -cnotmatch '^package:(/data/app/[a-zA-Z0-9_./~=-]+/base\.apk)$') {
        throw 'The guest did not identify a single base package.'
    }
    $guestPath = $Matches[1]
    if (($guestPath.Split('/') | Where-Object { $_ -in @('.','..') })) {
        throw 'The guest returned a noncanonical package path.'
    }
    $computed = Invoke-DeviceAdb -Arguments @('shell','run-as',$receipt.application_id,'sha256sum',$guestPath)
    if ($computed -notmatch '^([0-9a-fA-F]{64})\s') { throw 'The guest did not return an APK hash.' }
    return $Matches[1].ToUpperInvariant()
}

if ((Invoke-DeviceAdb -Arguments @('get-state')) -cne 'device' -or
    (Invoke-DeviceAdb -Arguments @('shell','getprop','sys.boot_completed')) -cne '1') {
    throw 'The selected Android device has not finished booting.'
}
$guestApi = Invoke-DeviceAdb -Arguments @('shell','getprop','ro.build.version.sdk')
if ($guestApi -cnotmatch '^[0-9]{1,3}$' -or [int]$guestApi -lt 26) {
    throw 'The selected Android device is below the APK minimum API 26.'
}
$isVirtual = (Invoke-DeviceAdb -Arguments @('shell','getprop','ro.kernel.qemu')) -ceq '1'
$isVirtual = $isVirtual -or (Invoke-DeviceAdb -Arguments @('shell','getprop','ro.boot.qemu')) -ceq '1'
if ($isVirtual -ne $emulatorSerial) { throw 'The selected Android device does not match the requested transport.' }
$abis = (Invoke-DeviceAdb -Arguments @('shell','getprop','ro.product.cpu.abilist')) -split ','
if ($abis -cnotcontains $receipt.abi) { throw "The selected device does not support $($receipt.abi)." }

$installedHash = Get-InstalledPackageHash
$installed = $false
$launchStatus = $null
if (!$Inspect) {
    if ($installedHash -cne $expectedHash) {
        $installStatus = Invoke-DeviceAdb -Arguments @('install','-r',$apk.FullName) -TimeoutSeconds 120
        if (($installStatus -split '\r?\n') -cnotcontains 'Success') {
            throw "The Android device did not confirm APK installation. $installStatus"
        }
        $installed = $true
        $installedHash = Get-InstalledPackageHash
    }
    if ($installedHash -cne $expectedHash) { throw 'The installed APK differs from the package receipt.' }

    $component = "$($receipt.application_id)/$($receipt.activity)"
    $launchStatus = Invoke-DeviceAdb -Arguments @('shell','am','start','-W','-n',$component) -TimeoutSeconds 35
    if (($launchStatus -split '\r?\n') -cnotcontains 'Status: ok') {
        throw "Android did not confirm the Epoch activity launch. $launchStatus"
    }
}

[ordered]@{
    serial = $Serial
    device_kind = $(if ($isVirtual) { 'emulator' } else { 'physical' })
    device_api = [int]$guestApi
    abi = $receipt.abi
    application_id = $receipt.application_id
    apk_sha256 = $expectedHash
    installed_sha256 = $installedHash
    exact_package_present = $installedHash -ceq $expectedHash
    apk_installed_this_run = $installed
    activity_launch_confirmed = $null -ne $launchStatus
} | ConvertTo-Json
