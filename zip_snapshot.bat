@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Snapshot the contents of this folder (main_epoch), with no extra ZIP root.
rem Usage: zip_snapshot.bat [snapshot_base.zip]
rem Generates project_snapshot_001.zip, _002.zip, etc. Never overwrites older ZIPs.
rem Any optional filename is a prefix: custom.zip -> custom_001.zip, etc.
rem Requires Windows PowerShell 5.1 or later. No external ZIP utility needed.

set "EPOCH_SNAPSHOT_SCRIPT=%~f0"
set "EPOCH_SNAPSHOT_ROOT=%~dp0"
set "EPOCH_SNAPSHOT_TARGET=%~1"
if not defined EPOCH_SNAPSHOT_TARGET set "EPOCH_SNAPSHOT_TARGET=project_snapshot.zip"

powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop'; $source = [IO.File]::ReadAllText($env:EPOCH_SNAPSHOT_SCRIPT); $marker = [regex]::Match($source, '(?m)^# POWERSHELL_SNAPSHOT_SCRIPT\r?$'); if (-not $marker.Success) { throw 'Embedded PowerShell script missing.' }; & ([ScriptBlock]::Create($source.Substring($marker.Index + $marker.Length)))"

set "SNAPSHOT_RESULT=%ERRORLEVEL%"
if not "%SNAPSHOT_RESULT%"=="0" echo ERROR: Snapshot failed. Existing snapshots were not modified.
exit /b %SNAPSHOT_RESULT%

# POWERSHELL_SNAPSHOT_SCRIPT
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = [System.IO.Path]::GetFullPath($env:EPOCH_SNAPSHOT_ROOT)
if (-not $root.EndsWith([System.IO.Path]::DirectorySeparatorChar.ToString())) {
    $root += [System.IO.Path]::DirectorySeparatorChar
}

