[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [string]$DependencyRoot,
    [string]$ToolchainFile
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$engcoderSource = (Resolve-Path -LiteralPath $SourceRoot).Path
$nativeSource = Join-Path $engcoderSource 'projects\native'
if (!(Test-Path -LiteralPath (Join-Path $nativeSource 'CMakeLists.txt'))) {
    throw 'SourceRoot must contain the supplied EngCoder projects/native source.'
}
if (!$DependencyRoot) {
    $DependencyRoot = Join-Path $engcoderSource 'out\build\windows-vs2022\vcpkg_installed'
}
$dependencyCache = (Resolve-Path -LiteralPath $DependencyRoot).Path
$toolchain = if ($ToolchainFile) { (Resolve-Path -LiteralPath $ToolchainFile).Path } else {
    Join-Path $engcoderSource '.deps\vcpkg\scripts\buildsystems\vcpkg.cmake'
}
if (!(Test-Path -LiteralPath $toolchain)) { throw 'The supplied vcpkg toolchain is missing.' }
foreach ($package in @('nlohmann_json', 'httplib', 'curl', 'unofficial-sqlite3')) {
    if (!(Test-Path -LiteralPath (Join-Path $dependencyCache "x64-windows\share\$package"))) {
        throw "Existing x64-windows dependency cache is missing $package; this installer does not download dependencies."
    }
}

# This is a local optional tool, not a vendored Epoch dependency or release
# payload. Its server/model/training lifecycle is deliberately not activated.
$installRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'addons\EngCoder'))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'addons')) + [IO.Path]::DirectorySeparatorChar
if (!$installRoot.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'EngCoder installation escaped the local addons directory.'
}
$buildRoot = Join-Path $installRoot 'build'
$binRoot = Join-Path $installRoot 'bin'
New-Item -ItemType Directory -Path $buildRoot, $binRoot -Force | Out-Null
& cmake -S $nativeSource -B $buildRoot -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DVCPKG_INSTALLED_DIR=$dependencyCache" `
    -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_INSTALL=OFF `
    -DENGCODER_BUILD_GUI=OFF -DENGCODER_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "EngCoder headless configuration failed: $LASTEXITCODE" }
& cmake --build $buildRoot --config Release --target engcoder_server engcoder_cli --parallel 2
if ($LASTEXITCODE -ne 0) { throw "EngCoder headless build failed: $LASTEXITCODE" }
$builtBin = Join-Path $buildRoot 'Release'
foreach ($binary in @('EngCoderServer.exe', 'EngCoderCLI.exe')) {
    $sourceBinary = Join-Path $builtBin $binary
    if (!(Test-Path -LiteralPath $sourceBinary)) { throw "Missing built backend: $binary" }
    Copy-Item -LiteralPath $sourceBinary -Destination (Join-Path $binRoot $binary)
}
Get-ChildItem -LiteralPath $builtBin -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $binRoot $_.Name)
}
Write-Output "Built headless EngCoder backend: $binRoot"
Write-Output 'No GUI, listener, model, training job or full-agent task was started.'
Write-Output 'Epoch continues to use its selected inference endpoint; this installation does not switch it to /api/tasks.'
