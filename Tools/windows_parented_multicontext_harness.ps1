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
    [switch]$SkipCloseProbe,
    [switch]$CaptureStartupProof
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
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
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

function Set-ScreenCursorPoint([int]$X, [int]$Y) {
    [void][EpochWin32Harness]::SetCursorPos($X, $Y)
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
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero
) {
    $useProxyContract =
        $ProxyHostHandle -ne [IntPtr]::Zero -and
        $ProxyChildHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($ProxyHostHandle) -and
        [EpochWin32Harness]::IsWindow($ProxyChildHandle)

    $dockParent = Get-ParentHandleValue $DockHandle
    $hostParent = Get-ParentHandleValue $ProxyHostHandle
    $childParent = Get-ParentHandleValue $ProxyChildHandle

    $directDocked =
        $dockParent -eq $ParentHwnd -and (
            $ProxyHostHandle -eq [IntPtr]::Zero -or (
                $ProxyChildHandle -eq [IntPtr]::Zero -and $hostParent -eq $ParentHwnd
            ) -or (
                $hostParent -eq $ParentHwnd -and $childParent -eq $ParentHwnd
            )
        )
    $proxyDocked =
        $useProxyContract -and
        $hostParent -eq $ParentHwnd -and
        $childParent -eq $ProxyHostHandle
    $midTopLevel = if ($useProxyContract) {
        $hostParent -ne $ParentHwnd -and $childParent -eq $ProxyHostHandle
    } else {
        $dockParent -ne $ParentHwnd
    }

    [pscustomobject]@{
        UseProxyContract = $useProxyContract
        DockHandleParent = $dockParent
        ProxyHostParent = $hostParent
        ProxyChildParent = $childParent
        Docked = $directDocked -or $proxyDocked
        MidTopLevel = $midTopLevel
    }
}

function Wait-ForDragContractState(
    [IntPtr]$DockHandle,
    [IntPtr]$ParentHwnd,
    [IntPtr]$ProxyHostHandle = [IntPtr]::Zero,
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero,
    [scriptblock]$Predicate,
    [int]$TimeoutMs = 900
) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $snapshot = $null
    do {
        $snapshot = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
        if (& $Predicate $snapshot) {
            return $snapshot
        }
        Start-Sleep -Milliseconds 25
    } while ((Get-Date) -lt $deadline)

    return $snapshot
}

