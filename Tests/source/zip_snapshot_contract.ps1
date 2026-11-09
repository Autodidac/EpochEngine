param(
    [string]$SnapshotScript = '',
    [string]$PreviousScript = '',
    [string]$FixtureRoot = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $SnapshotScript) { $SnapshotScript = Join-Path $repo 'zip_snapshot.bat' }
if (-not $FixtureRoot) {
    $FixtureRoot = Join-Path $repo ('Artifacts/Reports/snapshot-contract/' + [guid]::NewGuid().ToString('N'))
}
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts')) + [IO.Path]::DirectorySeparatorChar
if (-not $FixtureRoot.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Snapshot fixtures must stay under this checkout Artifacts directory.'
}
[void][IO.Directory]::CreateDirectory($FixtureRoot)
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Check([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}
function WriteFixture([string]$path, [string]$text) {
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}
function HashBytes([byte[]]$bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToBase64String($sha.ComputeHash($bytes)) }
    finally { $sha.Dispose() }
}
function RunSnapshot([string]$script, [string]$root, [string]$target) {
    $saved = @($env:EPOCH_SNAPSHOT_SCRIPT, $env:EPOCH_SNAPSHOT_ROOT, $env:EPOCH_SNAPSHOT_TARGET)
    try {
        $env:EPOCH_SNAPSHOT_SCRIPT = [IO.Path]::GetFullPath($script)
        $env:EPOCH_SNAPSHOT_ROOT = [IO.Path]::GetFullPath($root)
        $env:EPOCH_SNAPSHOT_TARGET = $target
        $source = [IO.File]::ReadAllText($script)
        $marker = [regex]::Match($source, '(?m)^# POWERSHELL_SNAPSHOT_SCRIPT\r?$')
        Check $marker.Success 'Actual embedded PowerShell owner is missing.'
        & ([ScriptBlock]::Create($source.Substring($marker.Index + $marker.Length)))
    } finally {
        $env:EPOCH_SNAPSHOT_SCRIPT = $saved[0]
        $env:EPOCH_SNAPSHOT_ROOT = $saved[1]
        $env:EPOCH_SNAPSHOT_TARGET = $saved[2]
    }
}
function ReadPayload([string]$path) {
    $result = @{}
    $archive = [IO.Compression.ZipFile]::OpenRead($path)
    try {
        foreach ($entry in $archive.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            $stream = $entry.Open()
            $memory = [IO.MemoryStream]::new()
            try { $stream.CopyTo($memory); $result[$entry.FullName] = HashBytes $memory.ToArray() }
            finally { $memory.Dispose(); $stream.Dispose() }
        }
    } finally { $archive.Dispose() }
    return $result
}

$sourceRoot = Join-Path $FixtureRoot 'source'
$expected = @{}
@(
    'README.md', 'Engine/src/build/build.real.cpp', 'Engine/docs/build/guide.md',
    'Tools/build/tool.cmake', 'Engine/ai/README.md', 'Library/build/consumer.cmake',
    'Library/workspace/source.txt', 'Library/api/source.hpp', 'Library/ai/source.cpp',
    'Library/Artifacts/source.txt'
) | ForEach-Object {
    $text = 'Real source: ' + $_
    $path = Join-Path $sourceRoot $_
    WriteFixture $path $text
    $expected[$_] = HashBytes ([IO.File]::ReadAllBytes($path))
}
$component = 'Engine/dep/EpochSelfCoding'
$heldNames = @('anchors', 'bootstrap', 'bootstrap_anchors', 'seed')
$policyName = $component + '/data_distribution.json'
$policy = [ordered]@{
    schema = 'epoch.self-coding-data-distribution.v1'
    held_paths = @($heldNames | ForEach-Object { 'assets/ai/native/training/' + $_ + '.jsonl' })
}
WriteFixture (Join-Path $sourceRoot $policyName) ($policy | ConvertTo-Json)
$expected[$policyName] = HashBytes ([IO.File]::ReadAllBytes((Join-Path $sourceRoot $policyName)))
foreach ($name in $heldNames) {
    WriteFixture (Join-Path $sourceRoot ($component + '/assets/ai/native/training/' + $name + '.jsonl')) 'Held fixture seed; no real training data.'
}
$approved = $component + '/assets/ai/native/training/approved.jsonl'
WriteFixture (Join-Path $sourceRoot $approved) 'Approved synthetic fixture.'
$expected[$approved] = HashBytes ([IO.File]::ReadAllBytes((Join-Path $sourceRoot $approved)))
$excluded = @('.codex', '.agents', 'build', 'Artifacts', 'addons', 'Projects', 'cache', 'logs', 'ai',
    'Engine/Bin', 'Engine/build', 'Engine/built', 'Engine/src/build/cache',
    'Engine/examples/EpochEditor/workspace', 'Engine/docs/api')
