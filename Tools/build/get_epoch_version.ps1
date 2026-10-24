param(
    [string]$VersionModule = '',
    [ValidateSet('source', 'windows-x64', 'linux-x64', 'macos-arm64')]
    [string]$Platform = 'source',
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

function Select-TrackedEditionDefaults {
    param([string]$Source, [ValidateSet(0, 1)][int]$Edition = 0)

    # This is deliberately not a general preprocessor. Admit only the exact
    # platform override/public-source/development-pin block emitted by Epoch.
    # Duplicate selected definitions still reach Read-TrackedVersionToken and
    # fail its existing ambiguity check.
    $pattern = @'
(?m)^#if defined\((?<override>EPOCH_OVERRIDE_(?<platform>WINDOWS|LINUX|MACOS)_PACKAGED_VERSION_(?<part>MAJOR|MINOR|REVISION))\)\r?\n#  define (?<name>EPOCH_\k<platform>_PACKAGED_VERSION_\k<part>_VALUE) \k<override>\r?\n#elif EPOCH_EDITION_PUBLIC\r?\n#  define \k<name> (?<public>EPOCH_VERSION_\k<part>_VALUE)\r?\n#else\r?\n#  define \k<name> (?<development>[0-9]+)\r?\n#endif\r?$
'@
    $selected = [regex]::Replace($Source, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        $token = if ($Edition -eq 1) { $match.Groups['public'].Value } else { $match.Groups['development'].Value }
        return '#  define ' + $match.Groups['name'].Value + ' ' + $token
    })
    if ($selected -cmatch '(?m)^#\s*(?:if|elif)\b[^\r\n]*\bEPOCH_EDITION_PUBLIC\b') {
        throw 'Unsupported or malformed tracked edition conditional.'
    }
    return $selected
}

function Read-TrackedEdition {
    param([string]$ModulePath, [string]$Source)

    $engineRoot = Split-Path -Parent (Split-Path -Parent ([System.IO.Path]::GetFullPath($ModulePath)))
    $markerPath = Join-Path $engineRoot 'include\epoch.edition.hpp'
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        # Immutable legacy modules predate editions. New conditional modules
        # cannot guess their identity when the physical source marker is absent.
        if ($Source -cmatch '\bEPOCH_EDITION_PUBLIC\b') {
            throw "Edition-aware version module has no physical source marker: $markerPath"
        }
        return 0
    }
    $marker = Get-Content -LiteralPath $markerPath -Raw
    $definitions = [regex]::Matches($marker, '(?m)^\s*#\s*define\s+EPOCH_SOURCE_EDITION_PUBLIC\b[^\r\n]*')
    if ($definitions.Count -ne 1 -or $definitions[0].Value -cnotmatch '^#define EPOCH_SOURCE_EDITION_PUBLIC ([01])$') {
        throw "Invalid physical source edition marker: $markerPath"
    }
    return [int]$Matches[1]
}

function Read-TrackedVersionToken {
    param([string]$Source, [string]$Name)

    $pattern = "(?m)^#\s*define\s+$([regex]::Escape($Name))\s+([A-Za-z_0-9]+)\s*$"
    $override = ($Name -creplace '^EPOCH_', 'EPOCH_OVERRIDE_') -creplace '_VALUE$', ''
    $definitions = @([regex]::Matches($Source, $pattern) |
        Where-Object { $_.Groups[1].Value -cne $override })
    if ($definitions.Count -ne 1) {
        throw "Expected one tracked $Name definition; found $($definitions.Count)."
    }
    return $definitions[0].Groups[1].Value
}

function Format-TrackedVersion {
    param([int]$Major, [int]$Minor, [int]$Revision)

    if ($Major -lt 0 -or $Minor -lt 0 -or $Revision -lt 0) {
        throw 'Epoch version components must be non-negative.'
    }
    # Preserve immutable historical filenames; new feature lines use semver.
    if ($Major -eq 0 -and $Minor -le 90) {
        return '{0}.{1}.{2:D2}' -f $Major, $Minor, $Revision
    }
    return '{0}.{1}.{2}' -f $Major, $Minor, $Revision
}

function Resolve-TrackedVersion {
    param([string]$Source, [string]$RequestedPlatform, [ValidateSet(0, 1)][int]$Edition = 0)

    $Source = Select-TrackedEditionDefaults -Source $Source -Edition $Edition

    $sourceParts = @{}
    foreach ($part in @('MAJOR', 'MINOR', 'REVISION')) {
        $name = 'EPOCH_VERSION_' + $part + '_VALUE'
        $token = Read-TrackedVersionToken -Source $Source -Name $name
        if ($token -cnotmatch '^[0-9]+$') {
            throw "$name must have one numeric tracked definition."
        }
        $sourceParts[$name] = [int]$token
    }

    $prefix = switch ($RequestedPlatform) {
        'source' { 'EPOCH_VERSION' }
        'windows-x64' { 'EPOCH_WINDOWS_PACKAGED_VERSION' }
        'linux-x64' { 'EPOCH_LINUX_PACKAGED_VERSION' }
        'macos-arm64' { 'EPOCH_MACOS_PACKAGED_VERSION' }
        default { throw "Unsupported platform authority: $RequestedPlatform" }
    }
    $parts = @{}
    foreach ($part in @('MAJOR', 'MINOR', 'REVISION')) {
        $name = $prefix + '_' + $part + '_VALUE'
        $token = Read-TrackedVersionToken -Source $Source -Name $name
        if ($token -cmatch '^[0-9]+$') {
            $parts[$part] = [int]$token
        }
        elseif ($token -ceq ('EPOCH_VERSION_' + $part + '_VALUE')) {
            $parts[$part] = $sourceParts[$token]
        }
        else {
            throw "Unsupported tracked alias for $name : $token"
        }
    }
    return Format-TrackedVersion -Major $parts.MAJOR -Minor $parts.MINOR -Revision $parts.REVISION
}

