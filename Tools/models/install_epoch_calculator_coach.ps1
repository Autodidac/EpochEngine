#Requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$RuntimeExecutable,
    [Parameter(Mandatory)][string]$ModelFile
)

$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'The pinned Calculator Coach helper targets Windows.' }

$runtimeName = 'epoch-llama-cpu.exe'
$runtimeBytes = 5010944L
$runtimeHash = 'C8A52AD44097A68C057676A23D57FD9C2C08AC3E0DE97E6A79C0FDAC0CA94BAE'
$modelName = 'Qwen3-0.6B-Q4_0.gguf'
$modelBytes = 428970080L
$modelHash = 'DA2572F16C06133561CE56ACCAA822216F2391EF4D37FBA427801CD6736417D4'

function Get-OrdinaryItem([string]$Path, [bool]$Directory, [switch]$AllowMissing) {
    try { $item = Get-Item -LiteralPath $Path -Force }
    catch [System.Management.Automation.ItemNotFoundException] {
        if ($AllowMissing) { return $null }
        throw
    }
    if ($item.PSIsContainer -ne $Directory -or $item.FullName -ine $Path -or $item.LinkType `
        -or ($item.Attributes -band ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::Device))) {
        throw "Refusing redirected or unexpected filesystem item: $Path"
    }
    return $item
}

function Resolve-OrdinaryLocalPath([string]$Path, [bool]$Directory) {
    $location = Get-Location
    if ($location.Provider.Name -ne 'FileSystem') { throw 'Set a filesystem working directory first.' }
    $full = [IO.Path]::GetFullPath($Path, $location.ProviderPath)
    if ($full -cnotmatch '^[A-Za-z]:\\' -or $full.Substring(3).Contains(':')) {
        throw 'Select an ordinary local Windows path.'
    }
    $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($full))
    if ($drive.DriveType -eq [IO.DriveType]::Network) { throw 'Select a local drive.' }
    $parts = [Collections.Generic.Stack[string]]::new()
    $full = [IO.Path]::TrimEndingDirectorySeparator($full)
    $parent = [IO.Path]::GetDirectoryName($full)
    while ($parent) {
        $parts.Push($parent)
        $parent = [IO.Path]::GetDirectoryName($parent)
    }
    foreach ($ancestor in $parts) { $null = Get-OrdinaryItem $ancestor $true }
    return Get-OrdinaryItem $full $Directory
}

function Open-PinnedFile([string]$Path, [long]$Bytes, [string]$Hash) {
    $item = Resolve-OrdinaryLocalPath $Path $false
    $stream = [IO.File]::Open($item.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        if ($stream.Length -ne $Bytes -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)) -cne $Hash) {
            throw "The pinned file identity differs: $($item.FullName)"
        }
        $stream.Position = 0
        return $stream
    } catch { $stream.Dispose(); throw }
}

function Confirm-Inventory([string]$Path, [string[]]$Names) {
    $null = Resolve-OrdinaryLocalPath $Path $true
    $children = @(Get-ChildItem -LiteralPath $Path -Force)
    if ($children.Count -ne $Names.Count -or @($children | Where-Object { $_.Name -cnotin $Names }).Count) {
        throw "The Calculator Coach directory contains unexpected or incomplete contents: $Path"
    }
}

function Confirm-Installation([string]$Path) {
    Confirm-Inventory $Path @('bin', 'model', 'requests')
    Confirm-Inventory (Join-Path $Path 'bin') @($runtimeName)
    Confirm-Inventory (Join-Path $Path 'model') @($modelName)
    Confirm-Inventory (Join-Path $Path 'requests') @()
    (Open-PinnedFile (Join-Path $Path "bin/$runtimeName") $runtimeBytes $runtimeHash).Dispose()
    (Open-PinnedFile (Join-Path $Path "model/$modelName") $modelBytes $modelHash).Dispose()
}

function New-OrdinaryDirectory([string]$Path) {
    $null = Resolve-OrdinaryLocalPath ([IO.Path]::GetDirectoryName($Path)) $true
    if (!(Get-OrdinaryItem $Path $true -AllowMissing)) { $null = [IO.Directory]::CreateDirectory($Path) }
    $null = Get-OrdinaryItem $Path $true
}

function Write-PinnedCopy([IO.Stream]$InputStream, [string]$Path) {
    $null = Resolve-OrdinaryLocalPath ([IO.Path]::GetDirectoryName($Path)) $true
    $output = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $InputStream.CopyTo($output); $output.Flush($true) }
    finally { $output.Dispose() }
}

function Remove-OwnedStage([string]$Path) {
    $null = Resolve-OrdinaryLocalPath $Path $true
    foreach ($relative in @("bin/$runtimeName", "model/$modelName")) {
        $file = Join-Path $Path $relative
        $parent = [IO.Path]::GetDirectoryName($file)
        if ((Get-OrdinaryItem $parent $true -AllowMissing) -and (Get-OrdinaryItem $file $false -AllowMissing)) {
            [IO.File]::Delete($file)
        }
    }
    foreach ($leaf in @('bin', 'model', 'requests')) {
        $folder = Join-Path $Path $leaf
        if (Get-OrdinaryItem $folder $true -AllowMissing) { [IO.Directory]::Delete($folder, $false) }
    }
    [IO.Directory]::Delete($Path, $false)
}

$project = (Resolve-OrdinaryLocalPath $ProjectRoot $true).FullName
$manifestPath = (Resolve-OrdinaryLocalPath (Join-Path $project 'project.epoch.json') $false).FullName
if ((Get-Item -LiteralPath $manifestPath -Force).Length -gt 65536) { throw 'Unexpected Calculator project manifest size.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.engine -cne 'epoch' -or $manifest.project_format -cne 'epoch-project-v1' `
    -or $manifest.id -cne 'BasicSoftwareExampleCalculator' -or $manifest.kind -cne 'tool' `
    -or $manifest.template_family -cne 'tool-project' -or $manifest.scene -cne 'worlds/tool.epoch' `
    -or $manifest.gui -cne 'Assets/Gui/main.epochgui' -or $manifest.default_script -cne 'tool_bootstrap') {
    throw 'Select a saved Basic AI-Enabled Calculator project.'
}
foreach ($relative in @($manifest.scene, $manifest.gui, 'scripts/tool_bootstrap.ascript.cpp')) {
    $null = Resolve-OrdinaryLocalPath (Join-Path $project $relative) $false
}

$library = Join-Path $project 'Library'
$assetParent = Join-Path $library 'AI'
$destination = Join-Path $assetParent 'CalculatorCoach'
$stage = $null
$runtimeInput = $null
$modelInput = $null
$status = 'already-installed'
try {
    $runtimeInput = Open-PinnedFile $RuntimeExecutable $runtimeBytes $runtimeHash
    $modelInput = Open-PinnedFile $ModelFile $modelBytes $modelHash
    New-OrdinaryDirectory $library
    New-OrdinaryDirectory $assetParent
    if (Get-OrdinaryItem $destination $true -AllowMissing) {
        Confirm-Installation $destination
    } else {
        $pending = Join-Path $assetParent ('.CalculatorCoach.install-' + [guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $pending
        $stage = $pending
        foreach ($leaf in @('bin', 'model', 'requests')) { New-OrdinaryDirectory (Join-Path $stage $leaf) }
        Write-PinnedCopy $runtimeInput (Join-Path $stage "bin/$runtimeName")
        Write-PinnedCopy $modelInput (Join-Path $stage "model/$modelName")
        Confirm-Installation $stage
        $null = Resolve-OrdinaryLocalPath $assetParent $true
        try {
            [IO.Directory]::Move($stage, $destination)
            $stage = $null
            $status = 'installed'
        } catch {
            if (!(Get-OrdinaryItem $destination $true -AllowMissing)) { throw }
            Confirm-Installation $destination
        }
    }
} finally {
    if ($modelInput) { $modelInput.Dispose() }
    if ($runtimeInput) { $runtimeInput.Dispose() }
    if ($stage) {
        try { Remove-OwnedStage $stage }
        catch { Write-Warning "Could not remove the unpublished Calculator Coach staging directory: $stage" }
    }
}

[ordered]@{
    status = $status
    project = $project
    runtime = Join-Path $destination "bin/$runtimeName"
    runtime_sha256 = $runtimeHash
    model = Join-Path $destination "model/$modelName"
    model_sha256 = $modelHash
    requests = Join-Path $destination 'requests'
} | ConvertTo-Json
