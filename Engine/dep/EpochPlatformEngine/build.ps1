param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [ValidateSet("msvc", "gcc", "clang")]
    [string]$Compiler = $(if ($IsWindows) { "msvc" } else { "gcc" })
)

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($Compiler -eq "msvc") {
    & "$root/scripts/build_msvc.bat" $Configuration
    exit $LASTEXITCODE
}

& "$root/scripts/build_posix.sh" $Compiler.ToLowerInvariant() $Configuration
exit $LASTEXITCODE
