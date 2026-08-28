[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [string]$StageRoot = "",

    [string]$Slug,

    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    $repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
    return $repoRoot.Path
}

function Convert-ToSlug {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "research"
    }

    $slug = $Value.ToLowerInvariant() -replace '[^a-z0-9]+', '-'
    $slug = $slug.Trim('-')
    if ([string]::IsNullOrWhiteSpace($slug)) {
        return "research"
    }

    return $slug
}

function New-TempPythonFile {
    param([string]$Content)

    $tempPath = Join-Path ([System.IO.Path]::GetTempPath()) ("epoch_research_" + [System.Guid]::NewGuid().ToString("N") + ".py")
    Set-Content -LiteralPath $tempPath -Value $Content -Encoding UTF8
    return $tempPath
}

function Get-PythonCandidates {
    return @(
        @{ Command = "python"; ExtraArgs = @() },
        @{ Command = "py"; ExtraArgs = @("-3.14") },
        @{ Command = "py"; ExtraArgs = @("-3") },
        @{ Command = "py"; ExtraArgs = @() }
    )
}

function Resolve-PythonInvocation {
    param([string]$RequireModule)

    foreach ($candidate in Get-PythonCandidates) {
        $python = Get-Command $candidate.Command -ErrorAction SilentlyContinue
        if (-not $python) {
            continue
        }

        if ([string]::IsNullOrWhiteSpace($RequireModule)) {
            return @{
                Executable = $python.Source
                Args = $candidate.ExtraArgs
            }
        }

        $probeArgs = @($candidate.ExtraArgs + @(
                "-c",
                "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('$RequireModule') else 3)"
            ))
        & $python.Source @probeArgs | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return @{
                Executable = $python.Source
                Args = $candidate.ExtraArgs
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($RequireModule)) {
        throw "python or py is required for this extraction path."
    }

    throw "No local python or py launcher with module '$RequireModule' was found."
}

function Invoke-PythonScript {
    param(
        [string]$ScriptContent,
        [string[]]$Arguments,
        [string]$RequireModule
    )

    $tempScript = New-TempPythonFile -Content $ScriptContent
    try {
        $invocation = Resolve-PythonInvocation -RequireModule $RequireModule
        $scriptArgs = @($invocation.Args + @($tempScript) + $Arguments)
        $output = & $invocation.Executable @scriptArgs 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw (($output | ForEach-Object { "$_" }) -join [Environment]::NewLine)
        }

        return $output
    }
    finally {
        Remove-Item -LiteralPath $tempScript -Force -ErrorAction SilentlyContinue
    }
}

function Extract-HtmlText {
    param([string]$Path)

    $raw = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $raw = [System.Text.RegularExpressions.Regex]::Replace($raw, '<script\b[^>]*>.*?</script>', ' ', 'IgnoreCase, Singleline')
    $raw = [System.Text.RegularExpressions.Regex]::Replace($raw, '<style\b[^>]*>.*?</style>', ' ', 'IgnoreCase, Singleline')
    $raw = [System.Text.RegularExpressions.Regex]::Replace($raw, '<[^>]+>', ' ')
    $raw = [System.Net.WebUtility]::HtmlDecode($raw)
    $raw = [System.Text.RegularExpressions.Regex]::Replace($raw, '\s+', ' ').Trim()
    return $raw
}

function Extract-PdfText {
    param([string]$Path)

    $script = @'
from pathlib import Path
import sys

source = Path(sys.argv[1])

try:
    from pypdf import PdfReader
except Exception as exc:
    sys.stderr.write(f"pypdf import failed: {exc}\n")
    sys.exit(2)

reader = PdfReader(str(source))
parts = []
for page in reader.pages:
    try:
        text = page.extract_text() or ""
    except Exception:
        text = ""
    if text:
        parts.append(text)

sys.stdout.buffer.write("\n\n".join(parts).encode("utf-8", errors="replace"))
'@

    return Invoke-PythonScript -ScriptContent $script -Arguments @($Path) -RequireModule "pypdf"
}

