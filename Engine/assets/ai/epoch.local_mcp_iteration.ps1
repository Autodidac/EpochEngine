param(
    [Parameter(Mandatory = $true)]
    [string] $PromptPath,

    [Parameter(Mandatory = $true)]
    [string] $WorkspaceRoot,

    [Parameter(Mandatory = $true)]
    [string] $ResponsePath,

    [Parameter(Mandatory = $true)]
    [string] $StatusPath,

    [Parameter(Mandatory = $true)]
    [string] $ReceiptPath,

    [Parameter(Mandatory = $true)]
    [string] $ThreadPath,

    [ValidateRange(30, 3600)]
    [int] $TimeoutSeconds = 900,

    [string] $ThreadId = ""
)

$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$started = [DateTimeOffset]::UtcNow
$server = $null
$serverPid = 0
$phase = "starting"

function Write-Utf8Atomic {
    param([string] $Path, [string] $Text)
    $directory = [System.IO.Path]::GetDirectoryName($Path)
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
    $temporary = "$Path.tmp.$PID"
    [System.IO.File]::WriteAllText(
        $temporary,
        $Text,
        [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Write-Status {
    param([string] $State, [string] $Detail = "")
    $elapsed = [int64]([DateTimeOffset]::UtcNow - $started).TotalMilliseconds
    $record = [ordered]@{
        schema = "epoch-local-mcp-status/v1"
        state = $State
        phase = $script:phase
        bridge_pid = $PID
        server_pid = $script:serverPid
        elapsed_ms = $elapsed
        timeout_seconds = $TimeoutSeconds
        detail = $Detail
    }
    Write-Utf8Atomic -Path $StatusPath -Text ($record | ConvertTo-Json -Compress)
}

function Resolve-CodexExecutable {
    if ($env:EPOCH_LOCAL_MCP_EXECUTABLE) {
        $candidate = [System.IO.Path]::GetFullPath($env:EPOCH_LOCAL_MCP_EXECUTABLE)
        if ([System.IO.File]::Exists($candidate)) { return $candidate }
        throw "EPOCH_LOCAL_MCP_EXECUTABLE does not name a file."
    }

    $command = Get-Command codex.exe -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) { return $command.Source }

    $binRoot = Join-Path $env:LOCALAPPDATA "OpenAI\Codex\bin"
    if ([System.IO.Directory]::Exists($binRoot)) {
        $candidate = Get-ChildItem -LiteralPath $binRoot -Directory |
            Sort-Object LastWriteTimeUtc -Descending |
            ForEach-Object { Join-Path $_.FullName "codex.exe" } |
            Where-Object { [System.IO.File]::Exists($_) } |
            Select-Object -First 1
        if ($candidate) { return $candidate }
    }
    throw "No Codex executable with the stdio MCP server was found."
}

function Send-Frame {
    param([hashtable] $Frame)
    $json = $Frame | ConvertTo-Json -Depth 16 -Compress
    $script:server.StandardInput.WriteLine($json)
    $script:server.StandardInput.Flush()
}

function Read-Frame {
    param([int] $ExpectedId)
    while ($true) {
        $remaining = [int]([Math]::Ceiling(
            $TimeoutSeconds * 1000 -
            ([DateTimeOffset]::UtcNow - $started).TotalMilliseconds))
        if ($remaining -le 0) { throw "Local MCP request timed out." }
        $read = $script:server.StandardOutput.ReadLineAsync()
        if (-not $read.Wait($remaining)) { throw "Local MCP request timed out." }
        $line = $read.Result
        if ($null -eq $line) { throw "Local MCP server closed stdout." }
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $frame = $line | ConvertFrom-Json
        if ($null -ne $frame.id -and [int]$frame.id -eq $ExpectedId) {
            if ($frame.error) {
                throw "Local MCP error $($frame.error.code): $($frame.error.message)"
            }
            return $frame
        }
    }
}

try {
    $promptFull = [System.IO.Path]::GetFullPath($PromptPath)
    $workspaceFull = [System.IO.Path]::GetFullPath($WorkspaceRoot)
    if (-not [System.IO.File]::Exists($promptFull)) { throw "Prompt file is missing." }
    if (-not [System.IO.Directory]::Exists($workspaceFull)) { throw "Sandbox workspace is missing." }
    $prompt = [System.IO.File]::ReadAllText($promptFull, [System.Text.Encoding]::UTF8)
    if ([string]::IsNullOrWhiteSpace($prompt)) { throw "Prompt is empty." }

    $codex = Resolve-CodexExecutable
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $codex
    $start.Arguments = "mcp-server"
    $start.WorkingDirectory = $workspaceFull
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    # Keep stderr inherited by the supervised bridge so the host's merged log
    # captures it without leaving an unread child pipe that can fill and stall.
    $start.RedirectStandardError = $false
    $server = [System.Diagnostics.Process]::new()
    $server.StartInfo = $start
    if (-not $server.Start()) { throw "Codex MCP server did not start." }
    $serverPid = $server.Id
    $phase = "initialize"
    Write-Status -State "running" -Detail "Local stdio MCP server started."

    Send-Frame @{
        jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{
            protocolVersion = "2025-06-18"; capabilities = @{}; clientInfo = @{
                name = "EpochCandidateLab"; version = "0.90.1"
            }
        }
    }
    $null = Read-Frame -ExpectedId 1
    Send-Frame @{ jsonrpc = "2.0"; method = "notifications/initialized"; params = @{} }

    $phase = "tools-list"
    Write-Status -State "running" -Detail "MCP initialized; verifying the Codex tool."
    Send-Frame @{ jsonrpc = "2.0"; id = 2; method = "tools/list"; params = @{} }
    $listed = Read-Frame -ExpectedId 2
    $toolNames = @($listed.result.tools | ForEach-Object { $_.name })
    # A Codex MCP thread belongs to the lifetime of one mcp-server process.
    # Candidate Lab therefore sends its durable plan/checkpoints in every new
    # bounded request instead of pretending a restarted server can resume it.
    $toolName = "codex"
    if ($toolNames -notcontains $toolName) { throw "MCP server does not expose $toolName." }

    $arguments = [ordered]@{
        prompt = $prompt
        cwd = $workspaceFull
        'approval-policy' = "never"
        sandbox = "workspace-write"
        'developer-instructions' =
            "Operate only inside the supplied disposable Candidate Lab workspace. Return the exact Epoch protocol packet requested by the prompt. Do not touch live source, Git, Site, releases, listeners, or other projects."
    }

    $phase = "model-request"
    Write-Status -State "running" -Detail "Bounded source request is running through local MCP."
    Send-Frame @{
        jsonrpc = "2.0"; id = 3; method = "tools/call"; params = @{
            name = $toolName; arguments = $arguments
        }
    }
    $reply = Read-Frame -ExpectedId 3
    if ($reply.result.isError) {
        $errorText = @($reply.result.content |
            Where-Object { $_.type -eq "text" } |
            Select-Object -First 1)
        $detail = if ($errorText.Count -gt 0) {
            [string]$errorText[0].text
        } else {
            "MCP tool returned an error result."
        }
        throw $detail
    }
    $structured = $reply.result.structuredContent
    $content = ""
    $returnedThread = $ThreadId
    if ($structured) {
        if ($structured.content -is [string]) { $content = $structured.content }
        if ($structured.threadId) { $returnedThread = [string]$structured.threadId }
    }
    if (-not $content) {
        $textBlock = @($reply.result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1)
        if ($textBlock.Count -gt 0) { $content = [string]$textBlock[0].text }
    }
    if ([string]::IsNullOrWhiteSpace($content)) { throw "MCP tool returned no text content." }

    Write-Utf8Atomic -Path $ResponsePath -Text $content
    Write-Utf8Atomic -Path $ThreadPath -Text $returnedThread
    $phase = "complete"
    $elapsed = [int64]([DateTimeOffset]::UtcNow - $started).TotalMilliseconds
    $receipt = [ordered]@{
        schema = "epoch-local-mcp-receipt/v1"
        status = "complete"
        bridge_pid = $PID
        server_pid = $serverPid
        elapsed_ms = $elapsed
        response_bytes = [System.Text.Encoding]::UTF8.GetByteCount($content)
        prompt_sha256 = (Get-FileHash -LiteralPath $promptFull -Algorithm SHA256).Hash.ToLowerInvariant()
        response_sha256 = (Get-FileHash -LiteralPath $ResponsePath -Algorithm SHA256).Hash.ToLowerInvariant()
        thread_id = $returnedThread
        previous_thread_id = $ThreadId
        executable_sha256 = (Get-FileHash -LiteralPath $codex -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Write-Utf8Atomic -Path $ReceiptPath -Text ($receipt | ConvertTo-Json -Compress)
    Write-Status -State "complete" -Detail "Response committed for host review."
    exit 0
}
catch {
    $phase = "failed"
    $message = $_.Exception.Message
    try {
        $receipt = [ordered]@{
            schema = "epoch-local-mcp-receipt/v1"
            status = "failed"
            bridge_pid = $PID
            server_pid = $serverPid
            elapsed_ms = [int64]([DateTimeOffset]::UtcNow - $started).TotalMilliseconds
            response_bytes = 0
            thread_id = $ThreadId
            error = $message
        }
        Write-Utf8Atomic -Path $ReceiptPath -Text ($receipt | ConvertTo-Json -Compress)
        Write-Status -State "failed" -Detail $message
    } catch {}
    exit 1
}
finally {
    if ($server -and -not $server.HasExited) {
        try { $server.Kill() } catch {}
        try { $server.WaitForExit(5000) | Out-Null } catch {}
    }
    if ($server) { $server.Dispose() }
}
