#requires -Version 5.1
<#
.SYNOPSIS
  Fetch XXMI and classic 3Dmigoto remotes (read-only tracking).

.DESCRIPTION
  Ensures the xxmi and 3dmigoto remotes exist, fetches latest refs, and
  optionally summarizes commits on xxmi/master that are not in HEAD.

.PARAMETER ShowNew
  After fetch, print commits reachable from xxmi/master but not HEAD.

.PARAMETER Include3Dmigoto
  Also fetch 3dmigoto/master (default: true).
#>
[CmdletBinding()]
param(
    [switch] $ShowNew,
    [bool] $Include3Dmigoto = $true
)

$ErrorActionPreference = 'Stop'

function Ensure-Remote {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [string] $Url
    )
    $existing = git remote get-url $Name 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($existing)) {
        Write-Host "Adding remote '$Name' -> $Url"
        git remote add $Name $Url
    }
    elseif ($existing -ne $Url) {
        Write-Warning "Remote '$Name' points to '$existing' (expected '$Url'). Leaving as-is."
    }
}

Push-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
try {
    Ensure-Remote -Name 'xxmi' -Url 'https://github.com/SpectrumQT/XXMI-Libs-Package.git'
    Ensure-Remote -Name '3dmigoto' -Url 'https://github.com/bo3b/3Dmigoto.git'

    Write-Host 'Fetching xxmi (with tags)...'
    git fetch xxmi --tags
    if ($LASTEXITCODE -ne 0) { throw 'git fetch xxmi failed' }

    if ($Include3Dmigoto) {
        Write-Host 'Fetching 3dmigoto master...'
        git fetch 3dmigoto master
        if ($LASTEXITCODE -ne 0) { throw 'git fetch 3dmigoto failed' }
    }

    Write-Host ''
    Write-Host 'Remotes:'
    git remote -v

    if ($ShowNew) {
        Write-Host ''
        Write-Host 'Commits on xxmi/master not in HEAD:'
        git log --oneline HEAD..xxmi/master
        if ($LASTEXITCODE -ne 0) {
            Write-Warning 'Could not compare HEAD..xxmi/master (merge base may be missing).'
        }
    }

    Write-Host ''
    Write-Host 'Done. Review with: git log --oneline HEAD..xxmi/master'
    Write-Host 'Cherry-pick with:  git cherry-pick <sha>'
}
finally {
    Pop-Location
}