foreach ($directory in $excluded) {
    WriteFixture (Join-Path $sourceRoot ($directory + '/must-not-open/private.txt')) 'Excluded fixture state.'
}
$disk = Join-Path $sourceRoot 'Artifacts/ToolEnvironments/fixture.vhdx'
WriteFixture $disk 'Tiny dummy disk; never a real VM.'
$numbered = Join-Path $sourceRoot 'snapshot_007.zip'
$base = Join-Path $sourceRoot 'snapshot.zip'
WriteFixture $numbered 'Existing immutable numbered archive.'
WriteFixture $base 'Existing immutable base file.'
$numberedHash = HashBytes ([IO.File]::ReadAllBytes($numbered))
$baseHash = HashBytes ([IO.File]::ReadAllBytes($base))

# Old owner proof is tiny and contains no real project data or large disk.
$oldEvidence = $null
if ($PreviousScript) {
    $oldRoot = Join-Path $FixtureRoot 'old-source'
    WriteFixture (Join-Path $oldRoot 'Tools/build/source.cmake') 'Must be included.'
    WriteFixture (Join-Path $oldRoot 'Artifacts/ToolEnvironments/fixture.vhdx') 'Must be pruned.'
    WriteFixture (Join-Path $oldRoot 'ai/private-state.txt') 'Must be pruned.'
    RunSnapshot $PreviousScript $oldRoot 'old.zip'
    $oldPayload = ReadPayload (Join-Path $oldRoot 'old_001.zip')
    Check ($oldPayload.ContainsKey('Artifacts/ToolEnvironments/fixture.vhdx')) 'Old owner did not reproduce the generated-root inclusion.'
    Check ($oldPayload.ContainsKey('ai/private-state.txt')) 'Old owner did not reproduce private runtime-state inclusion.'
    Check (-not $oldPayload.ContainsKey('Tools/build/source.cmake')) 'Old owner did not reproduce source build-folder omission.'
    $oldEvidence = [ordered]@{generated_disk_included=$true; private_state_included=$true; source_build_folder_omitted=$true}
}

$guard = Join-Path $sourceRoot 'Artifacts'
$originalAcl = Get-Acl -LiteralPath $guard
$acl = Get-Acl -LiteralPath $guard
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User
$deny = [Security.AccessControl.FileSystemAccessRule]::new($sid,
    [Security.AccessControl.FileSystemRights]::ListDirectory,
    [Security.AccessControl.AccessControlType]::Deny)
