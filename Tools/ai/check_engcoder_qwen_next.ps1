# Read-only EngCoder/Qwen host verification. Does not download, load, or start models.
[CmdletBinding()]
param([int]$Port = 14321)

$ErrorActionPreference = 'Stop'
if ($Port -lt 1024 -or $Port -gt 65535) { throw 'Port must be 1024..65535.' }
$base = "http://127.0.0.1:$Port"
Write-Host "EngCoder local endpoint: $base"
try {
    $health = Invoke-RestMethod -Method Get -Uri "$base/api/health" -TimeoutSec 5
    if ($null -ne $health.version) { Write-Host "Reported backend version: $($health.version)" }
    else { Write-Warning 'Health returned no explicit backend version; verify EngCoder 0.7.1+ separately.' }
} catch {
    Write-Warning "Health endpoint unavailable: $($_.Exception.Message)"
}
try {
    $result = Invoke-RestMethod -Method Get -Uri "$base/v1/models" -TimeoutSec 10
    $entries = @($result.data)
    $ids = @($entries | ForEach-Object { $_.id } | Where-Object { $_ })
    if ($ids.Count -eq 0) {
        Write-Warning 'No models are advertised. Configure/load Qwen Flash Next in EngCoder, then retry.'
        exit 2
    }
    Write-Host "Advertised models ($($ids.Count)):"
    $ids | ForEach-Object { Write-Host "  $_" }
    $next = @($ids | Where-Object { $_ -match '(?i)qwen.*(?:flash.?next|next.?flash)|qwenflash://' })
    if ($next.Count -eq 0) {
        Write-Warning 'No Qwen Next Flash model ID found. Do not guess a model ID; use the actual /v1/models value.'
        exit 3
    }
    Write-Host "Qwen Next Flash model(s) advertised: $($next.Count)"
    Write-Host 'Epoch: AI Controls > Connect EngCoder > Rescan Models > select the advertised Qwen ID > Confirm Model.'
    Write-Host 'This checks discovery only. Successful inference requires one explicit generation request.'
} catch {
    Write-Error "Cannot query $base/v1/models : $($_.Exception.Message)"
    exit 1
}
