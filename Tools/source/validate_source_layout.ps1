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
        # Incremental implementation tracking uses MSBuild wildcard inputs.
        # Resolve those against their literal owner directory; do not mistake
        # the wildcard itself for a missing source file or exempt empty groups.
        if ($node.LocalName -eq 'EpochImplementationFragment' -and $relative -match '[*?]') {
            $ownerDirectory = Join-Path "$RepoRoot/Engine" (Split-Path -Parent $relative)
            $pattern = Split-Path -Leaf $relative
            $implementationInputs = @(Get-ChildItem -LiteralPath $ownerDirectory -Filter $pattern -File -ErrorAction SilentlyContinue)
            if ($implementationInputs.Count -eq 0) {
                $failures.Add("$name references missing implementation inputs: $relative")
            }
            continue
        }
        if (!(Test-Path -LiteralPath (Join-Path "$RepoRoot/Engine" $relative) -PathType Leaf)) {
            $failures.Add("$name references missing source: $relative")
        }
    }
}
$cmake = Get-Content (Join-Path $RepoRoot 'Engine/CMakeLists.txt') -Raw
$cmake = $cmake.Replace('${EPOCH_MODULE_DIR}/', 'modules/')
foreach ($match in [regex]::Matches($cmake, '(?<![\w/])(?:src|modules|include)/[\w./-]+\.(?:cpp|hpp|h|ixx|inl)\b')) {
    if (!(Test-Path -LiteralPath (Join-Path "$RepoRoot/Engine" $match.Value) -PathType Leaf)) {
        $failures.Add("CMake references missing source: $($match.Value)")
    }
}
$examples = Join-Path $RepoRoot 'Engine/examples'
if (Test-Path -LiteralPath $examples -PathType Container) {
    foreach ($project in Get-ChildItem -LiteralPath $examples -Filter '*.vcxproj' -File -Recurse) {
        [xml]$projectXml = Get-Content -LiteralPath $project.FullName -Raw
        foreach ($node in $projectXml.SelectNodes('//*[local-name()="ClCompile"][@Update]')) {
            $value = $node.GetAttribute('Update')
            if (!$value.StartsWith('$(ProjectDir)') -or $value -notmatch '\.ixx$') { continue }
            $relative = $value.Substring('$(ProjectDir)'.Length)
            if (!(Test-Path -LiteralPath (Join-Path $project.DirectoryName $relative) -PathType Leaf)) {
                $failures.Add("$($project.Name) updates missing module: $relative")
            }
        }
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
