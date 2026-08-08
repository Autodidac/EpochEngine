param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = "Stop"
$roots = @(
    (Join-Path $RepoRoot "Engine\modules"),
    (Join-Path $RepoRoot "Engine\src"),
    (Join-Path $RepoRoot "Engine\include")
)
$extensions = @(".cpp", ".hpp", ".h", ".ixx", ".inl")
$ignoredFragments = @("\dep\", "\third_party\", "\stb\", "\Bin\", "\bin\")
$failures = [Collections.Generic.List[string]]::new()
$checked = 0

foreach ($file in Get-ChildItem -LiteralPath $roots -Recurse -File) {
    if ($file.Extension -notin $extensions) {
        continue
    }

    $ignored = $false
    foreach ($fragment in $ignoredFragments) {
        if ($file.FullName.IndexOf($fragment, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $ignored = $true
            break
        }
    }
    if ($ignored) {
        continue
    }

    ++$checked
    $baseName = $file.BaseName
    $separatorCount = ($baseName.ToCharArray() | Where-Object { $_ -eq '.' }).Count
    $relative = $file.FullName.Substring($RepoRoot.TrimEnd("\").Length).TrimStart("\")

    if ($separatorCount -ne 1) {
        $failures.Add("$relative must contain exactly one owner separator before its extension")
    }
    if ($baseName -cne $baseName.ToLowerInvariant()) {
        $failures.Add("$relative must use lowercase")
    }
    if ($baseName.Contains('-')) {
        $failures.Add("$relative must use underscores instead of hyphens")
    }

    if ($file.Extension -eq ".ixx") {
        $match = Select-String -LiteralPath $file.FullName -Pattern '^export module\s+([^;]+);' |
            Select-Object -First 1
        if ($null -eq $match) {
            continue
        }

        $moduleName = $match.Matches[0].Groups[1].Value
        $fileStem = $moduleName.Replace(':', '_')
        if ($fileStem -ne $baseName) {
            $failures.Add("$relative must match exported module '$moduleName' as '$fileStem$($file.Extension)'")
        }

        $primary = ($moduleName -split ':', 2)[0]
        $moduleSeparatorCount = ($primary.ToCharArray() | Where-Object { $_ -eq '.' }).Count
        if ($moduleSeparatorCount -ne 1) {
            $failures.Add("module '$moduleName' must contain exactly one owner separator")
        }
    }
}

if ($failures.Count -ne 0) {
    $failures | Sort-Object -Unique | ForEach-Object { Write-Output "[ERROR] $_" }
    exit 1
}

Write-Output "Epoch first-party source naming validation passed for $checked files."
