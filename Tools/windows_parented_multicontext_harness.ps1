param(
    [ValidateSet('Full','Single')]
    [string]$Mode = 'Full',

    [ValidateSet('raylib','sdl','sfml','vulkan','opengl','software')]
    [string]$Backend = 'raylib',

    [ValidateSet('','raylib','sdl','sfml','vulkan','opengl','software')]
    [string]$FocusedBackend = '',

    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug',

    [string]$OutputPath = '',

    [switch]$SkipMaximize,
    [switch]$SkipCloseProbe
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$user32 = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class EpochWin32Harness
{
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }

    [StructLayout(LayoutKind.Sequential)]
    public struct GUITHREADINFO
    {
        public int cbSize;
        public uint flags;
        public IntPtr hwndActive;
        public IntPtr hwndFocus;
        public IntPtr hwndCapture;
        public IntPtr hwndMenuOwner;
        public IntPtr hwndMoveSize;
        public IntPtr hwndCaret;
        public RECT rcCaret;
    }

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr hwnd, EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int cmdShow);
}
'@

Add-Type -TypeDefinition $user32

$WM_LBUTTONDOWN = 0x0201
$WM_MOUSEMOVE = 0x0200
$WM_LBUTTONUP = 0x0202
$WM_CLOSE = 0x0010
$MK_LBUTTON = 0x0001
$SW_MAXIMIZE = 3

function Get-LParam([int]$x, [int]$y) {
    $ux = $x -band 0xFFFF
    $uy = $y -band 0xFFFF
    return [IntPtr](($uy -shl 16) -bor $ux)
}

function Get-WindowTextValue([IntPtr]$Hwnd) {
    $sb = [System.Text.StringBuilder]::new(260)
    [void][EpochWin32Harness]::GetWindowTextW($Hwnd, $sb, $sb.Capacity)
    $sb.ToString()
}

function Get-ClassNameValue([IntPtr]$Hwnd) {
    $sb = [System.Text.StringBuilder]::new(260)
    [void][EpochWin32Harness]::GetClassNameW($Hwnd, $sb, $sb.Capacity)
    $sb.ToString()
}

function Get-ProcessTopWindows([int]$ProcessId) {
    $items = [System.Collections.Generic.List[object]]::new()
    $callback = [EpochWin32Harness+EnumWindowsProc]{
        param([IntPtr]$hwnd, [IntPtr]$lParam)
        $windowPid = 0
        [void][EpochWin32Harness]::GetWindowThreadProcessId($hwnd, [ref]$windowPid)
        if ($windowPid -eq $ProcessId) {
            $items.Add([pscustomobject]@{
                Hwnd = $hwnd
                Title = Get-WindowTextValue $hwnd
                Class = Get-ClassNameValue $hwnd
                Visible = [EpochWin32Harness]::IsWindowVisible($hwnd)
                Parent = [EpochWin32Harness]::GetParent($hwnd)
            })
        }
        return $true
    }
    [void][EpochWin32Harness]::EnumWindows($callback, [IntPtr]::Zero)
    $items
}

function Get-DescendantWindows([IntPtr]$Parent) {
    $items = [System.Collections.Generic.List[object]]::new()
    $callback = [EpochWin32Harness+EnumWindowsProc]{
        param([IntPtr]$hwnd, [IntPtr]$lParam)
        $rectInfo = Get-WindowRectObject $hwnd
        $items.Add([pscustomobject]@{
            Hwnd = $hwnd
            Title = Get-WindowTextValue $hwnd
            Class = Get-ClassNameValue $hwnd
            Visible = [EpochWin32Harness]::IsWindowVisible($hwnd)
            Parent = [EpochWin32Harness]::GetParent($hwnd)
            Rect = $rectInfo
        })
        return $true
    }
    [void][EpochWin32Harness]::EnumChildWindows($Parent, $callback, [IntPtr]::Zero)
    $items
}

function Wait-ForParentWindow([int]$ProcessId, [int]$TimeoutSeconds = 20) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $top = Get-ProcessTopWindows -ProcessId $ProcessId |
            Where-Object { $_.Visible -and $_.Parent -eq [IntPtr]::Zero } |
            Sort-Object { $_.Title.Length } -Descending
        if ($top) { return $top[0] }
        Start-Sleep -Milliseconds 200
    }
    return $null
}