$resolvedInput = Resolve-Path -LiteralPath $InputPath
$sourcePath = $resolvedInput.Path
$sourceItem = Get-Item -LiteralPath $sourcePath
$sourceName = $sourceItem.Name
$sourceExt = $sourceItem.Extension.ToLowerInvariant()
$sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant()

$repoRoot = Get-RepoRoot
$resolvedStageRoot = if ([string]::IsNullOrWhiteSpace($StageRoot)) {
    Join-Path $repoRoot "Engine/examples/EpochEditor/workspace/research/staged"
} elseif ([System.IO.Path]::IsPathRooted($StageRoot)) {
    $StageRoot
} else {
    Join-Path $repoRoot $StageRoot
}
$slugSource = if ([string]::IsNullOrWhiteSpace($Slug)) { $sourceItem.BaseName } else { $Slug }
$slugToken = Convert-ToSlug -Value $slugSource
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$stageDir = Join-Path $resolvedStageRoot ($timestamp + "-" + $slugToken)

if ((Test-Path -LiteralPath $stageDir) -and -not $Force) {
    throw "Stage directory already exists: $stageDir"
}

New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

$textKind = switch ($sourceExt) {
    ".pdf" { "pdf" }
    ".html" { "html" }
    ".htm" { "html" }
    ".txt" { "text" }
    ".md" { "markdown" }
    default { "text" }
}

$extractStatus = "ok"
$extractError = ""

try {
    $extractedText = switch ($textKind) {
        "pdf" { Extract-PdfText -Path $sourcePath }
        "html" { Extract-HtmlText -Path $sourcePath }
        default { Get-Content -LiteralPath $sourcePath -Raw }
    }
}
catch {
    $extractStatus = "error"
    $extractError = $_.Exception.Message
    $extractedText = @"
Extraction failed for:
$sourcePath

Reason:
$extractError

This staged import is still useful for provenance and review, but it needs a
working extractor before it should influence roadmap/docs/datasets/policy.
"@
}

$extractedText = ($extractedText -join [Environment]::NewLine).Trim()

$extractPath = Join-Path $stageDir "extracted.txt"
$reviewPath = Join-Path $stageDir "review.md"
$provenancePath = Join-Path $stageDir "provenance.json"

Set-Content -LiteralPath $extractPath -Value $extractedText -Encoding UTF8

$review = @(
    "# Research Review",
    "",
    "- Source path: $sourcePath",
    "- Source sha256: $sourceHash",
    "- Source kind: $textKind",
    "- Extraction status: $extractStatus",
    "- Promotion status: staged only",
    "- Default policy: do not rewrite roadmap, datasets, or automation policy until reviewed",
    "",
    "## Notes",
    "",
    "- Summarize what matters here.",
    "- Call out anything that belongs in the roadmap.",
    '- Note if the source is utility-only, such as `botface.html`, and should stay out of Epoch planning truth by default.',
    "",
    "## Promotion Targets",
    "",
    "- [ ] Roadmap",
    "- [ ] Docs",
    "- [ ] Dataset / schema",
    "- [ ] Automation policy"
) -join [Environment]::NewLine

Set-Content -LiteralPath $reviewPath -Value $review -Encoding UTF8

$provenance = [ordered]@{
    source_path = $sourcePath
    source_name = $sourceName
    source_kind = $textKind
    source_sha256 = $sourceHash
    extraction_status = $extractStatus
    extraction_error = $extractError
    imported_at = (Get-Date).ToUniversalTime().ToString("o")
    staged_dir = $stageDir
    extracted_text = $extractPath
    review_notes = $reviewPath
    promoted = $false
}

$provenance | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $provenancePath -Encoding UTF8

Write-Host "Staged research import:"
Write-Host "  Source    : $sourcePath"
Write-Host "  Stage dir : $stageDir"
Write-Host "  Extract   : $extractPath"
Write-Host "  Review    : $reviewPath"
Write-Host "  Provenance: $provenancePath"
