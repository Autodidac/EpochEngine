param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string[]]$ProjectAsset,
    [Parameter(Mandatory)][string]$NativeAppLibrary,
    [Parameter(Mandatory)][string]$SdlLibrary,
    [Parameter(Mandatory)][string]$CxxRuntimeLibrary,
    [Parameter(Mandatory)][string]$SdlSourceRoot,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$JdkRoot,
    [Parameter(Mandatory)][string]$OutputApk,
    [ValidateSet('x86_64','arm64-v8a')][string]$Abi = 'x86_64',
    [string]$NdkRoot,
    [string]$ApplicationId,
    [string]$DebugKeyStore,
    [string]$CalculatorCoachLibrary,
    [string]$FixedShorelineSnapshot,
    [string]$FixedShorelineSnapshotSha256
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)

function Get-RegularItem([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Expected a regular file: $Path"
    }
    return $item
}

function Confirm-ShorelineJsonFields($Object, [string[]]$Fields, [string[]]$OptionalFields = @()) {
    if ($Object.ValueKind -ne 'Object') { throw 'Expected a fixed shoreline descriptor object.' }
    $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($property in $Object.EnumerateObject()) {
        if (!$observed.Add($property.Name)) { throw 'Duplicate fixed shoreline descriptor field.' }
    }
    [string[]]$allowedFields = $Fields + $OptionalFields
    if (!$observed.IsSupersetOf($Fields) -or !$observed.IsSubsetOf($allowedFields)) {
        throw 'Unexpected fixed shoreline descriptor fields.'
    }
}

function Get-FixedShorelineAssets([string]$Descriptor, [string]$ExpectedHash) {
    $directory = 'assets/planetary/fixed-shoreline'
    $types = [ordered]@{
        'terrain.epoch_mesh' = @{ maximum=8MB; magic='^454D534801000000$' }
        'terrain.epoch_planet_patch' = @{ maximum=16MB; magic='^50504C540[2-6]000000$' }
        'terrain.water_appearance' = @{ maximum=52 + 4096 * 12 + 32; magic='^4550574101000000$' }
    }
    $descriptorFile = Get-RegularItem $Descriptor
    for ($ancestor = $descriptorFile.Directory; $ancestor; $ancestor = $ancestor.Parent) {
        if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'The fixed shoreline snapshot folder is redirected.'
        }
    }
    $stream = $descriptorFile.OpenRead()
    try {
        $size = $stream.Length
        if ($size -le 0 -or $size -gt 16KB) { throw 'The fixed shoreline descriptor exceeds its size bound.' }
        $hash = (Get-FileHash -InputStream $stream).Hash.ToLowerInvariant()
        if ($ExpectedHash -and $hash -cne $ExpectedHash) { throw 'The fixed shoreline descriptor differs from its native build pin.' }
        $stream.Position = 0
        $json = [Text.Json.JsonDocument]::Parse($stream, [Text.Json.JsonDocumentOptions]::new())
        try {
            $root = $json.RootElement
            Confirm-ShorelineJsonFields $root @('schema','files','project_id','edition','manifest_sha256',
                'scene_sha256','recipe_sha256','planetary_source','fft_source','recipe_key','request_id',
                'mesh_key','body','radius_m','origin_m','sample_seconds','view','cut_settings') @('density_clouds')
            if ($root.GetProperty('schema').GetString() -cne 'epoch.android.fixed_shoreline_snapshot.v1') {
                throw 'Unsupported fixed shoreline descriptor schema.'
            }
            $densityClouds = [Text.Json.JsonElement]::new()
            if ($root.TryGetProperty('density_clouds', [ref]$densityClouds) -and
                $densityClouds.ValueKind -notin @('True','False')) {
                throw 'The fixed shoreline density_clouds field must be Boolean.'
            }
            $view = $root.GetProperty('view')
            Confirm-ShorelineJsonFields $view @('position_m','forward','up')
            foreach ($vector in @($root.GetProperty('origin_m'),$view.GetProperty('position_m'),
                    $view.GetProperty('forward'),$view.GetProperty('up'))) {
                if ($vector.ValueKind -ne 'Array' -or $vector.GetArrayLength() -ne 3) {
                    throw 'The fixed shoreline descriptor requires three-component vectors.'
                }
                foreach ($component in $vector.EnumerateArray()) {
                    if ($component.ValueKind -ne 'Number') { throw 'Expected a numeric fixed shoreline vector component.' }
                }
            }
            if ($root.GetProperty('cut_settings').ValueKind -ne 'String') {
                throw 'The fixed shoreline cut settings must be an encoded recipe.'
            }
            $files = $root.GetProperty('files')
            if ($files.ValueKind -ne 'Array' -or $files.GetArrayLength() -ne $types.Count) {
                throw 'The fixed shoreline snapshot requires exactly three typed files.'
            }
            $assets = [ordered]@{
                "$directory/shoreline.json" = [pscustomobject]@{ file=$descriptorFile; bytes=$size; sha256=$hash; magic=$null }
            }
            foreach ($entry in $files.EnumerateArray()) {
                Confirm-ShorelineJsonFields $entry @('name','bytes','sha256')
                $name = $entry.GetProperty('name').GetString()
                $bytes = $entry.GetProperty('bytes').GetInt64()
                $sha256 = $entry.GetProperty('sha256').GetString()
                if ($name -cnotin $types.Keys -or $assets.Contains("$directory/$name") -or
                    $bytes -lt 8 -or $bytes -gt $types[$name].maximum -or $sha256 -cnotmatch '^[a-f0-9]{64}$') {
                    throw 'Invalid fixed shoreline file name, size, or SHA-256.'
                }
                $file = Get-RegularItem (Join-Path $descriptorFile.DirectoryName $name)
                if ($file.Length -ne $bytes) { throw "Fixed shoreline file size differs: $name" }
                $assets["$directory/$name"] = [pscustomobject]@{ file=$file; bytes=$bytes; sha256=$sha256; magic=$types[$name].magic }
            }
            return $assets
        } finally { $json.Dispose() }
    } finally { $stream.Dispose() }
}

