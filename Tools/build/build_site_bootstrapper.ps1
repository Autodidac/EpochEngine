[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$OutputRoot = '',
    [string]$VcpkgInstalledRoot = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath($RepoRoot)
if (!$OutputRoot) { $OutputRoot = Join-Path $repo 'build\site-bootstrapper' }
$output = [IO.Path]::GetFullPath($OutputRoot)
if ($output -eq $repo) { throw 'Bootstrapper output must be separate from source.' }
if (!$VcpkgInstalledRoot) { $VcpkgInstalledRoot = Join-Path $repo 'Engine\vcpkg_installed' }
$installed = [IO.Path]::GetFullPath($VcpkgInstalledRoot).TrimEnd('\') + '\'
$version = (& (Join-Path $repo 'Tools\build\get_epoch_version.ps1')).Trim()
$commit = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'A committed source checkout is required.' }
if (& git -C $repo status --porcelain --untracked-files=no) { throw 'Commit source changes before packaging.' }
$asset = "epoch_updater_shell_only_win10_x64_v$version.zip"
$zip = Join-Path $output $asset
if (Test-Path -LiteralPath $zip) { throw 'Use a new output directory; an existing package is immutable.' }
New-Item -ItemType Directory -Path $output -Force | Out-Null
$props = Join-Path $output 'bootstrap.props'
$escaped = [Security.SecurityElement]::Escape($output)
@"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <OutDir>$escaped\bin\</OutDir>
    <IntDir>$escaped\obj\`$(MSBuildProjectName)\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <Link><AdditionalLibraryDirectories>`$(OutDir);%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories></Link>
  </ItemDefinitionGroup>
</Project>
"@ | Set-Content -LiteralPath $props -Encoding utf8
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild was not found.' }
& $msbuild (Join-Path $repo 'Engine.sln') /t:EpochEditor /p:Configuration=Release /p:Platform=x64 /p:EpochUpdaterShell=true /m:1 /nologo /v:minimal "/p:VcpkgInstalledDir=$installed" "/p:ForceImportBeforeCppTargets=$props" "/flp:logfile=$output\build.log;verbosity=normal"
if ($LASTEXITCODE -ne 0) { throw "Bootstrapper compilation failed: $LASTEXITCODE" }
$bin = Join-Path $output 'bin'
$exe = Join-Path $bin 'EpochEditor.exe'
if ((Get-Item -LiteralPath $exe).VersionInfo.FileVersion -notlike "$version*") { throw 'Wrong executable version.' }
# Isolated builds must carry their own loader dependencies before any check.
$dependencyBin = Join-Path $installed 'x64-windows\bin'
Get-ChildItem -LiteralPath $dependencyBin -Filter '*.dll' | Copy-Item -Destination $bin -Force
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
$crt = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Redist\MSVC') -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'x64\Microsoft.VC143.CRT' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$crt) { throw 'VC runtime redistributables were not found.' }
Get-ChildItem -LiteralPath $crt -Filter '*.dll' | Copy-Item -Destination $bin -Force
# This is the documented pure contract lane; never launch the GUI or an update.
$test = Start-Process -FilePath $exe -ArgumentList '--engine-contract-self-test' -WorkingDirectory $bin -WindowStyle Hidden -PassThru -Wait
if ($test.ExitCode -ne 0) { throw "Bootstrapper contracts failed: $($test.ExitCode)" }
$stage = Join-Path $output ('payload-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $exe -Destination $stage
# Package the actual PE import closure, not every vcpkg renderer/tool DLL.
$dumpbin = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Tools\MSVC') -Directory |
    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$dumpbin) { throw 'dumpbin is required to verify bootstrapper DLL closure.' }
$queue = [Collections.Generic.Queue[string]]::new()
$queue.Enqueue($exe)
$selected = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
while ($queue.Count -gt 0) {
    $image = $queue.Dequeue()
    $imports = & $dumpbin /nologo /dependents $image
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE imports: $image" }
    foreach ($line in $imports) {
        if ($line -notmatch '^\s+([a-zA-Z0-9_.-]+\.dll)\s*$') { continue }
        $dll = $Matches[1]
        if (!$selected.Add($dll)) { continue }
        $found = @($bin, $dependencyBin, $crt) | ForEach-Object { Join-Path $_ $dll } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        if ($found) {
            Copy-Item -LiteralPath $found -Destination $stage
            $queue.Enqueue($found)
        } elseif ($dll -notmatch '^(api-ms-win-|ext-ms-win-)' -and
            !(Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$dll"))) {
            throw "Missing non-system PE dependency: $dll ($image)"
        }
    }
}
# The modern updater UI needs its fallback font, not legacy menus, scenes,
# training records, model descriptors or renderer demonstration textures.
$fontStage = Join-Path $stage 'assets\fonts'
New-Item -ItemType Directory -Path $fontStage -Force | Out-Null
foreach ($fontAsset in @('Roboto-Regular.ttf','OFL.txt')) {
    Copy-Item -LiteralPath (Join-Path $repo "Engine\assets\fonts\$fontAsset") -Destination $fontStage
}
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination $stage
& (Join-Path $repo 'Tools\build\collect_third_party_notices.ps1') -RepoRoot $repo -VcpkgInstalledRoot (Join-Path $installed 'x64-windows') -Destination $stage
@{ version=$version; source_commit=$commit; kind='updater-shell'; channel='GPT Site'; source_version_url='https://epoch.adamrushford.chatgpt.site/api/epoch/source-version'; source_url='https://epoch.adamrushford.chatgpt.site/api/epoch/source-archive/source.zip'; update_preference='verified packaged runtime first; authenticated source fallback'; verification='MSVC build and pure engine contracts; dependency import closure; GUI/update execution unverified' } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'bootstrapper.json') -Encoding utf8
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllBytes("$zip.sha256", [Text.Encoding]::ASCII.GetBytes("$hash  $asset`n"))
@{ version=$version; source_commit=$commit; build_recipe_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant(); asset=$zip; sha256=$hash; bytes=(Get-Item -LiteralPath $zip).Length; engine_contract_exit=0; gui_update_run=$false } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
Write-Host "Site bootstrapper staged: $zip"
