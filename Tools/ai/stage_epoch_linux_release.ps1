param(
    [string]$Version = '0.87.50',
    [string]$Configuration = 'Clang-Release',
    [string]$OutputRoot = "C:\tmp\epoch_release_v$Version"
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

function To-WslPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = [System.IO.Path]::GetFullPath($Path)
    $drive = $resolved.Substring(0, 1).ToLowerInvariant()
    $tail = $resolved.Substring(2).Replace('\', '/')
    return "/mnt/$drive$tail"
}

$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$engine = Join-Path $repo 'Engine'
$binaryRoot = Join-Path $engine "Bin\$Configuration"
$binary = Join-Path $binaryRoot 'epoch'
$assets = Join-Path $engine 'assets'
$font = Join-Path $assets 'fonts\Roboto-Regular.ttf'
$license = Join-Path $repo 'LICENSE'
$readme = Join-Path $repo 'README.md'

Require-Path -Path $binary -Label 'Linux epoch binary'
Require-Path -Path $assets -Label 'Runtime assets'
Require-Path -Path $font -Label 'Runtime GUI font'
Require-Path -Path $license -Label 'License'
Require-Path -Path $readme -Label 'README'

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $resolvedOutput.StartsWith('C:\tmp\epoch_release_v', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing unexpected output root: $resolvedOutput"
}

$stageName = "epoch_linux_x64_v$Version"
$stage = Join-Path $resolvedOutput $stageName
$tarball = Join-Path $resolvedOutput "$stageName.tar.gz"
$checksum = Join-Path $resolvedOutput "v$Version`_checksums.txt"
$windowsZipName = "epoch_win10_x64_v$Version.zip"
$windowsZip = Join-Path $resolvedOutput $windowsZipName

Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $tarball -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage 'logs') -Force | Out-Null

Copy-Item -LiteralPath $binary -Destination (Join-Path $stage 'epoch') -Force
Copy-Item -LiteralPath $assets -Destination (Join-Path $stage 'assets') -Recurse -Force
Copy-Item -LiteralPath $license -Destination (Join-Path $stage 'LICENSE') -Force
Copy-Item -LiteralPath $readme -Destination (Join-Path $stage 'README.md') -Force

$stageWsl = To-WslPath $stage
$outWsl = To-WslPath $resolvedOutput
$tarWsl = To-WslPath $tarball

wsl bash -lc "set -euo pipefail; chmod 755 '$stageWsl/epoch'; test -f '$stageWsl/assets/fonts/Roboto-Regular.ttf'; cd '$stageWsl'; ./epoch --version | grep -F 'Epoch v$Version' >/dev/null; ./epoch --engine-contract-self-test | grep -F 'engine_contract_self_test.result=pass' >/dev/null; tar -C '$outWsl' -czf '$tarWsl' '$stageName'; tar -tzvf '$tarWsl' '$stageName/epoch' '$stageName/assets/fonts/Roboto-Regular.ttf'"

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $tarball).Hash.ToLowerInvariant()
$lines = @()
if (Test-Path -LiteralPath $windowsZip) {
    $windowsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $windowsZip).Hash.ToLowerInvariant()
    $lines += "$windowsHash  $windowsZipName"
}
else {
    $lines += Get-Content -LiteralPath $checksum -ErrorAction SilentlyContinue |
        Where-Object { $_ -and ($_ -notmatch [regex]::Escape("$stageName.tar.gz") + '$') }
}
$lines += "$hash  $stageName.tar.gz"
[System.IO.File]::WriteAllLines($checksum, $lines, [System.Text.Encoding]::ASCII)

Get-Item -LiteralPath $tarball, $checksum | Select-Object FullName, Length