function Invoke-VersionSelfTest {
    $fixture = @'
#  define EPOCH_VERSION_MAJOR_VALUE EPOCH_OVERRIDE_VERSION_MAJOR
#  define EPOCH_VERSION_MAJOR_VALUE 0
#  define EPOCH_VERSION_MINOR_VALUE 90
#  define EPOCH_VERSION_REVISION_VALUE 3
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#  define EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE 3
#  define EPOCH_LINUX_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#  define EPOCH_LINUX_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#  define EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE 3
#  define EPOCH_MACOS_PACKAGED_VERSION_MAJOR_VALUE 0
#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89
#  define EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE 30
'@
    foreach ($name in @('source', 'windows-x64', 'linux-x64', 'macos-arm64')) {
        $expected = if ($name -eq 'macos-arm64') { '0.89.30' } else { '0.90.03' }
        if ((Resolve-TrackedVersion $fixture $name) -cne $expected) {
            throw "Version authority self-test failed: $name"
        }
    }
    foreach ($case in @(
        @(0, 89, 6, '0.89.06'), @(0, 89, 35, '0.89.35'),
        @(0, 90, 3, '0.90.03'), @(0, 90, 101, '0.90.101'), @(1, 0, 0, '1.0.0')
    )) {
        if ((Format-TrackedVersion $case[0] $case[1] $case[2]) -cne $case[3]) {
            throw 'Historical/canonical version spelling self-test failed.'
        }
    }
    # Platform pins remain independent of source major/minor, not only revision.
    $future = $fixture.Replace('EPOCH_VERSION_MAJOR_VALUE 0', 'EPOCH_VERSION_MAJOR_VALUE 1')
    if ((Resolve-TrackedVersion $future 'macos-arm64') -cne '0.89.30' -or
        (Resolve-TrackedVersion $future 'linux-x64') -cne '1.90.3') {
        throw 'Independent packaged authority self-test failed.'
    }
    $newline = [Environment]::NewLine
    foreach ($invalid in @(
        ($fixture + $newline + '# define EPOCH_VERSION_MINOR_VALUE 91'),
        $fixture.Replace('EPOCH_VERSION_REVISION_VALUE 3', 'EPOCH_VERSION_REVISION_VALUE bogus'),
        $fixture.Replace('EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89', 'EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MAJOR_VALUE'),
        $fixture.Replace('#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89', '')
    )) {
        $refused = $false
        try { $null = Resolve-TrackedVersion $invalid 'macos-arm64' }
        catch { $refused = $true }
        if (-not $refused) { throw 'Malformed tracked authority was accepted.' }
    }
    $editionFixture = $fixture.Replace('EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE 3', 'EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE 55').Replace('EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE 3', 'EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE 55')
    foreach ($entry in @(
        @('WINDOWS', 'REVISION', '55'), @('LINUX', 'REVISION', '55'),
        @('MACOS', 'MAJOR', '0'), @('MACOS', 'MINOR', '89'), @('MACOS', 'REVISION', '30')
    )) {
        $name = 'EPOCH_' + $entry[0] + '_PACKAGED_VERSION_' + $entry[1] + '_VALUE'
        $override = 'EPOCH_OVERRIDE_' + $entry[0] + '_PACKAGED_VERSION_' + $entry[1]
        $block = @(
            "#if defined($override)", "#  define $name $override",
            '#elif EPOCH_EDITION_PUBLIC', "#  define $name EPOCH_VERSION_$($entry[1])_VALUE",
            '#else', "#  define $name $($entry[2])", '#endif'
        ) -join "`n"
        $editionFixture = $editionFixture.Replace("#  define $name $($entry[2])", $block)
    }
    foreach ($name in @('source', 'windows-x64', 'linux-x64', 'macos-arm64')) {
        $expectedDev = if ($name -ceq 'source') { '0.90.03' } elseif ($name -ceq 'macos-arm64') { '0.89.30' } else { '0.90.55' }
        if ((Resolve-TrackedVersion $editionFixture $name 0) -cne $expectedDev -or
            (Resolve-TrackedVersion $editionFixture $name 1) -cne '0.90.03') {
            throw "Physical edition default self-test failed: $name"
        }
    }
    foreach ($invalid in @(
        ($editionFixture + $newline + $block),
        $editionFixture.Replace('#elif EPOCH_EDITION_PUBLIC', '#elif EPOCH_EDITION_PUBLIC || 1'),
        $editionFixture.Replace('EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE', 'EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MAJOR_VALUE'),
        $editionFixture.Replace('#if defined(EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MINOR)', '#if UNKNOWN_CONDITION')
    )) {
        $refused = $false
        try { $null = Resolve-TrackedVersion $invalid 'macos-arm64' 1 }
        catch { $refused = $true }
        if (-not $refused) { throw 'Malformed or ambiguous edition defaults were accepted.' }
    }
    Write-Output 'Epoch tracked version self-test passed.'
}

if ($SelfTest) {
    Invoke-VersionSelfTest
    return
}
if ([string]::IsNullOrWhiteSpace($VersionModule)) {
    $repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $VersionModule = Join-Path $repo 'Engine\modules\epoch.version.ixx'
}
if (-not (Test-Path -LiteralPath $VersionModule -PathType Leaf)) {
    throw "Epoch version module was not found: $VersionModule"
}
$source = Get-Content -LiteralPath $VersionModule -Raw
$edition = Read-TrackedEdition -ModulePath $VersionModule -Source $source
Resolve-TrackedVersion -Source $source -RequestedPlatform $Platform -Edition $edition
