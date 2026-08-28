param(
    [ValidateSet('Full','Single')]
    [string]$Mode = 'Full',

    [ValidateSet('raylib','sdl','sfml','vulkan','opengl','directx','software')]
    [string]$Backend = 'raylib',

    [ValidateSet('','raylib','sdl','sfml','vulkan','opengl','directx','software')]
    [string]$FocusedBackend = '',

    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug',

    [string]$OutputPath = '',

    [switch]$SkipMaximize,
    [switch]$SkipCloseProbe,
    [switch]$CaptureStartupProof,
    [switch]$StartupOnly,
    [switch]$AssetsInteractionProof,

    [string]$ProjectProfile = 'twodstudio'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

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
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr dpiContext);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int cmdShow);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SetActiveWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, uint dwAttribute, out RECT pvAttribute, int cbAttribute);
}
'@

Add-Type -TypeDefinition $user32

$DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = [IntPtr](-4)
$DWMWA_EXTENDED_FRAME_BOUNDS = 9
$WM_LBUTTONDOWN = 0x0201
$WM_MOUSEMOVE = 0x0200
$WM_LBUTTONUP = 0x0202
$WM_CLOSE = 0x0010
$MK_LBUTTON = 0x0001
$SW_MAXIMIZE = 3
$MOUSEEVENTF_LEFTDOWN = 0x0002
$MOUSEEVENTF_LEFTUP = 0x0004

function Initialize-DpiCaptureAwareness() {
    try {
        [void][EpochWin32Harness]::SetProcessDpiAwarenessContext($DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    }
    catch {}

    try {
        [void][EpochWin32Harness]::SetProcessDPIAware()
    }
    catch {}
}

Initialize-DpiCaptureAwareness

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
            $rectInfo = Get-WindowRectObject $hwnd
            $items.Add([pscustomobject]@{
                Hwnd = $hwnd
                Title = Get-WindowTextValue $hwnd
                Class = Get-ClassNameValue $hwnd
                Visible = [EpochWin32Harness]::IsWindowVisible($hwnd)
                Parent = [EpochWin32Harness]::GetParent($hwnd)
                Rect = $rectInfo
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

function Wait-ForParentWindow([int]$ProcessId, [int]$TimeoutSeconds = 50) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $top = Get-ProcessTopWindows -ProcessId $ProcessId |
            Where-Object { $_.Visible -and $_.Parent -eq [IntPtr]::Zero } |
            Sort-Object { $_.Title.Length } -Descending
        if ($top) { return $top[0] }
        Start-Sleep -Milliseconds 250
    }
    return $null
}

function Wait-ForBackendWindows([IntPtr]$Parent, [string[]]$Needles, [int]$TimeoutSeconds = 50) {
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
        Start-Sleep -Milliseconds 400
    }
    return $null
}

