#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string[]]$ProjectAsset,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$JdkRoot,
    [Parameter(Mandatory)][string]$SdlSourceRoot,
    [Parameter(Mandatory)][string]$AndroidDependencies,
    [Parameter(Mandatory)][string]$OutputApk,
    [ValidateSet('x86_64','arm64-v8a')][string]$Abi = 'x86_64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateRange(1,12)][int]$Parallel = 4,
    [string]$BuildDirectory,
    [string]$NdkRoot,
    [string]$ApplicationId,
    [string]$CalculatorCoachArchive,
    [string]$FixedShorelineSnapshot,
    [switch]$PackagedPlanetaryPose,
    [switch]$PackagedPlanetaryFlight,
    [switch]$ShorelineFlight
)

$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This Android APK build entry currently uses the Windows SDK and NDK tools.' }
$PackagedPlanetaryPose = $PackagedPlanetaryPose -or $PackagedPlanetaryFlight
if ($PackagedPlanetaryPose -and $FixedShorelineSnapshot) {
    throw 'Choose either a packaged planetary pose or a fixed shoreline.'
}
if ($ShorelineFlight -and (!$FixedShorelineSnapshot -or $PackagedPlanetaryPose)) {
    throw 'Shoreline flight requires a packaged shoreline descriptor supplied to -FixedShorelineSnapshot.'
}