[void]$acl.AddAccessRule($deny)
$locked = [IO.File]::Open($disk, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$unreadableProven = $false
try {
    Set-Acl -LiteralPath $guard -AclObject $acl
    try {
        $enumerator = [IO.Directory]::EnumerateFileSystemEntries($guard).GetEnumerator()
        [void]$enumerator.MoveNext()
    } catch [UnauthorizedAccessException] { $unreadableProven = $true }
    Check $unreadableProven 'Fixture directory did not reject traversal.'
    if ($PreviousScript) {
        $oldFailed = $false
        try { RunSnapshot $PreviousScript $sourceRoot 'old-protected.zip' }
        catch { $oldFailed = $true }
        Check $oldFailed 'Old owner unexpectedly traversed the protected fixture.'
    }
    RunSnapshot $SnapshotScript $sourceRoot 'snapshot.zip'
    Check ([IO.File]::Exists((Join-Path $sourceRoot 'snapshot_008.zip'))) 'Snapshot did not continue after the greatest existing suffix.'
    RunSnapshot $SnapshotScript $sourceRoot 'snapshot.zip'
    Check ([IO.File]::Exists((Join-Path $sourceRoot 'snapshot_009.zip'))) 'Second snapshot reused or overwrote a number.'
} finally {
    Set-Acl -LiteralPath $guard -AclObject $originalAcl
    $locked.Dispose()
}
Check ((HashBytes ([IO.File]::ReadAllBytes($numbered))) -ceq $numberedHash) 'Existing numbered archive changed.'
Check ((HashBytes ([IO.File]::ReadAllBytes($base))) -ceq $baseHash) 'Existing base file changed.'
$payload = ReadPayload (Join-Path $sourceRoot 'snapshot_009.zip')
Check ($payload.Count -eq $expected.Count) 'Flat snapshot contains unexpected or missing files.'
foreach ($name in $expected.Keys) {
    Check ($payload.ContainsKey($name)) ('Real source missing: ' + $name)
    Check ($payload[$name] -ceq $expected[$name]) ('ZIP SHA256 mismatch: ' + $name)
}

$outside = Join-Path $FixtureRoot 'junction-target'
WriteFixture (Join-Path $outside 'protected.txt') 'Must remain untouched.'
$junction = Join-Path $sourceRoot 'source-junction'
[void](New-Item -ItemType Junction -Path $junction -Target $outside)
try {
    $rejected = $false
    try { RunSnapshot $SnapshotScript $sourceRoot 'junction.zip' }
    catch { $rejected = $_.Exception.Message -like '*junction*' }
    Check $rejected 'Source junction was followed or silently omitted.'
    Check (-not [IO.File]::Exists((Join-Path $sourceRoot 'junction_001.zip'))) 'Failed junction run published a partial archive.'
} finally {
    Check (([IO.File]::GetAttributes($junction) -band [IO.FileAttributes]::ReparsePoint) -ne 0) 'Fixture cleanup path is not a junction.'
    [IO.Directory]::Delete($junction)
}
Check ([IO.File]::ReadAllText((Join-Path $outside 'protected.txt')) -ceq 'Must remain untouched.') 'Junction target changed.'
Check (@([IO.Directory]::EnumerateFiles($sourceRoot, '*.zip.*.tmp')).Count -eq 0) 'Failed snapshot left staging files.'
$policyPath = Join-Path $sourceRoot $policyName
$originalPolicy = [IO.File]::ReadAllBytes($policyPath)
try {
    $invalidPolicy = [ordered]@{schema=$policy.schema; held_paths=@('../outside.jsonl', 'duplicate', 'duplicate', 'seed')}
    WriteFixture $policyPath ($invalidPolicy | ConvertTo-Json)
    $heldRefused = $false
    try { RunSnapshot $SnapshotScript $sourceRoot 'invalid-hold.zip' }
    catch { $heldRefused = $_.Exception.Message -like '*distribution*' }
    Check $heldRefused 'Changed private seed policy did not fail closed.'
    Check (-not [IO.File]::Exists((Join-Path $sourceRoot 'invalid-hold_001.zip'))) 'Invalid policy published a source snapshot.'
} finally { [IO.File]::WriteAllBytes($policyPath, $originalPolicy) }

# Exercise the BAT entry point as well as its exact embedded owner. An ordinary
# source folder without any Epoch-specific component needs no extra tools/data.
$generic = Join-Path $FixtureRoot 'generic-source'
WriteFixture (Join-Path $generic 'source.txt') 'Generic source folder.'
Copy-Item -LiteralPath $SnapshotScript -Destination (Join-Path $generic 'zip_snapshot.bat')
Push-Location $generic
try {
    & cmd.exe /d /c 'zip_snapshot.bat entrypoint.zip'
    Check ($LASTEXITCODE -eq 0) 'Actual BAT entry point failed.'
} finally { Pop-Location }
$genericPayload = ReadPayload (Join-Path $generic 'entrypoint_001.zip')
Check ($genericPayload.Count -eq 2 -and $genericPayload.ContainsKey('source.txt') -and
       $genericPayload.ContainsKey('zip_snapshot.bat')) 'Generic BAT entry point did not produce a flat source ZIP.'
$result = [ordered]@{
    passed=$true; powershell=$PSVersionTable.PSVersion.ToString(); source_files=$expected.Count;
    unreadable_excluded_tree=$unreadableProven; exclusively_locked_dummy_vhdx=$true;
    flat_payload_sha256_verified=$true; sequence_and_no_overwrite=$true; junction_refused=$true;
    exact_private_seed_hold=$true; invalid_private_seed_policy_refused=$true;
    actual_bat_entrypoint=$true; generic_without_component=$true;
    old_owner=$oldEvidence; full_checkout_zip_ran=$false
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $FixtureRoot 'result.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 5
