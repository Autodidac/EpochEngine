param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
    [ValidateRange(1, 2147483647)][int]$MaximumLines = 4000
)
$ErrorActionPreference = 'Stop'
& python (Join-Path $PSScriptRoot 'validate_source_size.py') --repo-root $RepoRoot --maximum-lines $MaximumLines
exit $LASTEXITCODE
