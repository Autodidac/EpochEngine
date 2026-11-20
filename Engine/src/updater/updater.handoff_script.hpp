// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <string_view>
#if defined(_WIN32)
#include <filesystem>
#include <ostream>
#endif

namespace epochengine::updater::handoff_script
{
    // The installer keeps its prior executable until a checked same-directory
    // replacement is ready. Backups survive success for explicit recovery.
    inline constexpr std::string_view windows = R"epoch(param([ValidateRange(1,60)][int]$Attempts = 60)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($env:EPOCH_UPDATE_ROOT)
$work = [IO.Path]::GetFullPath($env:EPOCH_UPDATE_WORK)
$plan = $env:EPOCH_UPDATE_PLAN
$state = [IO.Path]::Combine($work, 'state')
$journal = [IO.Path]::Combine($work, 'journal.tsv')
$rollbackRequired = $false
function CheckedHash([string]$path) {
    if (![IO.File]::Exists($path)) { throw 'Required payload file is missing.' }
    $stream = [IO.File]::OpenRead($path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $hasher.Dispose(); $stream.Dispose() }
}
function SafePath([string]$base, [string]$relative) {
    if (!$relative -or [IO.Path]::IsPathRooted($relative)) { throw 'Invalid relative payload path.' }
    $path = [IO.Path]::GetFullPath([IO.Path]::Combine($base, $relative))
    if (!$path.StartsWith($base.TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Payload path escapes its owner.'
    }
    $probe = $path
    while ($probe -and $probe -ne $base) {
        if ([IO.File]::Exists($probe) -or [IO.Directory]::Exists($probe)) {
            if (([IO.File]::GetAttributes($probe) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Payload reparse points are not admitted.' }
        }
        $probe = [IO.Path]::GetDirectoryName($probe)
    }
    return $path
}
function CopyNew([string]$source, [string]$destination) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
    $input = [IO.File]::OpenRead($source)
    try {
        $output = [IO.File]::Open($destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $input.CopyTo($output); $output.Flush($true) } finally { $output.Dispose() }
    } finally { $input.Dispose() }
}
function AtomicInstall([string]$source, [string]$destination) {
    for ($attempt = 1; $attempt -le $Attempts; ++$attempt) {
        try {
            if ([IO.File]::Exists($destination)) { [IO.File]::Replace($source, $destination, [NullString]::Value) }
            else { [IO.File]::Move($source, $destination) }
            return
        } catch {
            if ($attempt -eq $Attempts) { throw ('Cannot replace ' + $destination + ': ' + $_.Exception.Message) }
            Start-Sleep -Seconds 1
        }
    }
}
function WriteState([string]$value) {
    [IO.File]::WriteAllText($state + '.next', $value)
    for ($i = 0; $i -lt 20; ++$i) {
        try {
            if ([IO.File]::Exists($state)) { [IO.File]::Replace($state + '.next', $state, [NullString]::Value) }
            else { [IO.File]::Move($state + '.next', $state) }
            return
        } catch {
            if ($i -eq 19) { throw ('Cannot record handoff state: ' + $_.Exception.Message) }
            Start-Sleep -Milliseconds 50
        }
    }
}
function ReadRows([string]$path) {
    $rows = New-Object 'Collections.Generic.List[object]'
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        $fields = $line.Split([char]9)
        if ($fields.Length -ne 3) { throw 'Invalid payload manifest row.' }
        $rows.Add($fields)
    }
    return ,$rows.ToArray()
}
function RestorePayload {
    WriteState 'rolling_back'
    $rows = ReadRows $journal
    for ($i = $rows.Length - 1; $i -ge 0; --$i) {
        $row = $rows[$i]
        $target = SafePath $root $row[0]
        $old = $row[1]; $new = $row[2]
        $current = if ([IO.File]::Exists($target)) { CheckedHash $target } else { '-' }
        if ($current -eq $old) { continue }
        if ($current -ne $new -and $current -ne '-') { throw 'Rollback refused an unrelated changed file.' }
        if ($old -eq '-') {
            if ([IO.File]::Exists($target)) { [IO.File]::Delete($target) }
        } else {
            $backup = SafePath ([IO.Path]::Combine($work, 'backup')) $row[0]
            if ((CheckedHash $backup) -ne $old) { throw 'Rollback backup digest mismatch.' }
            $recovery = $target + '.epoch-recover-' + [Guid]::NewGuid().ToString('N')
            CopyNew $backup $recovery
            AtomicInstall $recovery $target
            if ((CheckedHash $target) -ne $old) { throw 'Rollback verification failed.' }
        }
    }
    WriteState 'rolled_back'
}
try {
    if ($env:EPOCH_UPDATE_MODE -ne 'recover' -and ($env:EPOCH_UPDATE_PLAN_SHA256 -notmatch '^[a-f0-9]{64}$' -or (CheckedHash $plan) -ne $env:EPOCH_UPDATE_PLAN_SHA256)) { throw 'Payload manifest changed.' }
    if ([IO.Path]::GetDirectoryName($work) -ne $root) { throw 'Update staging must be in the installed directory.' }
    if ($env:EPOCH_UPDATE_MODE -eq 'stage') {
        if ([IO.Directory]::Exists($work) -or [IO.File]::Exists($work)) { throw 'Update staging already exists.' }
        [IO.Directory]::CreateDirectory($work) | Out-Null
        foreach ($row in (ReadRows $plan)) {
            if ($row[2] -notmatch '^[a-f0-9]{64}$') { throw 'Missing payload digest.' }
            $target = SafePath $root $row[1]
            $staged = SafePath ([IO.Path]::Combine($work, 'staged')) $row[1]
            if ((CheckedHash $row[0]) -ne $row[2]) { throw 'Admitted payload changed.' }
            CopyNew $row[0] $staged
            if ((CheckedHash $staged) -ne $row[2]) { throw 'Staged payload digest mismatch.' }
        }
        WriteState 'staged'
        exit 0
    }
    if ($env:EPOCH_UPDATE_MODE -eq 'recover') {
        if (![IO.File]::Exists($journal)) { throw 'No recovery journal exists.' }
        RestorePayload
        exit 0
    }
    if ($env:EPOCH_UPDATE_MODE -ne 'commit') { throw 'Unknown handoff operation.' }
    if (![IO.File]::Exists($state) -or [IO.File]::ReadAllText($state) -ne 'staged' -or [IO.File]::Exists($journal)) {
        throw 'Incomplete prior handoff requires recovery before another commit.'
    }
    $journalRows = New-Object 'Collections.Generic.List[string]'
    $rows = ReadRows $plan
    foreach ($row in $rows) {
        $target = SafePath $root $row[1]
        $staged = SafePath ([IO.Path]::Combine($work, 'staged')) $row[1]
        if ((CheckedHash $staged) -ne $row[2]) { throw 'Staged payload changed before installation.' }
        if ([IO.Directory]::Exists($target)) { throw 'Payload destination is a directory.' }
        $old = '-'
        if ([IO.File]::Exists($target)) {
            $old = CheckedHash $target
            if ($old -ne $row[2]) {
                $backup = SafePath ([IO.Path]::Combine($work, 'backup')) $row[1]
                CopyNew $target $backup
                if ((CheckedHash $backup) -ne $old) { throw 'Backup verification failed.' }
            }
        }
        $journalRows.Add($row[1] + "`t" + $old + "`t" + $row[2])
    }
    [IO.File]::WriteAllLines($journal, $journalRows.ToArray())
    WriteState 'committing'
    $rollbackRequired = $true
    for ($i = 0; $i -lt $rows.Length; ++$i) {
        $row = $rows[$i]
        $target = SafePath $root $row[1]
        $staged = SafePath ([IO.Path]::Combine($work, 'staged')) $row[1]
        $old = $journalRows[$i].Split([char]9)[1]
        $current = if ([IO.File]::Exists($target)) { CheckedHash $target } else { '-' }
        if ($current -ne $old) { throw 'Installed payload changed during handoff.' }
        if ($current -eq $row[2]) { continue }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
        AtomicInstall $staged $target
        if ((CheckedHash $target) -ne $row[2]) { throw 'Installed payload digest mismatch.' }
    }
    WriteState 'complete'
    $rollbackRequired = $false
    exit 0
} catch {
    $reason = $_.Exception.Message
    if ($rollbackRequired) {
        try { RestorePayload } catch { $reason += '; recovery remains required: ' + $_.Exception.Message }
    }
    if ($env:LOG) { [IO.File]::AppendAllText($env:LOG, '[ERROR] Update handoff refused: ' + $reason + "`n") }
    exit 1
}
)epoch";

#if defined(_WIN32)
    inline void write_windows_restart(std::ostream& script, const std::filesystem::path& candidate_data)
    {
        std::string_view arguments;
        if (!candidate_data.empty())
        {
            // Use ASCII script text while preserving the path's UTF-16 units.
            script << "$candidateDataRoot = [string]::new([char[]]@(";
            const char* separator = "";
            for (const wchar_t unit : candidate_data.native())
            {
                script << separator << static_cast<unsigned>(unit);
                separator = ",";
            }
            script << "))\n";
            arguments = R"epoch( -ArgumentList @('--candidate-data-root', ('"' + $candidateDataRoot + '"')))epoch";
        }
        script << "$restartedProcess = Start-Process -FilePath $targetExe -WorkingDirectory $targetDir -PassThru"
               << arguments << '\n';
    }

