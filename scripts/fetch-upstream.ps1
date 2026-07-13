#requires -Version 5.1
<#
.SYNOPSIS
  Fetch XXMI / 3Dmigoto remotes and optionally advance the xxmi-base branch.

.DESCRIPTION
  Ensures the xxmi and 3dmigoto remotes exist, fetches latest refs, optionally
  fast-forwards local xxmi-base to xxmi/master, and can summarize commits on
  xxmi-base (or xxmi/master) that are not yet in develop.

.PARAMETER ShowNew
  After fetch, print commits on xxmi-base not in develop (falls back to
  xxmi/master vs HEAD if develop is missing).

.PARAMETER UpdateBase
  Fast-forward local branch xxmi-base to match xxmi/master.

.PARAMETER PushBase
  After a successful UpdateBase, push xxmi-base to origin.

.PARAMETER Include3Dmigoto
  Also fetch 3dmigoto/master (default: true).
#>
[CmdletBinding()]
param(
    [switch] $ShowNew,
    [switch] $UpdateBase,
    [switch] $PushBase,
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
        git remote set-url --push $Name no_push
    }
    elseif ($existing -ne $Url) {
        Write-Warning "Remote '$Name' points to '$existing' (expected '$Url'). Leaving as-is."
    }
}

function Test-LocalBranch {
    param([Parameter(Mandatory)] [string] $Name)
    git show-ref --verify --quiet "refs/heads/$Name"
    return ($LASTEXITCODE -eq 0)
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

    if ($UpdateBase) {
        if (-not (Test-LocalBranch 'xxmi-base')) {
            Write-Host 'Creating local xxmi-base from xxmi/master...'
            git branch xxmi-base xxmi/master
            if ($LASTEXITCODE -ne 0) { throw 'failed to create xxmi-base' }
        }
        else {
            Write-Host 'Fast-forwarding xxmi-base to xxmi/master...'
            # Update ref without checking out, so the user stays on their branch.
            git fetch . xxmi/master:xxmi-base
            if ($LASTEXITCODE -ne 0) {
                throw @'
Failed to fast-forward xxmi-base.
xxmi-base may contain local commits. Keep it as a pure XXMI mirror:
  git switch xxmi-base
  git reset --hard xxmi/master   # only if you intend to discard local commits on xxmi-base
'@
            }
        }
        Write-Host ("xxmi-base is now at {0}" -f (git rev-parse --short xxmi-base))

        if ($PushBase) {
            Write-Host 'Pushing xxmi-base to origin...'
            git push -u origin xxmi-base
            if ($LASTEXITCODE -ne 0) { throw 'git push origin xxmi-base failed' }
        }
    }

    Write-Host ''
    Write-Host 'Remotes:'
    git remote -v

    if ($ShowNew) {
        Write-Host ''
        if (Test-LocalBranch 'develop') {
            $from = if (Test-LocalBranch 'xxmi-base') { 'xxmi-base' } else { 'xxmi/master' }
            Write-Host "Commits on $from not in develop:"
            git log --oneline "develop..$from"
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "Could not compare develop..$from"
            }
        }
        else {
            Write-Host 'Commits on xxmi/master not in HEAD:'
            git log --oneline HEAD..xxmi/master
        }
    }

    Write-Host ''
    Write-Host 'Done.'
    Write-Host '  Review:      git log --oneline develop..xxmi-base'
    Write-Host '  Cherry-pick: git switch develop; git cherry-pick <sha>'
    Write-Host '  Release:     git switch main; git merge --no-ff develop'
}
finally {
    Pop-Location
}
