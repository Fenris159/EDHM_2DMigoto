[CmdletBinding()]
param(
    [string]$Branch = '',
    [switch]$AllowMain,
    [switch]$RunClangTidy
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qualityRoot = Join-Path $repoRoot '.quality'
$downloadRoot = Join-Path $qualityRoot 'downloads'
$toolRoot = Join-Path $qualityRoot 'tools'
$wrapperRoot = Join-Path $toolRoot 'sonar-build-wrapper'
$scannerVersion = '8.1.0.6389'
$scannerRoot = Join-Path $toolRoot "sonar-scanner-$scannerVersion-windows-x64"
$outputRoot = Join-Path $qualityRoot 'bw-output'

function Remove-SafeDirectory {
    [CmdletBinding(SupportsShouldProcess)]
    param([Parameter(Mandatory)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRepo = [IO.Path]::GetFullPath($repoRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($fullRepo, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside the repository: $fullPath"
    }
    if ((Test-Path -LiteralPath $fullPath) -and $PSCmdlet.ShouldProcess($fullPath, 'Remove directory recursively')) {
        Remove-Item -LiteralPath $fullPath -Recurse -Force
    }
}

function Resolve-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $knownCandidates = @(
        'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe',
        'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe',
        'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe',
        'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe'
    )
    foreach ($candidate in $knownCandidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }
    foreach ($root in $roots) {
        $vswhere = Join-Path $root 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            continue
        }
        $installation = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($installation) {
            $candidate = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    throw 'MSBuild was not found. Install the Visual Studio C++ desktop workload with the v143 toolset.'
}

if (-not $env:SONAR_TOKEN) {
    throw 'Set SONAR_TOKEN in the current process before running a SonarCloud analysis.'
}

if (-not $Branch) {
    $Branch = (& git -C $repoRoot branch --show-current).Trim()
}
if ($Branch -eq 'main' -and -not $AllowMain) {
    throw 'Local analysis of main is blocked by default. Supply a dedicated branch name or use -AllowMain deliberately.'
}

New-Item -ItemType Directory -Force -Path $downloadRoot, $toolRoot | Out-Null

$wrapper = Get-ChildItem -LiteralPath $wrapperRoot -Filter build-wrapper-win-x86-64.exe -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $wrapper) {
    $archive = Join-Path $downloadRoot 'sonar-build-wrapper-win-x86.zip'
    Invoke-WebRequest -Uri 'https://sonarcloud.io/static/cpp/build-wrapper-win-x86.zip' -OutFile $archive
    Remove-SafeDirectory -Path $wrapperRoot
    Expand-Archive -LiteralPath $archive -DestinationPath $wrapperRoot
    $wrapper = Get-ChildItem -LiteralPath $wrapperRoot -Filter build-wrapper-win-x86-64.exe -File -Recurse |
        Select-Object -First 1
}

$scanner = Get-ChildItem -LiteralPath $scannerRoot -Filter sonar-scanner.bat -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $scanner) {
    $archive = Join-Path $downloadRoot "sonar-scanner-cli-$scannerVersion-windows-x64.zip"
    $uri = "https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$scannerVersion-windows-x64.zip"
    Invoke-WebRequest -Uri $uri -OutFile $archive
    Remove-SafeDirectory -Path $scannerRoot
    Expand-Archive -LiteralPath $archive -DestinationPath $toolRoot
    $scanner = Get-ChildItem -LiteralPath $scannerRoot -Filter sonar-scanner.bat -File -Recurse |
        Select-Object -First 1
}

Remove-SafeDirectory -Path $outputRoot
$msbuild = Resolve-MSBuild

Push-Location $repoRoot
try {
    & $wrapper.FullName --out-dir $outputRoot `
        $msbuild 'EDHM_2DMigoto.sln' `
        -target:Rebuild `
        -property:Configuration=Release `
        -property:Platform=x64 `
        -property:EdhmStrictBuild=true `
        -nodeReuse:false `
        -verbosity:minimal `
        -restore:false
    if ($LASTEXITCODE -ne 0) {
        throw "Wrapped MSBuild failed with exit code $LASTEXITCODE."
    }

    if ($RunClangTidy) {
        & (Join-Path $PSScriptRoot 'quality.ps1') -Mode ClangTidy -Scope All `
            -CompileDatabase '.quality/bw-output/compile_commands.json'
    }

    & $scanner.FullName `
        '-Dsonar.host.url=https://sonarcloud.io' `
        "-Dsonar.branch.name=$Branch" `
        '-Dsonar.cfamily.compile-commands=.quality/bw-output/compile_commands.json'
    if ($LASTEXITCODE -ne 0) {
        throw "SonarScanner failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