    inline void write_windows_transaction(std::ostream& script, const std::filesystem::path& candidate_data)
    {
        auto transaction = windows;
        if (!candidate_data.empty())
        {
            // PowerShell requires param(...) before the optional restart path.
            constexpr auto parameter_end = windows.find('\n') + 1u;
            static_assert(parameter_end != 0u);
            script << transaction.substr(0, parameter_end);
            transaction.remove_prefix(parameter_end);
            script << "if ($env:EPOCH_UPDATE_MODE -eq 'restart') {\n"
                      "  $ErrorActionPreference = 'Stop'\n"
                      "  $targetExe = $env:EPOCH_UPDATE_RESTART_EXE\n"
                      "  $targetDir = [IO.Path]::GetDirectoryName($targetExe)\n";
            write_windows_restart(script, candidate_data);
            script << "  exit 0\n}\n";
        }
        script << transaction;
    }
#endif

    inline constexpr std::string_view posix = R"epoch(epoch_payload_hash() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d ' ' -f 1
  elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d ' ' -f 1
  else return 1; fi
}
epoch_safe_relative() {
  case "$1" in ''|/*|..|../*|*/../*|*/..|./*|*/./*|*/.|*'\'*) return 1;; esac
}
epoch_no_links() {
  epoch_path=$1
  while [ "$epoch_path" != "$EPOCH_UPDATE_ROOT" ] && [ "$epoch_path" != / ]; do
    [ ! -L "$epoch_path" ] || return 1
    epoch_path=$(dirname "$epoch_path")
  done
}
epoch_write_state() {
  printf '%s' "$1" > "$EPOCH_UPDATE_WORK/state.next" || return 1
  mv -f "$EPOCH_UPDATE_WORK/state.next" "$EPOCH_UPDATE_WORK/state" || return 1
}
epoch_verify_plan() {
  [ -f "$EPOCH_UPDATE_PLAN" ] && [ ! -L "$EPOCH_UPDATE_PLAN" ] || return 1
  [ "$(epoch_payload_hash "$EPOCH_UPDATE_PLAN")" = "$EPOCH_UPDATE_PLAN_SHA256" ] || return 1
}
epoch_stage_payload() {
  epoch_verify_plan || return 1
  [ "$(dirname "$EPOCH_UPDATE_WORK")" = "$EPOCH_UPDATE_ROOT" ] || return 1
  mkdir "$EPOCH_UPDATE_WORK" || return 1
  epoch_tab=$(printf '\t')
  while IFS="$epoch_tab" read -r epoch_source epoch_relative epoch_hash; do
    epoch_safe_relative "$epoch_relative" || return 1
    epoch_no_links "$EPOCH_UPDATE_ROOT/$epoch_relative" || return 1
    case "$epoch_hash" in ''|*[!0-9a-f]*) return 1;; esac
    [ "${#epoch_hash}" -eq 64 ] || return 1
    [ -f "$epoch_source" ] && [ ! -L "$epoch_source" ] || return 1
    [ "$(epoch_payload_hash "$epoch_source")" = "$epoch_hash" ] || return 1
    epoch_staged="$EPOCH_UPDATE_WORK/staged/$epoch_relative"
    mkdir -p "$(dirname "$epoch_staged")" || return 1
    (set -C; : > "$epoch_staged") || return 1
    cp -p "$epoch_source" "$epoch_staged" || return 1
    [ "$(epoch_payload_hash "$epoch_staged")" = "$epoch_hash" ] || return 1
  done < "$EPOCH_UPDATE_PLAN"
  epoch_write_state staged
}
epoch_restore_payload() {
  [ -f "$EPOCH_UPDATE_WORK/journal.tsv" ] || return 1
  epoch_write_state rolling_back || return 1
  epoch_tab=$(printf '\t')
  # Originals remain immutable; restart recovery from the full journal after interruption.
  awk '{rows[NR]=$0} END {for (i=NR;i>0;--i) print rows[i]}' "$EPOCH_UPDATE_WORK/journal.tsv" > "$EPOCH_UPDATE_WORK/rollback.tsv" || return 1
  while IFS="$epoch_tab" read -r epoch_relative epoch_old epoch_new; do
    epoch_safe_relative "$epoch_relative" || return 1
    epoch_target="$EPOCH_UPDATE_ROOT/$epoch_relative"
    epoch_no_links "$epoch_target" || return 1
    epoch_current=-
    if [ -f "$epoch_target" ]; then epoch_current=$(epoch_payload_hash "$epoch_target") || return 1; fi
    [ "$epoch_current" = "$epoch_old" ] && continue
    [ "$epoch_current" = "$epoch_new" ] || [ "$epoch_current" = - ] || return 1
    if [ "$epoch_old" = - ]; then
      [ ! -e "$epoch_target" ] || rm "$epoch_target" || return 1
    else
      epoch_backup="$EPOCH_UPDATE_WORK/backup/$epoch_relative"
      [ "$(epoch_payload_hash "$epoch_backup")" = "$epoch_old" ] || return 1
      epoch_recovery=$(mktemp "$EPOCH_UPDATE_ROOT/.epoch-recover-XXXXXXXX") || return 1
      cp -p "$epoch_backup" "$epoch_recovery" || return 1
      mv -f "$epoch_recovery" "$epoch_target" || return 1
      [ "$(epoch_payload_hash "$epoch_target")" = "$epoch_old" ] || return 1
    fi
  done < "$EPOCH_UPDATE_WORK/rollback.tsv"
  epoch_write_state rolled_back
}
epoch_commit_payload() {
  epoch_mutating=0
  epoch_verify_plan || return 1
  [ -f "$EPOCH_UPDATE_WORK/state" ] && [ "$(cat "$EPOCH_UPDATE_WORK/state")" = staged ] || return 1
  [ ! -e "$EPOCH_UPDATE_WORK/journal.tsv" ] || return 1
  epoch_tab=$(printf '\t')
  : > "$EPOCH_UPDATE_WORK/journal.next" || return 1
  while IFS="$epoch_tab" read -r epoch_source epoch_relative epoch_hash; do
    epoch_safe_relative "$epoch_relative" || return 1
    epoch_target="$EPOCH_UPDATE_ROOT/$epoch_relative"
    epoch_staged="$EPOCH_UPDATE_WORK/staged/$epoch_relative"
    epoch_no_links "$epoch_target" || return 1
    [ ! -d "$epoch_target" ] || return 1
    [ "$(epoch_payload_hash "$epoch_staged")" = "$epoch_hash" ] || return 1
    epoch_old=-
    if [ -e "$epoch_target" ]; then
      [ -f "$epoch_target" ] || return 1
      epoch_old=$(epoch_payload_hash "$epoch_target") || return 1
      epoch_backup="$EPOCH_UPDATE_WORK/backup/$epoch_relative"
      mkdir -p "$(dirname "$epoch_backup")" || return 1
      (set -C; : > "$epoch_backup") || return 1
      cp -p "$epoch_target" "$epoch_backup" || return 1
      [ "$(epoch_payload_hash "$epoch_backup")" = "$epoch_old" ] || return 1
    fi
    printf '%s\t%s\t%s\n' "$epoch_relative" "$epoch_old" "$epoch_hash" >> "$EPOCH_UPDATE_WORK/journal.next" || return 1
  done < "$EPOCH_UPDATE_PLAN"
  mv "$EPOCH_UPDATE_WORK/journal.next" "$EPOCH_UPDATE_WORK/journal.tsv" || return 1
  epoch_write_state committing || return 1
  epoch_mutating=1
  while IFS="$epoch_tab" read -r epoch_relative epoch_old epoch_new; do
    epoch_target="$EPOCH_UPDATE_ROOT/$epoch_relative"
    epoch_current=-
    if [ -f "$epoch_target" ]; then epoch_current=$(epoch_payload_hash "$epoch_target") || return 1; fi
    [ "$epoch_current" = "$epoch_old" ] || return 1
    mkdir -p "$(dirname "$epoch_target")" || return 1
    mv -f "$EPOCH_UPDATE_WORK/staged/$epoch_relative" "$epoch_target" || return 1
    [ "$(epoch_payload_hash "$epoch_target")" = "$epoch_new" ] || return 1
  done < "$EPOCH_UPDATE_WORK/journal.tsv"
  epoch_write_state complete
}
epoch_install_payload() {
  if epoch_commit_payload; then return 0; fi
  if [ "$epoch_mutating" = 1 ] && [ -f "$EPOCH_UPDATE_WORK/journal.tsv" ]; then
    epoch_restore_payload || return 1
  fi
  return 1
}
)epoch";
}
