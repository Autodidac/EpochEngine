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
            $publishedRevision = $dep.published_upstream_revision
            if ($portable.published_revision -and $portable.published_revision -cne $publishedRevision) {
                throw "Portable published revision differs: $($dep.name)"
            }
            if ($portable.publication_status -eq 'published_source' -and
                $portable.upstream_revision -cne $publishedRevision) {
                throw "Published portable source pin differs from its published revision: $($dep.name)"
            }
            $separator = [IO.Path]::DirectorySeparatorChar
            $ownerRoot = (Resolve-Path -LiteralPath $dep.path).Path.TrimEnd($separator)
            $pathComparison = if ($separator -eq '\') { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
            $expected = @()
            foreach ($entry in $portable.files) {
                $file = [IO.Path]::GetFullPath((Join-Path $ownerRoot $entry.path))
                if (!$file.StartsWith($ownerRoot + $separator, $pathComparison)) { throw "Portable path escapes owner: $($entry.path)" }
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
                if ($LASTEXITCODE -ne 0) { throw "Could not read pinned portable source from GitHub: $($dep.name)" }
                if ($upstreamTree -ne $dep.upstream_tree) { throw "Pinned portable source identity differs: $($dep.name)" }
                if ($publishedRevision -and $publishedRevision -cne $dep.upstream_revision) {
                    $publishedCommit = & gh api "repos/$($dep.repository)/commits/$publishedRevision" --jq '.sha'
                    if ($LASTEXITCODE -ne 0) { throw "Could not read published portable source from GitHub: $($dep.name)" }
                    if ($publishedCommit -cne $publishedRevision) {
                        throw "Published portable source identity differs: $($dep.name)"
                    }
                }
            }
            if ($portable.publication_status -eq 'local_unpublished_changes') {
                if ($dep.publication_status -ne $portable.publication_status -or !$portable.pending_changes) {
                    throw "Pending portable source status is inconsistent: $($dep.name)"
                }
                foreach ($pending in $portable.pending_changes) {
                    $record = @($portable.files | Where-Object path -eq $pending.path)
                    if ($record.Count -ne 1 -or $record[0].sha256 -ne $pending.sha256 -or $pending.published_sha256 -notmatch '^[a-f0-9]{64}$') {
                        throw "Pending portable source identity is inconsistent: $($dep.name)/$($pending.path)"
                    }
                }
                $published = if ($publishedRevision) { "published revision $publishedRevision" } else { 'no published revision recorded' }
                Write-Output "$($dep.name): $($expected.Count) local portable files verified with unpublished changes. Upstream checkpoint $($portable.upstream_revision), $published."
            } elseif ($portable.pending_source_changes.bundled_only_changes) {
                foreach ($pending in $portable.pending_source_changes.bundled_only_changes) {
                    $record = @($portable.files | Where-Object path -eq $pending.path)
                    if ($record.Count -ne 1 -or $record[0].sha256 -ne $pending.sha256 -or $pending.base_checkpoint_sha256 -notmatch '^[a-f0-9]{64}$') {
                        throw "Bundled portable source identity is inconsistent: $($dep.name)/$($pending.path)"
                    }
                }
                Write-Output "$($dep.name): $($expected.Count) local portable files verified including bundled-only changes; checkpoint $($portable.upstream_revision) unchanged. Standalone synchronization/publication unverified."
            } else {
                Write-Output "$($dep.name): $($expected.Count) portable files verified at $($portable.upstream_revision); no full-tree mirror claim."
            }
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
