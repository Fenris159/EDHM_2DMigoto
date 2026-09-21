#requires -Version 5.1
<#
.SYNOPSIS
  Read / bump VERSION and compute release or pre-release tags (SemVer).

.EXAMPLE
  .\scripts\version.ps1 -Action get
  .\scripts\version.ps1 -Action bump-patch
  .\scripts\version.ps1 -Action tag -Channel release
  .\scripts\version.ps1 -Action tag -Channel prerelease -PreLabel beta
#>
[CmdletBinding()]
param(
    [ValidateSet('get', 'bump-major', 'bump-minor', 'bump-patch', 'bump-prerelease', 'tag', 'sync-notes')]
    [string] $Action = 'get',

    [ValidateSet('release', 'prerelease')]
    [string] $Channel = 'release',

    [ValidateSet('alpha', 'beta', 'rc')]
    [string] $PreLabel = 'beta',

    [string] $RepoRoot = ''
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
$versionFile = Join-Path $RepoRoot 'VERSION'
$changelogFile = Join-Path $RepoRoot 'CHANGELOG.md'
$notesFile = Join-Path $RepoRoot 'CURRENT_RELEASE_NOTES.md'

function Read-VersionRaw {
    if (-not (Test-Path $versionFile)) { throw "VERSION file not found: $versionFile" }
    return (Get-Content $versionFile -Raw).Trim()
}

function Parse-SemVer {
    param([Parameter(Mandatory)][string] $Raw)
    # X.Y.Z or X.Y.Z-label.N
    if ($Raw -notmatch '^(?<maj>\d+)\.(?<min>\d+)\.(?<pat>\d+)(?:-(?<pre>[a-zA-Z]+)\.(?<pren>\d+))?$') {
        throw "VERSION '$Raw' is not valid SemVer for this project (expected X.Y.Z or X.Y.Z-label.N)"
    }
    return [pscustomobject]@{
        Major        = [int]$Matches['maj']
        Minor        = [int]$Matches['min']
        Patch        = [int]$Matches['pat']
        PreLabel     = $Matches['pre']
        PreNumber    = if ($Matches['pren']) { [int]$Matches['pren'] } else { $null }
        Core         = "$($Matches['maj']).$($Matches['min']).$($Matches['pat'])"
        Raw          = $Raw
        IsPrerelease = [bool]$Matches['pre']
    }
}

function Format-SemVer {
    param($V, [string] $ForcePreLabel, [nullable[int]] $ForcePreNumber)
    $core = "$($V.Major).$($V.Minor).$($V.Patch)"
    $label = if ($ForcePreLabel) { $ForcePreLabel } else { $V.PreLabel }
    $num = if ($null -ne $ForcePreNumber) { $ForcePreNumber } else { $V.PreNumber }
    if ($label -and $null -ne $num) { return "$core-$label.$num" }
    return $core
}

function Write-Version {
    param([string] $Value)
    Set-Content -Path $versionFile -Value ($Value.Trim() + "`n") -NoNewline
}

function Find-ChangelogSectionStart {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string[]] $Lines,
        [Parameter(Mandatory)] [string] $VersionCore
    )
    $versionHeading = "^## \[$([regex]::Escape($VersionCore))\]"
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match $versionHeading) { return $i }
    }
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '^## \[Unreleased\]') { return $i }
    }
    return -1
}

function Find-ChangelogSectionEnd {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string[]] $Lines,
        [Parameter(Mandatory)] [int] $Start
    )
    for ($i = $Start + 1; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '^## \[' -or $Lines[$i] -match '^\[[^\]]+\]:\s*https?://') {
            return $i
        }
    }
    return $Lines.Count
}

function Sync-ReleaseNotes {
    param([string] $VersionCore)
    if (-not (Test-Path $changelogFile)) {
        Write-Warning "CHANGELOG.md missing; leaving CURRENT_RELEASE_NOTES.md unchanged"
        return
    }
    $lines = Get-Content $changelogFile
    $start = Find-ChangelogSectionStart -Lines $lines -VersionCore $VersionCore
    if ($start -lt 0) {
        Write-Warning "Could not find changelog section for $VersionCore"
        return
    }
    $end = Find-ChangelogSectionEnd -Lines $lines -Start $start
    $body = $lines[$start..($end - 1)] -join "`n"
    $header = @"
## EDHM_2DMigoto $VersionCore

Package build for Elite Dangerous HUD Mod (EDHM).
Built from this repository's automated release workflow.

"@
    $footer = @"

---
Full project history: [CHANGELOG.md](https://github.com/Fenris159/EDHM_2DMigoto/blob/HEAD/CHANGELOG.md)
"@
    Set-Content -Path $notesFile -Value ($header + $body.Trim() + "`n" + $footer.Trim() + "`n") -NoNewline
    Write-Output "Updated CURRENT_RELEASE_NOTES.md from CHANGELOG section [$VersionCore]"
}

$raw = Read-VersionRaw
$v = Parse-SemVer $raw

switch ($Action) {
    'get' {
        Write-Output $v.Raw
    }
    'bump-major' {
        $v.Major++; $v.Minor = 0; $v.Patch = 0; $v.PreLabel = $null; $v.PreNumber = $null
        $out = Format-SemVer $v
        Write-Version $out
        Write-Output $out
    }
    'bump-minor' {
        $v.Minor++; $v.Patch = 0; $v.PreLabel = $null; $v.PreNumber = $null
        $out = Format-SemVer $v
        Write-Version $out
        Write-Output $out
    }
    'bump-patch' {
        $v.Patch++; $v.PreLabel = $null; $v.PreNumber = $null
        $out = Format-SemVer $v
        Write-Version $out
        Write-Output $out
    }
    'bump-prerelease' {
        if (-not $v.IsPrerelease -or $v.PreLabel -ne $PreLabel) {
            # Start or reset pre-release series on current core
            $out = Format-SemVer $v -ForcePreLabel $PreLabel -ForcePreNumber 1
        }
        else {
            $out = Format-SemVer $v -ForcePreLabel $PreLabel -ForcePreNumber ($v.PreNumber + 1)
        }
        Write-Version $out
        Write-Output $out
    }
    'tag' {
        if ($Channel -eq 'release') {
            # Release tags are always clean X.Y.Z
            $tagVer = $v.Core
            if ($v.IsPrerelease) {
                Write-Warning "VERSION is pre-release ($($v.Raw)); release tag will use core $tagVer"
            }
            $tag = "v$tagVer"
            Write-Output $tag
            # Also export core for notes
            if ($env:GITHUB_OUTPUT) {
                "version=$tagVer" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
                "tag=$tag" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
                "prerelease=false" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
            }
        }
        else {
            if (-not $v.IsPrerelease) {
                $tagVer = Format-SemVer $v -ForcePreLabel $PreLabel -ForcePreNumber 1
            }
            else {
                $tagVer = $v.Raw
            }
            $tag = "v$tagVer"
            Write-Output $tag
            if ($env:GITHUB_OUTPUT) {
                "version=$tagVer" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
                "tag=$tag" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
                "prerelease=true" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
            }
        }
    }
    'sync-notes' {
        # Prefer exact version heading (e.g. 0.1.0-beta.1), else core 0.1.0 / Unreleased
        Sync-ReleaseNotes -VersionCore $v.Raw
        $notes = if (Test-Path $notesFile) { Get-Content $notesFile -Raw } else { '' }
        if ($notes -notmatch [regex]::Escape($v.Raw) -and $v.Raw -ne $v.Core) {
            Sync-ReleaseNotes -VersionCore $v.Core
        }
    }
}
