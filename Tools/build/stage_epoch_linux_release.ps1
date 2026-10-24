param(
    [string]$Version = '',
    [string]$Configuration = 'Clang-Release',
    [string]$BinaryRoot = '',
    [string]$VcpkgInstalledRoot = '',
    [string]$OutputRoot = '',
    [string]$WslDistribution = '',
    [switch]$SoftwareOpenGLSmoke,
    [switch]$SkipRuntimeSmoke
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

function To-LinuxPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = [System.IO.Path]::GetFullPath($Path)
    if ([System.IO.Path]::DirectorySeparatorChar -eq '/') { return $resolved }
    $drive = $resolved.Substring(0, 1).ToLowerInvariant()
    $tail = $resolved.Substring(2).Replace('\', '/')
    return "/mnt/$drive$tail"
}

function Invoke-LinuxScript {
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $normalized = $Script.Replace("`r", '')
    $encoded = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($normalized))
    if ([System.IO.Path]::DirectorySeparatorChar -eq '/') {
        & bash -c $normalized
    }
    else {
        if ([string]::IsNullOrWhiteSpace($WslDistribution)) {
            & wsl.exe bash -lc "printf '%s' '$encoded' | base64 --decode | bash"
        }
        else {
            & wsl.exe --distribution $WslDistribution --exec bash -lc "printf '%s' '$encoded' | base64 --decode | bash"
        }
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE."
    }
}

$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $repo 'Tools\build\epoch_package_layout.ps1')
$edition = Get-EpochSourceEdition -RepoRoot $repo
$packageRoot = Join-Path $repo 'build\packages'
$versionScript = Join-Path $repo 'Tools\build\get_epoch_version.ps1'
Require-Path -Path $versionScript -Label 'Epoch version reader'
$sourceVersion = (& $versionScript -VersionModule (Join-Path $repo 'Engine\modules\epoch.version.ixx')).Trim()
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $sourceVersion
}
elseif ($Version -cne $sourceVersion) {
    throw "Requested release version $Version does not match source version $sourceVersion."
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw 'Release version must contain only three numeric components.'
}

$engine = Join-Path $repo 'Engine'
if ([string]::IsNullOrWhiteSpace($BinaryRoot)) {
    $binaryRoot = Join-Path $repo ("build\cmake\linux\{0}\{1}" -f $edition.Directory, $Configuration)
}
else {
    $binaryRoot = $BinaryRoot
}
$binary = Join-Path $binaryRoot 'epoch'
$assets = Join-Path $engine 'assets'
$font = Join-Path $assets 'fonts\Roboto-Regular.ttf'
$license = Join-Path $repo 'LICENSE'
$readme = Join-Path $repo 'README.md'
$noticeScript = Join-Path $repo 'Tools\build\collect_third_party_notices.ps1'
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

$resolvedOutput = Get-EpochPackageOutputRoot -RepoRoot $repo -Edition $edition.Directory -Version $Version -OutputRoot $OutputRoot

