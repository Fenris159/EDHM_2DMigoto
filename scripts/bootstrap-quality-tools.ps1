[CmdletBinding()]
param(
    [switch]$SkipLlvm
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qualityRoot = Join-Path $repoRoot '.quality'
$downloadRoot = Join-Path $qualityRoot 'downloads'
$toolRoot = Join-Path $qualityRoot 'tools'
$moduleRoot = Join-Path $qualityRoot 'modules'

New-Item -ItemType Directory -Force -Path $downloadRoot, $toolRoot, $moduleRoot | Out-Null

$pssaVersion = '1.25.0'
$pssaManifest = Join-Path $moduleRoot "PSScriptAnalyzer/$pssaVersion/PSScriptAnalyzer.psd1"
if (-not (Test-Path -LiteralPath $pssaManifest)) {
    Save-Module -Name PSScriptAnalyzer -RequiredVersion $pssaVersion -Path $moduleRoot -Repository PSGallery
}

if (-not $SkipLlvm) {
    $llvmVersion = '23.1.1'
    $llvmRoot = Join-Path $toolRoot "llvm-$llvmVersion"
    $clangFormat = Get-ChildItem -LiteralPath $llvmRoot -Filter clang-format.exe -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not $clangFormat) {
        $msi = Join-Path $downloadRoot "LLVM-$llvmVersion-win64.msi"
        $uri = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$llvmVersion/LLVM-$llvmVersion-win64.msi"
        $expectedHash = '11AF43BA261A1158090DD6F0DA40D7A34F32DBFB4A76B43240BFFA1EBB140984'

        if (-not (Test-Path -LiteralPath $msi)) {
            Invoke-WebRequest -Uri $uri -OutFile $msi
        }

        $actualHash = (Get-FileHash -LiteralPath $msi -Algorithm SHA256).Hash
        if ($actualHash -ne $expectedHash) {
            throw "LLVM package hash mismatch. Expected $expectedHash, got $actualHash."
        }

        New-Item -ItemType Directory -Force -Path $llvmRoot | Out-Null
        $process = Start-Process -FilePath msiexec.exe -ArgumentList @(
            '/a',
            $msi,
            '/qn',
            "TARGETDIR=$llvmRoot"
        ) -Wait -PassThru -WindowStyle Hidden

        if ($process.ExitCode -ne 0) {
            throw "LLVM administrative extraction failed with exit code $($process.ExitCode)."
        }
    }
}

Write-Host "Quality tools are available under $qualityRoot"
