# EPOCH_MANAGED_GENERATED_FILE: windows_build_v1
param(
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFile = Join-Path $projectRoot 'PlatformerExample2D.vcxproj'
$repoRoot = if ($env:EPOCH_REPO_ROOT) {
    (Resolve-Path -LiteralPath $env:EPOCH_REPO_ROOT).Path
}
& python (Join-Path $repoRoot 'Tools/build/verify_epoch_project_source.py') --root $repoRoot --edition 'epoch.public.github.v1'
if ($LASTEXITCODE -ne 0) { throw 'Generated project source verification failed.' }
$solutionDir = $repoRoot.TrimEnd('\\') + '\\'
$vcpkgManifestRoot = Join-Path $repoRoot 'Engine'
$logDir = Join-Path $projectRoot 'Artifacts\Reports'
$binDir = Join-Path $projectRoot ('Artifacts\Runtime\windows\' + $Configuration + '\' + $Platform)
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
New-Item -ItemType Directory -Force -Path $binDir | Out-Null
$logPath = Join-Path $logDir ('build-' + $Configuration.ToLowerInvariant() + '-' + $Platform.ToLowerInvariant() + '.log')
$repoBuildLockPath = Join-Path $repoRoot 'Artifacts\generated-project-build.lock'
function Resolve-MSBuild {
    if (-not [string]::IsNullOrWhiteSpace($env:MSBUILD_EXE_PATH) -and (Test-Path -LiteralPath $env:MSBUILD_EXE_PATH)) {
        return $env:MSBUILD_EXE_PATH
    }
    $programFilesX86 = ${env:ProgramFiles(x86)}
    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
            $installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
            if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installation) -and [IO.Path]::IsPathRooted($installation)) {
                $msbuild64 = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
                if (Test-Path -LiteralPath $msbuild64 -PathType Leaf) { return $msbuild64 }
            }
        }
    }
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'Could not locate MSBuild. Set MSBUILD_EXE_PATH or install Visual Studio Build Tools.'
}
function Enter-EpochGeneratedBuildLock {
    $lockDir = Split-Path -Parent $repoBuildLockPath
    New-Item -ItemType Directory -Force -Path $lockDir | Out-Null
    $deadline = [DateTime]::UtcNow.AddMinutes(10)
    while ($true) {
        try {
            return [System.IO.File]::Open($repoBuildLockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        }
        catch [System.IO.IOException] {
            if ([DateTime]::UtcNow -gt $deadline) { throw 'Timed out waiting for another Epoch generated project build to finish.' }
            Start-Sleep -Milliseconds 250
        }
    }
}
if (-not (Test-Path -LiteralPath $projectFile)) {
    throw ('Missing generated project file: ' + $projectFile)
}
$msbuild = Resolve-MSBuild
$buildExit = 0
$buildLock = $null
Push-Location -LiteralPath $projectRoot
try {
    $buildLock = Enter-EpochGeneratedBuildLock
    '[INFO] Project root: ' + $projectRoot | Tee-Object -FilePath $logPath
    '[INFO] Repo root: ' + $repoRoot | Tee-Object -FilePath $logPath -Append
    '[INFO] SolutionDir: ' + $solutionDir | Tee-Object -FilePath $logPath -Append
    '[INFO] Build lock: ' + $repoBuildLockPath | Tee-Object -FilePath $logPath -Append
    '[INFO] Epoch child build project: ' + $projectFile | Tee-Object -FilePath $logPath -Append
    '[INFO] MSBuild: ' + $msbuild | Tee-Object -FilePath $logPath -Append
    $msbuildArgs = @(
        $projectFile,
        '/t:Build',
        ('/p:Configuration=' + $Configuration),
        ('/p:Platform=' + $Platform),
        '/p:PlatformToolset=v143',
        ('/p:SolutionDir=' + $solutionDir),
        ('/p:VcpkgManifestRoot=' + $vcpkgManifestRoot),
        '/m:1',
        '/nr:false',
        '/v:minimal'
    )
    # EPOCH_INSTALLED_DEPENDENCIES_BEGIN
    function ConvertTo-EpochMSBuildValue([string]$value) {
        return $value.Replace('%', '%25').Replace(';', '%3B')
    }
    function Get-EpochInstalledDependencies {
        if ($Platform -ne 'x64' -or $Configuration -notin @('Debug', 'Release')) {
            throw 'This generated Windows project supports x64 Debug or Release.'
        }
        $base = Join-Path $repoRoot 'Artifacts\Dependencies\vcpkg_installed'
        if (-not [string]::IsNullOrWhiteSpace($env:EPOCH_CANDIDATE_DEPENDENCY_ROOT)) {
            $base = $env:EPOCH_CANDIDATE_DEPENDENCY_ROOT
        }
        if (-not [System.IO.Path]::IsPathRooted($base)) { throw 'The installed dependency base must be an absolute path.' }
        $base = [System.IO.Path]::GetFullPath($base).TrimEnd('\', '/')
        $headers = @('asio.hpp', 'curl/curl.h', 'glad/glad.h', 'GLFW/glfw3.h', 'glm/glm.hpp', 'openssl/ssl.h', 'vulkan/vulkan.hpp', 'volk.h', 'shaderc/shaderc.hpp', 'spirv-tools/libspirv.hpp', 'SDL3/SDL.h', 'SFML/Graphics.hpp', 'raylib.h', 'zlib.h', 'nlohmann/json.hpp', 'httplib.h', 'sqlite3.h')
        $libraries = @('glad.lib', 'SDL3.lib', 'raylib.lib', 'sqlite3.lib', 'libcrypto.lib', 'libssl.lib', 'glfw3dll.lib', 'volk.lib', 'vulkan-1.lib', 'shaderc.lib', 'SPIRV-Tools.lib')
        if ($Configuration -eq 'Debug') {
            $libraryPath = 'debug\lib'
            $libraries += @('sfml-graphics-d.lib', 'sfml-window-d.lib', 'sfml-system-d.lib', 'libcurl-d.lib', 'zd.lib')
        } else {
            $libraryPath = 'lib'
            $libraries += @('sfml-graphics.lib', 'sfml-window.lib', 'sfml-system.lib', 'libcurl.lib', 'z.lib')
        }
        $triplet = Join-Path $base 'x64-windows'
        $firstMissing = $null
        # Prefer the canonical layout even when an obsolete nested install exists.
        foreach ($root in @($triplet, (Join-Path $triplet 'x64-windows'))) {
            $missing = $null
            foreach ($header in $headers) {
                $file = Join-Path $root ('include\' + $header)
                if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { $missing = $file; break }
            }
            if ($null -eq $missing) {
                foreach ($library in $libraries) {
                    $file = Join-Path $root (Join-Path $libraryPath $library)
                    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { $missing = $file; break }
                }
            }
            if ($null -eq $missing) {
                return @{ Base = (Split-Path -Parent $root) + '\'; Triplet = $root + '\' }
            }
            if ($null -eq $firstMissing) { $firstMissing = $missing }
        }
        throw ('Installed dependencies are incomplete for ' + $Configuration + '. Missing: ' + $firstMissing + '. Repair the Engine SDK dependencies explicitly; this project build will not download or reinstall them.')
    }
    $dependencies = Get-EpochInstalledDependencies
    $msbuildArgs += @('/p:VcpkgManifestInstall=false', '/p:VcpkgAutoBootstrap=false', '/p:VcpkgTriplet=x64-windows', ('/p:VcpkgInstalledDir=' + (ConvertTo-EpochMSBuildValue $dependencies.Base)), ('/p:EpochVcpkgInstallRoot=' + (ConvertTo-EpochMSBuildValue $dependencies.Triplet)), '/p:PreferredToolArchitecture=x64', '/p:UseMultiToolTask=false', '/p:CL_MPCount=2', '/p:EnforceProcessCountAcrossBuilds=true')
    ('[INFO] Installed dependency triplet: ' + $dependencies.Triplet + ' (reuse only; 2 compiler workers)') | Tee-Object -FilePath $logPath -Append
    $previousBuildEnvironment = @{}
    foreach ($name in @('VCPKG_MAX_CONCURRENCY', 'CMAKE_BUILD_PARALLEL_LEVEL', 'MSBUILDDISABLENODEREUSE')) {
        $previousBuildEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $(if ($name -eq 'MSBUILDDISABLENODEREUSE') { '1' } else { '2' }), 'Process')
    }
    # EPOCH_INSTALLED_DEPENDENCIES_END
    $nativeErrorPreference = $ErrorActionPreference
    try {
        # Windows PowerShell treats native stderr as ErrorRecord. Preserve it and the actual exit code.
        $ErrorActionPreference = 'Continue'
        & $msbuild @msbuildArgs 2>&1 | Tee-Object -FilePath $logPath -Append
        $buildExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $nativeErrorPreference
        foreach ($name in $previousBuildEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $previousBuildEnvironment[$name], 'Process') }
    }
    ('[INFO] MSBuild exit code: ' + $buildExit) | Tee-Object -FilePath $logPath -Append
}
catch {
    ('[ERROR] Project build host: ' + $_.Exception.Message) | Tee-Object -FilePath $logPath -Append
    $buildExit = 1
}
finally {
    if ($null -ne $buildLock) { $buildLock.Dispose() }
    Pop-Location
}
if ($buildExit -ne 0) {
    exit $buildExit
}
$exePath = Join-Path $projectRoot ('Artifacts\Runtime\windows\' + $Configuration + '\' + $Platform + '\' + 'PlatformerExample2D.exe')
'[INFO] Output: ' + $exePath | Tee-Object -FilePath $logPath -Append