function Wait-ForBackendWindows([IntPtr]$Parent, [string[]]$Needles, [int]$TimeoutSeconds = 20) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $children = Get-DescendantWindows -Parent $Parent
        $found = @{}
        foreach ($needle in $Needles) {
            $match = $children | Where-Object {
                $_.Visible -and ($_.Title -like "*$needle*" -or $_.Class -like "*$needle*")
            } | Select-Object -First 1
            if ($match) { $found[$needle] = $match }
        }
        if ($found.Count -eq $Needles.Count) {
            return [pscustomobject]@{
                Children = $children
                Found = $found
            }
        }
        Start-Sleep -Milliseconds 300
    }
    return $null
}

function Get-WindowRectObject([IntPtr]$Hwnd) {
    $rect = New-Object EpochWin32Harness+RECT
    if (-not [EpochWin32Harness]::GetWindowRect($Hwnd, [ref]$rect)) { return $null }
    [pscustomobject]@{
        Left = $rect.Left
        Top = $rect.Top
        Right = $rect.Right
        Bottom = $rect.Bottom
        Width = $rect.Right - $rect.Left
        Height = $rect.Bottom - $rect.Top
    }
}

function Convert-ScreenToClientPoint([IntPtr]$Hwnd, [int]$X, [int]$Y) {
    $pt = New-Object EpochWin32Harness+POINT
    $pt.X = $X
    $pt.Y = $Y
    [void][EpochWin32Harness]::ScreenToClient($Hwnd, [ref]$pt)
    [pscustomobject]@{ X = $pt.X; Y = $pt.Y }
}

function Invoke-LeftClick([IntPtr]$Hwnd, [int]$ClientX, [int]$ClientY) {
    [void][EpochWin32Harness]::PostMessageW($Hwnd, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, (Get-LParam $ClientX $ClientY))
    Start-Sleep -Milliseconds 50
    [void][EpochWin32Harness]::PostMessageW($Hwnd, $WM_LBUTTONUP, [IntPtr]::Zero, (Get-LParam $ClientX $ClientY))
}

function Invoke-UndockRedock([IntPtr]$DockHandle, [IntPtr]$ParentHwnd) {
    $parentRect = Get-WindowRectObject $ParentHwnd
    $beforeParent = [EpochWin32Harness]::GetParent($DockHandle)
    $startX = 20
    $startY = 10
    [void][EpochWin32Harness]::PostMessageW($DockHandle, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, (Get-LParam $startX $startY))
    Start-Sleep -Milliseconds 100

    $outsideScreenX = $parentRect.Right + 120
    $outsideScreenY = $parentRect.Top + 40
    $moveOutside = Convert-ScreenToClientPoint $DockHandle $outsideScreenX $outsideScreenY
    [void][EpochWin32Harness]::PostMessageW($DockHandle, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveOutside.X $moveOutside.Y))
    Start-Sleep -Milliseconds 250

    $midParent = [EpochWin32Harness]::GetParent($DockHandle)
    $midTopLevel = ($midParent -ne $ParentHwnd)

    $insideScreenX = $parentRect.Left + 60
    $insideScreenY = $parentRect.Top + 60
    $moveInside = Convert-ScreenToClientPoint $DockHandle $insideScreenX $insideScreenY
    [void][EpochWin32Harness]::PostMessageW($DockHandle, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveInside.X $moveInside.Y))
    Start-Sleep -Milliseconds 250

    $lateParent = [EpochWin32Harness]::GetParent($DockHandle)
    $lateRedocked = ($lateParent -eq $ParentHwnd)

    [void][EpochWin32Harness]::PostMessageW($DockHandle, $WM_LBUTTONUP, [IntPtr]::Zero, (Get-LParam $moveInside.X $moveInside.Y))
    Start-Sleep -Milliseconds 250
    $endParent = [EpochWin32Harness]::GetParent($DockHandle)

    [pscustomobject]@{
        BeforeDocked = ($beforeParent -eq $ParentHwnd)
        MidTopLevel = $midTopLevel
        LateRedocked = $lateRedocked
        EndRedocked = ($endParent -eq $ParentHwnd)
    }
}