function Invoke-UndockRedock(
    [IntPtr]$DockHandle,
    [IntPtr]$ParentHwnd,
    [IntPtr]$ProxyHostHandle = [IntPtr]::Zero,
    [IntPtr]$ProxyChildHandle = [IntPtr]::Zero,
    [string]$MidDragScreenshotPath = ''
) {
    $useProxyContract =
        $ProxyHostHandle -ne [IntPtr]::Zero -and
        $ProxyChildHandle -ne [IntPtr]::Zero -and
        [EpochWin32Harness]::IsWindow($ProxyHostHandle) -and
        [EpochWin32Harness]::IsWindow($ProxyChildHandle)
    $parentRect = Get-WindowRectObject $ParentHwnd
    $before = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
    $startX = 20
    $startY = 10
    Set-ScreenCursorPoint ($parentRect.Left + $startX) ($parentRect.Top + $startY)
    [void][EpochWin32Harness]::PostMessageW($DockHandle, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, (Get-LParam $startX $startY))
    Start-Sleep -Milliseconds 100

    $outsideScreenX = $parentRect.Right + 120
    $outsideScreenY = $parentRect.Top + 40
    Set-ScreenCursorPoint $outsideScreenX $outsideScreenY
    $outsideTarget = $DockHandle
    $moveOutside = Convert-ScreenToClientPoint $outsideTarget $outsideScreenX $outsideScreenY
    [void][EpochWin32Harness]::PostMessageW($outsideTarget, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveOutside.X $moveOutside.Y))
    if ($useProxyContract) {
        $deadline = (Get-Date).AddMilliseconds(1500)
        do {
            $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
            if ($mid.MidTopLevel) {
                break
            }
            Start-Sleep -Milliseconds 75
            Set-ScreenCursorPoint $outsideScreenX $outsideScreenY
            [void][EpochWin32Harness]::PostMessageW($outsideTarget, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveOutside.X $moveOutside.Y))
        } while ((Get-Date) -lt $deadline)
    } else {
        Start-Sleep -Milliseconds 250
        $mid = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
    }

    if (-not [string]::IsNullOrWhiteSpace($MidDragScreenshotPath)) {
        [void](Save-WindowScreenshot -Hwnd $ParentHwnd -Destination $MidDragScreenshotPath)
    }

    $insideScreenX = $parentRect.Left + 60
    $insideScreenY = $parentRect.Top + 60
    $moveTarget = if ($useProxyContract -and $mid.MidTopLevel -and $ProxyHostHandle -ne [IntPtr]::Zero) { $ProxyHostHandle } else { $DockHandle }
    $moveInside = Convert-ScreenToClientPoint $moveTarget $insideScreenX $insideScreenY
    Set-ScreenCursorPoint $insideScreenX $insideScreenY
    [void][EpochWin32Harness]::PostMessageW($moveTarget, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveInside.X $moveInside.Y))
    if ($useProxyContract) {
        $deadline = (Get-Date).AddMilliseconds(500)
        do {
            $late = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
            if ($late.MidTopLevel) {
                break
            }
            Start-Sleep -Milliseconds 75
            Set-ScreenCursorPoint $insideScreenX $insideScreenY
            $moveTarget = if ($late.MidTopLevel -and $ProxyHostHandle -ne [IntPtr]::Zero) { $ProxyHostHandle } else { $DockHandle }
            $moveInside = Convert-ScreenToClientPoint $moveTarget $insideScreenX $insideScreenY
            [void][EpochWin32Harness]::PostMessageW($moveTarget, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (Get-LParam $moveInside.X $moveInside.Y))
        } while ((Get-Date) -lt $deadline)
    } else {
        Start-Sleep -Milliseconds 250
        $late = Get-DragContractSnapshot -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle
    }

    $releaseTarget = if ($useProxyContract -and $late.MidTopLevel -and $ProxyHostHandle -ne [IntPtr]::Zero) { $ProxyHostHandle } else { $DockHandle }
    $releasePoint = Convert-ScreenToClientPoint $releaseTarget $insideScreenX $insideScreenY
    [void][EpochWin32Harness]::PostMessageW($releaseTarget, $WM_LBUTTONUP, [IntPtr]::Zero, (Get-LParam $releasePoint.X $releasePoint.Y))
    $end = Wait-ForDragContractState -DockHandle $DockHandle -ParentHwnd $ParentHwnd -ProxyHostHandle $ProxyHostHandle -ProxyChildHandle $ProxyChildHandle -Predicate { param($s) $s.Docked } -TimeoutMs 900

    [pscustomobject]@{
        ContractMode = if ($useProxyContract) { 'proxy-release-redock' } else { 'direct-live-redock' }
        MidDragScreenshotPath = $MidDragScreenshotPath
        BeforeDocked = $before.Docked
        MidTopLevel = $mid.MidTopLevel
        LateDetached = $late.MidTopLevel
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
$exe = Join-Path $root ("x64\{0}\ConsoleApplication1.exe" -f $Configuration)
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

$result = $null
$stillRunningAfterCloseProbe = $null
$closeProbeRequested = $false
$closeProbeTarget = ''
$outputReady = $false

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

    $backendChecks = @()
    foreach ($item in @(
        @{ Name = 'raylib'; DockHandle = $(if ($rayChild) { $rayChild.Hwnd } elseif ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($rayChild) { $rayChild.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($rayHost) { $rayHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = [IntPtr]::Zero },
        @{ Name = 'sdl'; DockHandle = $(if ($sdlHost) { $sdlHost.Hwnd } elseif ($sdlChild) { $sdlChild.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sdlHost) { $sdlHost.Hwnd } elseif ($sdlChild) { $sdlChild.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sdlHost) { $sdlHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = $(if ($sdlChild) { $sdlChild.Hwnd } else { [IntPtr]::Zero }) },
        @{ Name = 'sfml'; DockHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } elseif ($sfmlChild) { $sfmlChild.Hwnd } else { [IntPtr]::Zero }); FocusHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } elseif ($sfmlChild) { $sfmlChild.Hwnd } else { [IntPtr]::Zero }); HostHandle = $(if ($sfmlHost) { $sfmlHost.Hwnd } else { [IntPtr]::Zero }); ChildHandle = $(if ($sfmlChild) { $sfmlChild.Hwnd } else { [IntPtr]::Zero }) }
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
            -MidDragScreenshotPath $midDragScreenshotPath
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

    $stillRunningAfterCloseProbe = $null
    if (-not $SkipCloseProbe -and $Mode -eq 'Full' -and $sfmlChild) {
        $closeProbeRequested = $true
        $closeProbeTarget = 'SFML child WM_CLOSE'
        [void][EpochWin32Harness]::PostMessageW($sfmlChild.Hwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 600
        $proc.Refresh()
        $stillRunningAfterCloseProbe = -not $proc.HasExited
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
        StartupScreenshotPath = $startupScreenshotPath
        StartupContentProbes = $startupContentProbes
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
        Start-Sleep -Milliseconds 400
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
