#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$packager = Join-Path $root 'Tools/build/package_epoch_android_apk.ps1'
$tokens = $diagnostics = $null
$syntax = [System.Management.Automation.Language.Parser]::ParseFile($packager, [ref]$tokens, [ref]$diagnostics)
if ($diagnostics.Count) { throw ($diagnostics -join [Environment]::NewLine) }
$validator = $syntax.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Confirm-ManifestBoundary'
}, $false)
if (!$validator) { throw 'The Android manifest validator is missing.' }
. ([scriptblock]::Create($validator.Extent.Text))

$namespace = 'http://schemas.android.com/apk/res/android'
$template = Get-Content -LiteralPath "$root/Engine/src/platform/android/AndroidManifest.xml" -Raw
Confirm-ManifestBoundary ([xml]$template) $false

$coachTemplate = @'
    <service android:name="org.epochengine.runtime.EpochCalculatorCoachService"
        android:exported="false" android:isolatedProcess="true" />
    <activity android:name="org.epochengine.runtime.EpochCalculatorCoachModelActivity"
        android:exported="false" />
</application>
'@
$coach = $template.Replace('</application>', $coachTemplate)
Confirm-ManifestBoundary ([xml]$coach) $true

$rejections = @(
    @{ label='shared UID'; coach=$false; xml=$template.Replace('android:versionCode=', 'android:sharedUserId="org.epochengine.test" android:versionCode=') },
    @{ label='renamed process'; coach=$false; xml=$template.Replace('android:allowBackup=', 'android:process=":other" android:allowBackup=') },
    @{ label='extra launcher URI'; coach=$false; xml=$template.Replace('</intent-filter>', '<data android:scheme="https" /></intent-filter>') },
    @{ label='undeclared Coach'; coach=$false; xml=$coach },
    @{ label='missing Coach'; coach=$true; xml=$template },
    @{ label='nonisolated Coach'; coach=$true; xml=$coach.Replace('isolatedProcess="true"', 'isolatedProcess="false"') },
    @{ label='exported Coach'; coach=$true; xml=$coach.Replace('exported="false" android:isolatedProcess', 'exported="true" android:isolatedProcess') }
)
foreach ($rejection in $rejections) {
    $rejected = $false
    try { Confirm-ManifestBoundary ([xml]$rejection.xml) $rejection.coach }
    catch {
        if ($_.Exception.Message -cnotmatch '^(The Android manifest|EpochActivity must expose|Unexpected Android application component|An expected Android application component)') {
            throw
        }
        $rejected = $true
    }
    if (!$rejected) { throw "The manifest accepted $($rejection.label)." }
}
"Android manifest checks passed (2 accepted, $($rejections.Count) rejected)."
