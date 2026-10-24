param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$Version = '',
    [string]$OutputRoot = '',
    [string]$VcpkgInstalledRoot = '',
    [string]$ReleaseOutputRoot = ''
)

$ErrorActionPreference = 'Stop'

function Require-Path {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label was not found: $Path"
    }
}

function Copy-FileSet {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Get-ChildItem -LiteralPath $Source -Filter '*.dll' |
        Copy-Item -Destination $Destination -Force
}

$repo = [System.IO.Path]::GetFullPath($RepoRoot)
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
$OutputRoot = Get-EpochPackageOutputRoot -RepoRoot $repo -Edition $edition.Directory -Version $Version -OutputRoot $OutputRoot

$releaseOutput = if ([string]::IsNullOrWhiteSpace($ReleaseOutputRoot)) {
    Join-Path $repo ('build\bin\windows\{0}\x64\Release' -f $edition.Directory)
} else {
    [System.IO.Path]::GetFullPath($ReleaseOutputRoot)
}
$assets = Join-Path $repo 'Engine\assets'
$stageName = "epoch_win10_x64_v$Version"
$stage = Join-Path $OutputRoot $stageName
$zip = Join-Path $OutputRoot "$stageName.zip"
$checksumFile = Join-Path $OutputRoot "v$Version`_checksums.txt"
$verifyLogs = Join-Path $OutputRoot "verify_$stageName`_logs"
foreach ($target in @($stage, $zip, $verifyLogs, (Join-Path $OutputRoot "verify_$stageName"))) {
    $resolvedTarget = [System.IO.Path]::GetFullPath($target)
    if (-not $resolvedTarget.StartsWith($OutputRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Release staging target escapes the dedicated output directory: $resolvedTarget"
    }
}

Require-Path -Path $repo -Label 'Repo root'
Require-Path -Path $releaseOutput -Label 'Release output'
Require-Path -Path (Join-Path $releaseOutput 'EpochEditor.exe') -Label 'Release executable'
Require-Path -Path $assets -Label 'Runtime assets'
Require-Path -Path (Join-Path $repo 'README.md') -Label 'README'
Require-Path -Path (Join-Path $repo 'LICENSE') -Label 'LICENSE'
$noticeScript = Join-Path $repo 'Tools\build\collect_third_party_notices.ps1'
if ([string]::IsNullOrWhiteSpace($VcpkgInstalledRoot)) {
    $VcpkgInstalledRoot = Join-Path $repo 'Engine\vcpkg_installed\x64-windows'
    # Match the dependency layout selected by the MSBuild executable targets.
    $nestedRoot = Join-Path $VcpkgInstalledRoot 'x64-windows'
    if (-not (Test-Path -LiteralPath (Join-Path $VcpkgInstalledRoot 'include')) -and
        (Test-Path -LiteralPath (Join-Path $nestedRoot 'include'))) {
        $VcpkgInstalledRoot = $nestedRoot
    }
}
$vcpkgInstalled = [System.IO.Path]::GetFullPath($VcpkgInstalledRoot)
Require-Path -Path $noticeScript -Label 'Third-party notice collector'
Require-Path -Path $vcpkgInstalled -Label 'Windows vcpkg installed tree'
Require-Path -Path (Join-Path $vcpkgInstalled 'include') -Label 'Windows dependency headers'
Require-Path -Path (Join-Path $vcpkgInstalled 'share') -Label 'Windows dependency notices'

$crtRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC'
Require-Path -Path $crtRoot -Label 'VC redistributable root'
$crtPayload = Get-ChildItem -LiteralPath $crtRoot -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'x64\Microsoft.VC143.CRT' } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($crtPayload)) {
    throw 'No VC143 CRT payload directory was found.'
}
Require-Path -Path $crtPayload -Label 'VC143 CRT payload'

Remove-EpochPackageItem -LiteralPath $stage -PackageRoot $packageRoot -Recurse
Remove-EpochPackageItem -LiteralPath $zip -PackageRoot $packageRoot
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $releaseOutput 'EpochEditor.exe') -Destination $stage -Force
Copy-FileSet -Source $releaseOutput -Destination $stage
Copy-FileSet -Source $crtPayload -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'README.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination $stage -Force
Copy-Item -LiteralPath $assets -Destination (Join-Path $stage 'assets') -Recurse -Force
& $noticeScript -RepoRoot $repo -VcpkgInstalledRoot $vcpkgInstalled -Destination $stage

$versionOut = Join-Path $OutputRoot "epoch_release_v$Version`_windows_version_stdout.txt"
$versionErr = Join-Path $OutputRoot "epoch_release_v$Version`_windows_version_stderr.txt"
Remove-EpochPackageItem -LiteralPath $versionOut -PackageRoot $packageRoot
Remove-EpochPackageItem -LiteralPath $versionErr -PackageRoot $packageRoot
Remove-EpochPackageItem -LiteralPath $verifyLogs -PackageRoot $packageRoot -Recurse
New-Item -ItemType Directory -Path $verifyLogs -Force | Out-Null
$previousLogDir = $env:EPOCH_LOG_DIR
$env:EPOCH_LOG_DIR = $verifyLogs
try {
    $versionProcess = Start-Process `
        -FilePath (Join-Path $stage 'EpochEditor.exe') `
        -ArgumentList '--version' `
        -WorkingDirectory $stage `
        -WindowStyle Hidden `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $versionOut `
        -RedirectStandardError $versionErr
}
finally {
    $env:EPOCH_LOG_DIR = $previousLogDir
}
if ($versionProcess.ExitCode -ne 0) {
    throw "Staged Windows package version check exited with code $($versionProcess.ExitCode)."
}

