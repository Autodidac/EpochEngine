param(
    [string]$VersionModule = '',
    [ValidateSet('source', 'windows-x64', 'linux-x64', 'macos-arm64')]
    [string]$Platform = 'source',
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

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
    if ($Major -eq 0 -and $Minor -le 89) {
        return '{0}.{1}.{2:D2}' -f $Major, $Minor, $Revision
    }
    return '{0}.{1}.{2}' -f $Major, $Minor, $Revision
}

function Resolve-TrackedVersion {
    param([string]$Source, [string]$RequestedPlatform)

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
#  define EPOCH_VERSION_REVISION_VALUE 1
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#  define EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE 1
#  define EPOCH_LINUX_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#  define EPOCH_LINUX_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#  define EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE 1
#  define EPOCH_MACOS_PACKAGED_VERSION_MAJOR_VALUE 0
#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89
#  define EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE 30
'@
    foreach ($name in @('source', 'windows-x64', 'linux-x64', 'macos-arm64')) {
        $expected = if ($name -eq 'macos-arm64') { '0.89.30' } else { '0.90.1' }
        if ((Resolve-TrackedVersion $fixture $name) -cne $expected) {
            throw "Version authority self-test failed: $name"
        }
    }
    foreach ($case in @(
        @(0, 89, 6, '0.89.06'), @(0, 89, 35, '0.89.35'),
        @(0, 90, 1, '0.90.1'), @(0, 90, 100, '0.90.100'), @(1, 0, 0, '1.0.0')
    )) {
        if ((Format-TrackedVersion $case[0] $case[1] $case[2]) -cne $case[3]) {
            throw 'Historical/canonical version spelling self-test failed.'
        }
    }
    # Platform pins remain independent of source major/minor, not only revision.
    $future = $fixture.Replace('EPOCH_VERSION_MAJOR_VALUE 0', 'EPOCH_VERSION_MAJOR_VALUE 1')
    if ((Resolve-TrackedVersion $future 'macos-arm64') -cne '0.89.30' -or
        (Resolve-TrackedVersion $future 'linux-x64') -cne '1.90.1') {
        throw 'Independent packaged authority self-test failed.'
    }
    $newline = [Environment]::NewLine
    foreach ($invalid in @(
        ($fixture + $newline + '# define EPOCH_VERSION_MINOR_VALUE 91'),
        $fixture.Replace('EPOCH_VERSION_REVISION_VALUE 1', 'EPOCH_VERSION_REVISION_VALUE bogus'),
        $fixture.Replace('EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89', 'EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MAJOR_VALUE'),
        $fixture.Replace('#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE 89', '')
    )) {
        $refused = $false
        try { $null = Resolve-TrackedVersion $invalid 'macos-arm64' }
        catch { $refused = $true }
        if (-not $refused) { throw 'Malformed tracked authority was accepted.' }
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
Resolve-TrackedVersion -Source (Get-Content -LiteralPath $VersionModule -Raw) -RequestedPlatform $Platform