function Copy-FixedShorelineAsset($Entry, [string]$Destination) {
    $file = Get-RegularItem $Entry.file.FullName
    $source = $file.OpenRead()
    try {
        if ($source.Length -ne $Entry.bytes) { throw "Fixed shoreline file size changed: $($file.Name)" }
        if ($Entry.magic) {
            $header = [byte[]]::new(8)
            if ($source.Read($header, 0, $header.Length) -ne $header.Length -or
                [Convert]::ToHexString($header) -cnotmatch $Entry.magic) {
                throw "Invalid typed fixed shoreline file: $($file.Name)"
            }
            $source.Position = 0
        }
        $target = [IO.File]::Open($Destination, 'CreateNew', 'Write', 'None')
        try { $source.CopyTo($target) }
        finally { $target.Dispose() }
    } finally { $source.Dispose() }
}

function Invoke-PackagingTool([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$(Split-Path $Tool -Leaf) exited $LASTEXITCODE." }
}

function Confirm-ManifestBoundary([xml]$Manifest, [bool]$CoachEnabled) {
    $android = 'http://schemas.android.com/apk/res/android'
    $root = $Manifest.DocumentElement
    $application = $root.SelectSingleNode('application')
    if ($root.LocalName -cne 'manifest' -or $root.HasAttribute('sharedUserId',$android) -or
        $root.SelectNodes('*').Count -ne 2 -or !$root.SelectSingleNode('uses-sdk') -or
        !$application -or $application.HasAttribute('process',$android) -or
        $application.GetAttribute('allowBackup',$android) -cne 'false') {
        throw 'The Android manifest must declare one app with private storage and its own process.'
    }

    $components = @{
        'org.epochengine.runtime.EpochActivity' = @{ element='activity'; exported='true'; isolated='' }
    }
    if ($CoachEnabled) {
        $components['org.epochengine.runtime.EpochCalculatorCoachModelActivity'] = @{
            element='activity'; exported='false'; isolated=''
        }
        $components['org.epochengine.runtime.EpochCalculatorCoachService'] = @{
            element='service'; exported='false'; isolated='true'
        }
    }
    foreach ($node in $application.SelectNodes('*')) {
        $name = $node.GetAttribute('name',$android)
        $rule = $components[$name]
        if (!$rule -or $name -cnotin $components.Keys -or $node.LocalName -cne $rule.element -or
            $node.GetAttribute('exported',$android) -cne $rule.exported -or
            $node.HasAttribute('process',$android) -or
            $node.GetAttribute('isolatedProcess',$android) -cne $rule.isolated) {
            throw "Unexpected Android application component: $name"
        }
        if ($rule.exported -ceq 'true') {
            $filter = $node.SelectSingleNode('intent-filter')
            $action = if ($filter) { $filter.SelectSingleNode('action') }
            $category = if ($filter) { $filter.SelectSingleNode('category') }
            if (!$action -or !$category -or $node.SelectNodes('*').Count -ne 1 -or
                $filter.SelectNodes('*').Count -ne 2 -or
                $action.GetAttribute('name',$android) -cne 'android.intent.action.MAIN' -or
                $category.GetAttribute('name',$android) -cne 'android.intent.category.LAUNCHER') {
                throw 'EpochActivity must expose only its Android launcher intent.'
            }
        } elseif ($node.SelectNodes('*').Count) {
            throw "Private Android application component declares a handler: $name"
        }
        $components.Remove($name)
    }
    if ($components.Count) { throw 'An expected Android application component is missing.' }
}

function Confirm-SharedLibrary([string]$Path, [int]$Machine) {
    $item = Get-RegularItem $Path
    $header = [byte[]]::new(20)
    $stream = $item.OpenRead()
    try { $count = $stream.Read($header, 0, $header.Length) }
    finally { $stream.Dispose() }
    if ($count -ne $header.Length -or $header[0] -ne 0x7f -or $header[1] -ne 0x45 -or
        $header[2] -ne 0x4c -or $header[3] -ne 0x46 -or $header[4] -ne 2 -or $header[5] -ne 1 -or
        $header[16] -ne 3 -or $header[17] -ne 0 -or $header[18] -ne $Machine -or $header[19] -ne 0) {
        throw "Library is not a $Abi Android shared ELF: $Path"
    }
    return $item
}

function Confirm-Export([string]$ReadElf, [IO.FileInfo]$Library, [string]$Symbol) {
    $exports = & $ReadElf --dyn-syms --wide $Library.FullName
    $name = [regex]::Escape($Symbol)
    if ($LASTEXITCODE -ne 0 -or !($exports -cmatch "^\s*\d+:\s+\S+\s+\d+\s+FUNC\s+(?:GLOBAL|WEAK)\s+(?:DEFAULT|PROTECTED)\s+(?!UND\s)\S+\s+$name(?:@@?\S+)?\s*`$")) {
        throw "The shared library does not export $Symbol`: $($Library.FullName)"
    }
}

function Confirm-PackageContents([string]$Apk, $Expected, $ExpectedSizes) {
    $archive = [IO.Compression.ZipFile]::OpenRead($Apk)
    try {
        $verified = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName
            if (!$Expected.Contains($name)) {
                if ($name -cmatch '^(assets|lib)/' -and !$name.EndsWith('/')) { throw "Unexpected packaged file: $name" }
                continue
            }
            if (!$verified.Add($name)) { throw "Duplicate packaged file: $name" }
            if ($ExpectedSizes.ContainsKey($name) -and $entry.Length -ne $ExpectedSizes[$name]) {
                throw "Packaged file size differs: $name"
            }
            $stream = $entry.Open()
            try { $digest = (Get-FileHash -InputStream $stream).Hash }
            finally { $stream.Dispose() }
            if ($digest -ne $Expected[$name]) { throw "Packaged file bytes differ: $name" }
        }
        if (!$verified.SetEquals([string[]]@($Expected.Keys))) { throw 'The APK has an incomplete runtime closure.' }
    } finally { $archive.Dispose() }
}