$terrainPaths = [ordered]@{
    TERRAIN_MESH = 'Assets/Worlds/Planet.pose.epoch_mesh'
    TERRAIN_FRAME = 'Assets/Worlds/Planet.pose.frame'
    TERRAIN_RECIPE = 'Assets/Worlds/Planet.pose.epoch_planet'
}
$explicitAssets = @($ProjectAsset | ForEach-Object { $_.Replace('\','/') })
$terrainCount = @($terrainPaths.Values | Where-Object { $_ -in $explicitAssets }).Count
if ($terrainCount -and !$PackagedPlanetaryPose) {
    throw 'A packaged planetary terrain cut requires -PackagedPlanetaryPose.'
}
if ($terrainCount -and $PackagedPlanetaryFlight) {
    throw 'Android planetary flight uses the controller body mesh. Omit the fixed terrain cut.'
}
if ($terrainCount -and $terrainCount -ne $terrainPaths.Count) {
    throw 'Include the terrain cut mesh, frame, and recipe together in ProjectAsset.'
}

function Get-PlanetaryPoseInput([string]$Root, [string]$Relative, [long]$MaximumBytes) {
    $folder = Get-Item -LiteralPath $Root
    if (!$folder.PSIsContainer -or ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'A packaged planetary pose needs an ordinary project directory.'
    }
    foreach ($part in ($Relative.Split('/') | Select-Object -SkipLast 1)) {
        $folder = Get-Item -LiteralPath (Join-Path $folder.FullName $part)
        if (!$folder.PSIsContainer -or ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "The packaged planetary pose input has a redirected directory: $Relative"
        }
    }
    $file = Get-Item -LiteralPath (Join-Path $Root $Relative)
    if ($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $file.Length -le 0 -or $file.Length -gt $MaximumBytes) {
        throw "The packaged planetary pose input must be a nonempty regular file of at most $MaximumBytes bytes: $Relative"
    }
    return $file
}

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$project = (Resolve-Path -LiteralPath $ProjectRoot).Path
$sdk = (Resolve-Path -LiteralPath $SdkRoot).Path
$dependencies = (Resolve-Path -LiteralPath $AndroidDependencies).Path
if (!$NdkRoot) { $NdkRoot = Join-Path $sdk 'ndk/30.0.16248370' }
$ndk = (Resolve-Path -LiteralPath $NdkRoot).Path
$llvm = Join-Path $ndk 'toolchains/llvm/prebuilt/windows-x86_64'
$systemAbi = if ($Abi -eq 'x86_64') { 'x86_64-linux-android' } else { 'aarch64-linux-android' }
$entry = Join-Path $project 'CMakeLists.txt'
$manifest = Join-Path $project 'project.epoch.json'
$sdl = Join-Path $dependencies 'lib/libSDL3.so'
$runtime = Join-Path $llvm "sysroot/usr/lib/$systemAbi/libc++_shared.so"
foreach ($required in @($entry,$manifest,$sdl,$runtime,(Join-Path $llvm 'bin/clang-scan-deps.exe'))) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing Android build input: $required" }
}

$identity = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
    [Text.Encoding]::UTF8.GetBytes($project))).Substring(0,16).ToLowerInvariant()
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repo "Artifacts/Intermediate/android-project/$identity/$Abi/$Configuration" }
$build = [IO.Path]::GetFullPath($BuildDirectory)
$destination = [IO.Path]::GetFullPath($OutputApk)
if (Test-Path -LiteralPath $destination) { throw 'Supply a new APK output path.' }
$coachArchive = if ($CalculatorCoachArchive) { (Resolve-Path -LiteralPath $CalculatorCoachArchive).Path } else { '' }
$coachFlag = if ($coachArchive) { 'ON' } else { 'OFF' }
$fixedShorelinePin = ''
if ($FixedShorelineSnapshot) {
    if ($ShorelineFlight) {
        $managedEntry = Get-PlanetaryPoseInput $project 'source/epoch.main.cpp' 256KB
        if (!(Get-Content -LiteralPath $managedEntry.FullName -Raw).Contains('EPOCH_ANDROID_SHORELINE_FLIGHT')) {
            throw 'Refresh the managed Android Game entry before opting into shoreline flight.'
        }
    }
    $snapshot = Get-Item -LiteralPath $FixedShorelineSnapshot
    if ($snapshot.PSIsContainer -or ($snapshot.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Supply a regular fixed shoreline descriptor of at most 16 KiB.'
    }
    $snapshotReader = $snapshot.OpenRead()
    try {
        if ($snapshotReader.Length -le 0 -or $snapshotReader.Length -gt 16KB) {
            throw 'Supply a regular fixed shoreline descriptor of at most 16 KiB.'
        }
        $fixedShorelinePin = (Get-FileHash -InputStream $snapshotReader).Hash.ToLowerInvariant()
    } finally { $snapshotReader.Dispose() }
}
$posePins = [ordered]@{}
$posePaths = [ordered]@{}
$terrainPin = ''
if ($PackagedPlanetaryPose) {
    $managedEntry = Get-PlanetaryPoseInput $project 'source/epoch.main.cpp' 256KB
    $managedSource = Get-Content -LiteralPath $managedEntry.FullName -Raw
    if (!$managedSource.Contains(
        'epochengine::project::AndroidPackagedPlanetaryPose{EPOCH_ANDROID_PLANETARY_POSE_SHA256')) {
        throw 'Refresh the managed Android Game entry before opting into a packaged planetary pose.'
    }
    if ($terrainCount -and !$managedSource.Contains('EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256')) {
        throw 'Refresh the managed Android Game entry before packaging a planetary terrain cut.'
    }
    if ($PackagedPlanetaryFlight -and !$managedSource.Contains('EPOCH_ANDROID_PLANETARY_FLIGHT')) {
        throw 'Refresh the managed Android Game entry before opting into planetary flight.'
    }
    $manifestFile = Get-PlanetaryPoseInput $project 'project.epoch.json' 64KB
    $manifestBytes = [IO.File]::ReadAllBytes($manifestFile.FullName)
    if ($manifestBytes.Length -gt 64KB) { throw 'The saved Game manifest exceeds 64 KiB.' }
    $manifestText = [Text.UTF8Encoding]::new($false,$true).GetString($manifestBytes).TrimStart([char]0xFEFF)
    $savedProject = $manifestText | ConvertFrom-Json
    if ($savedProject.kind -cne 'game' -or $savedProject.entry_source -cne 'source/epoch.main.cpp' -or
        $savedProject.scene -isnot [string]) {
        throw 'A packaged planetary pose needs an ordinary saved Game manifest and its managed Android entry.'
    }
    $sceneRelative = $savedProject.scene.Replace('\','/')
    if ($sceneRelative -cnotmatch '^(Assets|worlds)/' -or
        $sceneRelative -match '[\x00-\x1F:]|//|/$|(?:^|/)\.{1,2}(?:/|$)') {
        throw 'The saved Game scene must be a project-relative runtime asset.'
    }
    $posePaths = [ordered]@{
        POSE = 'Assets/Worlds/Planet.epoch_exploration'
        RECIPE = 'Assets/Worlds/Planet.epoch_planet'
        MANIFEST = 'project.epoch.json'
        SCENE = $sceneRelative
    }
    $maximumBytes = @{
        POSE = 16KB; RECIPE = 1KB; SCENE = 16MB
        TERRAIN_MESH = 8MB; TERRAIN_FRAME = 4KB; TERRAIN_RECIPE = 1KB
    }
    if ($terrainCount) {
        foreach ($role in $terrainPaths.Keys) {
            $posePaths.Add($role, $terrainPaths[$role])
        }
    }
    foreach ($role in $posePaths.Keys) {
        $relative = $posePaths[$role]
        if ($relative -cnotin $explicitAssets) {
            throw "Include the packaged planetary pose input '$relative' in ProjectAsset with its exact case."
        }
        if ($role -ceq 'MANIFEST') {
            $posePins[$role] = [Convert]::ToHexString(
                [Security.Cryptography.SHA256]::HashData($manifestBytes)).ToLowerInvariant()
        } else {
            $file = Get-PlanetaryPoseInput $project $relative $maximumBytes[$role]
            $posePins[$role] = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        }
    }
    if ($terrainCount) {
        # The runtime hashes the same ordered, newline-terminated ASCII digests.
        $digestList = ($terrainPaths.Keys | ForEach-Object { $posePins[$_] }) -join "`n"
        $terrainPin = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
            [Text.Encoding]::ASCII.GetBytes($digestList + "`n"))).ToLowerInvariant()
    }
}
$ninja = (Get-Command ninja -CommandType Application).Source
$configure = @(
    '-S',$project,'-B',$build,'-G','Ninja',
    "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake",
    "-DANDROID_ABI=$Abi", '-DANDROID_PLATFORM=android-26', '-DANDROID_STL=c++_shared',
    "-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=$llvm/bin/clang-scan-deps.exe",
    "-DCMAKE_FIND_ROOT_PATH=$dependencies", "-DCMAKE_PREFIX_PATH=$dependencies",
    "-DOPENSSL_ROOT_DIR=$dependencies", '-DOPENSSL_USE_STATIC_LIBS=TRUE',
    "-Dasio_DIR=$dependencies/share/asio", "-DEPOCH_CALCULATOR_ANDROID_COACH=$coachFlag",
    "-DEPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE=$coachArchive",
    "-DEPOCH_ANDROID_FIXED_SHORELINE_SHA256=$fixedShorelinePin",
    "-DEPOCH_ANDROID_SHORELINE_FLIGHT=$(if ($ShorelineFlight) {'ON'} else {'OFF'})",
    "-DEPOCH_ANDROID_PLANETARY_POSE_SHA256=$($posePins['POSE'])",
    "-DEPOCH_ANDROID_PLANETARY_RECIPE_SHA256=$($posePins['RECIPE'])",
    "-DEPOCH_ANDROID_PLANETARY_MANIFEST_SHA256=$($posePins['MANIFEST'])",
    "-DEPOCH_ANDROID_PLANETARY_SCENE_SHA256=$($posePins['SCENE'])",
    "-DEPOCH_ANDROID_PLANETARY_TERRAIN_SHA256=$terrainPin",
    "-DEPOCH_ANDROID_PLANETARY_FLIGHT=$(if ($PackagedPlanetaryFlight) {'ON'} else {'OFF'})")

