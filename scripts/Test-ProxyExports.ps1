[CmdletBinding()]
param(
    [Parameter()]
    [string]$DllPath = 'x64\Release\d3d11.dll'
)

$ErrorActionPreference = 'Stop'

function Find-Dumpbin {
    $command = Get-Command 'dumpbin.exe' -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'dumpbin.exe is not on PATH and vswhere.exe was not found.'
    }

    $installation = & $vswhere -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $installation) {
        throw 'No Visual Studio installation with the C++ x64 tools was found.'
    }

    $toolsRoot = Join-Path $installation 'VC\Tools\MSVC'
    $candidate = Get-ChildItem -LiteralPath $toolsRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object {
            Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe'
        } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1

    if (-not $candidate) {
        throw "dumpbin.exe was not found below $toolsRoot."
    }
    return $candidate
}

$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path
$contractPath = Join-Path $PSScriptRoot 'proxy-exports.json'
$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
$dumpbin = Find-Dumpbin
$output = (& $dumpbin /nologo /exports $resolvedDll) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin failed for $resolvedDll with exit code $LASTEXITCODE."
}

$exportPattern = '(?m)^\s*(?<ordinal>\d+)\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(?<name>[A-Za-z_][A-Za-z0-9_]*)\b'
$actual = @{}
foreach ($match in [regex]::Matches($output, $exportPattern)) {
    $actual[$match.Groups['name'].Value] = [int]$match.Groups['ordinal'].Value
}

$failures = [System.Collections.Generic.List[string]]::new()
foreach ($expected in $contract.exports) {
    if (-not $actual.ContainsKey($expected.name)) {
        $failures.Add("Missing export: $($expected.name)")
        continue
    }

    if ($null -ne $expected.ordinal -and $actual[$expected.name] -ne [int]$expected.ordinal) {
        $failures.Add(
            "Ordinal mismatch for $($expected.name): expected $($expected.ordinal), found $($actual[$expected.name])"
        )
    }
}

if ($failures.Count -ne 0) {
    $failures | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    throw "Proxy export contract failed with $($failures.Count) error(s)."
}

$expectedNames = @($contract.exports | ForEach-Object name)
$additional = @($actual.Keys | Where-Object { $_ -notin $expectedNames } | Sort-Object)
Write-Host "Proxy export contract passed: $($contract.exports.Count) required exports."
if ($additional.Count -ne 0) {
    Write-Host "Additional exports (allowed): $($additional -join ', ')"
}
