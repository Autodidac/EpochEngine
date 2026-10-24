$ErrorActionPreference = 'Stop'
$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $repo 'Tools\build\epoch_package_layout.ps1')
$edition = Get-EpochSourceEdition -RepoRoot $repo
if ($edition.Id -notin @('epoch.public.github.v1', 'epoch.development.site.v1')) {
    throw 'Physical source edition was not recognized.'
}
$packageRoot = Join-Path $repo 'build\packages'
$output = Get-EpochPackageOutputRoot -RepoRoot $repo -Edition $edition.Directory -Version '0.90.183'
if ($output -cne (Join-Path (Join-Path $packageRoot $edition.Directory) 'v0.90.183')) {
    throw 'Default package output is not shared and edition-specific.'
}
foreach ($foreign in @($repo, $packageRoot, (Join-Path $repo 'build\packages-other\v0.90.183'), (Join-Path $packageRoot '..\escape'))) {
    $rejected = $false
    try { Assert-EpochPackagePath -LiteralPath $foreign -PackageRoot $packageRoot | Out-Null }
    catch { $rejected = $true }
    if (-not $rejected) { throw "Package boundary accepted $foreign" }
}
$foreignEdition = if ($edition.Directory -ceq 'public') { 'development' } else { 'public' }
$rejected = $false
try {
    Get-EpochPackageOutputRoot -RepoRoot $repo -Edition $edition.Directory -Version '0.90.183' `
        -OutputRoot (Join-Path (Join-Path $packageRoot $foreignEdition) 'v0.90.183') | Out-Null
}
catch { $rejected = $true }
if (-not $rejected) { throw 'Custom output crossed editions.' }
foreach ($name in @('windows', 'linux')) {
    $path = Join-Path $repo "Tools\build\stage_epoch_$($name)_release.ps1"
    $errors = $null
    $tree = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$null, [ref]$errors)
    if ($errors.Count) { throw "Staging script syntax failed: $path" }
    $unchecked = $tree.FindAll({ param($node)
        $node -is [System.Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Remove-Item'
    }, $true)
    if ($unchecked.Count) { throw "Stager bypasses checked deletion helper: $path" }
    $text = [System.IO.File]::ReadAllText($path)
    if ($text.IndexOf('edition=') -lt 0 -or $text.IndexOf('edition=') -gt $text.IndexOf('Write-EpochRuntimeDistribution')) {
        throw "Stager writes identity before checking embedded binary edition: $path"
    }
}
Write-Output 'package_layout_contract.result=pass (read-only; no staging, deletion, or runtime)'
