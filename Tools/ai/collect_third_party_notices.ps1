param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$VcpkgInstalledRoot,

    [Parameter(Mandatory = $true)]
    [string]$Destination
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

function Safe-ComponentName {
    param([Parameter(Mandatory = $true)][string]$Name)
    return ($Name -replace '[^A-Za-z0-9._-]', '_')
}

function Read-SpdxMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$FallbackName
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return [pscustomobject]@{
            Name = $FallbackName
            Version = 'unknown'
            Source = 'vcpkg registry'
            License = 'See preserved copyright file'
        }
    }

    $spdx = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $port = $spdx.packages |
        Where-Object { $_.SPDXID -eq 'SPDXRef-port' } |
        Select-Object -First 1
    if ($null -eq $port) {
        $port = $spdx.packages | Select-Object -First 1
    }

    return [pscustomobject]@{
        Name = if ($port.name) { [string]$port.name } else { $FallbackName }
        Version = if ($port.versionInfo) { [string]$port.versionInfo } else { 'unknown' }
        Source = if ($port.downloadLocation) { [string]$port.downloadLocation } else { 'vcpkg registry' }
        License = if ($port.licenseConcluded) { [string]$port.licenseConcluded } else { 'See preserved copyright file' }
    }
}

$repo = [System.IO.Path]::GetFullPath($RepoRoot)
$installed = [System.IO.Path]::GetFullPath($VcpkgInstalledRoot)
$destinationRoot = [System.IO.Path]::GetFullPath($Destination)
$share = Join-Path $installed 'share'
$licenseRoot = Join-Path $destinationRoot 'licenses'

Require-Path -Path $repo -Label 'Repository root'
Require-Path -Path $share -Label 'Resolved vcpkg share directory'

New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
Remove-Item -LiteralPath $licenseRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null

$notice = [System.Collections.Generic.List[string]]::new()
$components = [System.Collections.Generic.List[object]]::new()
$notice.Add('Epoch third-party notices')
$notice.Add('=========================')
$notice.Add('')
$notice.Add('Epoch itself is licensed separately in LICENSE. The components below retain their original licenses.')
$notice.Add('')

$copyrightFiles = Get-ChildItem -LiteralPath $share -Directory |
    ForEach-Object {
        $candidate = Join-Path $_.FullName 'copyright'
        if (Test-Path -LiteralPath $candidate) {
            Get-Item -LiteralPath $candidate
        }
    } |
    Sort-Object DirectoryName

if (-not $copyrightFiles) {
    throw "No vcpkg copyright files were found under $share"
}

foreach ($copyright in $copyrightFiles) {
    $portDirectory = Split-Path -Parent $copyright.FullName
    $fallbackName = Split-Path -Leaf $portDirectory
    $metadata = Read-SpdxMetadata -Path (Join-Path $portDirectory 'vcpkg.spdx.json') -FallbackName $fallbackName
    $safeName = Safe-ComponentName -Name $metadata.Name
    $preservedName = "$safeName.txt"
    $preservedPath = Join-Path $licenseRoot $preservedName
    Copy-Item -LiteralPath $copyright.FullName -Destination $preservedPath -Force
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $preservedPath).Hash.ToLowerInvariant()

    $notice.Add("Component: $($metadata.Name)")
    $notice.Add("Version: $($metadata.Version)")
    $notice.Add("License: $($metadata.License)")
    $notice.Add("Source: $($metadata.Source)")
    $notice.Add("Preserved license: licenses/$preservedName")
    $notice.Add("SHA-256: $hash")
    $notice.Add('')
    $notice.Add((Get-Content -LiteralPath $preservedPath -Raw).TrimEnd())
    $notice.Add('')
    $notice.Add(('=' * 78))
    $notice.Add('')

    $components.Add([pscustomobject]@{
        name = $metadata.Name
        version = $metadata.Version
        license = $metadata.License
        source = $metadata.Source
        notice = "licenses/$preservedName"
        sha256 = $hash
        origin = 'vcpkg'
    })
}

$bundled = @(
    [pscustomobject]@{
        Name = 'Epoch bundled GLAD loader'
        Version = 'repository source'
        License = 'Multiple permissive notices; see preserved file'
        Source = 'https://github.com/Dav1dde/glad'
        Path = Join-Path $repo 'Engine\third_party\glad\LICENSE'
        FileName = 'epoch-bundled-glad.txt'
    },
    [pscustomobject]@{
        Name = 'EpochGui'
        Version = 'repository source'
        License = 'See preserved file'
        Source = 'https://github.com/Autodidac/EpochGui'
        Path = Join-Path $repo 'Engine\dep\EpochGui\LICENSE'
        FileName = 'epochgui.txt'
    }
)

foreach ($component in $bundled) {
    Require-Path -Path $component.Path -Label "$($component.Name) license"
    $preservedPath = Join-Path $licenseRoot $component.FileName
    Copy-Item -LiteralPath $component.Path -Destination $preservedPath -Force
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $preservedPath).Hash.ToLowerInvariant()

    $notice.Add("Component: $($component.Name)")
    $notice.Add("Version: $($component.Version)")
    $notice.Add("License: $($component.License)")
    $notice.Add("Source: $($component.Source)")
    $notice.Add("Preserved license: licenses/$($component.FileName)")
    $notice.Add("SHA-256: $hash")
    $notice.Add('')
    $notice.Add((Get-Content -LiteralPath $preservedPath -Raw).TrimEnd())
    $notice.Add('')
    $notice.Add(('=' * 78))
    $notice.Add('')

    $components.Add([pscustomobject]@{
        name = $component.Name
        version = $component.Version
        license = $component.License
        source = $component.Source
        notice = "licenses/$($component.FileName)"
        sha256 = $hash
        origin = 'bundled source'
    })
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllLines(
    (Join-Path $destinationRoot 'THIRD_PARTY_NOTICES.txt'),
    $notice,
    $utf8NoBom)
[System.IO.File]::WriteAllText(
    (Join-Path $destinationRoot 'THIRD_PARTY_COMPONENTS.json'),
    ($components | ConvertTo-Json -Depth 5),
    $utf8NoBom)

Get-Item -LiteralPath `
    (Join-Path $destinationRoot 'THIRD_PARTY_NOTICES.txt'), `
    (Join-Path $destinationRoot 'THIRD_PARTY_COMPONENTS.json') |
    Select-Object FullName, Length
