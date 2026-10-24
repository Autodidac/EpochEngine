@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Save this file in main_epoch beside zip_snapshot.bat.
rem Recursively collect ONLY log files from the ENTIRE main_epoch tree,
rem including Engine, .hidden folders, workspace, caches and iterations.
rem Includes *.log, *.tlog, *.binlog, rotated logs and .ninja_log.
rem Does not collect source, binaries, directories, or ZIP files.
rem Usage: zip_logs.bat [logs.zip]
rem Output: epoch_logs_001.zip, epoch_logs_002.zip, etc. No overwrites.
rem Requires built-in Windows PowerShell 5.1 (no third-party utilities).

set "EPOCH_LOG_SCRIPT=%~f0"
set "EPOCH_LOG_ROOT=%~dp0"
set "EPOCH_LOG_TARGET=%~1"
if not defined EPOCH_LOG_TARGET set "EPOCH_LOG_TARGET=epoch_logs.zip"

powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; $s=[IO.File]::ReadAllText($env:EPOCH_LOG_SCRIPT); $m=[regex]::Match($s,'(?m)^# POWERSHELL_LOG_SCRIPT\r?$'); if(-not $m.Success){throw 'Embedded PowerShell script missing.'}; & ([ScriptBlock]::Create($s.Substring($m.Index+$m.Length)))"

set "LOG_SNAPSHOT_RESULT=%ERRORLEVEL%"
if not "%LOG_SNAPSHOT_RESULT%"=="0" echo ERROR: Log snapshot failed. Existing ZIPs were not changed.
exit /b %LOG_SNAPSHOT_RESULT%

