$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $repo 'Tools\build\epoch_package_layout.ps1')
$reports = Join-Path $repo 'Artifacts\Reports\runtime-checksums'
$scratch = Assert-EpochPackagePath -LiteralPath (Join-Path $reports ([guid]::NewGuid().ToString('N'))) -PackageRoot $reports
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$version = '0.91.13'
$edition = 'epoch.public.github.v1'
$index = Join-Path $scratch "v${version}_checksums.txt"
$names = @{ 'windows-x64' = "epoch_win10_x64_v$version.zip"; 'linux-x64' = "epoch_linux_x64_v$version.tar.gz" }
$common = @{ OutputRoot = $scratch; Edition = $edition; Version = $version }
function Set-TestArchive($platform, $content) {
    $name = $names[$platform]
    $archive = Join-Path $scratch $name
    [IO.File]::WriteAllText($archive, $content)
    $hash = (Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
    $proof = @{ artifact = $name; version = $version; edition = $edition; sha256 = $hash }
    if ($platform -eq 'windows-x64') {
        $proof += @{ schema = 'epoch.release_validation.v1'; platform = $platform; bytes = (Get-Item $archive).Length
            checks = @{version=$true; edition=$true; engine_contracts=$true; flat_archive=$true; notices=$true; clean_payload=$true} }
    } else {
        $proof += @{ schema='epoch.linux-package-validation.v1'; version_check='pass'; engine_contract='pass'; shared_libraries='pass'; archive_inventory='pass' }
    }
    [IO.File]::WriteAllText("$archive.build-validation.json", ($proof | ConvertTo-Json))
    [IO.File]::WriteAllText("$archive.sha256", "$hash  $name`n")
    "$hash  $name`n"
}
function Assert-Index([string]$expected) {
    if ([IO.File]::ReadAllText($index) -cne $expected) { throw 'Unexpected combined checksum contents.' }
}
try {
    foreach ($platform in @('windows','linux')) {
        $stager = [IO.File]::ReadAllText((Join-Path $repo "Tools/build/stage_epoch_${platform}_release.ps1"))
        $updates = [regex]::Matches($stager, '(?m)^Update-EpochRuntimeChecksums [^\r\n]+')
        $verification = if ($platform -eq 'windows') { "'.build-validation.json'" } else { "'Linux package audit failed;" }
        $verifiedAt = $stager.LastIndexOf($verification, [StringComparison]::Ordinal)
        if ($updates.Count -ne 2 -or !$updates[0].Value.EndsWith(' -Withdraw') -or
            $updates[1].Value.EndsWith(' -Withdraw') -or $verifiedAt -lt $updates[0].Index -or $verifiedAt -gt $updates[1].Index) {
            throw "Unexpected checksum admission order in the $platform stager."
        }
    }
    $windows = Set-TestArchive windows-x64 'windows one'
    $linux = Set-TestArchive linux-x64 'linux one'
    foreach ($first in @('windows-x64','linux-x64')) {
        [IO.File]::Delete($index)
        $second = if ($first -eq 'windows-x64') { 'linux-x64' } else { 'windows-x64' }
        Update-EpochRuntimeChecksums @common -Platform $first
        Assert-Index $(if ($first -eq 'windows-x64') { $windows } else { $linux })
        Update-EpochRuntimeChecksums @common -Platform $second
        Assert-Index ($windows + $linux)
    }
    [IO.File]::AppendAllText($index, ('0' * 64) + "  wrong.zip`n" + $linux)
    Update-EpochRuntimeChecksums @common -Platform windows-x64 -Withdraw
    Assert-Index $linux
    $windows = Set-TestArchive windows-x64 'replacement windows'
    $archive = Join-Path $scratch $names['windows-x64']
    $valid = [IO.File]::ReadAllText("$archive.build-validation.json")
    foreach ($bad in @($valid.Replace($edition, 'epoch.development.site.v1'), $valid.Replace($version, '0.91.14'))) {
        [IO.File]::WriteAllText("$archive.build-validation.json", $bad)
        $refused = $false
        try { Update-EpochRuntimeChecksums @common -Platform windows-x64 } catch { $refused = $true }
        if (!$refused) { throw 'Accepted a different release identity.' }
        Assert-Index $linux
    }
    [IO.File]::WriteAllText("$archive.build-validation.json", $valid)
    Update-EpochRuntimeChecksums @common -Platform windows-x64
    Update-EpochRuntimeChecksums @common -Platform windows-x64
    Assert-Index ($windows + $linux)
    [IO.File]::AppendAllText((Join-Path $scratch $names['linux-x64']), 'changed')
    Update-EpochRuntimeChecksums @common -Platform windows-x64
    Assert-Index $windows
    Update-EpochRuntimeChecksums @common -Platform windows-x64 -Withdraw
    Assert-Index ''
    'runtime_checksum_contract.result=pass'
} finally {
    Remove-EpochPackageItem -LiteralPath $scratch -PackageRoot $reports -Recurse
}
