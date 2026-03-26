param(
    [string[]]$Only = @()
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$user32Source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeCapture
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT
    {
        public int X;
        public int Y;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
}
"@

Add-Type -TypeDefinition $user32Source

function Get-WindowTitle {
    param([IntPtr]$Handle)

    $length = [NativeCapture]::GetWindowTextLength($Handle)
    if ($length -le 0) {
        return ""
    }

    $builder = New-Object System.Text.StringBuilder ($length + 1)
    [void][NativeCapture]::GetWindowText($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Get-VisibleWindows {
    param(
        [uint32]$ProcessId = 0,
        [string]$TitlePattern = ""
    )

    $windows = New-Object System.Collections.Generic.List[object]
    $regex = if ([string]::IsNullOrWhiteSpace($TitlePattern)) { $null } else { [regex]::new($TitlePattern) }

    $callback = [NativeCapture+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)

        if (-not [NativeCapture]::IsWindowVisible($hWnd)) {
            return $true
        }

        [uint32]$windowPid = 0
        [void][NativeCapture]::GetWindowThreadProcessId($hWnd, [ref]$windowPid)
        if ($ProcessId -ne 0 -and $windowPid -ne $ProcessId) {
            return $true
        }

        $title = Get-WindowTitle -Handle $hWnd
        if ($regex -and -not $regex.IsMatch($title)) {
            return $true
        }

        $windows.Add([pscustomobject]@{
            Handle = $hWnd
            ProcessId = $windowPid
            Title = $title
        })

        return $true
    }

    [void][NativeCapture]::EnumWindows($callback, [IntPtr]::Zero)
    return $windows
}

function Get-ClientRectOnScreen {
    param([IntPtr]$Handle)

    $clientRect = New-Object NativeCapture+RECT
    if ([NativeCapture]::GetClientRect($Handle, [ref]$clientRect)) {
        $topLeft = New-Object NativeCapture+POINT
        $topLeft.X = $clientRect.Left
        $topLeft.Y = $clientRect.Top

        $bottomRight = New-Object NativeCapture+POINT
        $bottomRight.X = $clientRect.Right
        $bottomRight.Y = $clientRect.Bottom

        if ([NativeCapture]::ClientToScreen($Handle, [ref]$topLeft) -and
            [NativeCapture]::ClientToScreen($Handle, [ref]$bottomRight)) {
            return [System.Drawing.Rectangle]::FromLTRB(
                $topLeft.X,
                $topLeft.Y,
                $bottomRight.X,
                $bottomRight.Y
            )
        }
    }

    $windowRect = New-Object NativeCapture+RECT
    if ([NativeCapture]::GetWindowRect($Handle, [ref]$windowRect)) {
        return [System.Drawing.Rectangle]::FromLTRB(
            $windowRect.Left,
            $windowRect.Top,
            $windowRect.Right,
            $windowRect.Bottom
        )
    }

    return $null
}

function Save-WindowCapture {
    param(
        [Parameter(Mandatory = $true)][object[]]$Windows,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $rects = @()
    foreach ($window in $Windows) {
        $rect = Get-ClientRectOnScreen -Handle $window.Handle
        if ($null -ne $rect -and $rect.Width -gt 0 -and $rect.Height -gt 0) {
            $rects += $rect
        }
    }

    if ($rects.Count -eq 0) {
        throw "No visible window rectangles were available for capture."
    }

    $left = ($rects | Measure-Object -Property Left -Minimum).Minimum
    $top = ($rects | Measure-Object -Property Top -Minimum).Minimum
    $right = ($rects | Measure-Object -Property Right -Maximum).Maximum
    $bottom = ($rects | Measure-Object -Property Bottom -Maximum).Maximum

    $bounds = [System.Drawing.Rectangle]::FromLTRB($left, $top, $right, $bottom)
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
        $directory = Split-Path -Parent $Path
        if (-not (Test-Path $directory)) {
            New-Item -ItemType Directory -Path $directory | Out-Null
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Wait-ForProcessWindows {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 25
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.MainWindowHandle -ne 0) {
            return @([pscustomobject]@{
                Handle = [IntPtr]$Process.MainWindowHandle
                ProcessId = [uint32]$Process.Id
                Title = ""
            })
        }
        $windows = Get-VisibleWindows -ProcessId ([uint32]$Process.Id)
        if ($windows.Count -gt 0) {
            return $windows
        }
        Start-Sleep -Milliseconds 500
    }

    return @()
}

function Wait-ForNewWindows {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Baseline,
        [string]$TitlePattern = "",
        [int]$TimeoutSeconds = 25
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $windows = Get-VisibleWindows -TitlePattern $TitlePattern | Where-Object {
            -not $Baseline.ContainsKey(($_.Handle).ToInt64())
        }
        if ($windows.Count -gt 0) {
            return $windows
        }
        Start-Sleep -Milliseconds 500
    }

    return @()
}

function Stop-ProcessSafe {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return
    }

    try {
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    catch {
    }
}

function Capture-WindowsScenario {
    param(
        [string]$Name,
        [string]$Executable,
        [string]$Arguments,
        [string]$WorkingDirectory,
        [string]$OutputPath,
        [int]$WarmupSeconds = 5
    )

    $process = $null
    try {
        if ([string]::IsNullOrWhiteSpace($Arguments)) {
            $process = Start-Process -FilePath $Executable -WorkingDirectory $WorkingDirectory -PassThru
        }
        else {
            $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $WorkingDirectory -PassThru
        }
        $windows = Wait-ForProcessWindows -Process $process
        if ($windows.Count -eq 0) {
            throw "No visible windows appeared for scenario '$Name'."
        }
        Start-Sleep -Seconds $WarmupSeconds
        $windows = Get-VisibleWindows -ProcessId ([uint32]$process.Id)
        Save-WindowCapture -Windows $windows -Path $OutputPath
    }
    finally {
        Stop-ProcessSafe -Process $process
    }
}

function Capture-WslScenario {
    param(
        [string]$Name,
        [string]$LinuxCommand,
        [string]$OutputPath,
        [int]$WarmupSeconds = 5
    )

    $baseline = @{}
    foreach ($window in Get-VisibleWindows) {
        $baseline[($window.Handle).ToInt64()] = $true
    }

    $process = $null
    try {
        $process = Start-Process -FilePath "wsl.exe" -ArgumentList @("bash", "-lc", $LinuxCommand) -PassThru
        $windows = Wait-ForNewWindows -Baseline $baseline
        if ($windows.Count -eq 0) {
            throw "No visible WSL windows appeared for scenario '$Name'."
        }
        Start-Sleep -Seconds $WarmupSeconds
        $refreshedWindows = Wait-ForNewWindows -Baseline $baseline
        if ($null -ne $refreshedWindows -and $refreshedWindows.Count -gt 0) {
            $windows = $refreshedWindows
        }
        Save-WindowCapture -Windows $windows -Path $OutputPath
    }
    finally {
        Stop-ProcessSafe -Process $process
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$imagesRoot = Join-Path $repoRoot "Images\\readme"
$windowsRuntimeDir = Join-Path $repoRoot "x64\\Release"
$windowsUpdaterShell = Join-Path $repoRoot "Engine\\Bin\\WinUpdaterShellRelease\\Release\\epoch.exe"
$linuxUpdaterShellDir = "/mnt/c/Users/iammi/.codex/worktrees/2a8f/epoch_vibed/Engine/Bin/Clang-Release-UpdaterShell"

$scenarios = @(
    @{ Name = "windows-opengl";      Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend opengl";  Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-opengl.png") },
    @{ Name = "windows-vulkan";      Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend vulkan";  Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-vulkan.png") },
    @{ Name = "windows-sdl";         Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend sdl";     Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-sdl.png") },
    @{ Name = "windows-sfml";        Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend sfml";    Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-sfml.png") },
    @{ Name = "windows-raylib";      Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend raylib";  Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-raylib.png") },
    @{ Name = "windows-software";    Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "--backend software"; Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-software.png") },
    @{ Name = "windows-multicontext";Type = "windows"; Exe = (Join-Path $windowsRuntimeDir "ConsoleApplication1.exe"); Args = "";                  Cwd = $windowsRuntimeDir; Out = (Join-Path $imagesRoot "windows-multicontext.png") },
    @{ Name = "linux-wsl-updater";   Type = "wsl";     Cmd = "cd '$linuxUpdaterShellDir' && ./epoch";                                            Out = (Join-Path $imagesRoot "linux-wsl-updater-shell.png") }
)

if ($Only.Count -gt 0) {
    $allowed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $Only) {
        [void]$allowed.Add($entry)
    }
    $scenarios = $scenarios | Where-Object { $allowed.Contains($_.Name) }
}

foreach ($scenario in $scenarios) {
    Write-Host "Capturing $($scenario.Name)..."
    if ($scenario.Type -eq "windows") {
        Capture-WindowsScenario -Name $scenario.Name -Executable $scenario.Exe -Arguments $scenario.Args -WorkingDirectory $scenario.Cwd -OutputPath $scenario.Out
    }
    else {
        Capture-WslScenario -Name $scenario.Name -LinuxCommand $scenario.Cmd -OutputPath $scenario.Out
    }
}

Write-Host "Screenshots written to $imagesRoot"