# POWERSHELL_LOG_SCRIPT
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Use the extended-length path prefix on ALL source operations. Generated
# worktree paths can exceed MAX_PATH, especially inside iteration caches.
function ToExtendedPath([string]$path) {
    if ($path.StartsWith('\\?\')) { return $path }
    if ($path.StartsWith('\\')) { return '\\?\UNC\' + $path.Substring(2) }
    return '\\?\' + $path
}

function IsLogFile([string]$name) {
    # Explicitly reject ZIPs even if someone names one example.log.zip.
    if ($name -match '(?i)\.zip(?:\.|$)') { return $false }
    # Match extensions, not arbitrary suffixes following ".log". The old
    # expression matched core.log.cpp, core.log.ixx, core.log.obj and IFC
    # artifacts. Keep genuine rotated/compressed logs without source files.
    if ($name -match '(?i)\.(?:log|tlog|binlog)$') { return $true }
    if ($name -match '(?i)\.(?:log|tlog|binlog)\.(?:[0-9]+|old|bak|gz|bz2|xz|zst|[0-9]+\.(?:gz|bz2|xz|zst))$') { return $true }
    return ($name -match '(?i)^\.ninja_log(?:\.[0-9]+)?$')
}

function SetEntryTime($entry, [datetime]$time) {
    if ($time.Year -lt 1980) { $time = [datetime]::new(1980, 1, 1) }
    if ($time.Year -gt 2107) { $time = [datetime]::new(2107, 12, 31, 23, 59, 58) }
    $entry.LastWriteTime = [datetimeoffset]::new($time)
}

$root = [IO.Path]::GetFullPath($env:EPOCH_LOG_ROOT)
if (-not $root.EndsWith('\')) { $root += '\' }
$longRoot = ToExtendedPath $root

# Limit the output argument to a file name. All ZIPs stay beside this .bat.
$requested = $env:EPOCH_LOG_TARGET
if ([IO.Path]::GetFileName($requested) -cne $requested -or
    -not [string]::Equals([IO.Path]::GetExtension($requested), '.zip',
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Usage: zip_logs.bat [base_name.zip] (filename only).'
}
$baseName = [IO.Path]::GetFileNameWithoutExtension($requested)
if ([string]::IsNullOrWhiteSpace($baseName)) { throw 'Invalid output base name.' }

$numberPattern = '^{0}_(\d+)\.zip$' -f [regex]::Escape($baseName)
$highestNumber = [long]0
foreach ($existing in [IO.Directory]::EnumerateFiles($longRoot, '*.zip')) {
    $match = [regex]::Match([IO.Path]::GetFileName($existing), $numberPattern,
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if ($match.Success) {
        $parsed = [long]0
        if ([long]::TryParse($match.Groups[1].Value, [ref]$parsed) -and
            $parsed -gt $highestNumber) { $highestNumber = $parsed }
    }
}
if ($highestNumber -eq [long]::MaxValue) { throw 'Log snapshot sequence exhausted.' }
$nextNumber = $highestNumber + 1

$temp = [IO.Path]::Combine($longRoot,
    '.epoch-logs-' + [guid]::NewGuid().ToString('N') + '.tmp')
$hashes = [Collections.Generic.Dictionary[string,string]]::new(
    [StringComparer]::Ordinal)
$buffer = New-Object byte[] (256 * 1024)
$readSharing = [IO.FileShare]([int][IO.FileShare]::ReadWrite -bor [int][IO.FileShare]::Delete)
$fileCount = 0
$skippedLinks = 0
$verified = 0

Write-Host "SEARCH ROOT: $root"
Write-Host 'SEARCH: every folder (including hidden and iterations); log files only'
Write-Host "OUTPUT SERIES: ${baseName}_001.zip, ${baseName}_002.zip, ..."

try {
    $stream = [IO.File]::Open($temp, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $archive = [IO.Compression.ZipArchive]::new(
            $stream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $pending = [Collections.Generic.Stack[string]]::new()
            $pending.Push($longRoot)
            while ($pending.Count -gt 0) {
                $directory = $pending.Pop()
                foreach ($path in [IO.Directory]::EnumerateFileSystemEntries($directory)) {
                    $attributes = [IO.File]::GetAttributes($path)
                    if (($attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                        # Do not follow symlinks/junctions outside the project or in loops.
                        $skippedLinks++
                        continue
                    }
                    if (($attributes -band [IO.FileAttributes]::Directory) -ne 0) {
                        $pending.Push($path)
                        continue
                    }
                    $name = [IO.Path]::GetFileName($path)
                    if (-not (IsLogFile $name)) { continue }

                    $relative = $path.Substring($longRoot.Length).Replace('\', '/')
                    # Read active logs with sharing enabled and cap each read to
                    # the length seen on open, so growing logs cannot hang the ZIP.
                    $source = [IO.File]::Open($path, [IO.FileMode]::Open,
                        [IO.FileAccess]::Read, $readSharing)
                    try {
                        $remaining = $source.Length
                        $entry = $archive.CreateEntry($relative,
                            [IO.Compression.CompressionLevel]::Optimal)
                        SetEntryTime $entry ([IO.File]::GetLastWriteTime($path))
                        $output = $entry.Open()
                        try {
                            $sha = [Security.Cryptography.SHA256]::Create()
                            try {
                                while ($remaining -gt 0) {
                                    $count = [int][Math]::Min([long]$buffer.Length, $remaining)
                                    $read = $source.Read($buffer, 0, $count)
                                    if ($read -le 0) { throw "Log truncated while reading: $relative" }
                                    [void]$sha.TransformBlock($buffer, 0, $read, $buffer, 0)
                                    $output.Write($buffer, 0, $read)
                                    $remaining -= $read
                                }
                                [void]$sha.TransformFinalBlock($buffer, 0, 0)
                                $hashes.Add($relative, [Convert]::ToBase64String($sha.Hash))
                            } finally {
                                $sha.Dispose()
                            }
                        } finally {
                            $output.Dispose()
                        }
                        $fileCount++
                    } finally {
                        $source.Dispose()
                    }
                }
            }
        } finally {
            $archive.Dispose()
        }
    } finally {
        $stream.Dispose()
    }

    if ($fileCount -eq 0) { throw 'No log files found. Empty ZIP not published.' }

    # Verify EVERY archived log, not just the ZIP central directory.
    $check = [IO.Compression.ZipFile]::OpenRead($temp)
    try {
        if ($check.Entries.Count -ne $fileCount) {
            throw 'ZIP verification failed: unexpected entry count.'
        }
        foreach ($entry in $check.Entries) {
            if (-not $hashes.ContainsKey($entry.FullName) -or
                -not (IsLogFile ([IO.Path]::GetFileName($entry.FullName)))) {
                throw "ZIP verification failed: unexpected file $($entry.FullName)"
            }
            $entryStream = $entry.Open()
            try {
                $sha = [Security.Cryptography.SHA256]::Create()
                try {
                    $digest = [Convert]::ToBase64String($sha.ComputeHash($entryStream))
                } finally {
                    $sha.Dispose()
                }
            } finally {
                $entryStream.Dispose()
            }
            if ($digest -cne $hashes[$entry.FullName]) {
                throw "ZIP verification failed: content mismatch $($entry.FullName)"
            }
            $verified++
        }
    } finally {
        $check.Dispose()
    }

    # Atomic publish: a failed read/check leaves previous ZIPs untouched.
    # Concurrent runs cannot overwrite each other; they choose another number.
    while ($true) {
        $name = '{0}_{1:D3}.zip' -f $baseName, $nextNumber
        $final = [IO.Path]::Combine($longRoot, $name)
        try {
            [IO.File]::Move($temp, $final)
            break
        } catch [IO.IOException] {
            if (-not [IO.File]::Exists($final)) { throw }
            if ($nextNumber -eq [long]::MaxValue) { throw 'Log sequence exhausted.' }
            $nextNumber++
        }
    }

    Write-Host "CREATED: $root$name"
    Write-Host "LOG FILES: $fileCount; SHA-256 VERIFIED: $verified"
    if ($skippedLinks -gt 0) {
        Write-Warning "$skippedLinks symlinks/junctions skipped (not followed outside tree)."
    }
} finally {
    if ([IO.File]::Exists($temp)) { [IO.File]::Delete($temp) }
}
