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
foreach ($match in [regex]::Matches($cmake, '(?<![\w/])(?:src|modules|include)/[\w./-]+\.(?:cpp|hpp|h|ixx|inl)\b')) {
    if (!(Test-Path -LiteralPath (Join-Path "$RepoRoot/Engine" $match.Value) -PathType Leaf)) {
        $failures.Add("CMake references missing source: $($match.Value)")
    }
}
# The extracted platform library is a first-party source owner, not a mirror.
foreach ($owner in @('EpochPlatformEngine', 'EpochAudioEngine', 'EpochMediaEngine', 'EpochSelfCoding')) {
$dependencyRoot = Join-Path $RepoRoot "Engine/dep/$owner"
$ownerProjects = @("$owner.vcxproj", "$owner.vcxproj.filters")
if ($owner -eq 'EpochSelfCoding') { $ownerProjects += @('EpochSelfCodingIntegration.vcxitems', 'EpochSelfCodingIntegration.vcxitems.filters') }
foreach ($name in $ownerProjects) {
    [xml]$doc = Get-Content (Join-Path $dependencyRoot $name) -Raw
    foreach ($node in $doc.SelectNodes('//*[local-name()="ClCompile" or local-name()="ClInclude"][@Include]')) {
        $relative = $node.GetAttribute('Include').Replace('$(MSBuildThisFileDirectory)', '')
        if (!(Test-Path -LiteralPath (Join-Path $dependencyRoot $relative) -PathType Leaf)) {
            $failures.Add("$name references missing source: $relative")
        }
    }
}
foreach ($file in Get-ChildItem -LiteralPath $dependencyRoot -Recurse -File | Where-Object { $_.Extension -in @('.cpp', '.hpp', '.ixx') }) {
    # Private integration is deliberately compiled by Engine and consumes its
    # existing contracts. The independently compiled core must stay closed.
    if ($owner -eq 'EpochSelfCoding' -and $file.FullName.StartsWith((Join-Path $dependencyRoot 'integration') + '\')) { continue }
    if (Select-String -LiteralPath $file.FullName -Pattern '#include\s+"(?:\.\./){3,}' -Quiet) {
        $failures.Add("$owner dependency reaches outside its source root: $($file.Name)")
    }
}
}
if ($failures.Count) { $failures | Sort-Object -Unique | ForEach-Object { Write-Output "[ERROR] $_" }; exit 1 }
Write-Output 'Source ownership and CMake/MSVC file references pass.'
