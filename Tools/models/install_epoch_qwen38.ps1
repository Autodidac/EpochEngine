param(
    [Parameter(Mandatory = $true)]
    [string]$EpochRuntimeRoot,
    [switch]$SkipRuntime,
    [switch]$SkipModel,
    [switch]$VerifyOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$llamaRelease = 'b10516'
$llamaRevision = 'b95502ba9aa0eb73a2f4fc8878d7fbe6a847a0b9'
$llamaArtifact = 'llama-b10516-bin-win-vulkan-x64.zip'
$llamaSha256 = '530f57d2a874ce017827c1e5a926812b9d5de4667248575d1372b1c0acf94d83'
$llamaUrl = 'https://github.com/ggml-org/llama.cpp/releases/download/b10516/llama-b10516-bin-win-vulkan-x64.zip'

$modelPackageId = 'os_model_qwen_3_8_27b'
$modelRevision = '4ca720788d1e01f1bff70c033e0d0028fd02e502'
$modelFileName = 'Qwen3.8-27B-UD-Q4_K_M.gguf'
$modelBytes = [int64]16464440224
$modelSha256 = '322e194ff79741c7baa497c240f677f54b201b0efab44ca8e50f122b39123482'
$modelReceiptSchema = 'epoch.local_ai.model.snapshot.v1'
$modelOfficialSource = 'https://huggingface.co/Qwen/Qwen3.8-27B'
$modelArtifactSource = 'https://huggingface.co/unsloth/Qwen3.8-27B-GGUF'
$modelUrl = "$modelArtifactSource/resolve/$modelRevision/$modelFileName`?download=true"

$runtimeRoot = [System.IO.Path]::GetFullPath($EpochRuntimeRoot)
$volumeRoot = [System.IO.Path]::GetPathRoot($runtimeRoot)
if ([string]::IsNullOrWhiteSpace($runtimeRoot) -or
    $runtimeRoot.TrimEnd('\') -eq $volumeRoot.TrimEnd('\')) {
    throw 'EpochRuntimeRoot must name the asset-bearing Epoch output directory, not a volume root.'
}

$packageRoot = Join-Path $runtimeRoot 'cache\packages\local_ai_llama_cpp_runtime'
$runtimeVersionRoot = Join-Path $packageRoot "versions\$llamaRelease"
$runtimeBinRoot = Join-Path $runtimeVersionRoot 'bin'
$runtimeExecutable = Join-Path $runtimeBinRoot 'llama-cli.exe'
$runtimeReceipt = Join-Path $packageRoot 'installed.runtime.json'
$runtimeArchive = Join-Path (Join-Path $packageRoot 'downloads') $llamaArtifact
$modelsCacheRoot = Join-Path $runtimeRoot 'cache\models'
$modelPackageRoot = Join-Path $modelsCacheRoot $modelPackageId
$modelVersionsRoot = Join-Path $modelPackageRoot 'versions'
$modelRoot = Join-Path $modelVersionsRoot $modelRevision
$modelPath = Join-Path $modelRoot $modelFileName
$modelReceipt = Join-Path $modelRoot 'installed.model.json'
$legacyModelRoot = $modelPackageRoot
$legacyModelPath = Join-Path $legacyModelRoot $modelFileName
$legacyModelReceipt = Join-Path $legacyModelRoot 'installed.model.json'

function Get-LowerSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Test-VerifiedFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Sha256,
        [int64]$Bytes = 0
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    if ($Bytes -gt 0 -and (Get-Item -LiteralPath $Path).Length -ne $Bytes) {
        return $false
    }
    return (Get-LowerSha256 -Path $Path) -eq $Sha256
}

function Get-ModelSnapshotReceipt {
    $bytesText = $modelBytes.ToString(
        [System.Globalization.CultureInfo]::InvariantCulture)
    $lines = @(
        '{'
        "  `"schema`": `"$modelReceiptSchema`","
        "  `"package_id`": `"$modelPackageId`","
        "  `"revision`": `"$modelRevision`","
        "  `"official_source`": `"$modelOfficialSource`","
        "  `"artifact_source`": `"$modelArtifactSource`","
        '  "automatic_execution": false,'
        '  "server_or_listener": false,'
        '  "artifacts": ['
        "    {`"file`": `"$modelFileName`", `"source_url`": `"$modelUrl`", `"bytes`": $bytesText, `"sha256`": `"$modelSha256`"}"
        '  ]'
        '}'
    )
    return ($lines -join "`n") + "`n"
}

function Write-ExactUtf8 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Contents
    )
    $temporary = "$Path.tmp-$([guid]::NewGuid().ToString('N'))"
    try {
        [System.IO.File]::WriteAllText(
            $temporary,
            $Contents,
            [System.Text.UTF8Encoding]::new($false, $true))
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Test-ExactUtf8File {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Expected
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    $encoding = [System.Text.UTF8Encoding]::new($false, $true)
    $expectedBytes = $encoding.GetBytes($Expected)
    $observedBytes = [System.IO.File]::ReadAllBytes($Path)
    if ($observedBytes.Length -ne $expectedBytes.Length) {
        return $false
    }
    for ($index = 0; $index -lt $expectedBytes.Length; ++$index) {
        if ($observedBytes[$index] -ne $expectedBytes[$index]) {
            return $false
        }
    }
    return $true
}

function Save-VerifiedDownload {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$Sha256,
        [int64]$Bytes = 0
    )
    $destinationRoot = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null
    if (Test-VerifiedFile -Path $Destination -Sha256 $Sha256 -Bytes $Bytes) {
        Write-Host "[OK] Reusing verified artifact: $Destination"
        return
    }

    $partial = "$Destination.partial"
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($null -ne $curl) {
        & $curl.Source --fail --location --retry 4 --continue-at - --output $partial $Url
        if ($LASTEXITCODE -ne 0) {
            throw "curl failed with exit code $LASTEXITCODE while downloading $Url"
        }
    }
    else {
        Invoke-WebRequest -Uri $Url -OutFile $partial -UseBasicParsing
    }

    if (-not (Test-VerifiedFile -Path $partial -Sha256 $Sha256 -Bytes $Bytes)) {
        throw "Downloaded artifact failed pinned size/SHA-256 validation: $partial"
    }
    Move-Item -LiteralPath $partial -Destination $Destination -Force
}

function Write-JsonReceipt {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][hashtable]$Value
    )
    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $temporary = "$Path.tmp"
    $Value | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporary -Encoding utf8
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Test-RuntimeInstall {
    if (-not (Test-Path -LiteralPath $runtimeExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath $runtimeReceipt -PathType Leaf)) {
        return $false
    }
    $receipt = Get-Content -LiteralPath $runtimeReceipt -Raw | ConvertFrom-Json
    return $receipt.schema -eq 'epoch.local_ai.runtime.install.v1' -and
        $receipt.release -eq $llamaRelease -and
        $receipt.revision -eq $llamaRevision -and
        $receipt.artifact -eq $llamaArtifact -and
        $receipt.artifact_sha256 -eq $llamaSha256 -and
        $receipt.server_or_listener -eq $false -and
        $receipt.auto_start -eq $false
}

function Test-ModelInstall {
    if (-not (Test-VerifiedFile -Path $modelPath -Sha256 $modelSha256 -Bytes $modelBytes)) {
        return $false
    }
    return Test-ExactUtf8File -Path $modelReceipt -Expected (Get-ModelSnapshotReceipt)
}

function Test-LegacyModelInstall {
    if (-not (Test-Path -LiteralPath $legacyModelReceipt -PathType Leaf) -or
        -not (Test-VerifiedFile -Path $legacyModelPath -Sha256 $modelSha256 -Bytes $modelBytes)) {
        return $false
    }
    $receipt = Get-Content -LiteralPath $legacyModelReceipt -Raw | ConvertFrom-Json
    return $receipt.schema -eq 'epoch.local_ai.model.install.v1' -and
        $receipt.package_id -eq $modelPackageId -and
        $receipt.revision -eq $modelRevision -and
        $receipt.file -eq $modelFileName -and
        $receipt.sha256 -eq $modelSha256
}

if ($VerifyOnly) {
    if ((Test-RuntimeInstall) -and (Test-ModelInstall)) {
        Write-Host '[PASS] Epoch-local Qwen3.8 installation is complete, pinned, and not activated.'
        exit 0
    }
    if ((Test-RuntimeInstall) -and (Test-LegacyModelInstall)) {
        Write-Host '[COMPAT] Legacy Qwen3.8 layout is valid; rerun without -VerifyOnly to publish the canonical versioned snapshot.'
        exit 0
    }
    throw 'Epoch-local Qwen3.8 installation is missing or does not match the pinned receipts.'
}

if (-not $SkipModel) {
    $drive = [System.IO.DriveInfo]::new($volumeRoot)
    if ($drive.AvailableFreeSpace -lt 19000000000) {
        throw 'At least 19 GB of free space is required before the Qwen3.8 GGUF download.'
    }
}

if (-not $SkipRuntime -and -not (Test-RuntimeInstall)) {
    Save-VerifiedDownload -Url $llamaUrl -Destination $runtimeArchive -Sha256 $llamaSha256
    $staging = "$runtimeVersionRoot.staging-$([guid]::NewGuid().ToString('N'))"
    try {
        New-Item -ItemType Directory -Force -Path $staging | Out-Null
        Expand-Archive -LiteralPath $runtimeArchive -DestinationPath $staging
        $cli = Get-ChildItem -LiteralPath $staging -Filter 'llama-cli.exe' -File -Recurse |
            Select-Object -First 1
        if ($null -eq $cli) {
            throw 'Pinned llama.cpp archive did not contain llama-cli.exe.'
        }
        if (Test-Path -LiteralPath $runtimeVersionRoot) {
            throw "Pinned runtime destination already exists but is incomplete: $runtimeVersionRoot"
        }
        New-Item -ItemType Directory -Force -Path $runtimeBinRoot | Out-Null
        Get-ChildItem -LiteralPath $cli.Directory.FullName -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $runtimeBinRoot -Recurse
        }
    }
    finally {
        if (Test-Path -LiteralPath $staging) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
    }
    if (-not (Test-Path -LiteralPath $runtimeExecutable -PathType Leaf)) {
        throw 'llama-cli.exe was not materialized at the canonical Epoch-local path.'
    }
    Write-JsonReceipt -Path $runtimeReceipt -Value @{
        schema = 'epoch.local_ai.runtime.install.v1'
        package_id = 'local_ai_llama_cpp_runtime'
        source = 'https://github.com/ggml-org/llama.cpp'
        release = $llamaRelease
        revision = $llamaRevision
        artifact = $llamaArtifact
        artifact_sha256 = $llamaSha256
        executable = $runtimeExecutable
        invocation = 'epoch_owned_child_process'
        auto_start = $false
        server_or_listener = $false
    }
}

if (-not $SkipModel -and -not (Test-ModelInstall)) {
    if (Test-Path -LiteralPath $modelRoot) {
        throw "Canonical Qwen3.8 version root exists but failed verification: $modelRoot"
    }
    New-Item -ItemType Directory -Force -Path $modelVersionsRoot | Out-Null
    $staging = Join-Path $modelVersionsRoot ('.stage-' + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $staging | Out-Null
        $stagedModel = Join-Path $staging $modelFileName
        $stagedReceipt = Join-Path $staging 'installed.model.json'
        if (Test-LegacyModelInstall) {
            Copy-Item -LiteralPath $legacyModelPath -Destination $stagedModel
        }
        else {
            Save-VerifiedDownload -Url $modelUrl -Destination $stagedModel -Sha256 $modelSha256 -Bytes $modelBytes
        }
        Write-ExactUtf8 -Path $stagedReceipt -Contents (Get-ModelSnapshotReceipt)
        if (-not (Test-VerifiedFile -Path $stagedModel -Sha256 $modelSha256 -Bytes $modelBytes) -or
            -not (Test-ExactUtf8File -Path $stagedReceipt -Expected (Get-ModelSnapshotReceipt))) {
            throw 'The staged Qwen3.8 snapshot failed exact artifact or receipt verification.'
        }
        Move-Item -LiteralPath $staging -Destination $modelRoot
    }
    finally {
        if (Test-Path -LiteralPath $staging) {
            Remove-Item -LiteralPath $staging -Recurse -Force
        }
    }
}

if (-not (Test-RuntimeInstall) -or -not (Test-ModelInstall)) {
    throw 'Epoch-local Qwen3.8 installation did not pass final receipt and integrity validation.'
}

Write-Host '[PASS] Epoch-local Qwen3.8 is installed and pinned; provider activation remains explicit.'
Write-Host "[INFO] Runtime: $runtimeExecutable"
Write-Host "[INFO] Model: $modelPath"
Write-Host '[INFO] External MCP/OpenAI-compatible inference remains available as a separate provider.'