function Get-WindowRectObject([IntPtr]$Hwnd) {
    $rect = New-Object EpochWin32Harness+RECT
    $rectSize = [System.Runtime.InteropServices.Marshal]::SizeOf([type][EpochWin32Harness+RECT])
    $gotRect = $false

    try {
        $hr = [EpochWin32Harness]::DwmGetWindowAttribute(
            $Hwnd,
            [uint32]$DWMWA_EXTENDED_FRAME_BOUNDS,
            [ref]$rect,
            $rectSize)
        if ($hr -eq 0) {
            $gotRect = $true
        }
    }
    catch {}

    if (-not $gotRect -and -not [EpochWin32Harness]::GetWindowRect($Hwnd, [ref]$rect)) { return $null }
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

function Set-ScreenCursorPoint([int]$X, [int]$Y) {
    [void][EpochWin32Harness]::SetCursorPos($X, $Y)
}

function Set-WindowForeground([IntPtr]$Hwnd) {
    if ($Hwnd -eq [IntPtr]::Zero -or -not [EpochWin32Harness]::IsWindow($Hwnd)) {
        return
    }

    [void][EpochWin32Harness]::SetActiveWindow($Hwnd)
    [void][EpochWin32Harness]::SetForegroundWindow($Hwnd)
}

function Invoke-PhysicalLeftDown() {
    [EpochWin32Harness]::mouse_event($MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
}

function Invoke-PhysicalLeftUp() {
    [EpochWin32Harness]::mouse_event($MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
}

function Move-CursorLinear(
    [int]$FromX,
    [int]$FromY,
    [int]$ToX,
    [int]$ToY,
    [int]$Steps = 8,
    [int]$StepDelayMs = 45
) {
    $count = [Math]::Max(1, $Steps)
    for ($step = 1; $step -le $count; $step++) {
        $t = $step / [double]$count
        $x = [int][Math]::Round($FromX + (($ToX - $FromX) * $t))
        $y = [int][Math]::Round($FromY + (($ToY - $FromY) * $t))
        Set-ScreenCursorPoint $x $y
        Start-Sleep -Milliseconds $StepDelayMs
    }
}

function Get-VirtualScreenRect() {
    $virtual = [System.Windows.Forms.SystemInformation]::VirtualScreen
    return [pscustomobject]@{
        Left = $virtual.Left
        Top = $virtual.Top
        Right = $virtual.Right
        Bottom = $virtual.Bottom
        Width = $virtual.Width
        Height = $virtual.Height
    }
}

function Invoke-LeftClick([IntPtr]$Hwnd, [int]$ClientX, [int]$ClientY) {
    [void][EpochWin32Harness]::PostMessageW($Hwnd, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, (Get-LParam $ClientX $ClientY))
    Start-Sleep -Milliseconds 50
    [void][EpochWin32Harness]::PostMessageW($Hwnd, $WM_LBUTTONUP, [IntPtr]::Zero, (Get-LParam $ClientX $ClientY))
}

function Get-ParentHandleValue([IntPtr]$Hwnd) {
    if ($Hwnd -eq [IntPtr]::Zero -or -not [EpochWin32Harness]::IsWindow($Hwnd)) {
        return [IntPtr]::Zero
    }

    return [EpochWin32Harness]::GetParent($Hwnd)
}

function Get-DragContractSnapshot(
    [IntPtr]$DockHandle,
    [IntPtr]$ParentHwnd,
    [IntPtr]$ProxyHostHandle = [IntPtr]::Zero,
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero,
    [int]$ProcessId = 0
) {
    $dockParent = Get-ParentHandleValue $DockHandle
    $hostParent = Get-ParentHandleValue $ProxyHostHandle
    $childParent = Get-ParentHandleValue $ProxyChildHandle
    $parentRect = Get-WindowRectObject $ParentHwnd
    $dockRect = if ($DockHandle -ne [IntPtr]::Zero -and [EpochWin32Harness]::IsWindow($DockHandle)) { Get-WindowRectObject $DockHandle } else { $null }
    $hostRect = if ($ProxyHostHandle -ne [IntPtr]::Zero -and [EpochWin32Harness]::IsWindow($ProxyHostHandle)) { Get-WindowRectObject $ProxyHostHandle } else { $null }
    $childRect = if ($ProxyChildHandle -ne [IntPtr]::Zero -and [EpochWin32Harness]::IsWindow($ProxyChildHandle)) { Get-WindowRectObject $ProxyChildHandle } else { $null }
    $hostVisible = $ProxyHostHandle -ne [IntPtr]::Zero -and [EpochWin32Harness]::IsWindow($ProxyHostHandle) -and [EpochWin32Harness]::IsWindowVisible($ProxyHostHandle)
    $useProxyContract =
        $ProxyHostHandle -ne [IntPtr]::Zero -and
        $ProxyChildHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($ProxyHostHandle) -and
        [EpochWin32Harness]::IsWindow($ProxyChildHandle)

    $hiddenProxyDocked =
        $useProxyContract -and
        $hostParent -eq $ParentHwnd -and
        $childParent -eq $ParentHwnd

    $topLevelWindows = @()
    if ($ProcessId -gt 0) {
        $topLevelWindows = Get-ProcessTopWindows -ProcessId $ProcessId |
            Where-Object { $_.Visible -and $_.Parent -eq [IntPtr]::Zero }
    }
    $hostTopLevelRecord = if ($ProxyHostHandle -ne [IntPtr]::Zero) {
        $topLevelWindows | Where-Object { $_.Hwnd -eq $ProxyHostHandle } | Select-Object -First 1
    } else {
        $null
    }
    $dockTopLevelRecord = if ($DockHandle -ne [IntPtr]::Zero) {
        $topLevelWindows | Where-Object { $_.Hwnd -eq $DockHandle } | Select-Object -First 1
    } else {
        $null
    }

    $directDocked =
        (-not $useProxyContract) -and
        $dockParent -eq $ParentHwnd
    $proxyDocked =
        $useProxyContract -and
        (
            ($hostParent -eq $ParentHwnd -and $childParent -eq $ProxyHostHandle) -or
            $hiddenProxyDocked
        )
    $directChildTopLevel =
        (-not $useProxyContract) -and
        $dockParent -eq [IntPtr]::Zero -and (
            $ProxyHostHandle -eq [IntPtr]::Zero -or (
                $hostParent -eq $ParentHwnd -and $childParent -ne $ProxyHostHandle
            )
        )
    $proxyHostTopLevel =
        $useProxyContract -and
        (
            $hostParent -eq [IntPtr]::Zero -or
            $null -ne $hostTopLevelRecord
        ) -and
        $childParent -eq $ProxyHostHandle
    $midTopLevel = if ($useProxyContract) {
        $proxyHostTopLevel -or $directChildTopLevel
    } else {
        $dockParent -ne $ParentHwnd
    }
    $escapedParentBounds = $false
    $topLevelRect = if ($proxyHostTopLevel) {
        if ($hostTopLevelRecord -and $hostTopLevelRecord.Rect) { $hostTopLevelRecord.Rect } else { $hostRect }
    } else {
        if ($dockTopLevelRecord -and $dockTopLevelRecord.Rect) { $dockTopLevelRecord.Rect } else { $dockRect }
    }
    if ($midTopLevel -and $topLevelRect -and $parentRect) {
        $escapedParentBounds =
            $topLevelRect.Left -lt $parentRect.Left -or
            $topLevelRect.Top -lt $parentRect.Top -or
            $topLevelRect.Right -gt $parentRect.Right -or
            $topLevelRect.Bottom -gt $parentRect.Bottom
    }

    [pscustomobject]@{
        UseProxyContract = $useProxyContract
        DockHandleParent = $dockParent
        ProxyHostParent = $hostParent
        ProxyChildParent = $childParent
        Docked = $directDocked -or $proxyDocked
        MidTopLevel = $midTopLevel
        EscapedParentBounds = $escapedParentBounds
        ParentRect = $parentRect
        DockRect = $dockRect
        ProxyHostRect = $hostRect
        ProxyChildRect = $childRect
        TopLevelWindows = $topLevelWindows
        ProxyHostTopLevelRecord = $hostTopLevelRecord
        DockTopLevelRecord = $dockTopLevelRecord
    }
}

function Wait-ForDragContractState(
    [IntPtr]$DockHandle,
    [IntPtr]$ParentHwnd,
    [IntPtr]$ProxyHostHandle = [IntPtr]::Zero,
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero,
    [int]$ProcessId = 0,
    [scriptblock]$Predicate,
    [int]$TimeoutMs = 2000
) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $snapshot = $null
    do {
        $snapshot = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
        if (& $Predicate $snapshot) {
            return $snapshot
        }
        Start-Sleep -Milliseconds 55
    } while ((Get-Date) -lt $deadline)

    return $snapshot
}

function Invoke-UndockRedock(
    [IntPtr]$DockHandle,
    [IntPtr]$ParentHwnd,
    [IntPtr]$ProxyHostHandle = [IntPtr]::Zero,
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero,
    [int]$ProcessId = 0,
    [string]$MidDragScreenshotPath = ''
) {
    $parentRect = Get-WindowRectObject $ParentHwnd
    $before = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
    $useProxyContract = $before.UseProxyContract
    $dockRect = if ($DockHandle -ne [IntPtr]::Zero -and [EpochWin32Harness]::IsWindow($DockHandle)) { Get-WindowRectObject $DockHandle } else { $null }
    $proxyHostVisible = $ProxyHostHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($ProxyHostHandle) -and
        [EpochWin32Harness]::IsWindowVisible($ProxyHostHandle)
    $proxyChildVisible = $ProxyChildHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($ProxyChildHandle) -and
        [EpochWin32Harness]::IsWindowVisible($ProxyChildHandle)

    $dragHandle = [IntPtr]::Zero
    $dragHandleKind = ''
    if ($useProxyContract -and $proxyHostVisible) {
        $dragHandle = $ProxyHostHandle
        $dragHandleKind = 'proxy-host'
    } elseif ($DockHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($DockHandle) -and
        $DockHandle -ne $ParentHwnd) {
        $dragHandle = $DockHandle
        $dragHandleKind = 'dock'
    } elseif ($useProxyContract -and $proxyChildVisible) {
        $dragHandle = $ProxyChildHandle
        $dragHandleKind = 'proxy-child'
    } else {
        throw "No valid dock/backend drag handle was found; refusing to drag the parent window."
    }

    if ($dragHandle -eq $ParentHwnd) {
        throw "Harness selected the parent as the drag handle; refusing misleading dock proof."
    }

    $dragRect = Get-WindowRectObject $dragHandle
    if (-not $dragRect) {
        throw "Selected drag handle '$dragHandleKind' has no window rect."
    }

    $virtualRect = Get-VirtualScreenRect
    $spaceRight = if ($parentRect) { $virtualRect.Right - $parentRect.Right } else { 0 }
    $spaceLeft = if ($parentRect) { $parentRect.Left - $virtualRect.Left } else { 0 }
    $escapeRight = $spaceRight -ge $spaceLeft
    $startX = 20
    $startY = 10
    $dragStartScreenX = $dragRect.Left + $startX
    $dragStartScreenY = $dragRect.Top + $startY
    Set-WindowForeground $ParentHwnd
    Set-ScreenCursorPoint $dragStartScreenX $dragStartScreenY
    Start-Sleep -Milliseconds 180
    Invoke-PhysicalLeftDown
    Start-Sleep -Milliseconds 260

    $edgeScreenX = if ($escapeRight) { $parentRect.Right - 8 } else { $parentRect.Left + 8 }
    $edgeScreenY = $parentRect.Top + 40
    Move-CursorLinear -FromX $dragStartScreenX -FromY $dragStartScreenY -ToX $edgeScreenX -ToY $edgeScreenY -Steps 9 -StepDelayMs 75
    Start-Sleep -Milliseconds 260

    $outsideOffset = 220
    $outsideScreenX = if ($escapeRight) {
        [Math]::Min($virtualRect.Right - 24, $parentRect.Right + $outsideOffset)
    } else {
        [Math]::Max($virtualRect.Left + 24, $parentRect.Left - $outsideOffset)
    }
    $outsideScreenY = $parentRect.Top + 40
    Move-CursorLinear -FromX $edgeScreenX -FromY $edgeScreenY -ToX $outsideScreenX -ToY $outsideScreenY -Steps 14 -StepDelayMs 105
    if ($useProxyContract) {
        $deadline = (Get-Date).AddMilliseconds(9000)
        do {
            $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
            if ($mid.MidTopLevel) {
                break
            }
            Start-Sleep -Milliseconds 260
            Set-ScreenCursorPoint $outsideScreenX $outsideScreenY
        } while ((Get-Date) -lt $deadline)
    } else {
        Start-Sleep -Milliseconds 1200
        $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
    }

    if ($useProxyContract -and $mid.MidTopLevel -and $ProxyHostHandle -ne [IntPtr]::Zero) {
        $farOutsideOffset = 520
        $farOutsideScreenX = if ($escapeRight) {
            [Math]::Min($virtualRect.Right - 24, $parentRect.Right + $farOutsideOffset)
        } else {
            [Math]::Max($virtualRect.Left + 24, $parentRect.Left - $farOutsideOffset)
        }
        $farOutsideScreenY = $parentRect.Top + 80
        $deadline = (Get-Date).AddMilliseconds(8000)
        do {
            Move-CursorLinear -FromX $outsideScreenX -FromY $outsideScreenY -ToX $farOutsideScreenX -ToY $farOutsideScreenY -Steps 8 -StepDelayMs 115
            Start-Sleep -Milliseconds 420
            $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
            if ($mid.EscapedParentBounds) {
                break
            }
        } while ((Get-Date) -lt $deadline)
    }

    if ($useProxyContract -and $mid.MidTopLevel) {
        Start-Sleep -Milliseconds 700
        $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
    }

    if (-not [string]::IsNullOrWhiteSpace($MidDragScreenshotPath)) {
        if ($mid.MidTopLevel) {
            $captureHandles = @($ParentHwnd)
            if ($useProxyContract -and $ProxyHostHandle -ne [IntPtr]::Zero) {
                $captureHandles += $ProxyHostHandle
            } elseif ($DockHandle -ne [IntPtr]::Zero) {
                $captureHandles += $DockHandle
            }
            [void](Save-CombinedWindowScreenshot -Handles $captureHandles -Destination $MidDragScreenshotPath)
        } else {
            [void](Save-WindowScreenshot -Hwnd $ParentHwnd -Destination $MidDragScreenshotPath)
        }
    }

    $insideScreenX = $parentRect.Left + 60
    $insideScreenY = $parentRect.Top + 60
    $dragFromX = if ($useProxyContract -and $mid.MidTopLevel) { $outsideScreenX } else { $edgeScreenX }
    $dragFromY = if ($useProxyContract -and $mid.MidTopLevel) { $outsideScreenY } else { $edgeScreenY }
    Move-CursorLinear -FromX $dragFromX -FromY $dragFromY -ToX $insideScreenX -ToY $insideScreenY -Steps 14 -StepDelayMs 95
    if ($useProxyContract) {
        $deadline = (Get-Date).AddMilliseconds(6500)
        do {
            $late = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
            if ($late.MidTopLevel) {
                break
            }
            Start-Sleep -Milliseconds 260
            Set-ScreenCursorPoint $insideScreenX $insideScreenY
        } while ((Get-Date) -lt $deadline)
    } else {
        Start-Sleep -Milliseconds 800
        $late = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId
    }

    Set-ScreenCursorPoint $insideScreenX $insideScreenY
    Invoke-PhysicalLeftUp
    $end = Wait-ForDragContractState -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -ProcessId $ProcessId -Predicate { param($s) $s.Docked } -TimeoutMs 6500

    [pscustomobject]@{
        ContractMode = if ($useProxyContract) { 'proxy-release-redock' } else { 'direct-live-redock' }
        DragHandle = $dragHandle
        DragHandleKind = $dragHandleKind
        DragHandleRect = $dragRect
        MidDragScreenshotPath = $MidDragScreenshotPath
        BeforeDocked = $before.Docked
        MidTopLevel = $mid.MidTopLevel
        MidEscapedParentBounds = $mid.EscapedParentBounds
        LateDetached = $late.MidTopLevel
        LateEscapedParentBounds = $late.EscapedParentBounds
        LateRedocked = $late.Docked
        EndRedocked = $end.Docked
        DockHandleParentBefore = $before.DockHandleParent
        DockHandleParentMid = $mid.DockHandleParent
        DockHandleParentLate = $late.DockHandleParent
        DockHandleParentEnd = $end.DockHandleParent
        ProxyHostParentBefore = $before.ProxyHostParent
        ProxyHostParentMid = $mid.ProxyHostParent
        ProxyHostParentLate = $late.ProxyHostParent
        ProxyHostParentEnd = $end.ProxyHostParent
        ProxyChildParentBefore = $before.ProxyChildParent
        ProxyChildParentMid = $mid.ProxyChildParent
        ProxyChildParentLate = $late.ProxyChildParent
        ProxyChildParentEnd = $end.ProxyChildParent
        MidDockRect = $mid.DockRect
        MidProxyHostRect = $mid.ProxyHostRect
        MidProxyChildRect = $mid.ProxyChildRect
        LateDockRect = $late.DockRect
        LateProxyHostRect = $late.ProxyHostRect
        LateProxyChildRect = $late.ProxyChildRect
        MidTopLevelWindows = $mid.TopLevelWindows
        LateTopLevelWindows = $late.TopLevelWindows
        EndTopLevelWindows = $end.TopLevelWindows
        MidProxyHostTopLevelRecord = $mid.ProxyHostTopLevelRecord
        LateProxyHostTopLevelRecord = $late.ProxyHostTopLevelRecord
        EndProxyHostTopLevelRecord = $end.ProxyHostTopLevelRecord
    }
}

function Get-ProxyHostProbe([IntPtr]$HostHandle, [IntPtr]$ParentHwnd, [int]$SettleTimeoutMs = 350) {
    if ($HostHandle -eq [IntPtr]::Zero -or -not [EpochWin32Harness]::IsWindow($HostHandle)) {
        return [pscustomobject]@{
            Present = $false
            Parent = [IntPtr]::Zero
            Visible = $false
            RehiddenInParent = $true
        }
    }

    $deadline = (Get-Date).AddMilliseconds($SettleTimeoutMs)
    $hostParent = [IntPtr]::Zero
    $hostVisible = $false
    do {
        $hostParent = [EpochWin32Harness]::GetParent($HostHandle)
        $hostVisible = [EpochWin32Harness]::IsWindowVisible($HostHandle)
        if ($hostParent -ne $ParentHwnd -or -not $hostVisible) {
            break
        }
        Start-Sleep -Milliseconds 25
    } while ((Get-Date) -lt $deadline)

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
    $json = $Result | ConvertTo-Json -Depth 8
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Destination, $json, $utf8NoBom)
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

function Save-CombinedWindowScreenshot([IntPtr[]]$Handles, [string]$Destination, [int]$Padding = 24) {
    $rects = @()
    foreach ($handle in $Handles) {
        if ($handle -eq [IntPtr]::Zero -or -not [EpochWin32Harness]::IsWindow($handle)) { continue }
        $rect = Get-WindowRectObject $handle
        if (-not $rect -or $rect.Width -le 0 -or $rect.Height -le 0) { continue }
        $rects += $rect
    }

    if (-not $rects -or $rects.Count -eq 0) {
        return $false
    }

    $left = ($rects | Measure-Object -Property Left -Minimum).Minimum - $Padding
    $top = ($rects | Measure-Object -Property Top -Minimum).Minimum - $Padding
    $right = ($rects | Measure-Object -Property Right -Maximum).Maximum + $Padding
    $bottom = ($rects | Measure-Object -Property Bottom -Maximum).Maximum + $Padding
    $width = [Math]::Max(1, $right - $left)
    $height = [Math]::Max(1, $bottom - $top)

    $dir = Split-Path -Parent $Destination
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($left, $top, 0, 0, $bitmap.Size)
        $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
        return $true
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-WindowRecordByHandle($Children, [IntPtr]$Hwnd) {
    if ($Hwnd -eq [IntPtr]::Zero) {
        return $null
    }

    $Children | Where-Object { $_.Hwnd -eq $Hwnd } | Select-Object -First 1
}

function Get-ContentSampleProbe([string]$ImagePath, $ParentRect, $WindowRecord) {
    if ([string]::IsNullOrWhiteSpace($ImagePath) -or -not (Test-Path $ImagePath) -or -not $ParentRect -or -not $WindowRecord -or -not $WindowRecord.Rect) {
        return [pscustomobject]@{
            Available = $false
            DistinctSampleColors = 0
            LumaRange = 0
            LikelyRendered = $false
        }
    }

    $bitmap = [System.Drawing.Bitmap]::FromFile($ImagePath)
    try {
        $rect = $WindowRecord.Rect
        $left = [Math]::Max(0, $rect.Left - $ParentRect.Left)
        $top = [Math]::Max(0, $rect.Top - $ParentRect.Top)
        $right = [Math]::Min($bitmap.Width, $rect.Right - $ParentRect.Left)
        $bottom = [Math]::Min($bitmap.Height, $rect.Bottom - $ParentRect.Top)
        $width = [Math]::Max(0, $right - $left)
        $height = [Math]::Max(0, $bottom - $top)

        if ($width -lt 8 -or $height -lt 8) {
            return [pscustomobject]@{
                Available = $false
                DistinctSampleColors = 0
                LumaRange = 0
                LikelyRendered = $false
            }
        }

        $sampleCols = 7
        $sampleRows = 7
        $insetX = [Math]::Max(2, [int]($width * 0.08))
        $insetY = [Math]::Max(2, [int]($height * 0.08))
        $minX = [Math]::Min($bitmap.Width - 1, $left + $insetX)
        $maxX = [Math]::Max($minX, [Math]::Min($bitmap.Width - 1, $right - 1 - $insetX))
        $minY = [Math]::Min($bitmap.Height - 1, $top + $insetY)
        $maxY = [Math]::Max($minY, [Math]::Min($bitmap.Height - 1, $bottom - 1 - $insetY))

        $colors = [System.Collections.Generic.HashSet[string]]::new()
        $minLuma = 255
        $maxLuma = 0

        for ($row = 0; $row -lt $sampleRows; $row++) {
            $py = if ($sampleRows -le 1) { $minY } else { [int]([Math]::Round($minY + (($maxY - $minY) * $row / ($sampleRows - 1)))) }
            for ($col = 0; $col -lt $sampleCols; $col++) {
                $px = if ($sampleCols -le 1) { $minX } else { [int]([Math]::Round($minX + (($maxX - $minX) * $col / ($sampleCols - 1)))) }
                $pixel = $bitmap.GetPixel($px, $py)
                [void]$colors.Add(('{0:X2}{1:X2}{2:X2}' -f $pixel.R, $pixel.G, $pixel.B))
                $luma = [int](($pixel.R * 299 + $pixel.G * 587 + $pixel.B * 114) / 1000)
                if ($luma -lt $minLuma) { $minLuma = $luma }
                if ($luma -gt $maxLuma) { $maxLuma = $luma }
            }
        }

        $lumaRange = $maxLuma - $minLuma
        $distinct = $colors.Count
        $likelyRendered = ($distinct -ge 6) -or ($lumaRange -ge 28)

        return [pscustomobject]@{
            Available = $true
            DistinctSampleColors = $distinct
            LumaRange = $lumaRange
            LikelyRendered = $likelyRendered
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$root = (Resolve-Path '.').Path
$runtimeDir = Join-Path $root ("x64\{0}" -f $Configuration)
$exe = Join-Path $runtimeDir 'EpochEditor.exe'
if (-not (Test-Path $exe)) {
    throw "Missing runtime at $exe"
}
if ($AssetsInteractionProof) {
    if ($ProjectProfile -notmatch '^[A-Za-z0-9_-]+$') {
        throw "Invalid project profile '$ProjectProfile'."
    }

    function Invoke-EpochCliProbe(
        [string[]]$Arguments,
        [int]$TimeoutSeconds
    ) {
        $probeLogRoot = Join-Path $runtimeDir 'logs'
        [System.IO.Directory]::CreateDirectory($probeLogRoot) | Out-Null
        $probeId = [Guid]::NewGuid().ToString('N')
        $stdoutPath = Join-Path $probeLogRoot ($probeId + '.stdout.log')
        $stderrPath = Join-Path $probeLogRoot ($probeId + '.stderr.log')

        try {
            $probe = Start-Process `
                -FilePath $exe `
                -WorkingDirectory $runtimeDir `
                -ArgumentList $Arguments `
                -NoNewWindow `
                -PassThru `
                -RedirectStandardOutput $stdoutPath `
                -RedirectStandardError $stderrPath
            if (-not $probe) {
                throw "Failed to launch Epoch CLI probe: $Arguments"
            }

            if (-not $probe.WaitForExit($TimeoutSeconds * 1000)) {
                Stop-Process -Id $probe.Id -Force -ErrorAction SilentlyContinue
                $probe.WaitForExit()
                throw "Epoch CLI probe timed out: $Arguments"
            }

            $stdout = if (Test-Path $stdoutPath) {
                Get-Content -LiteralPath $stdoutPath -Raw
            } else {
                ''
            }
            $stderr = if (Test-Path $stderrPath) {
                Get-Content -LiteralPath $stderrPath -Raw
            } else {
                ''
            }
            [pscustomobject]@{
                ExitCode = $probe.ExitCode
                StandardOutput = $stdout
                StandardError = $stderr
            }
        }
        finally {
            Remove-Item -LiteralPath $stdoutPath -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $stderrPath -Force -ErrorAction SilentlyContinue
        }
    }

    $tracePath = Join-Path $runtimeDir 'epoch_editor_auto_command.log'
    if (Test-Path $tracePath) {
        Remove-Item -LiteralPath $tracePath -Force
    }

    $automationInfo = New-Object System.Diagnostics.ProcessStartInfo
    $automationInfo.FileName = $exe
    $automationInfo.WorkingDirectory = $runtimeDir
    $automationInfo.Arguments = '--editor --backend opengl'
    $automationInfo.UseShellExecute = $false
    $automationInfo.CreateNoWindow = $true
    $automationInfo.EnvironmentVariables['EPOCH_EDITOR_AUTO_COMMAND'] =
        'assets-interaction-proof'
    $automationInfo.EnvironmentVariables['EPOCH_EDITOR_PROJECT_ID'] =
        $ProjectProfile

    $automation = [System.Diagnostics.Process]::Start($automationInfo)
    if (-not $automation) {
        throw 'Failed to launch the semantic Assets interaction proof.'
    }

    try {
        $deadline = (Get-Date).AddSeconds(90)
        do {
            Start-Sleep -Milliseconds 100
            $automation.Refresh()
            $traceReady =
                (Test-Path $tracePath) -and
                ((Get-Content -LiteralPath $tracePath -Raw) -match
                    'assets_interaction_proof\.result=(pass|fail)')
        } while (-not $traceReady -and -not $automation.HasExited -and
            (Get-Date) -lt $deadline)

        if (-not $traceReady) {
            throw 'Semantic Assets interaction proof did not produce a bounded result.'
        }
    }
    finally {
        $automation.Refresh()
        if (-not $automation.HasExited) {
            [void]$automation.CloseMainWindow()
            if (-not $automation.WaitForExit(2000)) {
                $automation.Kill()
                $automation.WaitForExit()
            }
        }
    }

    $assetTrace = Get-Content -LiteralPath $tracePath -Raw
    $engineContract =
        Invoke-EpochCliProbe @('--engine-contract-self-test') 300
    $projectLifecycle =
        Invoke-EpochCliProbe @(
            '--editor-project-self-test',
            $ProjectProfile
        ) 1200

    $semanticStages = @(
        'scene.prepare_isolated',
        'scene.select_renderable',
        'assets.texture.create_import',
        'assets.texture.select_compiled',
        'assets.texture.assign_to_selection',
        'assets.texture.clear_material',
        'scene.undo',
        'scene.redo',
        'scene.restore_assigned_for_save',
        'project.save',
        'project.reopen'
    )
    $semanticChecks = [ordered]@{}
    foreach ($stage in $semanticStages) {
        $semanticChecks[$stage] =
            $assetTrace -match (
                [regex]::Escape(
                    "assets_interaction_proof.$stage=pass"))
    }

    $engineOutput =
        $engineContract.StandardOutput +
        [Environment]::NewLine +
        $engineContract.StandardError
    $projectOutput =
        $projectLifecycle.StandardOutput +
        [Environment]::NewLine +
        $projectLifecycle.StandardError
    $checks = [ordered]@{
        SemanticAssetsResult =
            $assetTrace -match
                'assets_interaction_proof\.result=pass'
        EngineContract =
            $engineContract.ExitCode -eq 0 -and
            $engineOutput -match
                'engine_contract_self_test\.result=pass'
        ProjectMaterialize =
            $projectLifecycle.ExitCode -eq 0 -and
            $projectOutput -match
                'editor_project_self_test\.materialize=pass'
        ProjectSaveReopen =
            $projectOutput -match
                'editor_project_self_test\.save_reopen=pass'
        ProjectBuild =
            $projectOutput -match
                'editor_project_self_test\.build=pass'
        ExternalRun =
            $projectOutput -match
                'editor_project_self_test\.child_self_test=pass'
    }
    $allPassed =
        -not ($semanticChecks.Values -contains $false) -and
        -not ($checks.Values -contains $false)

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $OutputPath = Join-Path $runtimeDir (
            "logs\assets_interaction_proof_$stamp.json")
    }

    $result = [pscustomobject]@{
        Pass = $allPassed
        Configuration = $Configuration
        ProjectProfile = $ProjectProfile
        SemanticControlIds = $semanticStages
        SemanticChecks = $semanticChecks
        Checks = $checks
        AutomationTracePath = $tracePath
        AutomationTrace = $assetTrace
        EngineContractExitCode = $engineContract.ExitCode
        EngineContractOutput = $engineOutput
        ProjectLifecycleExitCode = $projectLifecycle.ExitCode
        ProjectLifecycleOutput = $projectOutput
    }
    Save-Result -Result $result -Destination $OutputPath
    Write-Output $OutputPath
    if (-not $allPassed) {
        throw "Assets interaction proof failed. Inspect $OutputPath"
    }
    return
}

$args = @('--editor')
if ($Mode -eq 'Full') {
    $args += '--backend'
    $args += 'auto'
    $args += '--window-mode'
    $args += 'parented'
}
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

$result = $null
$stillRunningAfterCloseProbe = $null
$closeProbeRequested = $false
$closeProbeTarget = ''
$outputReady = $false

try {
Start-Sleep -Seconds 10
    $parentWindow = Wait-ForParentWindow -ProcessId $proc.Id
    if (-not $parentWindow) {
        throw 'Timed out waiting for the parent editor window.'
    }

    if (-not $SkipMaximize) {
        [void][EpochWin32Harness]::ShowWindow($parentWindow.Hwnd, $SW_MAXIMIZE)
Start-Sleep -Milliseconds 2200
    }
Start-Sleep -Milliseconds 2200

    $needles = if ($Mode -eq 'Full') {
        @('GLFW','SDL','SFML','Vulkan','OpenGL','DirectX')
    } else {
        switch ($Backend) {
            'raylib' { @('GLFW') }
            'sdl' { @('SDL') }
            'sfml' { @('SFML') }
            'vulkan' { @('Vulkan') }
            'opengl' { @('OpenGL') }
            'directx' { @('DirectX') }
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

    Start-Sleep -Seconds 4
    $settledReady = Wait-ForBackendWindows -Parent $parentWindow.Hwnd -Needles $needles -TimeoutSeconds 15
    if ($settledReady) {
        $ready = $settledReady
    }

    $children = $ready.Children
    $topLevelWindowsBefore = Get-ProcessTopWindows -ProcessId $proc.Id | Select-Object Hwnd,Class,Title,Visible,Parent,Rect
    $rayHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*Raylib*' } | Select-Object -First 1
    $sdlHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*SDL*' } | Select-Object -First 1
    $sfmlHost = $children | Where-Object { $_.Class -eq 'EpochChild' -and $_.Title -like '*SFML*' } | Select-Object -First 1
    $rayChild = $children | Where-Object { $_.Class -eq 'GLFW30' } | Select-Object -First 1
    $sdlChild = $children | Where-Object { $_.Class -eq 'SDL_app' } | Select-Object -First 1
    $sfmlChild = $children | Where-Object { $_.Class -eq 'SFML_Window' } | Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $suffix = if ($Mode -eq 'Single') { "_$Backend" } else { '' }
        $OutputPath = Join-Path $root ("x64\{0}\logs\multicontext_harness_{1}{2}_{3}.json" -f $Configuration.ToLowerInvariant(), $Mode.ToLowerInvariant(), $suffix, $stamp)
    }

    $startupScreenshotPath = ''
    $startupContentProbes = @()
    if ($CaptureStartupProof)
    {
        $startupScreenshotPath = [System.IO.Path]::ChangeExtension($OutputPath, '.startup.png')
        [void](Save-WindowScreenshot -Hwnd $parentWindow.Hwnd -Destination $startupScreenshotPath)
        $parentRect = Get-WindowRectObject $parentWindow.Hwnd

        foreach ($startupItem in @(
            @{ Name = 'raylib'; Window = $(if ($rayChild) { $rayChild } elseif ($rayHost) { $rayHost } else { $null }) },
            @{ Name = 'sdl'; Window = $(if ($sdlChild) { $sdlChild } elseif ($sdlHost) { $sdlHost } else { $null }) },
            @{ Name = 'sfml'; Window = $(if ($sfmlChild) { $sfmlChild } elseif ($sfmlHost) { $sfmlHost } else { $null }) }
        )) {
            if ($Mode -eq 'Single' -and $startupItem.Name -ne $Backend) { continue }
            if ($Mode -eq 'Full' -and -not [string]::IsNullOrWhiteSpace($FocusedBackend) -and $startupItem.Name -ne $FocusedBackend) { continue }
            $startupContentProbes += [pscustomobject]@{
                Backend = $startupItem.Name
                Probe = Get-ContentSampleProbe -ImagePath $startupScreenshotPath -ParentRect $parentRect -WindowRecord $startupItem.Window
            }
        }
    }

    if ($StartupOnly)
    {
        $screenshotPath = [System.IO.Path]::ChangeExtension($OutputPath, '.png')
        $topLevelHandles = @($parentWindow.Hwnd) + @(
            (Get-ProcessTopWindows -ProcessId $proc.Id |
                Where-Object { $_.Visible -and $_.Parent -eq [IntPtr]::Zero } |
                ForEach-Object { $_.Hwnd })
        )
        $topLevelHandles = $topLevelHandles | Select-Object -Unique
        $savedCombinedScreenshot =
            Save-CombinedWindowScreenshot -Handles $topLevelHandles -Destination $screenshotPath
        if (-not $savedCombinedScreenshot -or -not (Test-Path $screenshotPath)) {
            [void](Save-WindowScreenshot -Hwnd $parentWindow.Hwnd -Destination $screenshotPath)
        }

        $result = [pscustomobject]@{
            Mode = $Mode
            Backend = $Backend
            FocusedBackend = $FocusedBackend
            StartupOnly = $true
            ProcessId = $proc.Id
            Parent = $parentWindow
            TopLevelWindowsBefore = $topLevelWindowsBefore
            VisibleChildrenBefore = $children | Select-Object Class,Title,Visible,Rect
            StartupScreenshotPath = $startupScreenshotPath
            StartupContentProbes = $startupContentProbes
            TopLevelWindowsAfterDrag = @()
            VisibleChildrenAfterDrag = @()
            Checks = @()
            CloseProbeRequested = $false
            CloseProbeTarget = ''
            StillRunningAfterCloseProbe = $null
            ScreenshotPath = $screenshotPath
        }
        $outputReady = $true
        return
    }

    $backendChecks = @()
    foreach ($item in @(
        @{ Name = 'raylib'; DockHandle = $(if ($rayHost) { $rayHost.Hwnd } elseif ($rayChild) { $rayChild.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($rayChild) { $rayChild.Hwnd } elseif ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = $(if ($rayChild) { $rayChild.Hwnd } else { [IntPtr]::Zero }) },
        @{ Name = 'sdl'; DockHandle = $(if ($sdlHost) { $sdlHost.Hwnd } elseif ($sdlChild) { $sdlChild.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sdlChild) { $sdlChild.Hwnd } elseif ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = $(if ($sdlChild) { $sdlChild.Hwnd } else { [IntPtr]::Zero }) },
        @{ Name = 'sfml'; DockHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } elseif ($sfmlChild) { $sfmlChild.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sfmlChild) { $sfmlChild.Hwnd } elseif ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = $(if ($sfmlChild) { $sfmlChild.Hwnd } else { [IntPtr]::Zero }) }
    )) {
        if ($Mode -eq 'Single' -and $item.Name -ne $Backend) { continue }
        if ($Mode -eq 'Full' -and -not [string]::IsNullOrWhiteSpace($FocusedBackend) -and $item.Name -ne $FocusedBackend) { continue }
        if ($item.DockHandle -eq [IntPtr]::Zero) { continue }

        $midDragScreenshotPath = [System.IO.Path]::ChangeExtension(
            $OutputPath,
            ('.' + $item.Name + '.mid.png'))
        $drag = Invoke-UndockRedock `
            -DockHandle $item.DockHandle `
            -ParentHwnd $parentWindow.Hwnd `
            -ProxyHostHandle $item.HostHandle `
            -ProxyChildHandle $item.ChildHandle `
            -ProcessId $proc.Id `
            -MidDragScreenshotPath $midDragScreenshotPath
        Invoke-LeftClick -Hwnd $item.FocusHandle -ClientX 24 -ClientY 24
Start-Sleep -Milliseconds 260
        $focus = Get-FocusProbe -TargetHwnd $item.FocusHandle
        $proxyHost = Get-ProxyHostProbe -HostHandle $item.HostHandle -ParentHwnd $parentWindow.Hwnd
        $backendChecks += [pscustomobject]@{
            Backend = $item.Name
            Drag = $drag
            Focus = $focus
            ProxyHost = $proxyHost
        }

Start-Sleep -Milliseconds 800
        if ($Mode -eq 'Full') {
[void](Wait-ForBackendWindows -Parent $parentWindow.Hwnd -Needles $needles -TimeoutSeconds 10)
        }
    }

Start-Sleep -Seconds 4
    $childrenAfterDrag = Get-DescendantWindows -Parent $parentWindow.Hwnd
    $topLevelWindowsAfterDrag = Get-ProcessTopWindows -ProcessId $proc.Id | Select-Object Hwnd,Class,Title,Visible,Parent,Rect
    $screenshotPath = ''

    $stillRunningAfterCloseProbe = $null
    if (-not $SkipCloseProbe -and $Mode -eq 'Full' -and $sfmlChild) {
        $closeProbeRequested = $true
        $closeProbeTarget = 'SFML child WM_CLOSE'
        [void][EpochWin32Harness]::PostMessageW($sfmlChild.Hwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 900
        $proc.Refresh()
        $stillRunningAfterCloseProbe = -not $proc.HasExited
    }

    $screenshotPath = [System.IO.Path]::ChangeExtension($OutputPath, '.png')
    $topLevelHandles = @($parentWindow.Hwnd) + @(
        (Get-ProcessTopWindows -ProcessId $proc.Id |
            Where-Object { $_.Visible -and $_.Parent -eq [IntPtr]::Zero } |
            ForEach-Object { $_.Hwnd })
    )
    $topLevelHandles = $topLevelHandles | Select-Object -Unique
    $savedCombinedScreenshot =
        Save-CombinedWindowScreenshot -Handles $topLevelHandles -Destination $screenshotPath
    if (-not $savedCombinedScreenshot -or -not (Test-Path $screenshotPath)) {
        [void](Save-WindowScreenshot -Hwnd $parentWindow.Hwnd -Destination $screenshotPath)
    }

    $result = [pscustomobject]@{
        Mode = $Mode
        Backend = $Backend
        FocusedBackend = $FocusedBackend
        ProcessId = $proc.Id
        Parent = $parentWindow
        TopLevelWindowsBefore = $topLevelWindowsBefore
        VisibleChildrenBefore = $children | Select-Object Class,Title,Visible,Rect
        StartupScreenshotPath = $startupScreenshotPath
        StartupContentProbes = $startupContentProbes
        TopLevelWindowsAfterDrag = $topLevelWindowsAfterDrag
        VisibleChildrenAfterDrag = $childrenAfterDrag | Select-Object Class,Title,Visible,Rect
        Checks = $backendChecks
        CloseProbeRequested = $closeProbeRequested
        CloseProbeTarget = $closeProbeTarget
        StillRunningAfterCloseProbe = $stillRunningAfterCloseProbe
        ScreenshotPath = $screenshotPath
    }
    $outputReady = $true
}
finally {
    if ($proc -and -not $proc.HasExited) {
        $proc.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 650
        if (-not $proc.HasExited) {
            $proc.Kill()
            $proc.WaitForExit()
        }
    }

    if ($outputReady -and $result) {
        if ($proc) {
            $proc.Refresh()
            $finalStillRunning = -not $proc.HasExited
        } else {
            $finalStillRunning = $false
        }

        $result | Add-Member -Force -NotePropertyName StillRunningAfterClose -NotePropertyValue $finalStillRunning
        Save-Result -Result $result -Destination $OutputPath
        Write-Output $OutputPath
    }
}
