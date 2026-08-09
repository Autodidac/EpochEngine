param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$Version = '',
    [string]$OutputRoot = ''
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

$releaseOutput = Join-Path $repo 'x64\Release'
$assets = Join-Path $repo 'Engine\assets'
$stageName = "epoch_win10_x64_v$Version"
$stage = Join-Path $OutputRoot $stageName
$zip = Join-Path $OutputRoot "$stageName.zip"
$checksumFile = Join-Path $OutputRoot "v$Version`_checksums.txt"
$verifyLogs = Join-Path $OutputRoot "verify_$stageName`_logs"

Require-Path -Path $repo -Label 'Repo root'
Require-Path -Path $releaseOutput -Label 'Release output'
Require-Path -Path (Join-Path $releaseOutput 'EpochEditor.exe') -Label 'Release executable'
Require-Path -Path $assets -Label 'Runtime assets'
Require-Path -Path (Join-Path $repo 'README.md') -Label 'README'
Require-Path -Path (Join-Path $repo 'LICENSE') -Label 'LICENSE'
$noticeScript = Join-Path $repo 'Tools\ai\collect_third_party_notices.ps1'
$vcpkgInstalled = Join-Path $repo 'Engine\vcpkg_installed\x64-windows'
Require-Path -Path $noticeScript -Label 'Third-party notice collector'
Require-Path -Path $vcpkgInstalled -Label 'Windows vcpkg installed tree'

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

Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
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
Remove-Item -LiteralPath $versionOut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $versionErr -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $verifyLogs -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $verifyLogs -Force | Out-Null
$previousLogDir = $env:EPOCH_LOG_DIR
$env:EPOCH_LOG_DIR = $verifyLogs
try {
    $versionProcess = Start-Process `
        -FilePath (Join-Path $stage 'EpochEditor.exe') `
        -ArgumentList '--version' `
        -WorkingDirectory $stage `
        -NoNewWindow `
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

Remove-Item -LiteralPath $verifyLogs -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $stage 'logs') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $stage 'cache') -Recurse -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force

$verifyRoot = Join-Path $OutputRoot "verify_$stageName"
Remove-Item -LiteralPath $verifyRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $verifyRoot -Force | Out-Null
Expand-Archive -LiteralPath $zip -DestinationPath $verifyRoot -Force
Require-Path -Path (Join-Path $verifyRoot 'EpochEditor.exe') -Label 'Flat release archive executable'
Require-Path -Path (Join-Path $verifyRoot 'THIRD_PARTY_NOTICES.txt') -Label 'Third-party notices'
Require-Path -Path (Join-Path $verifyRoot 'THIRD_PARTY_COMPONENTS.json') -Label 'Third-party component manifest'
if ((Test-Path -LiteralPath (Join-Path $verifyRoot 'logs')) -or (Test-Path -LiteralPath (Join-Path $verifyRoot 'cache'))) {
    throw 'Release archive must not include generated logs or runtime cache.'
}
Remove-Item -LiteralPath $verifyRoot -Recurse -Force -ErrorAction SilentlyContinue

$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $zip
('{0}  {1}' -f $hash.Hash.ToLowerInvariant(), (Split-Path -Leaf $hash.Path)) |
    Set-Content -LiteralPath $checksumFile -Encoding ascii

Get-Item -LiteralPath $zip | Select-Object FullName, Length
Get-Content -LiteralPath $checksumFile
