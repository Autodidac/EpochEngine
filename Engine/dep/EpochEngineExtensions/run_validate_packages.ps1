# SPDX-License-Identifier: LicenseRef-MIT-NoSell
# Copyright (c) 2026 Adam Rushford

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Script = Join-Path $RepoRoot "tools\validate_packages.py"

if (-not (Test-Path $Script)) {
    throw "Missing validator: $Script"
}

python $Script
exit $LASTEXITCODE