function Get-ProxyHostProbe([IntPtr]$HostHandle, [IntPtr]$ParentHwnd) {
    if ($HostHandle -eq [IntPtr]::Zero -or -not [EpochWin32Harness]::IsWindow($HostHandle)) {
        return [pscustomobject]@{
            Present = $false
            Parent = [IntPtr]::Zero
            Visible = $false
            RehiddenInParent = $true
        }
    }

    $hostParent = [EpochWin32Harness]::GetParent($HostHandle)
    $hostVisible = [EpochWin32Harness]::IsWindowVisible($HostHandle)
    [pscustomobject]@{
        Present = $true
        Parent = $hostParent
        Visible = $hostVisible
        RehiddenInParent = ($hostParent -eq $ParentHwnd -and -not $hostVisible)
    }
}

function Get-FocusProbe([IntPtr]$TargetHwnd) {
    $windowPid = 0
    $threadId = [EpochWin32Harness]::GetWindowThreadProcessId($TargetHwnd, [ref]$windowPid)
    $info = New-Object EpochWin32Harness+GUITHREADINFO
    $info.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type][EpochWin32Harness+GUITHREADINFO])
    if (-not [EpochWin32Harness]::GetGUIThreadInfo($threadId, [ref]$info)) {
        return [pscustomobject]@{ FocusHwnd = [IntPtr]::Zero; FocusClass = ''; FocusTitle = '' }
    }
    $focusClass = ''
    $focusTitle = ''
    if ($info.hwndFocus -ne [IntPtr]::Zero) {
        $focusClass = Get-ClassNameValue $info.hwndFocus
        $focusTitle = Get-WindowTextValue $info.hwndFocus
    }
    [pscustomobject]@{
        FocusHwnd = $info.hwndFocus
        FocusClass = $focusClass
        FocusTitle = $focusTitle
    }
}

function Save-Result([object]$Result, [string]$Destination) {
    $dir = Split-Path -Parent $Destination
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }
    $Result | ConvertTo-Json -Depth 8 | Set-Content -Path $Destination -Encoding UTF8
}

function Save-WindowScreenshot([IntPtr]$Hwnd, [string]$Destination) {
    $rect = Get-WindowRectObject $Hwnd
    if (-not $rect -or $rect.Width -le 0 -or $rect.Height -le 0) {
        return $false
    }

    $dir = Split-Path -Parent $Destination
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }

    $bitmap = New-Object System.Drawing.Bitmap $rect.Width, $rect.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
        return $true
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$root = Resolve-Path '.'
$exe = Join-Path $root ("x64\\{0}\\ConsoleApplication1.exe" -f $Configuration)
if (-not (Test-Path $exe)) {
    throw "Missing runtime at $exe"
}

