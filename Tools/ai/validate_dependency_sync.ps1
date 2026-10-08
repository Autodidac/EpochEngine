param([switch]$Remote)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repo
try {
    $manifest = Get-Content Engine/dep/dependencies.json -Raw | ConvertFrom-Json
    foreach ($dep in $manifest.dependencies) {
        if (!(Test-Path -LiteralPath $dep.path -PathType Container)) { throw "Missing dependency: $($dep.name)" }
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
