param(
    [string]$VersionModule = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($VersionModule)) {
    $repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $VersionModule = Join-Path $repo 'Engine\modules\epoch.version.ixx'
}

if (-not (Test-Path -LiteralPath $VersionModule -PathType Leaf)) {
    throw "Epoch version module was not found: $VersionModule"
}

$source = Get-Content -LiteralPath $VersionModule -Raw

function Read-VersionPart {
    param(
        [Parameter(Mandatory = $true)][string]$Name
    )

    $pattern = "(?m)^#\s+define\s+$([regex]::Escape($Name))\s+([0-9]+)\s*$"
    $matches = [regex]::Matches($source, $pattern)
    if ($matches.Count -ne 1) {
        throw "Expected one numeric $Name definition in $VersionModule; found $($matches.Count)."
    }

    return [int]$matches[0].Groups[1].Value
}

$major = Read-VersionPart -Name 'EPOCH_VERSION_MAJOR_VALUE'
$minor = Read-VersionPart -Name 'EPOCH_VERSION_MINOR_VALUE'
$revision = Read-VersionPart -Name 'EPOCH_VERSION_REVISION_VALUE'

if ($major -lt 0 -or $minor -lt 0 -or $revision -lt 0) {
    throw 'Epoch version components must be non-negative.'
}

'{0}.{1}.{2:D2}' -f $major, $minor, $revision
