param(
    [string]$Version = '',
    [string]$Configuration = 'Clang-Release',
    [string]$BinaryRoot = '',
    [string]$VcpkgInstalledRoot = '',
    [string]$OutputRoot = ''
)

$ErrorActionPreference = 'Stop'

function Require-Path {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function To-WslPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = [System.IO.Path]::GetFullPath($Path)
    $drive = $resolved.Substring(0, 1).ToLowerInvariant()
    $tail = $resolved.Substring(2).Replace('\', '/')
    return "/mnt/$drive$tail"
}

function Invoke-WslScript {
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $normalized = $Script.Replace("`r", '')
    $encoded = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($normalized))
    & wsl.exe bash -lc "printf '%s' '$encoded' | base64 --decode | bash"
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE."
    }
}

$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$versionScript = Join-Path $repo 'Tools\ai\get_epoch_version.ps1'
Require-Path -Path $versionScript -Label 'Epoch version reader'
$sourceVersion = (& $versionScript -VersionModule (Join-Path $repo 'Engine\modules\epoch.version.ixx')).Trim()
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $sourceVersion
}
elseif ($Version -cne $sourceVersion) {
    throw "Requested release version $Version does not match source version $sourceVersion."
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = "C:\tmp\epoch_release_v$Version"
}

$engine = Join-Path $repo 'Engine'
if ([string]::IsNullOrWhiteSpace($BinaryRoot)) {
    $binaryRoot = Join-Path $engine "Bin\$Configuration"
}
else {
    $binaryRoot = $BinaryRoot
}
$binary = Join-Path $binaryRoot 'epoch'
$assets = Join-Path $engine 'assets'
$font = Join-Path $assets 'fonts\Roboto-Regular.ttf'
$license = Join-Path $repo 'LICENSE'
$readme = Join-Path $repo 'README.md'
$noticeScript = Join-Path $repo 'Tools\ai\collect_third_party_notices.ps1'
if ([string]::IsNullOrWhiteSpace($VcpkgInstalledRoot)) {
    $vcpkgInstalled = Join-Path $binaryRoot 'vcpkg_installed\x64-linux-epoch'
}
else {
    $vcpkgInstalled = $VcpkgInstalledRoot
}

Require-Path -Path $binary -Label 'Linux epoch binary'
Require-Path -Path $assets -Label 'Runtime assets'
Require-Path -Path $font -Label 'Runtime GUI font'
Require-Path -Path $license -Label 'License'
Require-Path -Path $readme -Label 'README'
Require-Path -Path $noticeScript -Label 'Third-party notice collector'
Require-Path -Path $vcpkgInstalled -Label 'Linux vcpkg installed tree'

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $resolvedOutput.StartsWith('C:\tmp\epoch_release_v', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing unexpected output root: $resolvedOutput"
}

$stageName = "epoch_linux_x64_v$Version"
$stage = Join-Path $resolvedOutput $stageName
$tarball = Join-Path $resolvedOutput "$stageName.tar.gz"
$checksum = Join-Path $resolvedOutput "v$Version`_checksums.txt"
$windowsZipName = "epoch_win10_x64_v$Version.zip"
$windowsZip = Join-Path $resolvedOutput $windowsZipName
$verifyLogs = Join-Path $resolvedOutput "verify_$stageName`_logs"

Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $tarball -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Remove-Item -LiteralPath $verifyLogs -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $verifyLogs -Force | Out-Null

Copy-Item -LiteralPath $binary -Destination (Join-Path $stage 'epoch') -Force
if (Test-Path -LiteralPath (Join-Path $binaryRoot 'lib')) {
    Copy-Item -LiteralPath (Join-Path $binaryRoot 'lib') -Destination (Join-Path $stage 'lib') -Recurse -Force
}
Copy-Item -LiteralPath $assets -Destination (Join-Path $stage 'assets') -Recurse -Force
Copy-Item -LiteralPath $license -Destination (Join-Path $stage 'LICENSE') -Force
Copy-Item -LiteralPath $readme -Destination (Join-Path $stage 'README.md') -Force
& $noticeScript -RepoRoot $repo -VcpkgInstalledRoot $vcpkgInstalled -Destination $stage

