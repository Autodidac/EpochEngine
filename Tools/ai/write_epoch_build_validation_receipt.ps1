[CmdletBinding()]
param(
    [ValidateSet('', 'windows-x64', 'linux-x64', 'macos-arm64')]
    [string]$Platform = '',
    [ValidateSet('', 'msvc', 'clang', 'gcc')]
    [string]$Compiler = '',
    [ValidateSet('', 'debug', 'release')]
    [string]$Configuration = '',
    [string]$Target = '',
    [string]$Toolchain = '',
    [string]$ArtifactPath = '',
    [string]$SourceCommit = '',
    [string]$SourceTreeSha256 = '',
    [string]$SourceVersion = '',
    [string]$PackagedVersion = '',
    [string]$ChecksPath = '',
    [string]$OutputPath = '',
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$script:Schema = 'epoch.build-validation/v1'
$script:MaximumChecks = 64
$script:MaximumIdentifierBytes = 96
$script:MaximumDiagnosticBytes = 2048
$script:KnownLanes = @(
    'source_names',
    'compile',
    'engine_contract',
    'headless_ci',
    'package_inventory',
    'dependency_resolution',
    'shared_library_resolution',
    'renderer_smoke'
)
$script:KnownStatuses = @('passed', 'failed', 'skipped')

function Test-LowerHex {
    param(
        [AllowEmptyString()][string]$Value,
        [Parameter(Mandatory = $true)][int]$Bytes
    )

    return $Bytes -gt 0 -and
        $Value.Length -eq ($Bytes * 2) -and
        $Value -cmatch '^[0-9a-f]+$'
}

function Assert-Identifier {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [AllowEmptyString()][string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value) -or
        $Value.Length -gt $script:MaximumIdentifierBytes -or
        $Value.StartsWith('.') -or
        $Value.EndsWith('.') -or
        $Value -cnotmatch '^[A-Za-z0-9_.-]+$') {
        throw "$Name must be a non-empty bounded ASCII identifier."
    }
}

function ConvertTo-CanonicalJsonString {
    param([AllowEmptyString()][string]$Value)

    $builder = [System.Text.StringBuilder]::new($Value.Length + 2)
    [void]$builder.Append('"')
    foreach ($character in $Value.ToCharArray()) {
        $code = [int][char]$character
        switch ($code) {
            8  { [void]$builder.Append('\b'); continue }
            9  { [void]$builder.Append('\t'); continue }
            10 { [void]$builder.Append('\n'); continue }
            12 { [void]$builder.Append('\f'); continue }
            13 { [void]$builder.Append('\r'); continue }
            34 { [void]$builder.Append('\"'); continue }
            92 { [void]$builder.Append('\\'); continue }
        }

        if ($code -lt 0x20) {
            [void]$builder.Append(('\u{0:x4}' -f $code))
        }
        else {
            [void]$builder.Append($character)
        }
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Read-VersionMacro {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $pattern = "(?m)^#\s+define\s+$([regex]::Escape($Name))\s+([0-9]+)\s*$"
    $matches = [regex]::Matches($Source, $pattern)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one numeric $Name definition; found $($matches.Count)."
    }
    return [int]$matches[0].Groups[1].Value
}

function Resolve-VersionAuthorities {
    param(
        [Parameter(Mandatory = $true)][string]$VersionModule,
        [Parameter(Mandatory = $true)][string]$RequestedPlatform
    )

    $source = Get-Content -LiteralPath $VersionModule -Raw
    $major = Read-VersionMacro -Source $source -Name 'EPOCH_VERSION_MAJOR_VALUE'
    $minor = Read-VersionMacro -Source $source -Name 'EPOCH_VERSION_MINOR_VALUE'
    $revision = Read-VersionMacro -Source $source -Name 'EPOCH_VERSION_REVISION_VALUE'

    $prefix = switch ($RequestedPlatform) {
        'windows-x64' { 'EPOCH_WINDOWS_PACKAGED_VERSION' }
        'linux-x64' { 'EPOCH_LINUX_PACKAGED_VERSION' }
        'macos-arm64' { 'EPOCH_MACOS_PACKAGED_VERSION' }
        default { throw "Unsupported platform authority: $RequestedPlatform" }
    }

    # Tracked packaged major/minor defaults intentionally alias the source
    # macros; only build-time overrides replace them. A source admission receipt
    # describes tracked authorities, so resolve those aliases explicitly.
    $packagedMajor = $major
    $packagedMinor = $minor
    $packagedRevision = Read-VersionMacro -Source $source -Name "${prefix}_REVISION_VALUE"

    return [pscustomobject]@{
        Source = '{0}.{1}.{2}' -f $major, $minor, $revision
        Packaged = '{0}.{1}.{2}' -f $packagedMajor, $packagedMinor, $packagedRevision
    }
}

function Assert-SemanticVersion {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value
    )

    if ($Value -cnotmatch '^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,4}$') {
        throw "$Name must use bounded major.minor.revision digits."
    }
}

