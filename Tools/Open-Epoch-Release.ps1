# Requires Windows, Visual Studio 2022 and its COM automation registration.
# This launches one Visual Studio instance with Release x64 actively selected.
# Existing user .suo settings can override the .sln order on ordinary launches.
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Run on Windows with Visual Studio installed.' }
$solution = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Engine.sln'))
if (-not [IO.File]::Exists($solution)) { throw "Solution not found: $solution" }
$dte = New-Object -ComObject 'VisualStudio.DTE.17.0'
$dte.UserControl = $true
$dte.MainWindow.Visible = $true
$dte.Solution.Open($solution)
$found = $false
foreach ($configuration in $dte.Solution.SolutionBuild.SolutionConfigurations) {
    if ($configuration.Name -eq 'Release' -and $configuration.PlatformName -eq 'x64') {
        $configuration.Activate()
        $found = $true
        break
    }
}
if (-not $found) { throw 'Release|x64 solution configuration was not found.' }
Write-Host 'EpochEngine solution opened with Release|x64 selected.'
Write-Host 'Visual Studio remembers this per user/solution. Close VS normally to save the selection.'