$versionText = ''
if (Test-Path -LiteralPath $versionOut) {
    $versionText += Get-Content -LiteralPath $versionOut -Raw
}
if (Test-Path -LiteralPath $versionErr) {
    $versionText += Get-Content -LiteralPath $versionErr -Raw
}
Write-Host $versionText.Trim()
if ($versionText -notmatch [regex]::Escape("Epoch v$Version")) {
    throw "Staged Windows package reports the wrong version. Expected Epoch v$Version."
}

if ($versionText -notmatch ('(?:^|\s)edition=' + [regex]::Escape($edition.Id) + '(?:\s|$)')) {
    throw 'Staged binary edition does not match its physical source edition.'
}
Write-EpochRuntimeDistribution -Stage $stage -Edition $edition.Id -Version $Version

Remove-EpochPackageItem -LiteralPath $verifyLogs -PackageRoot $packageRoot -Recurse
$contractData = Join-Path $verifyLogs 'engine-contracts'
New-Item -ItemType Directory -Path $contractData -Force | Out-Null
$contractProcess = Start-Process -FilePath (Join-Path $stage 'EpochEditor.exe') `
    -ArgumentList @('--engine-contract-self-test', '--candidate-data-root', $contractData) `
    -WorkingDirectory $stage -WindowStyle Hidden -Wait -PassThru
if ($contractProcess.ExitCode -ne 0 -or
    -not (Get-ChildItem -LiteralPath $contractData -Recurse -File -Filter '*.log' |
        Select-String -SimpleMatch 'engine_contract_self_test.result=pass')) {
    throw 'Staged executable did not pass its build-safe engine contracts.'
}

Remove-EpochPackageItem -LiteralPath (Join-Path $stage 'logs') -PackageRoot $packageRoot -Recurse
Remove-EpochPackageItem -LiteralPath (Join-Path $stage 'cache') -PackageRoot $packageRoot -Recurse
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force

$verifyRoot = Join-Path $OutputRoot "verify_$stageName"
Remove-EpochPackageItem -LiteralPath $verifyRoot -PackageRoot $packageRoot -Recurse
New-Item -ItemType Directory -Path $verifyRoot -Force | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $verifyRoot -Force
Require-Path -Path (Join-Path $verifyRoot 'EpochEditor.exe') -Label 'Flat release archive executable'
Require-Path -Path (Join-Path $verifyRoot 'epoch-distribution.json') -Label 'Runtime edition marker'
$verifiedIdentity = Get-Content -LiteralPath (Join-Path $verifyRoot 'epoch-distribution.json') -Raw | ConvertFrom-Json
if ($verifiedIdentity.schema -cne 'epoch.distribution.v1' -or $verifiedIdentity.edition -cne $edition.Id -or $verifiedIdentity.version -cne $Version) {
    throw 'Runtime archive edition identity does not match its source.'
}
Require-Path -Path (Join-Path $verifyRoot 'THIRD_PARTY_NOTICES.txt') -Label 'Third-party notices'
Require-Path -Path (Join-Path $verifyRoot 'THIRD_PARTY_COMPONENTS.json') -Label 'Third-party component manifest'
if ((Test-Path -LiteralPath (Join-Path $verifyRoot 'logs')) -or (Test-Path -LiteralPath (Join-Path $verifyRoot 'cache'))) {
    throw 'Release archive must not include generated logs or runtime cache.'
}
Remove-EpochPackageItem -LiteralPath $verifyRoot -PackageRoot $packageRoot -Recurse

$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $zip
('{0}  {1}' -f $hash.Hash.ToLowerInvariant(), (Split-Path -Leaf $hash.Path)) |
    Set-Content -LiteralPath $checksumFile -Encoding ascii

Get-Item -LiteralPath $zip | Select-Object FullName, Length
Get-Content -LiteralPath $checksumFile
('{0}  {1}' -f $hash.Hash.ToLowerInvariant(), (Split-Path -Leaf $hash.Path)) |
    Set-Content -LiteralPath ($zip + '.sha256') -Encoding ascii
[ordered]@{
    schema = 'epoch.release_validation.v1'
    platform = 'windows-x64'
    edition = $edition.Id
    version = $Version
    artifact = Split-Path -Leaf $zip
    bytes = (Get-Item -LiteralPath $zip).Length
    sha256 = $hash.Hash.ToLowerInvariant()
    executable_sha256 = (Get-FileHash -LiteralPath (Join-Path $stage 'EpochEditor.exe')).Hash.ToLowerInvariant()
    checks = [ordered]@{ version = $true; edition = $true; engine_contracts = $true; flat_archive = $true; notices = $true; clean_payload = $true }
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($zip + '.build-validation.json') -Encoding utf8
Write-Host "package_inventory.result=pass artifact=$stageName.zip sha256=$($hash.Hash.ToLowerInvariant())"
