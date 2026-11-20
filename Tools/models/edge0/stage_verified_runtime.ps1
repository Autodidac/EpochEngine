param(
    [Parameter(Mandatory = $true)][string]$SourceBin,
    [Parameter(Mandatory = $true)][string]$Receipt,
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ReceiptSha256,
    [Parameter(Mandatory = $true)][string]$StateRoot,
    [switch]$VerifyOnly
)

# Admit a locally audited SDK into Epoch's existing runtime owner. This neither
# builds nor executes a model, downloads anything, nor asserts untested features.
# The caller must first review the source/build/qualification evidence named by
# the receipt. Its explicit digest binds this operation to that reviewed receipt.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$requiredFiles = @('ggml-base.dll', 'ggml-cpu.dll', 'ggml-cuda.dll', 'ggml.dll',
    'llama-common.dll', 'llama-server-impl.dll', 'llama-server.exe', 'llama.dll', 'mtmd.dll')
$requiredFeatures = @('qwen38-q2-padded-down-v1', 'qwen-moe-streaming-cache-v1')

function Get-Digest([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}
function Assert-PlainPath([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse points are not admitted: $($item.FullName)"
        }
        $item = if ($item -is [IO.FileInfo]) { $item.Directory } else { $item.Parent }
    }
}
function Assert-Sdk([string]$Bin, $Manifest) {
    Assert-PlainPath $Bin
    $actual = @(Get-ChildItem -LiteralPath $Bin -Force)
    if ($actual.Count -ne $requiredFiles.Count -or @($actual | Where-Object { $_.PSIsContainer }).Count) {
        throw 'SDK must contain exactly the nine audited runtime files and no directories.'
    }
    if (@($Manifest.files).Count -ne $requiredFiles.Count) { throw 'Incomplete SDK receipt.' }
    foreach ($name in $requiredFiles) {
        $entry = @($Manifest.files | Where-Object { $_.file -ceq $name })
        if ($entry.Count -ne 1) { throw "Missing or duplicate receipt entry: $name" }
        $path = Join-Path $Bin $name
        Assert-PlainPath $path
        if ($entry[0].sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
            [int64]$entry[0].bytes -le 0 -or
            (Get-Item -LiteralPath $path).Length -ne [int64]$entry[0].bytes -or
            (Get-Digest $path) -ne $entry[0].sha256.ToLowerInvariant()) {
            throw "SDK content does not match the audited receipt: $name"
        }
    }
}

$SourceBin = (Resolve-Path -LiteralPath $SourceBin).Path
$Receipt = (Resolve-Path -LiteralPath $Receipt).Path
Assert-PlainPath $Receipt
if ((Get-Digest $Receipt) -ne $ReceiptSha256.ToLowerInvariant()) { throw 'Receipt digest mismatch.' }
$manifest = Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json
if ($manifest.schema -cne 'epoch.qwen_flash.local_sdk.v1' -or
    $manifest.runtime -cne 'qwen-flash' -or
    $manifest.backend_commit -cne '167742d9dfa01306786e7d3cfd31fcb8a8b185dc' -or
    $manifest.build_recipe -cne 'engcoder-qwenflash-2026.11.15-edge0-stream1') {
    throw 'Receipt does not identify the admitted pinned streaming SDK.'
}
if (@($manifest.features).Count -ne $requiredFeatures.Count) { throw 'Unexpected feature claims.' }
foreach ($feature in $requiredFeatures) {
    if ($feature -cnotin $manifest.features) { throw "Missing verified feature: $feature" }
}
# Evidence paths are relative to the receipt, allowing the complete proof bundle
# to remain checkout-local. They are never copied into public source or packages.
foreach ($role in @('source', 'build', 'qualification')) {
    $evidence = @($manifest.provenance | Where-Object { $_.role -ceq $role })
    if ($evidence.Count -lt 1) { throw "Missing reviewed $role provenance." }
    foreach ($entry in $evidence) {
        $path = Join-Path (Split-Path -Parent $Receipt) $entry.file
        Assert-PlainPath $path
        if ($entry.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
            (Get-Digest $path) -ne $entry.sha256.ToLowerInvariant()) {
            throw "Changed $role provenance: $($entry.file)"
        }
    }
}
Assert-Sdk $SourceBin $manifest
$StateRoot = [IO.Path]::GetFullPath($StateRoot)
if ($StateRoot.TrimEnd('\', '/') -eq [IO.Path]::GetPathRoot($StateRoot).TrimEnd('\', '/')) {
    throw 'StateRoot must be the existing native-model state directory, not a drive root.'
}
if ($VerifyOnly) { Write-Output 'Verified receipt, provenance and all nine SDK hashes; no files changed.'; return }
Assert-PlainPath $StateRoot
$runtimes = Join-Path $StateRoot 'runtimes'
Assert-PlainPath $runtimes
$destination = Join-Path $runtimes 'qwen-flash'
if (Test-Path -LiteralPath $destination) { Assert-PlainPath $destination }
foreach ($process in @(Get-Process -Name 'llama-server*' -ErrorAction SilentlyContinue)) {
    $processPath = $process.Path
    if ([string]::IsNullOrWhiteSpace($processPath)) { throw 'Cannot verify an active model worker identity.' }
    foreach ($prefix in @($destination, $SourceBin)) {
        if ($processPath.StartsWith($prefix.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Retire the owned model worker before staging this runtime.'
        }
    }
}
$suffix = [Guid]::NewGuid().ToString('N')
$stage = Join-Path $runtimes "qwen-flash.staging-$suffix"
$backup = Join-Path $runtimes "qwen-flash.previous-$suffix"
New-Item -ItemType Directory -Path (Join-Path $stage 'bin') | Out-Null
foreach ($name in $requiredFiles) { Copy-Item -LiteralPath (Join-Path $SourceBin $name) -Destination (Join-Path $stage 'bin') }
Assert-Sdk (Join-Path $stage 'bin') $manifest
Copy-Item -LiteralPath $Receipt -Destination (Join-Path $stage 'runtime.json')
if ((Get-Digest (Join-Path $stage 'runtime.json')) -ne $ReceiptSha256.ToLowerInvariant()) { throw 'Staged receipt changed.' }
$movedPrevious = $false
try {
    if (Test-Path -LiteralPath $destination) {
        Move-Item -LiteralPath $destination -Destination $backup
        $movedPrevious = $true
    }
    Move-Item -LiteralPath $stage -Destination $destination
} catch {
    if ($movedPrevious -and -not (Test-Path -LiteralPath $destination)) {
        Move-Item -LiteralPath $backup -Destination $destination
    }
    throw
}
Write-Output "Staged verified runtime: $destination"
if ($movedPrevious) { Write-Output "Previous runtime preserved: $backup" }
