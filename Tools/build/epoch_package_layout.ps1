# Shared runtime packaging identity and checkout-local deletion boundary.
function Get-EpochSourceEdition {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    $markerPath = Join-Path $RepoRoot 'Engine\include\epoch.edition.hpp'
    $marker = [System.IO.File]::ReadAllText($markerPath)
    $definitions = [regex]::Matches($marker, '(?m)^\s*#\s*define\s+EPOCH_SOURCE_EDITION_PUBLIC\b[^\r\n]*')
    if ($definitions.Count -ne 1 -or $definitions[0].Value -notmatch '^#define EPOCH_SOURCE_EDITION_PUBLIC ([01])$') {
        throw "Invalid physical source edition marker: $markerPath"
    }
    $isPublic = $Matches[1] -ceq '1'
    [pscustomobject]@{
        Id = $(if ($isPublic) { 'epoch.public.github.v1' } else { 'epoch.development.site.v1' })
        Directory = $(if ($isPublic) { 'public' } else { 'development' })
    }
}

function Assert-EpochPackagePath {
    param(
        [Parameter(Mandatory = $true)][string]$LiteralPath,
        [Parameter(Mandatory = $true)][string]$PackageRoot
    )
    $root = [System.IO.Path]::GetFullPath($PackageRoot).TrimEnd('\', '/')
    $target = [System.IO.Path]::GetFullPath($LiteralPath).TrimEnd('\', '/')
    if (-not $target.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package target is outside checkout build/packages: $target"
    }
    # Lexical containment alone is insufficient when an existing directory is a
    # junction/symlink. Inspect every existing component before any mutation.
    $current = $target
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                throw "Package target crosses a reparse point: $current"
            }
        }
        $current = [System.IO.Path]::GetDirectoryName($current)
    }
    return $target
}

function Get-EpochPackageOutputRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Edition,
        [Parameter(Mandatory = $true)][string]$Version,
        [string]$OutputRoot = ''
    )
    if ($Version -notmatch '^\d+\.\d+\.\d+$' -or $Edition -notin @('public', 'development')) {
        throw 'Invalid package version or edition.'
    }
    $packageRoot = Join-Path $RepoRoot 'build\packages'
    $editionRoot = Join-Path $packageRoot $Edition
    if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
        $OutputRoot = Join-Path $editionRoot "v$Version"
    }
    # A caller cannot put public and private payloads in the same output tree.
    return Assert-EpochPackagePath -LiteralPath $OutputRoot -PackageRoot $editionRoot
}

function Remove-EpochPackageItem {
    param(
        [Parameter(Mandatory = $true)][string]$LiteralPath,
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [switch]$Recurse
    )
    $target = Assert-EpochPackagePath -LiteralPath $LiteralPath -PackageRoot $PackageRoot
    Remove-Item -LiteralPath $target -Recurse:$Recurse -Force -ErrorAction SilentlyContinue
}

function Write-EpochRuntimeDistribution {
    param(
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)][string]$Edition,
        [Parameter(Mandatory = $true)][string]$Version
    )
    $identity = [ordered]@{ schema = 'epoch.distribution.v1'; edition = $Edition; version = $Version }
    [System.IO.File]::WriteAllText((Join-Path $Stage 'epoch-distribution.json'),
        ($identity | ConvertTo-Json -Compress), [System.Text.UTF8Encoding]::new($false))
}