$args = @('--editor')
if ($Mode -eq 'Single') {
    $args += '--parented'
    $args += '--backend'
    $args += $Backend
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.WorkingDirectory = Split-Path -Parent $exe
$psi.Arguments = [string]::Join(' ', $args)
$psi.UseShellExecute = $true
$proc = [System.Diagnostics.Process]::Start($psi)
if (-not $proc) {
    throw 'Failed to launch editor runtime.'
}

try {
    Start-Sleep -Seconds 4
    $parentWindow = Wait-ForParentWindow -ProcessId $proc.Id
    if (-not $parentWindow) {
        throw 'Timed out waiting for the parent editor window.'
    }

    if (-not $SkipMaximize) {
        [void][EpochWin32Harness]::ShowWindow($parentWindow.Hwnd, $SW_MAXIMIZE)
        Start-Sleep -Milliseconds 400
    }

    $needles = if ($Mode -eq 'Full') {
        @('GLFW','SDL','SFML','Vulkan','OpenGL','Software')
    } else {
        switch ($Backend) {
            'raylib' { @('GLFW') }
            'sdl' { @('SDL') }
            'sfml' { @('SFML') }
            'vulkan' { @('Vulkan') }
            'opengl' { @('OpenGL') }
            'software' { @('Software') }
        }
    }
    $ready = Wait-ForBackendWindows -Parent $parentWindow.Hwnd -Needles $needles
    if (-not $ready) {
        $childrenDump = Get-DescendantWindows -Parent $parentWindow.Hwnd |
            Select-Object Class,Title,Visible
        $childrenDump | ConvertTo-Json -Depth 4 | Write-Output
        throw 'Timed out waiting for expected backend panes.'
    }

    $children = $ready.Children
    $rayHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*Raylib*' } | Select-Object -First 1
    $sdlHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*SDL*' } | Select-Object -First 1
    $sfmlHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*SFML*' } | Select-Object -First 1
    $rayChild = $children | Where-Object { $_.Class -eq 'GLFW30' } | Select-Object -First 1
    $sdlChild = $children | Where-Object { $_.Class -eq 'SDL_app' } | Select-Object -First 1
    $sfmlChild = $children | Where-Object { $_.Class -eq 'SFML_Window' } | Select-Object -First 1

    $backendChecks = @()
    foreach ($item in @(
        @{ Name = 'raylib'; DockHandle = $(if ($rayChild) { $rayChild.Hwnd } elseif ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($rayChild) { $rayChild.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }) },
        @{ Name = 'sdl'; DockHandle = $(if ($sdlChild) { $sdlChild.Hwnd } elseif ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sdlChild) { $sdlChild.Hwnd } elseif ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }) },
        @{ Name = 'sfml'; DockHandle = $(if ($sfmlChild) { $sfmlChild.Hwnd } elseif ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sfmlChild) { $sfmlChild.Hwnd } elseif ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }) }
    )) {
        if ($Mode -eq 'Single' -and $item.Name -ne $Backend) { continue }
        if ($Mode -eq 'Full' -and -not [string]::IsNullOrWhiteSpace($FocusedBackend) -and $item.Name -ne $FocusedBackend) { continue }
        if ($item.DockHandle -eq [IntPtr]::Zero) { continue }

        $drag = Invoke-UndockRedock -DockHandle $item.DockHandle -ParentHwnd $parentWindow.Hwnd
        Invoke-LeftClick -Hwnd $item.FocusHandle -ClientX 24 -ClientY 24
        Start-Sleep -Milliseconds 150
        $focus = Get-FocusProbe -TargetHwnd $item.FocusHandle
        $proxyHost = Get-ProxyHostProbe -HostHandle $item.HostHandle -ParentHwnd $parentWindow.Hwnd
        $backendChecks += [pscustomobject]@{
            Backend = $item.Name
            Drag = $drag
            Focus = $focus
            ProxyHost = $proxyHost
        }

        Start-Sleep -Milliseconds 500
        if ($Mode -eq 'Full') {
            [void](Wait-ForBackendWindows -Parent $parentWindow.Hwnd -Needles $needles -TimeoutSeconds 5)
        }
    }

    Start-Sleep -Seconds 2
    $childrenAfterDrag = Get-DescendantWindows -Parent $parentWindow.Hwnd
    $screenshotPath = ''

    $stillRunningAfterClose = $true
    if (-not $SkipCloseProbe -and $Mode -eq 'Full' -and $sfmlChild) {
        [void][EpochWin32Harness]::PostMessageW($sfmlChild.Hwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 400
        $proc.Refresh()
        $stillRunningAfterClose = -not $proc.HasExited
    }

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $suffix = if ($Mode -eq 'Single') { "_$Backend" } else { '' }
        $OutputPath = Join-Path $root ("x64\\{0}\\logs\\multicontext_harness_{1}{2}_{3}.json" -f $Configuration.ToLowerInvariant(), $Mode.ToLowerInvariant(), $suffix, $stamp)
    }
    $screenshotPath = [System.IO.Path]::ChangeExtension($OutputPath, '.png')
    [void](Save-WindowScreenshot -Hwnd $parentWindow.Hwnd -Destination $screenshotPath)

    $result = [pscustomobject]@{
        Mode = $Mode
        Backend = $Backend
        FocusedBackend = $FocusedBackend
        ProcessId = $proc.Id
        Parent = $parentWindow
        VisibleChildrenBefore = $children | Select-Object Class,Title,Visible,Rect
        VisibleChildrenAfterDrag = $childrenAfterDrag | Select-Object Class,Title,Visible,Rect
        Checks = $backendChecks
        StillRunningAfterClose = $stillRunningAfterClose
        ScreenshotPath = $screenshotPath
    }
    Save-Result -Result $result -Destination $OutputPath
    Write-Output $OutputPath
}
finally {
    if ($proc -and -not $proc.HasExited) {
        $proc.CloseMainWindow() | Out-Null
        Start-Sleep -Milliseconds 400
        if (-not $proc.HasExited) {
            $proc.Kill()
            $proc.WaitForExit()
        }
    }
}