Require-Path -Path (Join-Path $stage 'lib\libsfml-graphics.so.3.0') -Label 'Packaged SFML graphics runtime'
Require-Path -Path (Join-Path $stage 'lib\libsfml-window.so.3.0') -Label 'Packaged SFML window runtime'
Require-Path -Path (Join-Path $stage 'lib\libsfml-system.so.3.0') -Label 'Packaged SFML system runtime'
Require-Path -Path (Join-Path $stage 'lib\libvulkan.so.1') -Label 'Packaged Vulkan loader'

$stageWsl = To-WslPath $stage
$outWsl = To-WslPath $resolvedOutput
$tarWsl = To-WslPath $tarball
$verifyLogsWsl = To-WslPath $verifyLogs

$validationScript = @'
set -euo pipefail
chmod 755 '__STAGE__/epoch'
test -f '__STAGE__/assets/fonts/Roboto-Regular.ttf'
cd '__STAGE__'

runpath="$(readelf -d ./epoch | sed -n 's/.*RUNPATH.*\[\(.*\)\].*/\1/p')"
test "$runpath" = '$ORIGIN/lib'

ldd_output="$(ldd ./epoch)"
if grep -F 'not found' <<<"$ldd_output"; then
    echo 'Packaged Linux binary has unresolved shared libraries.' >&2
    exit 1
fi
if grep -E 'vcpkg_installed|/home/|/Users/|/work/' <<<"$ldd_output"; then
    echo 'Packaged Linux binary resolved a dependency from a build-machine path.' >&2
    exit 1
fi
vulkan_path="$(awk '/libvulkan\.so\.1 =>/ { print $3; exit }' <<<"$ldd_output")"
test -n "$vulkan_path"
test "$(readlink -f "$vulkan_path")" = "$(readlink -f ./lib/libvulkan.so.1)"

EPOCH_LOG_DIR='__LOGS__' ./epoch --version | grep -F 'Epoch v__VERSION__' >/dev/null
EPOCH_LOG_DIR='__LOGS__' ./epoch --engine-contract-self-test | grep -F 'engine_contract_self_test.result=pass' >/dev/null
EPOCH_LOG_DIR='__LOGS__' timeout --signal=INT --kill-after=3s 30s \
    ./epoch --editor --renderer opengl --smoke
'@
$validationScript = $validationScript.Replace('__STAGE__', $stageWsl)
$validationScript = $validationScript.Replace('__LOGS__', $verifyLogsWsl)
$validationScript = $validationScript.Replace('__VERSION__', $Version)

Invoke-WslScript -Script $validationScript -Label 'Linux staged-package validation'

# Validation paths can still create executable-local runtime artifacts before
# their environment override is consumed. Public packages never carry them.
Remove-Item -LiteralPath (Join-Path $stage 'logs') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $stage 'cache') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $verifyLogs -Recurse -Force -ErrorAction SilentlyContinue

$archiveScript = "set -euo pipefail; tar -C '$outWsl' -czf '$tarWsl' '$stageName'; tar -tzvf '$tarWsl' '$stageName/epoch' '$stageName/assets/fonts/Roboto-Regular.ttf' '$stageName/THIRD_PARTY_NOTICES.txt' '$stageName/THIRD_PARTY_COMPONENTS.json'; if tar -tzf '$tarWsl' | grep -E '/(logs|cache)/'; then echo 'Release archive must not include generated logs or runtime cache.' >&2; exit 1; fi"
Invoke-WslScript -Script $archiveScript -Label 'Linux release archive validation'

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $tarball).Hash.ToLowerInvariant()
$lines = @()
if (Test-Path -LiteralPath $windowsZip) {
    $windowsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $windowsZip).Hash.ToLowerInvariant()
    $lines += "$windowsHash  $windowsZipName"
}
else {
    $lines += Get-Content -LiteralPath $checksum -ErrorAction SilentlyContinue |
        Where-Object { $_ -and ($_ -notmatch [regex]::Escape("$stageName.tar.gz") + '$') }
}
$lines += "$hash  $stageName.tar.gz"
[System.IO.File]::WriteAllLines($checksum, $lines, [System.Text.Encoding]::ASCII)

Get-Item -LiteralPath $tarball, $checksum | Select-Object FullName, Length
