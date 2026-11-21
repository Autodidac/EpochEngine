param([string]$Report, [string]$OwnerScript, [string]$Repo, [string]$ChannelFixtures)
$ErrorActionPreference = 'Stop'
$ownerText = [IO.File]::ReadAllText($OwnerScript)
$digestText = [IO.File]::ReadAllText((Join-Path $Report 'digest.ps1'))
foreach ($text in @($ownerText, $digestText)) {
    $parseErrors = $null
    [void][Management.Automation.Language.Parser]::ParseInput($text, [ref]$null, [ref]$parseErrors)
    if ($parseErrors.Count) { throw $parseErrors[0] }
}
function Write-Step([string]$Level, [string]$Text) {}
function New-SourceTree([string]$Root, [string]$Edition = '1') {
    foreach ($dir in @('Engine/include', 'Engine/modules', 'Tools/build')) {
        New-Item -ItemType Directory -Path (Join-Path $Root $dir) -Force | Out-Null
    }
    [IO.File]::WriteAllText((Join-Path $Root 'Engine.sln'), 'fixture')
    [IO.File]::WriteAllText((Join-Path $Root 'Engine/vcpkg.json'), '{}')
    [IO.File]::WriteAllText((Join-Path $Root 'source-projection.json'), '{}')
    [IO.File]::WriteAllText((Join-Path $Root 'Engine/include/epoch.edition.hpp'), "#define EPOCH_SOURCE_EDITION_PUBLIC $Edition`n")
    [IO.File]::WriteAllText((Join-Path $Root 'Engine/modules/epoch.version.ixx'), "#define EPOCH_VERSION_MAJOR_VALUE 0`n#define EPOCH_VERSION_MINOR_VALUE 90`n#define EPOCH_VERSION_REVISION_VALUE 185`n")
    Copy-Item -LiteralPath (Join-Path $Repo 'Tools/build/get_epoch_version.ps1') -Destination (Join-Path $Root 'Tools/build/get_epoch_version.ps1')
}
foreach ($case in @('flat', 'wrapped', 'ambiguous', 'foreign', 'version', 'projection', 'private', 'tampered', 'missing-digest', 'development', 'owner-runtime')) {
    $homePath = Join-Path $Report ('powershell-' + $case)
    $tree = Join-Path $homePath 'tree'
    $stagingDir = Join-Path $homePath 'staging'
    $sourceRoot = Join-Path $homePath 'admitted'
    $sourceArchive = Join-Path $homePath 'source.zip'
    $expectedEdition = '1'
    $expectedSourceVersion = '0.90.185'
    $wrapped = $case -in @('wrapped', 'ambiguous')
    $root = if ($wrapped) { Join-Path $tree 'EpochEngine-tag' } else { $tree }
    New-SourceTree $root
    switch ($case) {
        'ambiguous' { New-SourceTree (Join-Path $tree 'other') }
        'foreign' { [IO.File]::WriteAllText((Join-Path $root 'Engine/include/epoch.edition.hpp'), "#define EPOCH_SOURCE_EDITION_PUBLIC 0`n") }
        'version' { $expectedSourceVersion = '0.90.186' }
        'projection' { Remove-Item -LiteralPath (Join-Path $root 'source-projection.json') }
        'private' { New-Item -ItemType Directory -Path (Join-Path $root 'Engine/dep/EpochSelfCoding') -Force | Out-Null }
        'development' {
            $expectedEdition = '0'
            [IO.File]::WriteAllText((Join-Path $root 'Engine/include/epoch.edition.hpp'), "#define EPOCH_SOURCE_EDITION_PUBLIC 0`n")
        }
    }
    if ($case -eq 'owner-runtime') {
        $expectedEdition = '0'
        [IO.File]::WriteAllLines((Join-Path $root 'Engine/include/epoch.edition.hpp'),
            [string[]]@('#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_SELF_CODING 0'))
    }
    Compress-Archive -Path (Join-Path $tree '*') -DestinationPath $sourceArchive
    $expectedSourceSha = (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($case -eq 'tampered') { $expectedSourceSha = 'a' * 64 }
    if ($case -eq 'missing-digest') { $expectedSourceSha = '' }
    # A conflicting unsigned companion cannot authorize the wrong digest.
    [IO.File]::WriteAllText(($sourceArchive + '.sha256'), (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant())
    $succeeded = $false
    try {
        & ([scriptblock]::Create($digestText))
        Expand-Archive -LiteralPath $sourceArchive -DestinationPath $stagingDir
        & ([scriptblock]::Create($ownerText))
        $succeeded = $true
    } catch {
        if ($case -in @('flat', 'wrapped', 'development')) { throw }
    }
    $expected = $case -in @('flat', 'wrapped', 'development')
    if ($succeeded -ne $expected) { throw "Incorrect source admission: $case" }
    if ($expected -and -not (Test-Path -LiteralPath (Join-Path $sourceRoot 'Engine.sln'))) { throw 'Incomplete source moved.' }
    if (-not $expected -and (Test-Path -LiteralPath $sourceRoot)) { throw 'Refused source was published.' }
}
foreach ($archiveEdition in @('public', 'development')) {
    foreach ($archiveVersion in @('0.91.13', '0.91.14')) {
        $sourceArchive = Join-Path $ChannelFixtures "$archiveEdition-source-$archiveVersion.zip"
        $expectedSourceSha = (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant()
        foreach ($edition in @('public', 'development')) {
            $expectedEdition = if ($edition -eq 'public') { '1' } else { '0' }
            foreach ($case in @('match', 'wrong-version', 'missing-version')) {
                $homePath = Join-Path $Report "route-$archiveEdition-$archiveVersion-$edition-$case"
                $stagingDir = Join-Path $homePath 'staging'
                $sourceRoot = Join-Path $homePath 'admitted'
                $expected = ($edition -eq $archiveEdition) -and ($case -eq 'match')
                $expectedSourceVersion = switch ($case) {
                    'match' { $archiveVersion }
                    'wrong-version' { if ($archiveVersion -eq '0.91.13') { '0.91.14' } else { '0.91.13' } }
                    'missing-version' { '' }
                }
                $succeeded = $false
                try {
                    & ([scriptblock]::Create($digestText))
                    Expand-Archive -LiteralPath $sourceArchive -DestinationPath $stagingDir
                    & ([scriptblock]::Create($ownerText))
                    $succeeded = $true
                } catch {
                    if ($expected) { throw }
                }
                if ($succeeded -ne $expected) { throw "Incorrect ZIP admission: $archiveEdition $archiveVersion as $edition $case" }
                if ($succeeded) {
                    $versionModule = Join-Path $sourceRoot 'Engine/modules/epoch.version.ixx'
                    $fixtureModule = Join-Path $ChannelFixtures "$archiveEdition-source-$archiveVersion/Engine/modules/epoch.version.ixx"
                    if ((Get-FileHash -LiteralPath $versionModule).Hash -cne (Get-FileHash -LiteralPath $fixtureModule).Hash) {
                        throw 'Authenticated source version changed during ZIP readback.'
                    }
                } elseif (Test-Path -LiteralPath $sourceRoot) {
                    throw 'Refused ZIP was published.'
                }
            }
        }
    }
}
Write-Output 'source_worker.contract=pass (Public and Development 0.91.13/0.91.14 ZIP readback, edition and version binding; no network/build)'