function Read-Checks {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$RequestedPlatform
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Validation checks were not found: $Path"
    }

    # Windows PowerShell 5.1 can preserve a top-level JSON array as one nested
    # pipeline value when ConvertFrom-Json is wrapped directly in @(...).
    # Capture first, then enumerate explicitly so both Windows PowerShell and
    # modern PowerShell normalize the document to the same row sequence.
    $decodedDocument = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $decoded = @()
    foreach ($decodedEntry in $decodedDocument) {
        $decoded += $decodedEntry
    }
    if ($decoded.Count -eq 0 -or $decoded.Count -gt $script:MaximumChecks) {
        throw "Validation checks must contain 1-$($script:MaximumChecks) entries."
    }

    $seen = @{}
    $normalized = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in $decoded) {
        $lane = [string]$entry.lane
        $status = [string]$entry.status
        $duration = [uint64]$entry.duration_ms
        $evidence = [string]$entry.evidence_sha256
        $diagnostic = [string]$entry.diagnostic

        if ($script:KnownLanes -cnotcontains $lane) {
            throw "Unknown validation lane: $lane"
        }
        if ($seen.ContainsKey($lane)) {
            throw "Duplicate validation lane: $lane"
        }
        $seen[$lane] = $true
        if ($script:KnownStatuses -cnotcontains $status) {
            throw "Unknown validation status for ${lane}: $status"
        }
        if ($diagnostic.Length -gt $script:MaximumDiagnosticBytes) {
            throw "Validation diagnostic exceeds the bounded size for $lane."
        }
        if ($status -eq 'passed' -and -not (Test-LowerHex -Value $evidence -Bytes 32)) {
            throw "Passed validation lane $lane requires one lowercase SHA-256 digest."
        }
        if ($status -ne 'passed' -and
            -not [string]::IsNullOrEmpty($evidence) -and
            -not (Test-LowerHex -Value $evidence -Bytes 32)) {
            throw "Validation lane $lane contains a malformed evidence digest."
        }
        if ($status -eq 'failed') {
            throw "Validation lane $lane failed and cannot be admitted."
        }

        $normalized.Add([pscustomobject]@{
            lane = $lane
            status = $status
            duration_ms = $duration
            evidence_sha256 = $evidence
            diagnostic = $diagnostic
        })
    }

    $required = @(
        'source_names',
        'compile',
        'engine_contract',
        'headless_ci',
        'package_inventory',
        'dependency_resolution'
    )
    if ($RequestedPlatform -ne 'windows-x64') {
        $required += 'shared_library_resolution'
    }
    foreach ($lane in $required) {
        if (-not $seen.ContainsKey($lane)) {
            throw "Required validation lane is missing: $lane"
        }
        $entry = $normalized | Where-Object lane -CEQ $lane
        if ($entry.status -cne 'passed') {
            throw "Required validation lane was not proven: $lane"
        }
    }

    return @($normalized | Sort-Object { [array]::IndexOf($script:KnownLanes, $_.lane) })
}

