param([switch]$Remote)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repo
try {
    $manifest = Get-Content Engine/dep/dependencies.json -Raw | ConvertFrom-Json
    foreach ($dep in $manifest.dependencies) {
        if (!(Test-Path -LiteralPath $dep.path -PathType Container)) { throw "Missing dependency: $($dep.name)" }
        if ($dep.portable_source_manifest) {
            $portable = Get-Content -LiteralPath $dep.portable_source_manifest -Raw | ConvertFrom-Json
            if ($portable.upstream_revision -ne $dep.upstream_revision) { throw "Portable source pin differs: $($dep.name)" }
            $ownerRoot = (Resolve-Path -LiteralPath $dep.path).Path.TrimEnd('\')
            $expected = @()
            foreach ($entry in $portable.files) {
                $file = [IO.Path]::GetFullPath((Join-Path $ownerRoot $entry.path))
                if (!$file.StartsWith($ownerRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Portable path escapes owner: $($entry.path)" }
                if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Portable source hash differs: $($dep.name)/$($entry.path)" }
                $expected += $entry.path
            }
            $actual = @()
            foreach ($scope in $portable.scope) {
                $actual += Get-ChildItem -LiteralPath (Join-Path $ownerRoot $scope) -Recurse -File | ForEach-Object { $_.FullName.Substring($ownerRoot.Length + 1).Replace('\', '/') }
            }
            if (Compare-Object ($expected | Sort-Object) ($actual | Sort-Object)) { throw "Portable source inventory differs: $($dep.name)" }
            if ($Remote) {
                $upstreamTree = & gh api "repos/$($dep.repository)/commits/$($dep.upstream_revision)" --jq '.commit.tree.sha'
                if ($LASTEXITCODE -ne 0 -or $upstreamTree -ne $dep.upstream_tree) { throw "Pinned portable source identity differs: $($dep.name)" }
            }
            Write-Output "$($dep.name): $($expected.Count) portable files verified at $($portable.upstream_revision); no full-tree mirror claim."
        }
        if ($dep.mode -ne 'exact_mirror') {
            Write-Output "$($dep.name): $($dep.mode); no standalone mirror claim."
            continue
        }
        $tree = & git rev-parse "HEAD:$($dep.path)"
        if ($LASTEXITCODE -ne 0 -or $tree -ne $dep.tree) { throw "Recorded mirror tree differs: $($dep.name)" }
        $dirty = & git status --porcelain -- $dep.path
        if ($LASTEXITCODE -ne 0 -or $dirty) { throw "$($dep.name) has changes requiring a verified standalone sync and manifest update." }
        if ($Remote) {
            $head = & gh api "repos/$($dep.repository)/commits/main" --jq '.sha'
            if ($LASTEXITCODE -ne 0 -or $head -ne $dep.revision) { throw "Upstream head changed: $($dep.name); review it before changing the pinned snapshot." }
            $upstreamTree = & gh api "repos/$($dep.repository)/commits/$($dep.revision)" --jq '.commit.tree.sha'
            if ($LASTEXITCODE -ne 0 -or $upstreamTree -ne $dep.tree) { throw "Upstream mirror tree mismatch: $($dep.name)" }
        }
        Write-Output "$($dep.name): exact clean mirror at $($dep.revision)."
    }
} finally { Pop-Location }