$stageName = "epoch_linux_x64_v$Version"
$stage = Join-Path $resolvedOutput $stageName
$tarball = Join-Path $resolvedOutput "$stageName.tar.gz"
$checksum = Join-Path $resolvedOutput "v$Version`_checksums.txt"
$windowsZipName = "epoch_win10_x64_v$Version.zip"
$windowsZip = Join-Path $resolvedOutput $windowsZipName
$verifyLogs = Join-Path $resolvedOutput ("verify_$stageName`_logs/" + [guid]::NewGuid().ToString('N'))
foreach ($target in @($stage, $tarball, $verifyLogs)) {
    $resolvedTarget = [System.IO.Path]::GetFullPath($target)
    if (-not $resolvedTarget.StartsWith($resolvedOutput + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Release staging target escapes the dedicated output directory: $resolvedTarget"
    }
}

Remove-EpochPackageItem -LiteralPath $stage -PackageRoot $packageRoot -Recurse
Remove-EpochPackageItem -LiteralPath $tarball -PackageRoot $packageRoot
New-Item -ItemType Directory -Path $stage -Force | Out-Null
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

$stageWsl = To-LinuxPath $stage
$binaryRootWsl = To-LinuxPath $binaryRoot
$outWsl = To-LinuxPath $resolvedOutput
$tarWsl = To-LinuxPath $tarball
$verifyLogsWsl = To-LinuxPath $verifyLogs

$validationScript = @'
set -euo pipefail
mkdir -p '__LOGS__/scratch'
export TMPDIR='__LOGS__/scratch'
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
if grep -F 'vcpkg_installed' <<<"$ldd_output"; then
    echo 'Packaged Linux binary resolved a dependency from vcpkg build output.' >&2
    exit 1
fi
if grep -F '__BUILD_ROOT__/' <<<"$ldd_output"; then
    echo 'Packaged Linux binary resolved a dependency from the engine build root.' >&2
    exit 1
fi
for library in libsfml-graphics.so.3.0 libsfml-window.so.3.0 libsfml-system.so.3.0 libvulkan.so.1; do
    resolved="$(awk -v name="$library" '$1 == name && $2 == "=>" { print $3; exit }' <<<"$ldd_output")"
    expected="__STAGE__/lib/$library"
    if [[ -z "$resolved" || "$(readlink -f "$resolved")" != "$(readlink -f "$expected")" ]]; then
        printf 'Packaged dependency %s resolved outside its staged lib directory: %s\n' "$library" "$resolved" >&2
        exit 1
    fi
done
max_glibc="$(readelf --version-info ./epoch ./lib/*.so* 2>/dev/null | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sort -Vu | tail -n1)"
if [[ -z "$max_glibc" || "$(printf '%s\n' 'GLIBC_2.35' "$max_glibc" | sort -V | tail -n1)" != 'GLIBC_2.35' ]]; then
    printf 'Packaged Linux runtime exceeds the GLIBC_2.35 baseline: %s\n' "$max_glibc" >&2
    exit 1
fi
max_glibcxx="$(readelf --version-info ./epoch ./lib/*.so* 2>/dev/null | grep -oE 'GLIBCXX_[0-9]+\.[0-9]+\.[0-9]+' | sort -Vu | tail -n1)"
if [[ -z "$max_glibcxx" || "$(printf '%s\n' 'GLIBCXX_3.4.30' "$max_glibcxx" | sort -V | tail -n1)" != 'GLIBCXX_3.4.30' ]]; then
    printf 'Packaged Linux runtime exceeds the Ubuntu 22.04 GLIBCXX_3.4.30 baseline: %s\n' "$max_glibcxx" >&2
    exit 1
fi
printf 'shared_library_resolution.result=pass glibc=%s glibcxx=%s runpath=%s\n' "$max_glibc" "$max_glibcxx" "$runpath"

version_output="$(EPOCH_LOG_DIR='__LOGS__' ./epoch --version)"
printf '%s\n' "$version_output" | grep -F 'Epoch v__VERSION__' >/dev/null
printf '%s\n' "$version_output" | grep -F 'edition=__EDITION__' >/dev/null
EPOCH_LOG_DIR='__LOGS__' ./epoch --engine-contract-self-test | grep -F 'engine_contract_self_test.result=pass' >/dev/null
printf 'engine_contract.result=pass version=%s\n' '__VERSION__'
__RUNTIME_SMOKE__
'@
$validationScript = $validationScript.Replace('__STAGE__', $stageWsl)
$validationScript = $validationScript.Replace('__BUILD_ROOT__', $binaryRootWsl)
$validationScript = $validationScript.Replace('__LOGS__', $verifyLogsWsl)
$validationScript = $validationScript.Replace('__VERSION__', $Version)
$validationScript = $validationScript.Replace('__EDITION__', $edition.Id)
$runtimeSmoke = if ($SkipRuntimeSmoke) {
    "printf '%s\n' 'Linux packaged runtime smoke explicitly skipped; package remains build/contract proven only.'"
}
elseif ($SoftwareOpenGLSmoke) {
    "EPOCH_LOG_DIR='__LOGS__' LIBGL_ALWAYS_SOFTWARE=1 timeout --signal=INT --kill-after=3s 30s xvfb-run -a --server-args='-screen 0 1280x720x24 -nolisten tcp' ./epoch --editor --renderer opengl --smoke"
}
else {
    "EPOCH_LOG_DIR='__LOGS__' timeout --signal=INT --kill-after=3s 30s ./epoch --editor --renderer opengl --smoke"
}
$runtimeSmoke = $runtimeSmoke.Replace('__LOGS__', $verifyLogsWsl)
$validationScript = $validationScript.Replace('__RUNTIME_SMOKE__', $runtimeSmoke)

Invoke-LinuxScript -Script $validationScript -Label 'Linux staged-package validation'

# Validation paths can still create executable-local runtime artifacts before
# their environment override is consumed. Public packages never carry them.
Remove-EpochPackageItem -LiteralPath (Join-Path $stage 'logs') -PackageRoot $packageRoot -Recurse
Remove-EpochPackageItem -LiteralPath (Join-Path $stage 'cache') -PackageRoot $packageRoot -Recurse

Write-EpochRuntimeDistribution -Stage $stage -Edition $edition.Id -Version $Version

$archiveScript = "set -euo pipefail; tar -C '$outWsl' -czf '$tarWsl' '$stageName'; tar -tzvf '$tarWsl' '$stageName/epoch' '$stageName/epoch-distribution.json' '$stageName/assets/fonts/Roboto-Regular.ttf' '$stageName/THIRD_PARTY_NOTICES.txt' '$stageName/THIRD_PARTY_COMPONENTS.json'; if tar -tzf '$tarWsl' | grep -E '/(logs|cache)/'; then echo 'Release archive must not include generated logs or runtime cache.' >&2; exit 1; fi"
Invoke-LinuxScript -Script $archiveScript -Label 'Linux release archive validation'

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
Write-Host "package_inventory.result=pass artifact=$stageName.tar.gz sha256=$hash"

# The machine-readable receipt describes only checks that completed above.
$validation = [ordered]@{
    schema = 'epoch.linux-package-validation.v1'
    edition = $edition.Id
    version = $Version
    artifact = "$stageName.tar.gz"
    sha256 = $hash
    configuration = $Configuration
    version_check = 'pass'
    engine_contract = 'pass'
    shared_libraries = 'pass'
    archive_inventory = 'pass'
    runtime_smoke = $(if ($SkipRuntimeSmoke) { 'not_run' } else { 'pass' })
    runtime_renderer = $(if ($SkipRuntimeSmoke) { 'not_run' } elseif ($SoftwareOpenGLSmoke -or $env:LIBGL_ALWAYS_SOFTWARE -eq '1') { 'software_opengl' } else { 'opengl' })
    hardware_parity = $false
}
[IO.File]::WriteAllText("$tarball.sha256", "$hash  $stageName.tar.gz`n", [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$tarball.build-validation.json", ($validation | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
& python (Join-Path $repo 'Tools/build/audit_epoch_linux_package.py') --archive $tarball
if ($LASTEXITCODE -ne 0) { throw 'Linux package audit failed; no artifact is admitted for upload.' }
