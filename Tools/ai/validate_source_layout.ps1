param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
$ErrorActionPreference = 'Stop'
$failures = [Collections.Generic.List[string]]::new()
$src = Join-Path $RepoRoot 'Engine/src'
foreach ($file in Get-ChildItem -LiteralPath $src -File) {
    if ($file.Extension -in @('.cpp', '.hpp', '.h', '.ixx', '.inl')) {
        $failures.Add("Unowned implementation at src root: $($file.Name)")
    }
}
foreach ($name in @('Engine.vcxitems', 'Engine.vcxitems.filters')) {
    [xml]$doc = Get-Content (Join-Path $RepoRoot "Engine/$name") -Raw
    foreach ($node in $doc.SelectNodes('//*[@Include]')) {
        $value = $node.GetAttribute('Include')
        if (!$value.StartsWith('$(MSBuildThisFileDirectory)')) { continue }
        $relative = $value.Replace('$(MSBuildThisFileDirectory)', '').Replace('\', '/')
        if ($relative -notmatch '^(src|include|modules)/') { continue }
        if (!(Test-Path -LiteralPath (Join-Path "$RepoRoot/Engine" $relative) -PathType Leaf)) {
            $failures.Add("$name references missing source: $relative")
        }
    }
}
$cmake = Get-Content (Join-Path $RepoRoot 'Engine/CMakeLists.txt') -Raw
foreach ($match in [regex]::Matches($cmake, '(?<![\w/])(?:src|modules|include)/[\w./-]+\.(?:cpp|hpp|h|ixx)\b')) {
    if (!(Test-Path -LiteralPath (Join-Path "$RepoRoot/Engine" $match.Value) -PathType Leaf)) {
        $failures.Add("CMake references missing source: $($match.Value)")
    }
}
if ($failures.Count) { $failures | Sort-Object -Unique | ForEach-Object { Write-Output "[ERROR] $_" }; exit 1 }
Write-Output 'Source ownership and CMake/MSVC file references pass.'