$requested = $env:EPOCH_SNAPSHOT_TARGET
if ([System.IO.Path]::IsPathRooted($requested)) {
    $zipPath = [System.IO.Path]::GetFullPath($requested)
} else {
    $zipPath = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($root, $requested))
}
if (-not [string]::Equals([System.IO.Path]::GetExtension($zipPath), '.zip',
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output filename must end in .zip.'
}

# Directory names are excluded at ANY depth, case-insensitively.
# Keep two explicitly documented exceptions: Engine/src/build is C++ source,
# and Engine/docs/build contains project documentation.
$skipDirectoryNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
@(
    '.git', '.vs', '.idea',
    'Debug', 'Release', 'ReleaseWithDebugInfo', 'RelWithDebInfo', 'MinSizeRel',
    'x64', 'x86', 'out', 'build', 'build_clang', 'build_gcc', 'built',
    'vcpkg_installed', 'bin', 'backup', 'app1',
    'Cpp20_Ultimate_Project_Updater', 'atlas_dump',
    'CMakeFiles', 'ipch', 'obj', '.cache', '__pycache__', 'node_modules'
) | ForEach-Object { [void]$skipDirectoryNames.Add($_) }

# Path-specific generated data; do NOT globally exclude 'workspace' or 'api'.
$skipDirectoryPaths = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
@(
    'Engine/examples/EpochEditor/workspace',
    'Engine/docs/api'
) | ForEach-Object { [void]$skipDirectoryPaths.Add($_) }

# File types that should never be included in a SOURCE snapshot.
# Do NOT exclude *.cmake: the project has real CMake source/configuration files.
$skipFilePatterns = @(
    '*.d', '*.slo', '*.lo', '*.o', '*.obj', '*.gch', '*.pch',
    '*.so', '*.so.*', '*.dylib', '*.dll', '*.mod', '*.smod',
    '*.lai', '*.la', '*.a', '*.lib', '*.exe', '*.out', '*.app',
    '*.ninja_log', '*.pdb', '*.idb', '*.log', '*.tlog',
    '*.bin', '*.zip', '*.zip.*.tmp', '*.ilk', '*.exp', '*.ipch', '*.lastbuildstate',
    '*.pyc', '*.pyo', '*.ninja_deps', '*.suo', '*.sdf',
    '*.opensdf', '*.vc.db', '*.vc.opendb', '*.vcxproj.user',
    'CMakeCache.txt', 'cmake_install.cmake', 'CTestTestfile.cmake',
    'CPackConfig.cmake', 'CPackSourceConfig.cmake'
)

function SkipFile([string]$name) {
    foreach ($pattern in $skipFilePatterns) {
        if ($name -like $pattern) { return $true }
    }
    return $false
}

function SetEntryTime($entry, [datetime]$localTime) {
    # ZIP timestamps must be in [1980, 2107].
    if ($localTime.Year -lt 1980) { $localTime = [datetime]::new(1980, 1, 1) }
    if ($localTime.Year -gt 2107) { $localTime = [datetime]::new(2107, 12, 31, 23, 59, 58) }
    $entry.LastWriteTime = [System.DateTimeOffset]::new($localTime)
}

$comparison = [System.StringComparison]::OrdinalIgnoreCase
$zipDir = [System.IO.Path]::GetDirectoryName($zipPath)
if (-not [System.IO.Directory]::Exists($zipDir)) {
    [void][System.IO.Directory]::CreateDirectory($zipDir)
}

# The requested ZIP is a naming base, not a file to overwrite.
# Find the greatest existing suffix so deleting _001 never reuses that number
# while higher-numbered snapshots remain. Number width grows beyond 999.
$baseName = [System.IO.Path]::GetFileNameWithoutExtension($zipPath)
$numberedName = '^{0}_(\d+)\.zip$' -f [regex]::Escape($baseName)
$highestNumber = [long]0
foreach ($existing in [System.IO.Directory]::EnumerateFiles($zipDir, '*.zip')) {
    $match = [regex]::Match([System.IO.Path]::GetFileName($existing),
        $numberedName, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if ($match.Success) {
        $n = [long]0
        if ([long]::TryParse($match.Groups[1].Value, [ref]$n) -and
            $n -gt $highestNumber) {
            $highestNumber = $n
        }
    }
}
if ($highestNumber -eq [long]::MaxValue) {
    throw 'Snapshot sequence number exhausted.'
}
$nextNumber = $highestNumber + 1

# Write to a temporary file on the destination volume, verify it fully,
# then rename it into place. Temporary ZIPs are filtered from the snapshot,
# including stale or concurrent staging files from other runs.
$tempZip = $zipPath + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
$hashes = [System.Collections.Generic.Dictionary[string,string]]::new(
    [System.StringComparer]::Ordinal)
$fileCount = 0
$directoryCount = 0
$skippedDirectoryCount = 0
$skippedFileCount = 0
$buffer = New-Object byte[] (256 * 1024)

Write-Host "SOURCE: $root"
Write-Host "OUTPUT SERIES: $zipDir\${baseName}_###.zip"

try {
    $stream = [System.IO.File]::Open($tempZip, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $archive = [System.IO.Compression.ZipArchive]::new(
            $stream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $pending = [System.Collections.Generic.Stack[string]]::new()
            $pending.Push($root)
            while ($pending.Count -gt 0) {
                $directory = $pending.Pop()
                if (-not [string]::Equals($directory, $root, $comparison)) {
                    $relativeDirectory = $directory.Substring($root.Length).Replace('\', '/')
                    $folderEntry = $archive.CreateEntry($relativeDirectory + '/')
                    SetEntryTime $folderEntry ([System.IO.Directory]::GetLastWriteTime($directory))
                    $directoryCount++
                }

                foreach ($path in [System.IO.Directory]::EnumerateFileSystemEntries($directory)) {
                    # The staging and final archives MUST NOT zip themselves.
                    if ([string]::Equals($path, $tempZip, $comparison) -or
                        [string]::Equals($path, $zipPath, $comparison)) { continue }

                    $relative = $path.Substring($root.Length).Replace('\', '/')
                    $leaf = [System.IO.Path]::GetFileName($path)
                    $attributes = [System.IO.File]::GetAttributes($path)
                    $isDirectory = ($attributes -band [System.IO.FileAttributes]::Directory) -ne 0
                    if ($isDirectory) {
                        $excludedName = $skipDirectoryNames.Contains($leaf) -or
                            $leaf.StartsWith('cmake-build-', $comparison)
                        if ([string]::Equals($relative, 'Engine/src/build', $comparison) -or
                            [string]::Equals($relative, 'Engine/docs/build', $comparison)) {
                            $excludedName = $false
                        }
                        if ($excludedName -or $skipDirectoryPaths.Contains($relative)) {
                            $skippedDirectoryCount++
                            continue
                        }
                    } elseif (SkipFile $leaf) {
                        $skippedFileCount++
                        continue
                    }

                    # Junctions/symlinks cannot be safely followed in a portable source ZIP.
                    # Report failure instead of silently making a partial archive or looping.
                    if (($attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                        throw "Symbolic link or junction requires explicit handling: $relative"
                    }

                    if ($isDirectory) {
                        $pending.Push($path)
                        continue
                    }

                    $lastWriteUtc = [System.IO.File]::GetLastWriteTimeUtc($path)
                    $source = [System.IO.File]::Open($path, [System.IO.FileMode]::Open,
                        [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
                    try {
                        $originalLength = $source.Length
                        $entry = $archive.CreateEntry($relative,
                            [System.IO.Compression.CompressionLevel]::Optimal)
                        SetEntryTime $entry ([System.IO.File]::GetLastWriteTime($path))
                        $destination = $entry.Open()
                        try {
                            $sha = [System.Security.Cryptography.SHA256]::Create()
                            try {
                                $total = [long]0
                                while (($read = $source.Read($buffer, 0, $buffer.Length)) -gt 0) {
                                    [void]$sha.TransformBlock($buffer, 0, $read, $buffer, 0)
                                    $destination.Write($buffer, 0, $read)
                                    $total += $read
                                }
                                [void]$sha.TransformFinalBlock($buffer, 0, 0)
                                $hashes.Add($relative, [Convert]::ToBase64String($sha.Hash))
                            } finally {
                                $sha.Dispose()
                            }
                        } finally {
                            $destination.Dispose()
                        }
                        if ($total -ne $originalLength -or
                            $source.Length -ne $originalLength -or
                            [System.IO.File]::GetLastWriteTimeUtc($path) -ne $lastWriteUtc) {
                            throw "Source changed during snapshot: $relative"
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

    # Test the finished staging ZIP, including complete byte-level SHA-256 checks
    # of all entries before replacing any existing snapshot.
    $check = [System.IO.Compression.ZipFile]::OpenRead($tempZip)
    try {
        if ($check.Entries.Count -ne ($fileCount + $directoryCount)) {
            throw 'ZIP verification failed: entry count differs.'
        }
        $verifiedCount = 0
        foreach ($entry in $check.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            if (-not $hashes.ContainsKey($entry.FullName)) {
                throw "ZIP verification failed: unexpected entry $($entry.FullName)"
            }
            $entryStream = $entry.Open()
            try {
                $sha = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $digest = [Convert]::ToBase64String($sha.ComputeHash($entryStream))
                } finally {
                    $sha.Dispose()
                }
            } finally {
                $entryStream.Dispose()
            }
            if ($digest -cne $hashes[$entry.FullName]) {
                throw "ZIP verification failed: corrupted entry $($entry.FullName)"
            }
            $verifiedCount++
        }
        if ($verifiedCount -ne $fileCount) {
            throw 'ZIP verification failed: file count differs.'
        }
    } finally {
        $check.Dispose()
    }

    # Never overwrite a prior ZIP. File.Move fails if a destination exists.
    # If two runs chose the same number, the loser tries the next number.
    while ($true) {
        $zipPath = [System.IO.Path]::Combine($zipDir,
            ('{0}_{1:D3}.zip' -f $baseName, $nextNumber))
        try {
            [System.IO.File]::Move($tempZip, $zipPath)
            break
        } catch [System.IO.IOException] {
            if (-not [System.IO.File]::Exists($zipPath)) { throw }
            if ($nextNumber -eq [long]::MaxValue) {
                throw 'Snapshot sequence number exhausted.'
            }
            $nextNumber++
        }
    }

    $size = ([System.IO.FileInfo]$zipPath).Length
    Write-Host "CREATED: $zipPath"
    Write-Host "DONE: $fileCount files, $directoryCount folders; verified $verifiedCount files"
    Write-Host "SKIPPED: $skippedDirectoryCount directory trees, $skippedFileCount files"
    Write-Host "ZIP SIZE: $size bytes"
} finally {
    if ([System.IO.File]::Exists($tempZip)) {
        [System.IO.File]::Delete($tempZip)
    }
}