$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$sdk = (Resolve-Path -LiteralPath $SdkRoot).Path
$jdk = (Resolve-Path -LiteralPath $JdkRoot).Path
$buildTools = Join-Path $sdk 'build-tools/36.0.0'
$androidJar = (Get-RegularItem (Join-Path $sdk 'platforms/android-36/android.jar')).FullName
if (!$NdkRoot) { $NdkRoot = Join-Path $sdk 'ndk/30.0.16248370' }
$readElf = (Get-RegularItem (Join-Path $NdkRoot 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe')).FullName
$sdlTree = Join-Path ((Resolve-Path -LiteralPath $SdlSourceRoot).Path) 'android-project/app/src/main/java/org/libsdl/app'
$activity = Get-RegularItem (Join-Path $sdlTree 'SDLActivity.java')
$sdlFiles = @(Get-ChildItem -LiteralPath $sdlTree -File -Filter '*.java' |
    Sort-Object Name | ForEach-Object { (Get-RegularItem $_.FullName).FullName })
if ($sdlFiles.Count -lt 2) { throw "SDL Android Java source closure is missing beside $($activity.FullName)." }
$manifest = Get-RegularItem (Join-Path $root 'project.epoch.json')
$project = Get-Content -LiteralPath $manifest.FullName -Raw | ConvertFrom-Json
if ($project.project_format -ne 'epoch-project-v1' -or !$project.id) { throw 'Invalid saved Epoch project manifest.' }
if (!$ApplicationId) { $ApplicationId = 'org.epochengine.' + ([string]$project.id).ToLowerInvariant() }
if ($ApplicationId -cnotmatch '^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$') {
    throw 'Supply an Android application ID with lowercase dot-separated segments.'
}
$appLabel = if ($project.display_name) { [string]$project.display_name } else { [string]$project.id }
$destination = [IO.Path]::GetFullPath($OutputApk)
if ([IO.Path]::GetExtension($destination) -ine '.apk' -or (Test-Path -LiteralPath $destination)) {
    throw 'Supply a new .apk output path.'
}

$machine = if ($Abi -eq 'x86_64') { 62 } else { 183 }
$libraries = [ordered]@{
    'libSDL3.so' = Confirm-SharedLibrary $SdlLibrary $machine
    'libmain.so' = Confirm-SharedLibrary $NativeAppLibrary $machine
    'libc++_shared.so' = Confirm-SharedLibrary $CxxRuntimeLibrary $machine
}
Confirm-Export $readElf $libraries['libSDL3.so'] 'SDL_GetVersion'
Confirm-Export $readElf $libraries['libmain.so'] 'SDL_main'
if ($project.kind -ceq 'editor') {
    Confirm-Export $readElf $libraries['libmain.so'] 'Java_org_epochengine_runtime_EpochActivity_publishProjectArchive'
}
$coachEnabled = [bool]$CalculatorCoachLibrary
$mainExports = & $readElf --dyn-syms --wide $libraries['libmain.so'].FullName
if ($LASTEXITCODE -ne 0 -or [bool]($mainExports -cmatch '\s+epoch_calculator_android_coach_enabled\s*$') -ne $coachEnabled) {
    throw 'Package the Calculator adapter and isolated Coach worker together.'
}
if ($coachEnabled) {
    if ($project.kind -cne 'tool' -or $project.id -cne 'BasicSoftwareExampleCalculator') {
        throw 'This worker is scoped to the Calculator example.'
    }
    $libraries['libepoch_coach.so'] = Confirm-SharedLibrary $CalculatorCoachLibrary $machine
    foreach ($method in @('prepareGeneration','generate','cancelGeneration')) {
        Confirm-Export $readElf $libraries['libepoch_coach.so'] ("Java_org_epochengine_runtime_EpochCalculatorCoachService_" + $method)
    }
}
$systemAbi = if ($Abi -eq 'x86_64') { 'x86_64-linux-android' } else { 'aarch64-linux-android' }
$systemLibraries = Join-Path $NdkRoot "toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/$systemAbi/26"
foreach ($name in $libraries.Keys) {
    $elf = & $readElf --program-headers --dynamic --wide $libraries[$name].FullName
    if ($LASTEXITCODE) { throw "Could not inspect the Android library: $name" }
    $loads = @($elf | Where-Object { $_ -match '^\s*LOAD\s' })
    if (!$loads.Count) { throw "Android library has no loadable segments: $name" }
    foreach ($load in $loads) {
        if ($load -notmatch '\s0x([a-fA-F0-9]+)\s*$' -or [Convert]::ToUInt64($Matches[1],16) -lt 16384) {
            throw "Android library lacks 16 KiB segment alignment: $name"
        }
    }
    foreach ($entry in $elf | Where-Object { $_ -match '\(NEEDED\)' }) {
        if ($entry -cnotmatch '\[(lib[a-zA-Z0-9_+.-]+\.so)\]\s*$') {
            throw "Invalid native dependency name in $name."
        }
        $dependency = $Matches[1]
        if (!$libraries.Contains($dependency) -and
            !(Test-Path -LiteralPath (Join-Path $systemLibraries $dependency) -PathType Leaf)) {
            throw "Unpackaged dependency unavailable at Android API 26: $name -> $dependency"
        }
    }
}
$projectAssets = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$content = [ordered]@{}
$fixedShorelineAssets = [ordered]@{}
if ($FixedShorelineSnapshotSha256 -and !$FixedShorelineSnapshot) {
    throw 'The fixed shoreline build pin requires its descriptor.'
}
if ($FixedShorelineSnapshot) {
    $fixedShorelineAssets = Get-FixedShorelineAssets $FixedShorelineSnapshot $FixedShorelineSnapshotSha256
    foreach ($path in $fixedShorelineAssets.Keys) { $content[$path] = $fixedShorelineAssets[$path].file }
}
foreach ($asset in $ProjectAsset) {
    $relative = $asset.Replace('\','/')
    if ($relative -match '(^/|[\x00-\x1F:]|//|/$|(?:^|/)\.{1,2}(?:/|$))' -or
        ($relative -cne 'project.epoch.json' -and $relative -notmatch '^(Assets|worlds|scripts|Packages)/')) {
        throw "Invalid project runtime asset: $asset"
    }
    if (($relative -match '\.(cpp|c|cc|h|hpp|hh|hxx|cxx|ixx|inl)$') -and
        ($relative -notmatch '^scripts/.+\.ascript\.cpp$')) {
        throw "Compile project source into libmain.so before packaging: $asset"
    }
    if ($relative -match '^Packages/\.runtime/planetary_terrain/') {
        throw "Exclude the desktop Planetary exporter and its receipt from Android project assets ($asset)."
    }
    if ($relative -match '\.(gguf|safetensors)$') {
        throw 'Keep model weights outside the base Android APK.'
    }
    if (!$projectAssets.Add($relative)) { throw "Project runtime asset is listed twice: $asset" }
    $folder = $root
    foreach ($part in ($relative.Split('/') | Select-Object -SkipLast 1)) {
        $folder = Join-Path $folder $part
        $owner = Get-Item -LiteralPath $folder
        if (!$owner.PSIsContainer -or ($owner.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Project asset folder is not a regular directory: $folder"
        }
    }
    $content['project/' + $relative] = Get-RegularItem (Join-Path $root $relative)
}
foreach ($required in @('project.epoch.json',$project.scene,$project.gui)) {
    if ($required -and !$projectAssets.Contains(([string]$required).Replace('\','/'))) {
        throw "Include the manifest-declared runtime asset '$required' in ProjectAsset."
    }
}
foreach ($name in @('Roboto-Regular.ttf','OFL.txt')) {
    $content["assets/fonts/$name"] = Get-RegularItem (Join-Path $repo "Engine/assets/fonts/$name")
}
if ($coachEnabled) {
    $notice = Get-RegularItem (Join-Path $root 'android/llama.cpp.LICENSE')
    if ((Get-FileHash -LiteralPath $notice.FullName).Hash -ne '94F29BBED6A22C35B992C5C6EBF0E7C92F13B836B90F36F461C9CF2F0F1D010D') {
        throw 'The Calculator Coach llama.cpp notice differs from the pinned source.'
    }
    $content['assets/licenses/llama.cpp.txt'] = $notice
}

$work = Join-Path $repo ('Artifacts/Intermediate/android-apk/' + [guid]::NewGuid().ToString('N'))
$classes = Join-Path $work 'classes'
$dex = Join-Path $work 'dex'
$apkAssets = Join-Path $work 'apk-assets'
foreach ($folder in @($classes,$dex,$apkAssets,(Join-Path $work 'tmp'))) {
    [IO.Directory]::CreateDirectory($folder) | Out-Null
}
$index = foreach ($path in ($content.Keys | Sort-Object)) {
    $staged = Join-Path $apkAssets "epoch/runtime/$path"
    [IO.Directory]::CreateDirectory((Split-Path -Parent $staged)) | Out-Null
    $pin = $fixedShorelineAssets[$path]
    if ($pin) { Copy-FixedShorelineAsset $pin $staged }
    else { Copy-Item -LiteralPath $content[$path].FullName -Destination $staged }
    $item = Get-RegularItem $staged
    $hash = (Get-FileHash -LiteralPath $staged).Hash.ToLowerInvariant()
    if ($pin -and ($item.Length -ne $pin.bytes -or $hash -cne $pin.sha256)) {
        throw "Fixed shoreline asset differs from its reviewed identity: $path"
    }
    [pscustomobject]@{ path = $path; bytes = $item.Length; sha256 = $hash }
}
$indexFile = Join-Path $apkAssets 'epoch/runtime.index'
[IO.File]::WriteAllLines($indexFile,
    [string[]]@($index | ForEach-Object { "$($_.path)`t$($_.bytes)`t$($_.sha256)" }), $utf8)

$hostRoot = Join-Path $repo 'Engine/src/platform/android'
[xml]$androidManifest = Get-Content -LiteralPath (Join-Path $hostRoot 'AndroidManifest.xml') -Raw
$androidManifest.manifest.SetAttribute('package',$ApplicationId)
$androidManifest.manifest.application.SetAttribute('label','http://schemas.android.com/apk/res/android',$appLabel) | Out-Null
if ($coachEnabled) {
    $service = $androidManifest.CreateElement('service')
    $androidNamespace = 'http://schemas.android.com/apk/res/android'
    $service.SetAttribute('name',$androidNamespace,'org.epochengine.runtime.EpochCalculatorCoachService') | Out-Null
    $service.SetAttribute('exported',$androidNamespace,'false') | Out-Null
    $service.SetAttribute('isolatedProcess',$androidNamespace,'true') | Out-Null
    $null = $androidManifest.manifest.application.AppendChild($service)
    $picker = $androidManifest.CreateElement('activity')
    $picker.SetAttribute('name',$androidNamespace,'org.epochengine.runtime.EpochCalculatorCoachModelActivity') | Out-Null
    $picker.SetAttribute('exported',$androidNamespace,'false') | Out-Null
    $picker.SetAttribute('theme',$androidNamespace,'@android:style/Theme.Material.Dialog.NoActionBar') | Out-Null
    $null = $androidManifest.manifest.application.AppendChild($picker)
}
Confirm-ManifestBoundary $androidManifest $coachEnabled
$manifestPath = Join-Path $work 'AndroidManifest.xml'
$androidManifest.Save($manifestPath)
$hostFiles = @('EpochActivity.java','EpochPackagedContent.java','EpochProjectArchive.java') |
    ForEach-Object { (Get-RegularItem (Join-Path $hostRoot $_)).FullName }
if ($coachEnabled) {
    $hostFiles = @($hostFiles) + @('EpochCalculatorCoach.java','EpochCalculatorCoachService.java','EpochCalculatorCoachModelActivity.java' |
        ForEach-Object { (Get-RegularItem (Join-Path $root "android/$_")).FullName })
}

if (!$DebugKeyStore) { $DebugKeyStore = Join-Path $repo 'Artifacts/Dependencies/android-tools/android-user/epoch-debug.keystore' }
$key = [IO.Path]::GetFullPath($DebugKeyStore)
$priorEnvironment = @{}
foreach ($name in @('JAVA_HOME','TEMP','TMP','ANDROID_USER_HOME')) {
    $priorEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$env:JAVA_HOME = $jdk
$env:TEMP = Join-Path $work 'tmp'
$env:TMP = $env:TEMP
$env:ANDROID_USER_HOME = Join-Path $work 'android-user'
try {
    $jar = Join-Path $work 'classes.jar'
    $dexFile = Join-Path $dex 'classes.dex'
    $unsigned = Join-Path $work 'unsigned.apk'
    $aligned = Join-Path $work 'aligned.apk'
    $signed = Join-Path $work 'signed.apk'
    Invoke-PackagingTool (Join-Path $jdk 'bin/javac.exe') (@('-encoding','UTF-8','-source','8','-target','8',
        '-bootclasspath',$androidJar,'-d',$classes) + $hostFiles + $sdlFiles)
    Invoke-PackagingTool (Join-Path $jdk 'bin/jar.exe') @('--create','--file',$jar,'-C',$classes,'.')
    Invoke-PackagingTool (Join-Path $buildTools 'd8.bat') @('--lib',$androidJar,'--min-api','26','--output',$dex,$jar)
    Invoke-PackagingTool (Join-Path $buildTools 'aapt2.exe') @('link','-I',$androidJar,'--manifest',$manifestPath,'-o',$unsigned)

    $archive = [IO.Compression.ZipFile]::Open($unsigned,[IO.Compression.ZipArchiveMode]::Update)
    try {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$dexFile,
            'classes.dex',[IO.Compression.CompressionLevel]::NoCompression) | Out-Null
        # APK asset names use forward slashes on every build host.
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$indexFile,
            'assets/epoch/runtime.index') | Out-Null
        foreach ($entry in $index) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
                (Join-Path $apkAssets ('epoch/runtime/' + $entry.path)),
                ('assets/epoch/runtime/' + $entry.path)) | Out-Null
        }
        foreach ($name in $libraries.Keys) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$libraries[$name].FullName,
                "lib/$Abi/$name",[IO.Compression.CompressionLevel]::NoCompression) | Out-Null
        }
    } finally { $archive.Dispose() }
    $archive = [IO.Compression.ZipFile]::OpenRead($unsigned)
    try {
        $manifestStream = $archive.GetEntry('AndroidManifest.xml').Open()
        try { $manifestDigest = (Get-FileHash -InputStream $manifestStream).Hash }
        finally { $manifestStream.Dispose() }
    } finally { $archive.Dispose() }
    Invoke-PackagingTool (Join-Path $buildTools 'zipalign.exe') @('-P','16','4',$unsigned,$aligned)
    if (!(Test-Path -LiteralPath $key)) {
        [IO.Directory]::CreateDirectory((Split-Path -Parent $key)) | Out-Null
        Invoke-PackagingTool (Join-Path $jdk 'bin/keytool.exe') @('-genkeypair','-keystore',$key,'-storepass','android',
            '-keypass','android','-alias','androiddebugkey','-keyalg','RSA','-keysize','2048','-validity','10000',
            '-dname','CN=Epoch Android Debug,O=Epoch,C=US')
    }
    $key = (Get-RegularItem $key).FullName
    Invoke-PackagingTool (Join-Path $buildTools 'apksigner.bat') @('sign','--ks',$key,'--ks-pass','pass:android',
        '--ks-key-alias','androiddebugkey','--key-pass','pass:android','--out',$signed,$aligned)
    Invoke-PackagingTool (Join-Path $buildTools 'apksigner.bat') @('verify','--verbose','--print-certs',$signed)
    Invoke-PackagingTool (Join-Path $buildTools 'zipalign.exe') @('-c','-P','16','4',$signed)
    $permissions = & (Join-Path $buildTools 'aapt2.exe') dump permissions $signed
    if ($LASTEXITCODE -ne 0 -or ($permissions -match '^uses-permission')) {
        throw 'The packaged permission audit failed.'
    }
    $expected = [ordered]@{
        'AndroidManifest.xml' = $manifestDigest
        'classes.dex' = (Get-FileHash -LiteralPath $dexFile).Hash
        'assets/epoch/runtime.index' = (Get-FileHash -LiteralPath $indexFile).Hash
    }
    foreach ($entry in $index) { $expected['assets/epoch/runtime/' + $entry.path] = $entry.sha256 }
    foreach ($name in $libraries.Keys) { $expected["lib/$Abi/$name"] = (Get-FileHash -LiteralPath $libraries[$name].FullName).Hash }
    $fixedSizes = @{}
    foreach ($path in $fixedShorelineAssets.Keys) { $fixedSizes["assets/epoch/runtime/$path"] = $fixedShorelineAssets[$path].bytes }
    Confirm-PackageContents $signed $expected $fixedSizes
    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
    [IO.File]::Move($signed,$destination)
} finally {
    foreach ($name in $priorEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$priorEnvironment[$name],'Process')
    }
}

$report = [ordered]@{
    apk = $destination
    apk_sha256 = (Get-FileHash -LiteralPath $destination).Hash
    application_id = $ApplicationId
    activity = 'org.epochengine.runtime.EpochActivity'
    android_manifest_sha256 = $manifestDigest
    abi = $Abi
    calculator_coach = $coachEnabled
    native_libraries = @($libraries.Keys | ForEach-Object {
        [ordered]@{ path = "lib/$Abi/$_"; sha256 = (Get-FileHash -LiteralPath $libraries[$_].FullName).Hash }
    })
    assets = @($index)
    work_directory = $work
}
if ($FixedShorelineSnapshot) {
    $shorelineDescriptorPath = 'assets/planetary/fixed-shoreline/shoreline.json'
    $report.fixed_shoreline_snapshot = [ordered]@{
        descriptor = $shorelineDescriptorPath
        descriptor_sha256 = $fixedShorelineAssets[$shorelineDescriptorPath].sha256
        verified_assets = $fixedShorelineAssets.Count
    }
}
$report | ConvertTo-Json -Depth 6
