[CmdletBinding()]
param(
    [switch]$Remove,
    [switch]$LegacyMSVC
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
# Fixed compiler-owned roots only. No arbitrary caller path, projects or runtime.
$relative = if ($LegacyMSVC) { 'build\obj' } else { 'Artifacts\Intermediate' }
$target = [IO.Path]::GetFullPath((Join-Path $repo $relative))
if (-not $target.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Intermediate target escaped the checkout.'
}
if (-not (Test-Path -LiteralPath $target)) {
    Write-Output "No compiler files at $target"
    exit 0
}
# Check every ancestor before traversal, then every entry before removal.
$cursor = Get-Item -LiteralPath $target -Force
while ($cursor) {
    if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing redirected path: $($cursor.FullName)" }
    if ($cursor.FullName.TrimEnd('\','/') -eq $repo.TrimEnd('\','/')) { break }
    $cursor = $cursor.Parent
}
if (-not $cursor) { throw 'Intermediate root has no verified checkout ancestor.' }
$entries = @(Get-ChildItem -LiteralPath $target -Force -Recurse)
if (@($entries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
    throw 'Intermediate tree contains a redirected entry.'
}
$tracked = @(& git -C $repo ls-files -- $relative.Replace('\','/'))
if ($LASTEXITCODE -ne 0 -or $tracked.Count) { throw 'Could not prove that the intermediate tree is untracked.' }
$compilers = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('MSBuild.exe','cl.exe','link.exe','cmake.exe','ninja.exe','clang.exe','clang++.exe','gcc.exe','g++.exe')
})
if ($Remove -and $compilers.Count) { throw 'Finish active compiler/build processes before cleaning intermediates.' }
$files = @($entries | Where-Object { -not $_.PSIsContainer })
$bytes = ($files | Measure-Object -Property Length -Sum).Sum
$report = [ordered]@{
    recorded_utc = [DateTime]::UtcNow.ToString('o')
    target = $target
    files = $files.Count
    bytes = [long]$bytes
    mode = $(if ($Remove) { 'remove' } else { 'preview' })
    removed = $false
    preserved = @('Runtime','Install','Packages','Reports','Projects','ai candidate source')
}
$reportRoot = Join-Path $repo 'Artifacts\Reports\cleanup'
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
$receipt = Join-Path $reportRoot ([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff') + '.json')
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('{0}: {1:N2} GiB, {2} files' -f $target, ($bytes / 1GB), $files.Count)
if ($Remove) {
    Remove-Item -LiteralPath $target -Recurse -Force
    $report.removed = -not (Test-Path -LiteralPath $target)
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receipt -Encoding UTF8
    if (-not $report.removed) { throw 'Intermediate removal did not complete.' }
    Write-Output 'Compiler files removed. Source, runnable products and reports retained.'
} else {
    Write-Output 'Preview only. Add -Remove after builds have stopped to delete these compiler files.'
}
Write-Output "Receipt: $receipt"
