#requires -Version 5.1
<#
.SYNOPSIS
  Populate vendor/edhm-runtime from a built d3d11.dll and release metadata.

.EXAMPLE
  .\scripts\update-vendor-cache.ps1 -DllPath dist\d3d11.dll -Version 0.1.0-beta.1 -Tag v0.1.0-beta.1 -Channel prerelease
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $DllPath,

    [Parameter(Mandatory)]
    [string] $Version,

    [Parameter(Mandatory)]
    [string] $Tag,

    [ValidateSet('release', 'prerelease')]
    [string] $Channel = 'prerelease',

    [string] $RepoRoot = '',

    [string] $ReleaseUrl = ''
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) {
    if ($PSScriptRoot) {
        $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    } else {
        $RepoRoot = (Get-Location).Path
    }
}

if (-not (Test-Path $DllPath)) {
    throw "DLL not found: $DllPath"
}

$vendor = Join-Path $RepoRoot 'vendor\edhm-runtime'
New-Item -ItemType Directory -Force -Path $vendor | Out-Null

$destDll = Join-Path $vendor 'd3d11.dll'
Copy-Item -LiteralPath $DllPath -Destination $destDll -Force

$hash = (Get-FileHash -LiteralPath $destDll -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -Path (Join-Path $vendor 'd3d11.dll.sha256') -Value "$hash  d3d11.dll`n" -NoNewline -Encoding ascii

$ver = $Version.Trim().TrimStart('v')
$tag = $Tag.Trim()
if ($tag -notmatch '^v') { $tag = "v$tag" }

Set-Content -Path (Join-Path $vendor 'VERSION') -Value ($ver + "`n") -NoNewline
Set-Content -Path (Join-Path $vendor 'SOURCE_TAG') -Value ($tag + "`n") -NoNewline
Set-Content -Path (Join-Path $vendor 'CHANNEL') -Value ($Channel + "`n") -NoNewline

if (-not $ReleaseUrl) {
    $ReleaseUrl = "https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/$tag"
}
Set-Content -Path (Join-Path $vendor 'RELEASE_URL') -Value ($ReleaseUrl + "`n") -NoNewline

Write-Host "Vendor cache updated:"
Write-Host "  VERSION     = $ver"
Write-Host "  SOURCE_TAG  = $tag"
Write-Host "  CHANNEL     = $Channel"
Write-Host "  SHA256      = $hash"
Write-Host "  RELEASE_URL = $ReleaseUrl"
Get-Item $destDll | Format-List FullName, Length
