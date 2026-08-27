/*
 * This file is part of the Epoch Project.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

module updater.system;

import updater.config;
import updater.source_access;

#if defined(_WIN32)
namespace epochengine::updater
{
    [[nodiscard]] bool launch_source_update_worker(
        const UpdateChannel& channel,
        const std::filesystem::path& target_binary,
        const bool silent_worker,
        const PreparedSourceArchive& prepared_source)
    {
        const bool has_prepared_source =
            prepared_source.ok && !prepared_source.archive_path.empty();
        const auto cleanup_prepared_source = [&]()
            {
                if (!prepared_source.cleanup_root.empty())
                    system_detail::remove_update_cache_path_best_effort(prepared_source.cleanup_root);
            };

        if (!has_prepared_source && channel.source_url.empty())
        {
            system_detail::log_error("Source update URL is not configured.");
            return false;
        }

        if (!has_prepared_source && !channel.source_url.starts_with("https://"))
        {
            system_detail::log_error("Source update URL was rejected because Epoch requires HTTPS transport.");
            return false;
        }

        const auto msbuild = system_detail::find_msbuild_path();
        if (msbuild.empty())
        {
            cleanup_prepared_source();
            system_detail::log_error("Could not locate MSBuild. Install Visual Studio Build Tools or set MSBUILD_EXE_PATH.");
            return false;
        }

        const auto run_token = system_detail::make_source_update_run_token();
        const auto work_root = system_detail::source_work_root(target_binary);
        const auto run_dir = system_detail::source_run_dir(target_binary, run_token);
        const std::string archive_identity = has_prepared_source
            ? prepared_source.archive_path.string()
            : channel.source_url;
        const auto archive_path = system_detail::source_archive_path(target_binary, archive_identity, run_token);
        const auto staging_dir = system_detail::source_staging_dir(target_binary, run_token);
        const auto final_dir = system_detail::source_final_dir(target_binary, run_token);
        const auto target_dir = target_binary.parent_path();
        const auto build_log = system_detail::source_update_log_path_for(target_binary);
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);
        const auto cancel_path = system_detail::source_cancel_path(target_binary);
        const auto active_run_path = system_detail::source_active_run_path(target_binary);
        const auto worker_script = system_detail::make_temp_powershell_script_path("source_update_worker");
        const auto built_runtime_dir = system_detail::source_runtime_output_dir(final_dir);
        const auto built_binary = system_detail::source_runtime_binary_path(final_dir, target_binary);
        const auto manifest_root = system_detail::source_manifest_root(final_dir);
        const auto solution = system_detail::source_solution_path(final_dir);
        const auto target_assets_dir = target_dir / "assets";
        const auto built_assets_dir = built_runtime_dir / "assets";
        const auto source_repo_assets_dir = manifest_root / "assets";
        const auto managed_tools_root = system_detail::source_run_tools_root(target_binary, run_token);
        const auto source_fallback_urls = PROJECT_SOURCE_FALLBACK_URLS();

        std::filesystem::path existing_active_run;
        if (system_detail::source_update_session_active(target_binary))
        {
            const bool has_active_run =
                system_detail::source_update_active_run_exists(target_binary, &existing_active_run);
            system_detail::append_log_line(
                handoff_log,
                has_active_run
                    ? "[WARN] Existing source update worker is still active at: " + existing_active_run.string()
                    : "[WARN] Existing source update session is still settling; not launching a second worker.");
            system_detail::log_info("Source update worker launch skipped because an existing worker is still active.");
            cleanup_prepared_source();
            return true;
        }

        {
            std::error_code cleanup_ec;
            system_detail::cleanup_stale_source_update_runs(target_binary, run_dir);
            system_detail::cleanup_stale_source_tool_runs(target_binary, managed_tools_root);
            cleanup_ec.clear();
            std::filesystem::remove(cancel_path, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(build_log, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(handoff_log, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(system_detail::staged_update_handoff_script_path(), cleanup_ec);
        }

        {
            std::error_code active_ec;
            std::filesystem::create_directories(run_dir, active_ec);
            if (active_ec)
            {
                cleanup_prepared_source();
                system_detail::log_error("Failed to prepare source update run directory: " + run_dir.string());
                return false;
            }

            active_ec.clear();
            std::filesystem::create_directories(active_run_path.parent_path(), active_ec);
            if (active_ec)
            {
                system_detail::remove_update_cache_path_best_effort(run_dir);
                cleanup_prepared_source();
                system_detail::log_error("Failed to prepare source update marker directory: " + active_run_path.parent_path().string());
                return false;
            }

            std::ofstream active_run(active_run_path, std::ios::binary | std::ios::trunc);
            if (active_run)
                active_run << run_dir.string() << '\n';
            else
                system_detail::log_error("Failed to write source update active-run marker; Cancel will still signal the worker.");
        }

        std::ofstream ps(worker_script, std::ios::binary);
        if (!ps)
        {
            std::error_code marker_ec;
            std::filesystem::remove(active_run_path, marker_ec);
            system_detail::remove_update_cache_path_best_effort(run_dir);
            cleanup_prepared_source();
            system_detail::log_error("Failed to create source update worker script.");
            return false;
        }

        const auto triplet = SOURCE_BUILD_PLATFORM() + std::string{ "-windows" };
        const auto esc = [](const std::string& value)
            {
                return system_detail::powershell_escape_single_quoted(value);
            };

        [&]()
            {
        ps
            << "$ErrorActionPreference = 'Stop'\n"
            << "$ProgressPreference = 'SilentlyContinue'\n"
            << "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\n"
            << "$sourceUrl = '" << esc(channel.source_url) << "'\n"
            << "$sourceChecksumUrl = $sourceUrl + '.sha256'\n"
            << "$preparedSourceArchive = '" << esc(prepared_source.archive_path.string()) << "'\n"
            << "$preparedSourceCleanupRoot = '" << esc(prepared_source.cleanup_root.string()) << "'\n"
            << "$sourceFallbackUrls = @(\n";

        for (const auto& fallback_url : source_fallback_urls)
        {
            if (!fallback_url.empty() && fallback_url != channel.source_url)
                ps << "  '" << esc(fallback_url) << "'\n";
        }

        ps
            << ")\n"
            << "$workRoot = '" << esc(work_root.string()) << "'\n"
            << "$runDir = '" << esc(run_dir.string()) << "'\n"
            << "$sourceArchive = '" << esc(archive_path.string()) << "'\n"
            << "$sourceChecksum = $sourceArchive + '.sha256'\n"
            << "$stagingDir = '" << esc(staging_dir.string()) << "'\n"
            << "$sourceRoot = '" << esc(final_dir.string()) << "'\n"
            << "$manifestRoot = '" << esc(manifest_root.string()) << "'\n"
            << "$solution = '" << esc(solution.string()) << "'\n"
            << "$buildLog = '" << esc(build_log.string()) << "'\n"
            << "$handoffLog = '" << esc(handoff_log.string()) << "'\n"
            << "$cancelPath = '" << esc(cancel_path.string()) << "'\n"
            << "$activeRunPath = '" << esc(active_run_path.string()) << "'\n"
            << "$targetExe = '" << esc(target_binary.string()) << "'\n"
            << "$targetDir = '" << esc(target_dir.string()) << "'\n"
            << "$targetAssetsDir = '" << esc(target_assets_dir.string()) << "'\n"
            << "$builtDir = '" << esc(built_runtime_dir.string()) << "'\n"
            << "$builtExe = '" << esc(built_binary.string()) << "'\n"
            << "$builtAssetsDir = '" << esc(built_assets_dir.string()) << "'\n"
            << "$sourceRepoAssetsDir = '" << esc(source_repo_assets_dir.string()) << "'\n"
            << "$managedToolsRoot = '" << esc(managed_tools_root.string()) << "'\n"
            << "$vcpkgDefaultRef = '" << esc(std::string{ VCPKG_DEFAULT_REF }) << "'\n"
            << "$vcpkgDefaultArchiveUrl = '" << esc(VCPKG_ARCHIVE_URL(VCPKG_DEFAULT_REF)) << "'\n"
            << "$vcpkgArchiveBaseUrl = '" << esc(VCPKG_ARCHIVE_BASE_URL()) << "'\n"
            << "$vcpkgExeName = '" << esc(VCPKG_EXECUTABLE_NAME()) << "'\n"
            << "$vcpkgBootstrapName = '" << esc(VCPKG_BOOTSTRAP_SCRIPT_NAME()) << "'\n"
            << "$gitReleaseApiUrl = '" << esc(GIT_WINDOWS_RELEASE_API_URL()) << "'\n"
            << "$msbuildExe = '" << esc(msbuild.string()) << "'\n"
            << "$triplet = '" << esc(triplet) << "'\n"
            << "$buildTarget = '" << esc(SOURCE_BUILD_TARGET()) << "'\n"
            << "$buildConfiguration = '" << esc(SOURCE_BUILD_CONFIGURATION()) << "'\n"
            << "$buildPlatform = '" << esc(SOURCE_BUILD_PLATFORM()) << "'\n"
            << "$workerPath = $MyInvocation.MyCommand.Path\n"
            << "$workerHidden = $" << (silent_worker ? "true" : "false") << "\n"
            << "$utf8NoBom = New-Object System.Text.UTF8Encoding($false)\n"
            << "function Append-Text([string]$Path, [string]$Text) {\n"
            << "  if ([string]::IsNullOrEmpty($Path) -or [string]::IsNullOrEmpty($Text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  [System.IO.File]::AppendAllText($Path, $Text, $utf8NoBom)\n"
            << "}\n"
            << "function Get-SafeToken([string]$Value) {\n"
            << "  if ([string]::IsNullOrWhiteSpace($Value)) {\n"
            << "    return 'default'\n"
            << "  }\n"
            << "  $chars = $Value.ToCharArray() | ForEach-Object {\n"
            << "    if ([char]::IsLetterOrDigit($_) -or $_ -eq '-' -or $_ -eq '_') { $_ } else { '_' }\n"
            << "  }\n"
            << "  $safe = -join $chars\n"
            << "  $safe = $safe.TrimEnd('_')\n"
            << "  if ([string]::IsNullOrWhiteSpace($safe)) {\n"
            << "    return 'default'\n"
            << "  }\n"
            << "  return $safe\n"
            << "}\n"
            << "function Get-ShortToken([string]$Value, [int]$MaxLength = 12) {\n"
            << "  $safe = Get-SafeToken $Value\n"
            << "  if ($safe.Length -gt $MaxLength) {\n"
            << "    $safe = $safe.Substring(0, $MaxLength)\n"
            << "  }\n"
            << "  return $safe.TrimEnd('_')\n"
            << "}\n"
            << "function Get-VcpkgRef {\n"
            << "  $manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "  if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "    return $vcpkgDefaultRef\n"
            << "  }\n"
            << "  $text = Get-Content -LiteralPath $manifestFile -Raw -ErrorAction SilentlyContinue\n"
            << "  if ([string]::IsNullOrWhiteSpace($text)) {\n"
            << "    return $vcpkgDefaultRef\n"
            << "  }\n"
            << "  $match = [regex]::Match($text, '\"builtin-baseline\"\\s*:\\s*\"([0-9A-Fa-f]+)\"')\n"
            << "  if ($match.Success) {\n"
            << "    return $match.Groups[1].Value.ToLowerInvariant()\n"
            << "  }\n"
            << "  return $vcpkgDefaultRef\n"
            << "}\n"
            << "function Get-VcpkgArchiveUrl([string]$Ref) {\n"
            << "  if ($Ref -eq $vcpkgDefaultRef) {\n"
            << "    return $vcpkgDefaultArchiveUrl\n"
            << "  }\n"
            << "  return ($vcpkgArchiveBaseUrl + $Ref + '.zip')\n"
            << "}\n"
            << "function Get-ReleaseAssetUrl([string]$ApiUrl, [string]$IncludeFragment, [string]$IncludeSuffix, [string]$ExcludeFragment) {\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0'; 'Accept' = 'application/vnd.github+json' }\n"
            << "  $release = Invoke-RestMethod -Headers $headers -Uri $ApiUrl -UseBasicParsing\n"
            << "  $fallback = $null\n"
            << "  foreach ($asset in $release.assets) {\n"
            << "    if ($null -eq $asset) { continue }\n"
            << "    $url = [string]$asset.browser_download_url\n"
            << "    if ([string]::IsNullOrWhiteSpace($url)) { continue }\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($IncludeFragment) -and -not $url.Contains($IncludeFragment)) { continue }\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($IncludeSuffix) -and -not $url.EndsWith($IncludeSuffix)) { continue }\n"
            << "    if ([string]::IsNullOrWhiteSpace($ExcludeFragment) -or -not $url.Contains($ExcludeFragment)) {\n"
            << "      return $url\n"
            << "    }\n"
            << "    if ($null -eq $fallback) { $fallback = $url }\n"
            << "  }\n"
            << "  return $fallback\n"
            << "}\n";
            }();

        [&]()
            {
        ps
            << "function Append-FileToBuildLog([string]$Path) {\n"
            << "  if (-not (Test-Path -LiteralPath $Path)) {\n"
            << "    return\n"
            << "  }\n"
            << "  $text = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue\n"
            << "  if ([string]::IsNullOrEmpty($text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  if (-not $text.EndsWith([Environment]::NewLine)) {\n"
            << "    $text += [Environment]::NewLine\n"
            << "  }\n"
            << "  Append-Text $buildLog $text\n"
            << "}\n"
            << "function Append-ToolTextToBuildLog([string]$Text) {\n"
            << "  if ([string]::IsNullOrEmpty($Text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  if (-not $Text.EndsWith([Environment]::NewLine)) {\n"
            << "    $Text += [Environment]::NewLine\n"
            << "  }\n"
            << "  Append-Text $buildLog $Text\n"
            << "}\n"
            << "function Complete-ToolOutput($StdoutTask, $StderrTask) {\n"
            << "  try { Append-ToolTextToBuildLog ([string]$StdoutTask.Result) } catch { }\n"
            << "  try { Append-ToolTextToBuildLog ([string]$StderrTask.Result) } catch { }\n"
            << "}\n"
            << "function Write-Step([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  if (-not $workerHidden) { Write-Host $line }\n"
            << "  Append-Text $buildLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Write-Handoff([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  Append-Text $handoffLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Invoke-DownloadWithFallback([string]$PrimaryUrl, [string[]]$FallbackUrls, [string]$OutFile, [string]$Label) {\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0'; 'Accept' = 'application/octet-stream, application/vnd.github+json' }\n"
            << "  $urls = New-Object System.Collections.Generic.List[string]\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($PrimaryUrl)) { $urls.Add($PrimaryUrl) }\n"
            << "  foreach ($fallbackUrl in $FallbackUrls) {\n"
            << "    if ([string]::IsNullOrWhiteSpace($fallbackUrl)) { continue }\n"
            << "    $duplicate = $false\n"
            << "    foreach ($existingUrl in $urls) {\n"
            << "      if ([string]::Equals($existingUrl, $fallbackUrl, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "        $duplicate = $true\n"
            << "        break\n"
            << "      }\n"
            << "    }\n"
            << "    if (-not $duplicate) { $urls.Add($fallbackUrl) }\n"
            << "  }\n"
            << "  $lastError = ''\n"
            << "  foreach ($candidateUrl in $urls) {\n"
            << "    try {\n"
            << "      $candidateUri = [Uri]$candidateUrl\n"
            << "      if ($candidateUri.Scheme -ne 'https') { throw 'Epoch requires HTTPS transport.' }\n"
            << "      Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue\n"
            << "      Write-Step 'INFO' ($Label + ' URL: ' + $candidateUrl)\n"
            << "      Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $candidateUrl -OutFile $OutFile\n"
            << "      if ((Test-Path -LiteralPath $OutFile) -and ((Get-Item -LiteralPath $OutFile).Length -gt 0)) {\n"
            << "        Write-Step 'INFO' ($Label + ' downloaded successfully.')\n"
            << "        return\n"
            << "      }\n"
            << "      $lastError = 'downloaded file was empty'\n"
            << "      Write-Step 'WARN' ($Label + ' produced an empty file from ' + $candidateUrl + '.')\n"
            << "    }\n"
            << "    catch {\n"
            << "      $lastError = $_.Exception.Message\n"
            << "      if ([string]::IsNullOrWhiteSpace($lastError)) { $lastError = $_.ToString() }\n"
            << "      Write-Step 'WARN' ($Label + ' failed from ' + $candidateUrl + ': ' + $lastError)\n"
            << "    }\n"
            << "  }\n"
            << "  throw ($Label + ' failed from all configured URLs. Last error: ' + $lastError)\n"
            << "}\n"
            << "function Test-Cancel {\n"
            << "  if (Test-Path -LiteralPath $cancelPath) {\n"
            << "    Write-Step 'WARN' 'Source update cancel requested by operator.'\n"
            << "    Write-Handoff 'WARN' 'Source update cancel requested before runtime replacement.'\n"
            << "    Remove-Item -LiteralPath $cancelPath -Force -ErrorAction SilentlyContinue\n"
            << "    Write-Step 'WARN' 'Source update cancel complete.'\n"
            << "    Write-Handoff 'WARN' 'Source update cancel complete.'\n"
            << "    Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n"
            << "    exit 130\n"
            << "  }\n"
            << "}\n"
            << "function Clear-StaleSourceRuns {\n"
            << "  if (-not (Test-Path -LiteralPath $workRoot)) { return }\n"
            << "  $runFull = [System.IO.Path]::GetFullPath($runDir)\n"
            << "  Get-ChildItem -LiteralPath $workRoot -Force -ErrorAction SilentlyContinue | ForEach-Object {\n"
            << "    $name = $_.Name\n"
            << "    $managed = ($name -eq 'sx' -or $name -eq 'src' -or $name.StartsWith('run_') -or $name.StartsWith('.stale') -or $name.StartsWith('source_snapshot'))\n"
            << "    if ($managed) {\n"
            << "      $itemFull = [System.IO.Path]::GetFullPath($_.FullName)\n"
            << "      if (-not [string]::Equals($itemFull, $runFull, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "        Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "      }\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
            }();

        [&]()
            {
        ps
            << "trap {\n"
            << "  $message = $_.Exception.Message\n"
            << "  if ([string]::IsNullOrWhiteSpace($message)) {\n"
            << "    $message = $_.ToString()\n"
            << "  }\n"
            << "  Write-Step 'ERROR' $message\n"
            << "  Write-Handoff 'ERROR' $message\n"
            << "  Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $sourceChecksum -Force -ErrorAction SilentlyContinue\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($preparedSourceCleanupRoot)) {\n"
            << "    Remove-Item -LiteralPath $preparedSourceCleanupRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  }\n"
            << "  Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n"
            << "  Start-Sleep -Milliseconds 100\n"
            << "  Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  exit 1\n"
            << "}\n"
            << "function Join-ProcessArguments([string[]]$Arguments) {\n"
            << "  $parts = New-Object System.Collections.Generic.List[string]\n"
            << "  foreach ($argument in $Arguments) {\n"
            << "    if ($null -eq $argument) { continue }\n"
            << "    if ($argument.Length -eq 0) { $parts.Add('\"\"'); continue }\n"
            << "    if ($argument.IndexOfAny([char[]]@(' ', \"`t\", '\"')) -lt 0) { $parts.Add($argument); continue }\n"
            << "    $escaped = $argument.Replace('\\\\', '\\\\').Replace('\"', '\\\"')\n"
            << "    $parts.Add(('\"' + $escaped + '\"'))\n"
            << "  }\n"
            << "  return [string]::Join(' ', $parts)\n"
            << "}\n"
            << "function Stop-ProcessTree([int]$RootPid) {\n"
            << "  try {\n"
            << "    $all = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue\n"
            << "    $children = @{}\n"
            << "    foreach ($proc in $all) {\n"
            << "      if (-not $children.ContainsKey($proc.ParentProcessId)) { $children[$proc.ParentProcessId] = @() }\n"
            << "      $children[$proc.ParentProcessId] += $proc\n"
            << "    }\n"
            << "    $ids = New-Object System.Collections.Generic.List[int]\n"
            << "    function Add-ChildTree([int]$ProcessIdToAdd) {\n"
            << "      $ids.Add($ProcessIdToAdd) | Out-Null\n"
            << "      if ($children.ContainsKey($ProcessIdToAdd)) {\n"
            << "        foreach ($child in $children[$ProcessIdToAdd]) { Add-ChildTree $child.ProcessId }\n"
            << "      }\n"
            << "    }\n"
            << "    Add-ChildTree $RootPid\n"
            << "    foreach ($id in ($ids | Sort-Object -Descending)) {\n"
            << "      Stop-Process -Id $id -Force -ErrorAction SilentlyContinue\n"
            << "    }\n"
            << "  }\n"
            << "  catch {\n"
            << "    Stop-Process -Id $RootPid -Force -ErrorAction SilentlyContinue\n"
            << "  }\n"
            << "}\n"
            << "function Invoke-Tool([string]$FilePath, [string[]]$Arguments, [string]$WorkingDir, [string]$StepName, [string]$ExpectedOutputPath = '') {\n"
            << "  $toolProcess = $null\n"
            << "  $stdoutTask = $null\n"
            << "  $stderrTask = $null\n"
            << "  Push-Location $WorkingDir\n"
            << "  try {\n"
            << "    $argumentText = Join-ProcessArguments $Arguments\n"
            << "    $launchFilePath = $FilePath\n"
            << "    $launchArguments = $argumentText\n"
            << "    $extension = [System.IO.Path]::GetExtension($FilePath)\n"
            << "    if ([string]::Equals($extension, '.bat', [System.StringComparison]::OrdinalIgnoreCase) -or [string]::Equals($extension, '.cmd', [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "      $comspec = [System.Environment]::GetEnvironmentVariable('ComSpec')\n"
            << "      if ([string]::IsNullOrWhiteSpace($comspec)) { $comspec = 'C:\\Windows\\System32\\cmd.exe' }\n"
            << "      $launchFilePath = $comspec\n"
            << "      $launchArguments = '/d /s /c \"\"' + $FilePath + '\"'\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($argumentText)) { $launchArguments += ' ' + $argumentText }\n"
            << "      $launchArguments += '\"'\n"
            << "    }\n"
            << "    $startInfo = New-Object System.Diagnostics.ProcessStartInfo\n"
            << "    $startInfo.FileName = $launchFilePath\n"
            << "    $startInfo.Arguments = $launchArguments\n"
            << "    $startInfo.WorkingDirectory = $WorkingDir\n"
            << "    $startInfo.UseShellExecute = $false\n"
            << "    $startInfo.RedirectStandardOutput = $true\n"
            << "    $startInfo.RedirectStandardError = $true\n"
            << "    $startInfo.CreateNoWindow = $true\n"
            << "    $toolProcess = New-Object System.Diagnostics.Process\n"
            << "    $toolProcess.StartInfo = $startInfo\n"
            << "    if (-not $toolProcess.Start()) { throw ($StepName + ' failed to launch.') }\n"
            << "    $stdoutTask = $toolProcess.StandardOutput.ReadToEndAsync()\n"
            << "    $stderrTask = $toolProcess.StandardError.ReadToEndAsync()\n"
            << "    while (-not $toolProcess.WaitForExit(1000)) {\n"
            << "      if (Test-Path -LiteralPath $cancelPath) {\n"
            << "        Write-Step 'WARN' ($StepName + ' canceled; stopping child process tree.')\n"
            << "        Stop-ProcessTree $toolProcess.Id\n"
            << "        Start-Sleep -Milliseconds 250\n"
            << "        try { $toolProcess.WaitForExit(5000) | Out-Null } catch { }\n"
            << "        Complete-ToolOutput $stdoutTask $stderrTask\n"
            << "        Test-Cancel\n"
            << "      }\n"
            << "    }\n"
            << "    try { $toolProcess.WaitForExit() } catch { }\n"
            << "    Complete-ToolOutput $stdoutTask $stderrTask\n"
            << "    try { $toolProcess.Refresh() } catch { }\n"
            << "    $toolExitCode = $toolProcess.ExitCode\n"
            << "    $toolExitCodeText = [string]$toolExitCode\n";
            }();

        [&]()
            {
        ps
            << "    if ([string]::IsNullOrWhiteSpace($toolExitCodeText)) {\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($ExpectedOutputPath) -and (Test-Path -LiteralPath $ExpectedOutputPath)) {\n"
            << "        Write-Step 'WARN' ($StepName + ' did not report an exit code, but expected output exists; continuing.')\n"
            << "        Write-Handoff 'WARN' ($StepName + ' completed without an exit code; verified expected output exists.')\n"
            << "        return\n"
            << "      }\n"
            << "      throw ($StepName + ' did not report an exit code.')\n"
            << "    }\n"
            << "    if ($toolExitCode -ne 0) {\n"
            << "      throw ($StepName + ' failed with exit code ' + $toolExitCode + '.')\n"
            << "    }\n"
            << "  }\n"
            << "  finally {\n"
            << "    Pop-Location\n"
            << "    if ($null -ne $toolProcess) { try { $toolProcess.Dispose() } catch { } }\n"
            << "  }\n"
            << "}\n"
            << "function Resolve-GitExe {\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:GIT_EXE_PATH) -and (Test-Path -LiteralPath $env:GIT_EXE_PATH)) {\n"
            << "    Write-Step 'INFO' ('Using configured git: ' + $env:GIT_EXE_PATH)\n"
            << "    return $env:GIT_EXE_PATH\n"
            << "  }\n"
            << "  $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue\n"
            << "  if ($null -ne $gitCommand -and -not [string]::IsNullOrWhiteSpace($gitCommand.Source) -and (Test-Path -LiteralPath $gitCommand.Source)) {\n"
            << "    Write-Step 'INFO' ('Using git from PATH: ' + $gitCommand.Source)\n"
            << "    return $gitCommand.Source\n"
            << "  }\n"
            << "  $commonCandidates = @(\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\cmd\\git.exe'),\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\bin\\git.exe'),\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\mingw64\\bin\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\cmd\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\bin\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\mingw64\\bin\\git.exe')\n"
            << "  )\n"
            << "  foreach ($candidate in $commonCandidates) {\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {\n"
            << "      Write-Step 'INFO' ('Using installed git: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  $managedGitRoot = Join-Path $managedToolsRoot 'g'\n"
            << "  $managedGitCandidates = @(\n"
            << "    (Join-Path $managedGitRoot 'cmd\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'bin\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'mingw64\\bin\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'usr\\bin\\git.exe')\n"
            << "  )\n"
            << "  foreach ($candidate in $managedGitCandidates) {\n"
            << "    if (Test-Path -LiteralPath $candidate) {\n"
            << "      Write-Step 'INFO' ('Using managed git: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  New-Item -ItemType Directory -Path $managedToolsRoot -Force | Out-Null\n"
            << "  $gitArchiveUrl = Get-ReleaseAssetUrl $gitReleaseApiUrl 'MinGit-' '-64-bit.zip' 'busybox'\n"
            << "  if ([string]::IsNullOrWhiteSpace($gitArchiveUrl)) {\n"
            << "    throw 'Could not locate a managed Git release asset.'\n"
            << "  }\n"
            << "  $gitArchive = Join-Path $managedToolsRoot 'git.zip'\n"
            << "  $gitStaging = Join-Path $managedToolsRoot 'gx'\n"
            << "  Remove-Item -LiteralPath $gitArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $gitStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $managedGitRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Write-Step 'INFO' 'Downloading managed git.'\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "  Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $gitArchiveUrl -OutFile $gitArchive\n"
            << "  Expand-Archive -LiteralPath $gitArchive -DestinationPath $gitStaging -Force\n"
            << "  Move-Item -LiteralPath $gitStaging -Destination $managedGitRoot -Force\n"
            << "  Remove-Item -LiteralPath $gitArchive -Force -ErrorAction SilentlyContinue\n"
            << "  foreach ($candidate in $managedGitCandidates) {\n"
            << "    if (Test-Path -LiteralPath $candidate) {\n"
            << "      Write-Step 'INFO' ('Managed git ready at: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  throw 'Managed git extraction did not produce git.exe.'\n"
            << "}\n"
            << "function Test-VcpkgRoot([string]$Root) {\n"
            << "  if ([string]::IsNullOrWhiteSpace($Root)) { return $false }\n"
            << "  try { $fullRoot = [System.IO.Path]::GetFullPath($Root) } catch { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot $vcpkgExeName))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'ports'))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'versions'))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'scripts\\buildsystems\\vcpkg.cmake'))) { return $false }\n"
            << "  return $true\n"
            << "}\n"
            << "function Resolve-InstalledVcpkgExe {\n"
            << "  $candidates = @()\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_EXE_PATH) -and (Test-Path -LiteralPath $env:VCPKG_EXE_PATH)) {\n"
            << "    $candidates += (Split-Path -Parent $env:VCPKG_EXE_PATH)\n"
            << "  }\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:EPOCH_UPDATER_VCPKG_ROOT)) { $candidates += $env:EPOCH_UPDATER_VCPKG_ROOT }\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) { $candidates += $env:VCPKG_ROOT }\n"
            << "  $vcpkgCommand = Get-Command $vcpkgExeName -ErrorAction SilentlyContinue\n"
            << "  if ($null -ne $vcpkgCommand -and -not [string]::IsNullOrWhiteSpace($vcpkgCommand.Source)) {\n"
            << "    $candidates += (Split-Path -Parent $vcpkgCommand.Source)\n"
            << "  }\n";
            }();

        [&]()
            {
        ps
            << "  if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {\n"
            << "    $candidates += (Join-Path $env:USERPROFILE 'source\\repos\\vcpkg')\n"
            << "    $candidates += (Join-Path $env:USERPROFILE 'vcpkg')\n"
            << "  }\n"
            << "  $seen = @{}\n"
            << "  foreach ($candidate in $candidates) {\n"
            << "    if ([string]::IsNullOrWhiteSpace($candidate)) { continue }\n"
            << "    try { $fullCandidate = [System.IO.Path]::GetFullPath($candidate) } catch { continue }\n"
            << "    $key = $fullCandidate.ToLowerInvariant()\n"
            << "    if ($seen.ContainsKey($key)) { continue }\n"
            << "    $seen[$key] = $true\n"
            << "    if (Test-VcpkgRoot $fullCandidate) {\n"
            << "      Write-Step 'INFO' ('Using installed vcpkg: ' + $fullCandidate)\n"
            << "      Write-Handoff 'INFO' 'Using installed vcpkg toolchain.'\n"
            << "      return (Join-Path $fullCandidate $vcpkgExeName)\n"
            << "    }\n"
            << "    Write-Step 'WARN' ('Ignoring unusable vcpkg root: ' + $fullCandidate)\n"
            << "  }\n"
            << "  return ''\n"
            << "}\n"
            << "function Resolve-VcpkgExe {\n"
            << "  Test-Cancel\n"
            << "  $installedVcpkgExe = Resolve-InstalledVcpkgExe\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($installedVcpkgExe) -and (Test-Path -LiteralPath $installedVcpkgExe)) {\n"
            << "    return $installedVcpkgExe\n"
            << "  }\n"
            << "  $vcpkgRef = Get-VcpkgRef\n"
            << "  $safeRef = Get-ShortToken $vcpkgRef\n"
            << "  $managedVcpkgRoot = Join-Path $managedToolsRoot ('v-' + $safeRef)\n"
            << "  $managedVcpkgExe = Join-Path $managedVcpkgRoot $vcpkgExeName\n"
            << "  if (Test-Path -LiteralPath $managedVcpkgExe) {\n"
            << "    Write-Step 'INFO' ('Using managed vcpkg: ' + $managedVcpkgRoot)\n"
            << "    Write-Handoff 'INFO' 'Using cached managed vcpkg toolchain.'\n"
            << "    return $managedVcpkgExe\n"
            << "  }\n"
            << "  $vcpkgArchive = Join-Path $managedToolsRoot ('v-' + $safeRef + '.zip')\n"
            << "  $vcpkgStaging = Join-Path $managedToolsRoot ('vx-' + $safeRef)\n"
            << "  $bootstrapScript = Join-Path $managedVcpkgRoot $vcpkgBootstrapName\n"
            << "  New-Item -ItemType Directory -Path $managedToolsRoot -Force | Out-Null\n"
            << "  Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Write-Step 'INFO' ('Downloading managed vcpkg (' + $vcpkgRef + ').')\n"
            << "  Write-Handoff 'INFO' 'Downloading managed vcpkg toolchain.'\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "  Test-Cancel\n"
            << "  try {\n"
            << "    Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri (Get-VcpkgArchiveUrl $vcpkgRef) -OutFile $vcpkgArchive\n"
            << "    Test-Cancel\n"
            << "  }\n"
            << "  catch {\n"
            << "    if ($vcpkgRef -eq $vcpkgDefaultRef) {\n"
            << "      throw\n"
            << "    }\n"
            << "    Write-Step 'WARN' ('Managed vcpkg ref ' + $vcpkgRef + ' was not downloadable: ' + $_.Exception.Message + '. Falling back to ' + $vcpkgDefaultRef + '.')\n"
            << "    Write-Handoff 'WARN' 'Pinned vcpkg snapshot was unavailable; using the managed fallback toolchain.'\n"
            << "    $vcpkgRef = $vcpkgDefaultRef\n"
            << "    $safeRef = Get-ShortToken $vcpkgRef\n"
            << "    $managedVcpkgRoot = Join-Path $managedToolsRoot ('v-' + $safeRef)\n"
            << "    $managedVcpkgExe = Join-Path $managedVcpkgRoot $vcpkgExeName\n"
            << "    $vcpkgArchive = Join-Path $managedToolsRoot ('v-' + $safeRef + '.zip')\n"
            << "    $vcpkgStaging = Join-Path $managedToolsRoot ('vx-' + $safeRef)\n"
            << "    $bootstrapScript = Join-Path $managedVcpkgRoot $vcpkgBootstrapName\n"
            << "    Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    Write-Step 'INFO' ('Downloading managed vcpkg fallback (' + $vcpkgRef + ').')\n"
            << "    Write-Handoff 'INFO' 'Downloading managed vcpkg fallback toolchain.'\n"
            << "    Test-Cancel\n"
            << "    Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri (Get-VcpkgArchiveUrl $vcpkgRef) -OutFile $vcpkgArchive\n"
            << "    Test-Cancel\n"
            << "  }\n"
            << "  Test-Cancel\n"
            << "  Expand-Archive -LiteralPath $vcpkgArchive -DestinationPath $vcpkgStaging -Force\n"
            << "  Test-Cancel\n"
            << "  $bootstrapCandidate = Get-ChildItem -LiteralPath $vcpkgStaging -Recurse -File -Filter $vcpkgBootstrapName -ErrorAction SilentlyContinue | Select-Object -First 1\n"
            << "  if ($null -eq $bootstrapCandidate) {\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    throw 'Managed vcpkg archive did not contain bootstrap-vcpkg.bat.'\n"
            << "  }\n"
            << "  $extractedRoot = Split-Path -Parent $bootstrapCandidate.FullName\n"
            << "  $stagingFull = [System.IO.Path]::GetFullPath($vcpkgStaging)\n"
            << "  $rootFull = [System.IO.Path]::GetFullPath($extractedRoot)\n"
            << "  if ([string]::Equals($stagingFull, $rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "    New-Item -ItemType Directory -Path $managedVcpkgRoot -Force | Out-Null\n"
            << "    Copy-Item -Path (Join-Path $vcpkgStaging '*') -Destination $managedVcpkgRoot -Recurse -Force\n"
            << "  }\n"
            << "  else {\n"
            << "    New-Item -ItemType Directory -Path $managedVcpkgRoot -Force | Out-Null\n"
            << "    Copy-Item -Path (Join-Path $extractedRoot '*') -Destination $managedVcpkgRoot -Recurse -Force\n"
            << "  }\n"
            << "  Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  if (-not (Test-Path -LiteralPath $bootstrapScript)) {\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    throw 'Managed vcpkg bootstrap script is missing after extraction repair.'\n"
            << "  }\n"
            << "  Test-Cancel\n"
            << "  Write-Step 'INFO' 'Bootstrapping managed vcpkg.'\n"
            << "  Write-Handoff 'INFO' 'Bootstrapping managed vcpkg toolchain.'\n"
            << "  Invoke-Tool $bootstrapScript @('-disableMetrics') $managedVcpkgRoot 'vcpkg bootstrap' $managedVcpkgExe\n"
            << "  Test-Cancel\n";
            }();

        [&]()
            {
        ps
            << "  if (-not (Test-Path -LiteralPath $managedVcpkgExe)) {\n"
            << "    throw 'Managed vcpkg bootstrap did not produce vcpkg.exe.'\n"
            << "  }\n"
            << "  Write-Step 'INFO' ('Managed vcpkg ready at: ' + $managedVcpkgExe)\n"
            << "  Write-Handoff 'INFO' 'Managed vcpkg toolchain is ready.'\n"
            << "  return $managedVcpkgExe\n"
            << "}\n"
            << "function Prepare-ManifestForManagedVcpkg([string]$ManifestRoot, [string]$VcpkgRoot) {\n"
            << "  $manifestFile = Join-Path $ManifestRoot 'vcpkg.json'\n"
            << "  if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "    throw 'Source manifest file is missing for managed vcpkg preparation.'\n"
            << "  }\n"
            << "  $configFile = Join-Path $ManifestRoot 'vcpkg-configuration.json'\n"
            << "  Remove-Item -LiteralPath $configFile -Force -ErrorAction SilentlyContinue\n"
            << "  $gitExe = Resolve-GitExe\n"
            << "  $gitDir = Join-Path $VcpkgRoot '.git'\n"
            << "  $head = ''\n"
            << "  if (Test-Path -LiteralPath $gitDir) {\n"
            << "    try {\n"
            << "      $resolvedGitDir = (& $gitExe '-C' $VcpkgRoot 'rev-parse' '--absolute-git-dir' 2>$null | Out-String).Trim()\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($resolvedGitDir)) {\n"
            << "        $resolvedGitDir = [System.IO.Path]::GetFullPath($resolvedGitDir)\n"
            << "        $expectedGitDir = [System.IO.Path]::GetFullPath($gitDir)\n"
            << "        if ([string]::Equals($resolvedGitDir, $expectedGitDir, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "          $head = (& $gitExe ('--git-dir=' + $gitDir) ('--work-tree=' + $VcpkgRoot) 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "        }\n"
            << "        else {\n"
            << "          Write-Step 'WARN' ('Managed vcpkg git resolution escaped the sandboxed repo: ' + $resolvedGitDir)\n"
            << "        }\n"
            << "      }\n"
            << "    }\n"
            << "    catch {\n"
            << "      $head = ''\n"
            << "    }\n"
            << "  }\n"
            << "  if ([string]::IsNullOrWhiteSpace($head)) {\n"
            << "    Write-Step 'INFO' 'Initializing managed vcpkg git registry snapshot.'\n"
            << "    Invoke-Tool $gitExe @('-C', $VcpkgRoot, 'init') $VcpkgRoot 'git init'\n"
            << "    if (-not (Test-Path -LiteralPath $gitDir)) {\n"
            << "      throw 'Managed vcpkg git init did not create a local .git directory.'\n"
            << "    }\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'user.name', 'Epoch Updater') $VcpkgRoot 'git config user.name'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'user.email', 'updater@epoch.local') $VcpkgRoot 'git config user.email'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'core.longpaths', 'true') $VcpkgRoot 'git config core.longpaths'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'add', '--all') $VcpkgRoot 'git add'\n"
            << "    try {\n"
            << "      Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'commit', '--no-gpg-sign', '-m', 'Managed vcpkg registry snapshot') $VcpkgRoot 'git commit'\n"
            << "    }\n"
            << "    catch {\n"
            << "    }\n"
            << "    try {\n"
            << "      $head = (& $gitExe ('--git-dir=' + $gitDir) ('--work-tree=' + $VcpkgRoot) 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "    }\n"
            << "    catch {\n"
            << "      $head = ''\n"
            << "    }\n"
            << "  }\n"
            << "  if ([string]::IsNullOrWhiteSpace($head)) {\n"
            << "    throw 'Managed vcpkg git registry did not produce a usable HEAD revision.'\n"
            << "  }\n"
            << "  $head = $head.Trim().ToLowerInvariant()\n"
            << "  $manifestText = Get-Content -LiteralPath $manifestFile -Raw -ErrorAction Stop\n"
            << "  $baselineRegex = [regex]'\"builtin-baseline\"\\s*:\\s*\"[0-9A-Fa-f]+\"'\n"
            << "  if ($baselineRegex.IsMatch($manifestText)) {\n"
            << "    $manifestText = $baselineRegex.Replace($manifestText, ('\"builtin-baseline\": \"' + $head + '\"'), 1)\n"
            << "  }\n"
            << "  else {\n"
            << "    $braceIndex = $manifestText.IndexOf('{')\n"
            << "    if ($braceIndex -lt 0) {\n"
            << "      throw 'Source manifest is malformed and could not be updated.'\n"
            << "    }\n"
            << "    $manifestText = $manifestText.Insert($braceIndex + 1, [Environment]::NewLine + '  \"builtin-baseline\": \"' + $head + '\",')\n"
            << "  }\n"
            << "  [System.IO.File]::WriteAllText($manifestFile, $manifestText, $utf8NoBom)\n"
            << "  Write-Step 'INFO' 'Reconfigured the source snapshot to use the managed vcpkg git registry.'\n"
            << "}\n"
            << "function Stage-CMakePolicyOverlayPort([string]$VcpkgRoot, [string]$OverlayRoot, [string]$PortName) {\n"
            << "  $sourcePortDir = Join-Path $VcpkgRoot ('ports\\' + $PortName)\n"
            << "  if (-not (Test-Path -LiteralPath $sourcePortDir)) {\n"
            << "    Write-Step 'WARN' ('Managed vcpkg ' + $PortName + ' port was not found; skipping overlay patch.')\n"
            << "    return $false\n"
            << "  }\n"
            << "  $overlayPortDir = Join-Path $OverlayRoot $PortName\n"
            << "  Remove-Item -LiteralPath $overlayPortDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Copy-Item -LiteralPath $sourcePortDir -Destination $overlayPortDir -Recurse -Force\n"
            << "  $portfile = Join-Path $overlayPortDir 'portfile.cmake'\n"
            << "  if (-not (Test-Path -LiteralPath $portfile)) {\n"
            << "    Write-Step 'WARN' ('Staged ' + $PortName + ' overlay port has no portfile.cmake.')\n"
            << "    return $false\n"
            << "  }\n"
            << "  $portfileText = Get-Content -LiteralPath $portfile -Raw -ErrorAction Stop\n"
            << "  $portfileText = $portfileText.Replace(\"`r`n\", \"`n\").Replace(\"`r\", \"`n\")\n"
            << "  if (-not $portfileText.Contains('CMAKE_POLICY_VERSION_MINIMUM=3.5')) {\n"
            << "    $optionsMarker = \"`n    OPTIONS`n\"\n"
            << "    if ($portfileText.Contains($optionsMarker)) {\n"
            << "      $portfileText = $portfileText.Replace($optionsMarker, $optionsMarker + \"        -DCMAKE_POLICY_VERSION_MINIMUM=3.5`n\")\n"
            << "    }\n";
            }();

        [&]()
            {
        ps
            << "    else {\n"
            << "      $configureMarker = 'vcpkg_cmake_configure('\n"
            << "      $configureIndex = $portfileText.IndexOf($configureMarker, [System.StringComparison]::Ordinal)\n"
            << "      if ($configureIndex -lt 0) {\n"
            << "        Write-Step 'WARN' ('Could not patch the ' + $PortName + ' overlay port; continuing without that policy overlay.')\n"
            << "        return $false\n"
            << "      }\n"
            << "      $lineBreak = $portfileText.IndexOf(\"`n\", $configureIndex)\n"
            << "      if ($lineBreak -lt 0) {\n"
            << "        Write-Step 'WARN' ('Could not patch the ' + $PortName + ' overlay port; continuing without that policy overlay.')\n"
            << "        return $false\n"
            << "      }\n"
            << "      $portfileText = $portfileText.Insert($lineBreak + 1, \"    OPTIONS`n        -DCMAKE_POLICY_VERSION_MINIMUM=3.5`n\")\n"
            << "    }\n"
            << "    [System.IO.File]::WriteAllText($portfile, $portfileText.Replace(\"`n\", [Environment]::NewLine), $utf8NoBom)\n"
            << "  }\n"
            << "  Write-Step 'INFO' ('Prepared a ' + $PortName + ' overlay port for modern CMake policy handling.')\n"
            << "  return $true\n"
            << "}\n"
            << "function Prepare-CMakePolicyOverlay([string]$VcpkgRoot) {\n"
            << "  $overlayRoot = Join-Path $managedToolsRoot 'ov'\n"
            << "  New-Item -ItemType Directory -Path $overlayRoot -Force | Out-Null\n"
            << "  $stagedAny = $false\n"
            << "  foreach ($portName in @('freetype', 'glad', 'glfw3', 'libogg', 'libvorbis', 'raylib', 'sdl3', 'sfml', 'shaderc', 'spirv-tools', 'zlib')) {\n"
            << "    $stagedPort = Stage-CMakePolicyOverlayPort $VcpkgRoot $overlayRoot $portName\n"
            << "    if ($stagedPort -eq $true) {\n"
            << "      $stagedAny = $true\n"
            << "    }\n"
            << "  }\n"
            << "  if (-not $stagedAny) {\n"
            << "    return ''\n"
            << "  }\n"
            << "  Write-Step 'INFO' 'Prepared updater overlay ports for modern CMake policy handling.'\n"
            << "  return $overlayRoot\n"
            << "}\n"
            << "New-Item -ItemType Directory -Path $workRoot -Force | Out-Null\n"
            << "Clear-StaleSourceRuns\n"
            << "Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "New-Item -ItemType Directory -Path $runDir -Force | Out-Null\n"
            << "Set-Content -LiteralPath $activeRunPath -Value ($runDir + [Environment]::NewLine + 'pid=' + $PID) -NoNewline -Encoding UTF8\n"
            << "Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $handoffLog -Force -ErrorAction SilentlyContinue\n"
            << "Write-Step 'INFO' 'Source update worker started.'\n"
            << "Write-Handoff 'INFO' 'Source update worker started.'\n"
            << "Write-Handoff 'INFO' ('Build log: ' + $buildLog)\n"
            << "Write-Step 'INFO' ('Source root: ' + $sourceRoot)\n"
            << "Write-Step 'INFO' ('Manifest root: ' + $manifestRoot)\n"
            << "Write-Step 'INFO' ('MSBuild: ' + $msbuildExe)\n"
            << "Write-Step 'INFO' ('Source updater build lane: ' + $buildConfiguration + '|' + $buildPlatform + ' (runtime may currently be Debug).')\n"
            << "Write-Handoff 'INFO' ('Source updater build lane: ' + $buildConfiguration + '|' + $buildPlatform + '. Debug launches are updated through the Release output lane.')\n"
            << "$env:MSBUILDDISABLENODEREUSE = '1'\n"
            << "Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceChecksum -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "New-Item -ItemType Directory -Path $runDir -Force | Out-Null\n"
            << "if (Test-Path -LiteralPath $sourceRoot) {\n"
            << "  $staleSourceRoot = $sourceRoot + '.stale.' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()\n"
            << "  Move-Item -LiteralPath $sourceRoot -Destination $staleSourceRoot -Force -ErrorAction SilentlyContinue\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $sourceRoot) {\n"
            << "  throw ('Could not clear stale source update root: ' + $sourceRoot)\n"
            << "}\n"
            << "New-Item -ItemType Directory -Path (Split-Path -Parent $sourceArchive) -Force | Out-Null\n"
            << "Test-Cancel\n"
            << "if (-not [string]::IsNullOrWhiteSpace($preparedSourceArchive)) {\n"
            << "  if (-not (Test-Path -LiteralPath $preparedSourceArchive -PathType Leaf)) {\n"
            << "    throw 'The authorized source archive disappeared before the worker could claim it.'\n"
            << "  }\n"
            << "  Copy-Item -LiteralPath $preparedSourceArchive -Destination $sourceArchive -Force\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($preparedSourceCleanupRoot)) {\n"
            << "    Remove-Item -LiteralPath $preparedSourceCleanupRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  }\n"
            << "  Write-Step 'INFO' 'Using process-authorized AES-GCM verified source snapshot.'\n"
            << "} else {\n"
            << "  Write-Step 'INFO' 'Downloading latest " << PROJECT_SOURCE_ARCHIVE_LABEL() << ".'\n"
            << "  Invoke-DownloadWithFallback $sourceUrl $sourceFallbackUrls $sourceArchive 'Source snapshot download'\n"
            << "  Invoke-DownloadWithFallback $sourceChecksumUrl @() $sourceChecksum 'Source snapshot checksum download'\n"
            << "  $sourceChecksumText = (Get-Content -LiteralPath $sourceChecksum -Raw).Trim()\n"
            << "  $expectedSourceSha = (($sourceChecksumText -split '\\s+')[0]).ToLowerInvariant()\n"
            << "  if ($expectedSourceSha -notmatch '^[0-9a-f]{64}$') {\n"
            << "    throw 'Source snapshot checksum metadata is invalid.'\n"
            << "  }\n"
            << "  $actualSourceSha = (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant()\n"
            << "  if ($actualSourceSha -ne $expectedSourceSha) {\n"
            << "    throw 'Source snapshot SHA-256 verification failed.'\n"
            << "  }\n"
            << "  Write-Step 'INFO' 'Source snapshot SHA-256 verified.'\n"
            << "}\n"
            << "Test-Cancel\n"
            << "Expand-Archive -LiteralPath $sourceArchive -DestinationPath $stagingDir -Force\n"
            << "Test-Cancel\n"
            << "$extractedRoot = Get-ChildItem -LiteralPath $stagingDir -Directory | Select-Object -First 1\n"
            << "if ($null -ne $extractedRoot) {\n"
            << "  Move-Item -LiteralPath $extractedRoot.FullName -Destination $sourceRoot -Force\n"
            << "} else {\n"
            << "  New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $stagingDir '*') -Destination $sourceRoot -Recurse -Force\n"
            << "}\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n";
            }();

        [&]()
            {
        ps
            << "$manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "  $nestedRoot = Get-ChildItem -LiteralPath $sourceRoot -Directory -ErrorAction SilentlyContinue | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'Engine\\vcpkg.json') } | Select-Object -First 1\n"
            << "  if ($null -ne $nestedRoot) {\n"
            << "    $repairRoot = $sourceRoot + '.nested.' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()\n"
            << "    $nestedName = $nestedRoot.Name\n"
            << "    Move-Item -LiteralPath $sourceRoot -Destination $repairRoot -Force\n"
            << "    Move-Item -LiteralPath (Join-Path $repairRoot $nestedName) -Destination $sourceRoot -Force\n"
            << "    Remove-Item -LiteralPath $repairRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    $manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "    Write-Step 'INFO' 'Repaired nested source snapshot root.'\n"
            << "  }\n"
            << "}\n"
            << "if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "  throw ('Downloaded source snapshot did not contain Engine\\vcpkg.json at: ' + $manifestFile)\n"
            << "}\n"
            << "Write-Step 'INFO' ('Source snapshot ready at: ' + $sourceRoot)\n"
            << "Write-Handoff 'INFO' 'Source snapshot downloaded and extracted.'\n"
            << "Test-Cancel\n"
            << "if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {\n"
            << "  Write-Step 'INFO' ('Checking inherited VCPKG_ROOT: ' + $env:VCPKG_ROOT)\n"
            << "}\n"
            << "$vcpkgExe = Resolve-VcpkgExe\n"
            << "Test-Cancel\n"
            << "$vcpkgRoot = Split-Path -Parent $vcpkgExe\n"
            << "$managedInstallRoot = Join-Path $manifestRoot 'vcpkg_installed'\n"
            << "Remove-Item Env:VCPKG_ROOT -Force -ErrorAction SilentlyContinue\n"
            << "$env:VCPKG_ROOT = $vcpkgRoot\n"
            << "Write-Step 'INFO' ('Pinned worker-local VCPKG_ROOT to selected toolchain: ' + $vcpkgRoot)\n"
            << "$managedToolsFull = [System.IO.Path]::GetFullPath($managedToolsRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)\n"
            << "$vcpkgRootFull = [System.IO.Path]::GetFullPath($vcpkgRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)\n"
            << "$usingManagedVcpkg = [string]::Equals($vcpkgRootFull, $managedToolsFull, [System.StringComparison]::OrdinalIgnoreCase) -or $vcpkgRootFull.StartsWith($managedToolsFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or $vcpkgRootFull.StartsWith($managedToolsFull + [System.IO.Path]::AltDirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)\n"
            << "if ($usingManagedVcpkg) {\n"
            << "  Prepare-ManifestForManagedVcpkg $manifestRoot $vcpkgRoot\n"
            << "} else {\n"
            << "  Write-Step 'INFO' 'Using installed vcpkg registry; source manifest baseline left intact.'\n"
            << "}\n"
            << "Test-Cancel\n"
            << "$overlayRoot = Prepare-CMakePolicyOverlay $vcpkgRoot\n"
            << "Write-Step 'INFO' 'Restoring source dependencies with vcpkg.'\n"
            << "Write-Handoff 'INFO' 'Restoring source dependencies with vcpkg.'\n"
            << "$vcpkgArgs = @('install', '--triplet', $triplet, ('--x-manifest-root=' + $manifestRoot), ('--x-builtin-ports-root=' + (Join-Path $vcpkgRoot 'ports')), ('--x-builtin-registry-versions-dir=' + (Join-Path $vcpkgRoot 'versions')))\n"
            << "if (-not [string]::IsNullOrWhiteSpace($overlayRoot)) {\n"
            << "  $vcpkgArgs += ('--overlay-ports=' + $overlayRoot)\n"
            << "}\n"
            << "Invoke-Tool $vcpkgExe $vcpkgArgs $manifestRoot 'vcpkg restore'\n"
            << "Write-Handoff 'INFO' 'Source dependencies restored.'\n"
            << "Test-Cancel\n"
            << "$buildSucceeded = $false\n"
            << "for ($attempt = 1; $attempt -le 3 -and -not $buildSucceeded; ++$attempt) {\n"
            << "  try {\n"
            << "    Test-Cancel\n"
            << "    Write-Step 'INFO' ('MSBuild attempt ' + $attempt + ' started.')\n"
            << "    Write-Handoff 'INFO' ('MSBuild Release attempt ' + $attempt + ' started; node reuse is disabled and long compiles may stay on this line for several minutes.')\n"
            << "    Invoke-Tool $msbuildExe @($solution, ('/t:' + $buildTarget), ('/p:Configuration=' + $buildConfiguration), ('/p:Platform=' + $buildPlatform), ('/p:VcpkgRoot=' + $vcpkgRoot), ('/p:VcpkgManifestRoot=' + $manifestRoot), ('/p:VcpkgInstalledDir=' + $managedInstallRoot), '/p:VcpkgManifestInstall=false', ('/p:VcpkgTriplet=' + $triplet), '/p:UseMultiToolTask=false', '/p:BuildInParallel=false', '/m:1', '/nr:false', '/clp:ErrorsOnly') $sourceRoot ('MSBuild attempt ' + $attempt)\n"
            << "    Write-Handoff 'INFO' ('MSBuild Release attempt ' + $attempt + ' completed successfully.')\n"
            << "    $buildSucceeded = $true\n"
            << "  }\n"
            << "  catch {\n"
            << "    Write-Step 'WARN' $_.Exception.Message\n"
            << "    Write-Handoff 'WARN' $_.Exception.Message\n"
            << "    if ($attempt -lt 3) {\n"
            << "      Write-Step 'INFO' 'Retrying the source build after restore.'\n"
            << "      Start-Sleep -Seconds 5\n"
            << "      Test-Cancel\n"
            << "    }\n"
            << "  }\n"
            << "}\n"
            << "if (-not $buildSucceeded) {\n"
            << "  throw 'Source build failed after three attempts.'\n"
            << "}\n"
            << "if (-not (Test-Path -LiteralPath $builtExe)) {\n"
            << "  throw 'Built runtime output is missing after source update.'\n"
            << "}\n"
            << "Write-Step 'INFO' ('Built runtime ready at: ' + $builtExe)\n"
            << "Write-Handoff 'INFO' ('Built runtime ready at: ' + $builtExe)\n"
            << "Write-Handoff 'INFO' 'Waiting for runtime handoff.'\n"
            << "for ($attempt = 1; $attempt -le 600; ++$attempt) {\n"
            << "  Test-Cancel\n"
            << "  try {\n"
            << "    if (Test-Path -LiteralPath $targetExe) {\n"
            << "      Remove-Item -LiteralPath $targetExe -Force -ErrorAction Stop\n"
            << "    }\n"
            << "  }\n"
            << "  catch {\n"
            << "  }\n"
            << "  if (-not (Test-Path -LiteralPath $targetExe)) {\n"
            << "    break\n"
            << "  }\n"
            << "  if ($attempt -eq 1 -or ($attempt % 15) -eq 0) {\n"
            << "    Write-Handoff 'INFO' ('Waiting for target runtime unlock attempt ' + $attempt + '.')\n"
            << "  }\n"
            << "  Start-Sleep -Seconds 1\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $targetExe) {\n"
            << "  throw 'Timed out waiting for target runtime executable to unlock.'\n"
            << "}\n"
            << "Test-Cancel\n"
            << "Copy-Item -LiteralPath $builtExe -Destination $targetExe -Force\n"
            << "Get-ChildItem -LiteralPath $builtDir -File | Where-Object { $_.Extension -in '.dll', '.manifest' } | ForEach-Object {\n"
            << "  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $targetDir $_.Name) -Force\n"
            << "}\n";
            }();

        [&]()
            {
        ps
            << "if (Test-Path -LiteralPath $builtAssetsDir) {\n"
            << "  New-Item -ItemType Directory -Path $targetAssetsDir -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $builtAssetsDir '*') -Destination $targetAssetsDir -Recurse -Force\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $sourceRepoAssetsDir) {\n"
            << "  New-Item -ItemType Directory -Path $targetAssetsDir -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $sourceRepoAssetsDir '*') -Destination $targetAssetsDir -Recurse -Force\n"
            << "}\n"
            << "Write-Handoff 'INFO' 'Source runtime files copied successfully.'\n"
            << "Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceChecksum -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $cancelPath -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item Env:EPOCH_UPDATER_SHELL_AUTO_COMMAND -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item Env:EPOCH_EDITOR_AUTO_COMMAND -Force -ErrorAction SilentlyContinue\n"
            << "$env:EPOCH_POST_UPDATE_STARTUP_DELAY_MS = '3000'\n"
            << "$restartedProcess = Start-Process -FilePath $targetExe -WorkingDirectory $targetDir -PassThru\n"
            << "Remove-Item Env:EPOCH_POST_UPDATE_STARTUP_DELAY_MS -Force -ErrorAction SilentlyContinue\n"
            << "Start-Sleep -Milliseconds 900\n"
            << "try {\n"
            << "  $focusShell = New-Object -ComObject WScript.Shell\n"
            << "  [void]$focusShell.AppActivate($restartedProcess.Id)\n"
            << "} catch {\n"
            << "}\n"
            << "Write-Handoff 'INFO' 'Restarted updated runtime.'\n"
            << "Start-Sleep -Seconds 1\n"
            << "Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n";
            }();

        ps.close();

        if (!system_detail::launch_powershell_script(worker_script, silent_worker, build_log))
        {
            std::error_code marker_ec;
            std::filesystem::remove(active_run_path, marker_ec);
            system_detail::remove_update_cache_path_best_effort(run_dir);
            cleanup_prepared_source();
            system_detail::log_error("Failed to launch source update worker.");
            return false;
        }

        system_detail::log_info("Source update worker launched successfully.");
        return true;
    }
}
#endif