function ConvertTo-CanonicalReceipt {
    param([Parameter(Mandatory = $true)]$Receipt)

    $output = [System.Text.StringBuilder]::new(2048)
    [void]$output.Append('{"schema":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.schema))
    [void]$output.Append(',"source_version":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.source_version))
    [void]$output.Append(',"packaged_version":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.packaged_version))
    [void]$output.Append(',"source_commit":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.source_commit))
    [void]$output.Append(',"source_tree_sha256":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.source_tree_sha256))
    [void]$output.Append(',"platform":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.platform))
    [void]$output.Append(',"compiler":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.compiler))
    [void]$output.Append(',"configuration":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.configuration))
    [void]$output.Append(',"target":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.target))
    [void]$output.Append(',"toolchain":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.toolchain))
    [void]$output.Append(',"artifact":{"name":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.artifact.name))
    [void]$output.Append(',"sha256":')
    [void]$output.Append((ConvertTo-CanonicalJsonString $Receipt.artifact.sha256))
    [void]$output.Append(',"size_bytes":')
    [void]$output.Append([string]$Receipt.artifact.size_bytes)
    [void]$output.Append('},"checks":[')

    for ($index = 0; $index -lt $Receipt.checks.Count; ++$index) {
        if ($index -ne 0) {
            [void]$output.Append(',')
        }
        $check = $Receipt.checks[$index]
        [void]$output.Append('{"lane":')
        [void]$output.Append((ConvertTo-CanonicalJsonString $check.lane))
        [void]$output.Append(',"status":')
        [void]$output.Append((ConvertTo-CanonicalJsonString $check.status))
        [void]$output.Append(',"duration_ms":')
        [void]$output.Append([string]$check.duration_ms)
        [void]$output.Append(',"evidence_sha256":')
        [void]$output.Append((ConvertTo-CanonicalJsonString $check.evidence_sha256))
        [void]$output.Append(',"diagnostic":')
        [void]$output.Append((ConvertTo-CanonicalJsonString $check.diagnostic))
        [void]$output.Append('}')
    }
    [void]$output.Append(']}')
    return $output.ToString()
}

function Get-Sha256Hex {
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = $hasher.ComputeHash($Bytes)
        return -join ($hash | ForEach-Object { $_.ToString('x2') })
    }
    finally {
        $hasher.Dispose()
    }
}

function Write-ReceiptDocument {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Canonical
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $outputDirectory = Split-Path -Parent $resolvedPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    }

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $receiptBytes = $utf8NoBom.GetBytes($Canonical + "`n")
    $receiptSha256 = Get-Sha256Hex -Bytes $receiptBytes
    $sidecarBytes = $utf8NoBom.GetBytes(
        "$receiptSha256  $([System.IO.Path]::GetFileName($resolvedPath))`n")

    [System.IO.File]::WriteAllBytes($resolvedPath, $receiptBytes)
    [System.IO.File]::WriteAllBytes("$resolvedPath.sha256", $sidecarBytes)
    return $receiptSha256
}

function Invoke-SelfTest {
    $selfTestRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $selfTestVersionModule = Join-Path $selfTestRepoRoot 'Engine\modules\epoch.version.ixx'
    foreach ($selfTestPlatform in @('windows-x64', 'linux-x64', 'macos-arm64')) {
        $selfTestAuthorities = Resolve-VersionAuthorities -VersionModule $selfTestVersionModule -RequestedPlatform $selfTestPlatform
        Assert-SemanticVersion -Name 'self-test source authority' -Value $selfTestAuthorities.Source
        Assert-SemanticVersion -Name 'self-test packaged authority' -Value $selfTestAuthorities.Packaged
    }

    $checks = @(
        [pscustomobject]@{ lane='source_names'; status='passed'; duration_ms=[uint64]1; evidence_sha256=('1' * 64); diagnostic='names' },
        [pscustomobject]@{ lane='compile'; status='passed'; duration_ms=[uint64]2; evidence_sha256=('2' * 64); diagnostic='compile' },
        [pscustomobject]@{ lane='engine_contract'; status='passed'; duration_ms=[uint64]3; evidence_sha256=('3' * 64); diagnostic='contract' },
        [pscustomobject]@{ lane='headless_ci'; status='passed'; duration_ms=[uint64]4; evidence_sha256=('4' * 64); diagnostic='headless' },
        [pscustomobject]@{ lane='package_inventory'; status='passed'; duration_ms=[uint64]5; evidence_sha256=('5' * 64); diagnostic='package' },
        [pscustomobject]@{ lane='dependency_resolution'; status='passed'; duration_ms=[uint64]6; evidence_sha256=('6' * 64); diagnostic='deps' },
        [pscustomobject]@{ lane='renderer_smoke'; status='skipped'; duration_ms=[uint64]0; evidence_sha256=''; diagnostic='pixels explicitly unclaimed' }
    )
    $selfTestChecksPath = Join-Path (
        [System.IO.Path]::GetTempPath()) (
        'epoch-build-validation-checks-' + [guid]::NewGuid().ToString('N') + '.json')
    try {
        $checksJson = ConvertTo-Json -InputObject $checks -Depth 4
        [System.IO.File]::WriteAllText(
            $selfTestChecksPath,
            $checksJson + "`n",
            [System.Text.UTF8Encoding]::new($false))
        $fileChecks = @(Read-Checks -Path $selfTestChecksPath -RequestedPlatform 'windows-x64')
        if ($fileChecks.Count -ne $checks.Count -or
            $fileChecks[0].lane -cne 'source_names' -or
            $fileChecks[-1].lane -cne 'renderer_smoke') {
            throw 'File-backed validation checks did not normalize to the canonical lane sequence.'
        }
    }
    finally {
        Remove-Item -LiteralPath $selfTestChecksPath -Force -ErrorAction SilentlyContinue
    }
    $receipt = [pscustomobject]@{
        schema = $script:Schema
        source_version = '0.89.32'
        packaged_version = '0.89.30'
        source_commit = ('a' * 40)
        source_tree_sha256 = ('b' * 64)
        platform = 'windows-x64'
        compiler = 'msvc'
        configuration = 'release'
        target = 'EpochEditor'
        toolchain = 'msvc-19.44'
        artifact = [pscustomobject]@{
            name = 'EpochEditor.exe'
            sha256 = ('c' * 64)
            size_bytes = [uint64]1234
        }
        checks = $checks
    }
    $first = ConvertTo-CanonicalReceipt $receipt
    $second = ConvertTo-CanonicalReceipt $receipt
    if ($first -cne $second -or $first -notmatch '^\{"schema":"epoch\.build-validation/v1"') {
        throw 'Canonical validation receipt serialization is not deterministic.'
    }
    $roundtrip = $first | ConvertFrom-Json
    if ($roundtrip.target -cne 'EpochEditor' -or $roundtrip.checks.Count -ne 7) {
        throw 'Canonical validation receipt did not survive JSON roundtrip.'
    }

    $selfTestReceiptPath = Join-Path (
        [System.IO.Path]::GetTempPath()) (
        'epoch-build-validation-receipt-' + [guid]::NewGuid().ToString('N') + '.json')
    try {
        $writtenSha256 = Write-ReceiptDocument -Path $selfTestReceiptPath -Canonical $first
        $fileSha256 = (Get-FileHash -LiteralPath $selfTestReceiptPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $receiptBytes = [System.IO.File]::ReadAllBytes($selfTestReceiptPath)
        $sidecar = [System.IO.File]::ReadAllText("$selfTestReceiptPath.sha256")
        $expectedSidecar = "$fileSha256  $([System.IO.Path]::GetFileName($selfTestReceiptPath))`n"
        if ($writtenSha256 -cne $fileSha256 -or
            $sidecar -cne $expectedSidecar -or
            $receiptBytes.Length -ne ([System.Text.Encoding]::UTF8.GetByteCount($first) + 1) -or
            $receiptBytes[-1] -ne 10) {
            throw 'Validation receipt sidecar did not bind the exact canonical UTF-8 file bytes.'
        }
    }
    finally {
        Remove-Item -LiteralPath $selfTestReceiptPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath "$selfTestReceiptPath.sha256" -Force -ErrorAction SilentlyContinue
    }
    Write-Output 'Epoch build-validation receipt self-test passed.'
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

foreach ($requiredValue in @{
    Platform = $Platform
    Compiler = $Compiler
    Configuration = $Configuration
    Target = $Target
    Toolchain = $Toolchain
    ArtifactPath = $ArtifactPath
    SourceTreeSha256 = $SourceTreeSha256
    ChecksPath = $ChecksPath
    OutputPath = $OutputPath
}.GetEnumerator()) {
    if ([string]::IsNullOrWhiteSpace([string]$requiredValue.Value)) {
        throw "$($requiredValue.Key) is required."
    }
}

if ($Configuration -cne 'release') {
    throw 'Site admission receipts require an exact Release configuration.'
}
Assert-Identifier -Name 'Target' -Value $Target
Assert-Identifier -Name 'Toolchain' -Value $Toolchain

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$versionModule = Join-Path $repoRoot 'Engine\modules\epoch.version.ixx'
$authorities = Resolve-VersionAuthorities -VersionModule $versionModule -RequestedPlatform $Platform
if ([string]::IsNullOrWhiteSpace($SourceVersion)) {
    $SourceVersion = $authorities.Source
}
if ([string]::IsNullOrWhiteSpace($PackagedVersion)) {
    $PackagedVersion = $authorities.Packaged
}
Assert-SemanticVersion -Name 'SourceVersion' -Value $SourceVersion
Assert-SemanticVersion -Name 'PackagedVersion' -Value $PackagedVersion
if ($SourceVersion -cne $authorities.Source) {
    throw "SourceVersion does not match the tracked source authority $($authorities.Source)."
}
if ($PackagedVersion -cne $authorities.Packaged) {
    throw "PackagedVersion does not match the tracked $Platform authority $($authorities.Packaged)."
}

if ([string]::IsNullOrWhiteSpace($SourceCommit)) {
    $SourceCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to resolve the local committed source identity.'
    }
}
if (-not (Test-LowerHex -Value $SourceCommit -Bytes 20)) {
    throw 'SourceCommit must be one exact lowercase 40-character Git object ID.'
}
if (-not (Test-LowerHex -Value $SourceTreeSha256 -Bytes 32)) {
    throw 'SourceTreeSha256 must be one lowercase SHA-256 digest.'
}

$resolvedArtifact = [System.IO.Path]::GetFullPath($ArtifactPath)
if (-not (Test-Path -LiteralPath $resolvedArtifact -PathType Leaf)) {
    throw "Validation artifact was not found: $resolvedArtifact"
}
$artifactItem = Get-Item -LiteralPath $resolvedArtifact
$artifactSha256 = (Get-FileHash -LiteralPath $resolvedArtifact -Algorithm SHA256).Hash.ToLowerInvariant()
$checks = @(Read-Checks -Path $ChecksPath -RequestedPlatform $Platform)

$receipt = [pscustomobject]@{
    schema = $script:Schema
    source_version = $SourceVersion
    packaged_version = $PackagedVersion
    source_commit = $SourceCommit
    source_tree_sha256 = $SourceTreeSha256
    platform = $Platform
    compiler = $Compiler
    configuration = $Configuration
    target = $Target
    toolchain = $Toolchain
    artifact = [pscustomobject]@{
        name = $artifactItem.Name
        sha256 = $artifactSha256
        size_bytes = [uint64]$artifactItem.Length
    }
    checks = $checks
}

$canonical = ConvertTo-CanonicalReceipt $receipt
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$receiptSha256 = Write-ReceiptDocument -Path $resolvedOutput -Canonical $canonical

Write-Output "Epoch build-validation receipt: $resolvedOutput"
Write-Output "SHA-256: $receiptSha256"
