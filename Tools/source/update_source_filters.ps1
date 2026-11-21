param([switch]$Write)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot '../build/epoch_package_layout.ps1')
$edition = Get-EpochSourceEdition -RepoRoot $repo
$target = Join-Path $repo 'Engine/Engine.vcxitems.filters'
$original = [IO.File]::ReadAllText($target)
[xml]$doc = $original
$ns = [Xml.XmlNamespaceManager]::new($doc.NameTable)
$uri = 'http://schemas.microsoft.com/developer/msbuild/2003'
$ns.AddNamespace('m', $uri)
$filters = [Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
$guids = @{}
foreach ($node in $doc.SelectNodes('/m:Project/m:ItemGroup/m:Filter', $ns)) {
    $guids[$node.GetAttribute('Include')] = $node.UniqueIdentifier
}
foreach ($node in @($doc.SelectNodes('/m:Project/m:ItemGroup/*[@Include]', $ns))) {
    if ($node.LocalName -eq 'Filter') { $null = $node.ParentNode.RemoveChild($node); continue }
    $relative = $node.GetAttribute('Include').Replace('$(MSBuildThisFileDirectory)', '').Replace('/', '\')
    $parts = $relative.Split('\')
    $folder = switch ($parts[0]) {
        'src' { 'Source Files\' + (($parts | Select-Object -Skip 1 | Select-Object -SkipLast 1) -join '\') }
        'modules' {
            $category = if ($parts.Count -gt 2) { $parts[1..($parts.Count - 2)] -join '\' }
                        else { ($parts[-1] -split '\.')[0] }
            'Module Files\' + $category
        }
        'include' {
            if ($parts.Count -gt 2) { 'Public Headers\' + (($parts | Select-Object -Skip 1 | Select-Object -SkipLast 1) -join '\') }
            else { 'Public Headers\' + (($parts[-1] -split '\.')[0]) }
        }
        'resource' { 'Resource Files' }
        default { 'Build Metadata' }
    }
    if ($node.LocalName -eq 'ClInclude' -and $parts[0] -eq 'src') {
        $kind = if ([IO.Path]::GetExtension($relative) -eq '.inl') {
            'Implementation Files'
        } else {
            'Internal Headers'
        }
        $folder = $folder.Replace('Source Files', $kind)
    }
    $folder = $folder.TrimEnd('\')
    foreach ($child in @($node.SelectNodes('m:Filter', $ns))) { $null = $node.RemoveChild($child) }
    $entry = $doc.CreateElement('Filter', $uri); $entry.InnerText = $folder
    $null = $node.AppendChild($entry)
    $parent = $folder
    while ($parent) {
        $null = $filters.Add($parent)
        $index = $parent.LastIndexOf('\')
        $parent = if ($index -ge 0) { $parent.Substring(0, $index) } else { '' }
    }
}
foreach ($group in @($doc.SelectNodes('/m:Project/m:ItemGroup', $ns))) {
    if ($group.SelectNodes('*').Count -eq 0) { $null = $group.ParentNode.RemoveChild($group) }
}
$definitions = $doc.CreateElement('ItemGroup', $uri)
foreach ($folder in $filters) {
    $filter = $doc.CreateElement('Filter', $uri); $filter.SetAttribute('Include', $folder)
    $id = $doc.CreateElement('UniqueIdentifier', $uri)
    if ($guids.ContainsKey($folder)) { $id.InnerText = $guids[$folder] }
    else {
        $hash = [Security.Cryptography.SHA256]::Create()
        try { $digest = $hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($folder)) } finally { $hash.Dispose() }
        $id.InnerText = ([Guid]::new([byte[]]$digest[0..15])).ToString('B')
    }
    $null = $filter.AppendChild($id)
    # Keep edition-specific folder definitions inside the same projection boundary as their items.
    if ($edition.Directory -eq 'development' -and !$edition.OwnerRuntime -and
        $folder.EndsWith('\updater\owner', [StringComparison]::Ordinal)) {
        $region = 'owner_updater_' + (($folder -split '\\')[0] -replace ' ', '_').ToLowerInvariant() + '_filter'
        $prefix = ' EPOCH_' + 'OWNER_SOURCE_'
        $null = $definitions.AppendChild($doc.CreateComment($prefix + 'BEGIN: ' + $region + ' '))
        $null = $definitions.AppendChild($filter)
        $null = $definitions.AppendChild($doc.CreateComment($prefix + 'END: ' + $region + ' '))
    } else {
        $null = $definitions.AppendChild($filter)
    }
}
$firstGroup = $doc.SelectSingleNode('/m:Project/m:ItemGroup', $ns)
$null = $doc.DocumentElement.InsertBefore($definitions, $firstGroup)
$settings = [Xml.XmlWriterSettings]::new()
$settings.Indent = $true; $settings.IndentChars = '  '
$settings.NewLineChars = "`r`n"; $settings.NewLineHandling = 'Replace'
$settings.Encoding = [Text.UTF8Encoding]::new($false)
$stream = [IO.MemoryStream]::new(); $writer = [Xml.XmlWriter]::Create($stream, $settings)
$doc.Save($writer); $writer.Dispose()
$expected = [Text.Encoding]::UTF8.GetString($stream.ToArray()) + "`r`n"
$stream.Dispose()
# The source projection writes a single-quoted declaration and omits the final newline.
$normalized = $original.Replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="utf-8"?>')
if ($Write) {
    [IO.File]::WriteAllText($target, $expected, [Text.UTF8Encoding]::new($false))
    Write-Output "Updated source filters: $($filters.Count) ownership folders."
} elseif ($normalized.Replace("`r`n", "`n").TrimEnd("`n") -cne
          $expected.Replace("`r`n", "`n").TrimEnd("`n")) {
    throw 'Source filters differ from physical ownership. Run Tools/source/update_source_filters.ps1 -Write.'
} else { Write-Output 'Source filters match physical ownership.' }
