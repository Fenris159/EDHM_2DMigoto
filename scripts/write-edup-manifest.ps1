#requires -Version 5.1
<#
.SYNOPSIS
  Write a version-pinned download manifest for EDUP's renamed EDHM runtime.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $DllPath,
    [Parameter(Mandatory)] [string] $Version,
    [Parameter(Mandatory)] [string] $Tag,
    [Parameter(Mandatory)] [ValidateSet('release', 'prerelease')] [string] $Channel,
    [Parameter(Mandatory)] [string] $ReleaseUrl,
    [Parameter(Mandatory)] [string] $OutFile
)

$ErrorActionPreference = 'Stop'
$binary = Get-Item -LiteralPath $DllPath
if ($binary.PSIsContainer) { throw "Expected a DLL file: $DllPath" }
$versionText = $Version.Trim().TrimStart('v')
$tagText = $Tag.Trim()
if ($tagText -notmatch '^v') { $tagText = "v$tagText" }
if ($tagText -ne "v$versionText") { throw 'Manifest version and tag must match' }
$marker = '/releases/tag/'
$markerIndex = $ReleaseUrl.LastIndexOf($marker, [StringComparison]::Ordinal)
if ($markerIndex -lt 0) { throw 'ReleaseUrl must name a GitHub release tag' }
$repositoryUrl = $ReleaseUrl.Substring(0, $markerIndex)
$assetBase = "$repositoryUrl/releases/download/$tagText"
$hash = (Get-FileHash -LiteralPath $binary.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    schema_version = 1
    version        = $versionText
    source_tag     = $tagText
    channel        = $Channel
    architecture   = 'x64'
    file_name      = 'd3d11_edhm.dll'
    size_bytes     = $binary.Length
    sha256         = $hash
    download_url   = "$assetBase/d3d11_edhm.dll"
    checksum_url   = "$assetBase/d3d11_edhm.dll.sha256"
    release_url    = $ReleaseUrl
    source_url     = "$repositoryUrl/tree/$tagText"
}
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutFile)
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($destination)) | Out-Null
$json = ($manifest | ConvertTo-Json -Depth 4) -replace "`r`n", "`n"
[System.IO.File]::WriteAllText($destination, $json + "`n", [System.Text.UTF8Encoding]::new($false))
Write-Host "EDUP manifest written: $destination ($versionText, $hash)"