$mutable = Join-Path $repo ('Artifacts/Intermediate/android-project-stage/' + [guid]::NewGuid().ToString('N'))
$temp = (New-Item -ItemType Directory -Force (Join-Path $mutable 'tmp')).FullName
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
try {
    $env:TEMP = $temp
    $env:TMP = $temp
    & cmake @configure | Out-Host
    if ($LASTEXITCODE) { throw "Android project configuration failed (exit $LASTEXITCODE)." }
    & cmake --build $build --target epoch_project_runtime --parallel $Parallel | Out-Host
    if ($LASTEXITCODE) { throw "Android project build failed (exit $LASTEXITCODE)." }

    $native = Join-Path $build "apk/jniLibs/$Abi"
    $names = @('libmain.so')
    if ($coachArchive) { $names += 'libepoch_coach.so' }
    foreach ($name in $names) {
        & "$llvm/bin/llvm-strip.exe" --strip-debug -o "$mutable/$name" "$native/$name"
        if ($LASTEXITCODE) { throw "Could not assemble the native library $name." }
    }
    $package = @{
        ProjectRoot=$project; ProjectAsset=$ProjectAsset; NativeAppLibrary="$mutable/libmain.so";
        SdlLibrary=$sdl; CxxRuntimeLibrary=$runtime; SdlSourceRoot=$SdlSourceRoot;
        SdkRoot=$sdk; NdkRoot=$ndk; JdkRoot=$JdkRoot; Abi=$Abi; OutputApk=$destination;
        ApplicationId=$ApplicationId
    }
    if ($coachArchive) { $package.CalculatorCoachLibrary = "$mutable/libepoch_coach.so" }
    if ($FixedShorelineSnapshot) {
        $package.FixedShorelineSnapshot = $snapshot.FullName
        $package.FixedShorelineSnapshotSha256 = $fixedShorelinePin
    }
    if ($PackagedPlanetaryPose) { $package.OutputApk = Join-Path $mutable 'planetary-pose.apk' }
    $result = & "$PSScriptRoot/package_epoch_android_apk.ps1" @package | ConvertFrom-Json
    if ($FixedShorelineSnapshot) {
        $result | Add-Member NoteProperty packaged_shoreline_flight ([bool]$ShorelineFlight)
    }
    if ($PackagedPlanetaryPose) {
        $verified = [ordered]@{}
        foreach ($role in $posePaths.Keys) {
            $relative = $posePaths[$role]
            $asset = $result.assets | Where-Object path -CEQ "project/$relative"
            if (!$asset -or $asset.sha256 -cne $posePins[$role]) {
                throw "The packaged planetary pose input changed during the build: $relative"
            }
            $verified[$relative] = $asset.sha256
        }
        [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        [IO.File]::Move($package.OutputApk,$destination)
        $result.apk = $destination
        $result | Add-Member NoteProperty packaged_planetary_pose $verified
        $result | Add-Member NoteProperty packaged_planetary_flight ([bool]$PackagedPlanetaryFlight)
        if ($terrainPin) {
            $result | Add-Member NoteProperty packaged_planetary_terrain_sha256 $terrainPin
        }
    }
    $result | Add-Member NoteProperty build_host 'windows'
    $result | Add-Member NoteProperty configuration $Configuration
    $result | Add-Member NoteProperty build_directory $build
    $result | Add-Member NoteProperty minimum_api 26
    $result | ConvertTo-Json -Depth 6
} finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
