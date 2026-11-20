# Run from an x64 Visual Studio Developer PowerShell. All outputs are checkout-local.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DecoderSource,
    [Parameter(Mandatory)][string]$DecoderBuild,
    [Parameter(Mandatory)][string]$InstalledBin,
    [string]$PackHeader,
    [string]$PackHeaderSha256,
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9_-]*$')][string]$Name = 'epoch-edge0-candidate'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$reports = (Resolve-Path (Join-Path $root 'Artifacts/Reports')).Path
$output = Join-Path $reports $Name
$DecoderSource = (Resolve-Path -LiteralPath $DecoderSource).Path
$DecoderBuild = (Resolve-Path -LiteralPath $DecoderBuild).Path
$InstalledBin = (Resolve-Path -LiteralPath $InstalledBin).Path
$commit = '167742d9dfa01306786e7d3cfd31fcb8a8b185dc'

if (Test-Path -LiteralPath $output) { throw "Candidate output exists at $output" }
if ([bool]$PackHeader -ne [bool]$PackHeaderSha256) { throw 'An alternate pack header requires its exact SHA-256' }
if ($PackHeader) {
    $PackHeader = (Resolve-Path -LiteralPath $PackHeader).Path
    $headerHasher = [System.Security.Cryptography.SHA256]::Create()
    try { $actualHeaderHash = [BitConverter]::ToString($headerHasher.ComputeHash([System.IO.File]::ReadAllBytes($PackHeader))).Replace('-', '') }
    finally { $headerHasher.Dispose() }
    if ($PackHeaderSha256 -notmatch '^[a-fA-F0-9]{64}$' -or $actualHeaderHash -ne $PackHeaderSha256) {
        throw 'Alternate pack header does not match the reviewed SHA-256'
    }
}
if ((& git -C $DecoderSource rev-parse HEAD) -ne $commit) { throw 'Unexpected native decoder source revision' }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Use an x64 Visual Studio Developer PowerShell' }

function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command exited with $LASTEXITCODE" }
}

New-Item -ItemType Directory -Path $output | Out-Null
$source = (New-Item -ItemType Directory -Path (Join-Path $output 'source')).FullName
$archive = Join-Path $output 'decoder.tar'
Invoke-Checked git @('-C', $DecoderSource, 'archive', '--format=tar', "--output=$archive", $commit)
Invoke-Checked tar.exe @('-xf', $archive, '-C', $source)
$overlayArgs = @('-B', (Join-Path $PSScriptRoot 'apply_qwen_overlay.py'), $source)
if ($PackHeader) { $overlayArgs += @('--pack-header', $PackHeader, '--pack-header-sha256', $PackHeaderSha256) }
Invoke-Checked python $overlayArgs

$package = (New-Item -ItemType Directory -Path (Join-Path $output 'existing-ggml')).FullName
$decoderCmake = $DecoderBuild.Replace('\', '/')
$config = @'
set(epoch_decoder_build [==[@DECODER_BUILD@]==])
foreach(name IN ITEMS ggml ggml-base ggml-cpu)
    add_library(ggml::${name} SHARED IMPORTED)
    set_target_properties(ggml::${name} PROPERTIES
        IMPORTED_IMPLIB "${epoch_decoder_build}/ggml/src/Release/${name}.lib"
        IMPORTED_LOCATION "${epoch_decoder_build}/bin/Release/${name}.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/ggml/include")
endforeach()
target_link_libraries(ggml::ggml INTERFACE ggml::ggml-base ggml::ggml-cpu)
target_compile_definitions(ggml::ggml INTERFACE GGML_SHARED)
'@
$config.Replace('@DECODER_BUILD@', $decoderCmake) |
    Set-Content -LiteralPath (Join-Path $package 'ggml-config.cmake') -Encoding utf8

$build = Join-Path $output 'build'
Invoke-Checked cmake @('-S', $source, '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    '-DBUILD_SHARED_LIBS=ON', '-DLLAMA_USE_SYSTEM_GGML=ON', "-Dggml_DIR=$package", '-DLLAMA_BUILD_COMMON=ON',
    '-DLLAMA_BUILD_TESTS=OFF', '-DLLAMA_BUILD_EXAMPLES=OFF', '-DLLAMA_BUILD_TOOLS=OFF',
    '-DLLAMA_BUILD_SERVER=OFF', '-DLLAMA_CURL=OFF')
Invoke-Checked cmake @('--build', $build, '--target', 'llama-common', '--parallel', '1')

$bin = (New-Item -ItemType Directory -Path (Join-Path $output 'bin')).FullName
Get-ChildItem -LiteralPath $InstalledBin -File |
    Where-Object { $_.Name -eq 'llama-server.exe' -or ($_.Extension -eq '.dll' -and $_.Name -notin @('llama.dll', 'llama-common.dll')) } |
    Copy-Item -Destination $bin
Copy-Item -LiteralPath (Join-Path $build 'bin/llama.dll') -Destination $bin
Copy-Item -LiteralPath (Join-Path $build 'bin/llama-common.dll') -Destination $bin
$hasher = [System.Security.Cryptography.SHA256]::Create()
try {
    foreach ($binaryName in @('llama-server.exe', 'llama.dll', 'llama-common.dll')) {
        $path = Join-Path $bin $binaryName
        $stream = [System.IO.File]::OpenRead($path)
        try {
            [pscustomobject]@{
                Algorithm = 'SHA256'
                Hash = [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '')
                Path = $path
            }
        }
        finally {
            $stream.Dispose()
        }
    }
}
finally {
    $hasher.Dispose()
}
