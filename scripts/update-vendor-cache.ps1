#requires -Version 5.1
<#
.SYNOPSIS
  Populate EDHM and EDUP vendor caches from one built DLL and release metadata.

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
    }
    else {
        $RepoRoot = (Get-Location).Path
    }
}

if (-not (Test-Path $DllPath)) {
    throw "DLL not found: $DllPath"
}

$ver = $Version.Trim().TrimStart('v')
$tag = $Tag.Trim()
if ($tag -notmatch '^v') { $tag = "v$tag" }
if (-not $ReleaseUrl) {
    $ReleaseUrl = "https://github.com/Fenris159/EDHM_2DMigoto/releases/tag/$tag"
}
$sourceHash = (Get-FileHash -LiteralPath $DllPath -Algorithm SHA256).Hash.ToLowerInvariant()
$caches = @(
    @{ Directory = 'edhm-runtime'; FileName = 'd3d11.dll' },
    @{ Directory = 'edup-runtime'; FileName = 'd3d11_edhm.dll' }
)
$stagedManifest = [System.IO.Path]::GetTempFileName()
try {
    # Validate all manifest metadata before replacing either cache.
    & (Join-Path $PSScriptRoot 'write-edup-manifest.ps1') `
        -DllPath $DllPath -Version $ver -Tag $tag -Channel $Channel `
        -ReleaseUrl $ReleaseUrl -OutFile $stagedManifest
    foreach ($cache in $caches) {
        $vendor = Join-Path (Join-Path $RepoRoot 'vendor') $cache.Directory
        New-Item -ItemType Directory -Force -Path $vendor | Out-Null
        $destDll = Join-Path $vendor $cache.FileName
        Copy-Item -LiteralPath $DllPath -Destination $destDll -Force
        $hash = (Get-FileHash -LiteralPath $destDll -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -ne $sourceHash) { throw "Vendor DLL differs from the built runtime: $destDll" }
        Set-Content -Path (Join-Path $vendor "$($cache.FileName).sha256") -Value "$hash  $($cache.FileName)`n" -NoNewline -Encoding ascii
        Set-Content -Path (Join-Path $vendor 'VERSION') -Value ($ver + "`n") -NoNewline
        Set-Content -Path (Join-Path $vendor 'SOURCE_TAG') -Value ($tag + "`n") -NoNewline
        Set-Content -Path (Join-Path $vendor 'CHANNEL') -Value ($Channel + "`n") -NoNewline
        Set-Content -Path (Join-Path $vendor 'RELEASE_URL') -Value ($ReleaseUrl + "`n") -NoNewline
        if ($cache.Directory -eq 'edup-runtime') {
            Copy-Item -LiteralPath $stagedManifest -Destination (Join-Path $vendor 'manifest.json') -Force
        }
        Write-Host "Vendor cache updated: $($cache.Directory) $ver ($hash)"
        Get-Item $destDll | Format-List FullName, Length
    }
}
finally {
    Remove-Item -LiteralPath $stagedManifest -Force -ErrorAction SilentlyContinue
}
