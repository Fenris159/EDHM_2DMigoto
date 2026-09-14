[CmdletBinding()]
param(
    [ValidateSet('FormatCheck', 'Format', 'PowerShell', 'Build', 'ClangTidy', 'All')]
    [string]$Mode = 'All',

    [ValidateSet('Changed', 'All')]
    [string]$Scope = 'Changed',

    [string]$BaseRef = 'origin/main',

    [string]$CompileDatabase = '',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$null = $Scope, $BaseRef, $Configuration

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qualityRoot = Join-Path $repoRoot '.quality'
$moduleRoot = Join-Path $qualityRoot 'modules'
$env:PSModulePath = "$moduleRoot$([IO.Path]::PathSeparator)$env:PSModulePath"

function Resolve-Executable {
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @()
    if ($env:LLVM_ROOT) {
        $candidates += Join-Path $env:LLVM_ROOT "bin/$Name.exe"
    }
    if ($env:ProgramFiles) {
        $candidates += Join-Path $env:ProgramFiles "LLVM/bin/$Name.exe"
    }
    if (${env:ProgramFiles(x86)}) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} "LLVM/bin/$Name.exe"
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    if (Test-Path -LiteralPath (Join-Path $qualityRoot 'tools')) {
        $local = Get-ChildItem -LiteralPath (Join-Path $qualityRoot 'tools') -Filter "$Name.exe" -File -Recurse |
            Select-Object -First 1
        if ($local) {
            return $local.FullName
        }
    }

    throw "$Name was not found. Run scripts/bootstrap-quality-tools.ps1 first."
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

    $vswhereCandidates = @()
    if (${env:ProgramFiles(x86)}) {
        $vswhereCandidates += Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    }
    if ($env:ProgramFiles) {
        $vswhereCandidates += Join-Path $env:ProgramFiles 'Microsoft Visual Studio/Installer/vswhere.exe'
    }

    foreach ($vswhere in $vswhereCandidates) {
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

function Test-IsExcludedPath {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $normalized = $Path.Replace('\', '/').TrimStart('./')
    $excludedPrefixes = @(
        'DirectXTK/Src/Shaders/Compiled/',
        'vendor/',
        'pcre2/',
        'Nektra/',
        'crc32c-hw-1.0.5/'
    )

    foreach ($prefix in $excludedPrefixes) {
        if ($normalized.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    return $false
}

function Get-QualityFiles {
    param(
        [Parameter(Mandatory)]
        [string[]]$Extensions
    )

    Push-Location $repoRoot
    try {
        if ($Scope -eq 'All') {
            $paths = @(& git ls-files)
        }
        else {
            $paths = @(& git diff --name-only --diff-filter=ACMR "$BaseRef...HEAD" 2>$null)
            if ($LASTEXITCODE -ne 0) {
                $paths = @(& git diff --name-only --diff-filter=ACMR 'HEAD^' 'HEAD')
            }
            $paths += @(& git diff --name-only --diff-filter=ACMR)
            $paths += @(& git ls-files --others --exclude-standard)
        }
    }
    finally {
        Pop-Location
    }

    $extensionSet = @{}
    foreach ($extension in $Extensions) {
        $extensionSet[$extension.ToLowerInvariant()] = $true
    }

    return @($paths |
            Where-Object { $_ -and -not (Test-IsExcludedPath $_) } |
            Sort-Object -Unique |
            Where-Object { $extensionSet.ContainsKey([IO.Path]::GetExtension($_).ToLowerInvariant()) } |
            ForEach-Object { Join-Path $repoRoot $_ } |
            Where-Object { Test-Path -LiteralPath $_ })
}

function Invoke-Format {
    param([switch]$Apply)

    $files = @(Get-QualityFiles -Extensions @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl'))
    if ($files.Count -eq 0) {
        Write-Host 'No maintained native files selected for formatting.'
        return
    }

    $clangFormat = Resolve-Executable -Name 'clang-format'
    if ($Apply) {
        & $clangFormat -i --style=file @files
    }
    else {
        & $clangFormat --dry-run --Werror --style=file @files
    }

    if ($LASTEXITCODE -ne 0) {
        throw "clang-format failed with exit code $LASTEXITCODE."
    }
}

function Invoke-PowerShellAnalysis {
    $requiredVersion = '1.25.0'
    $module = Get-Module -ListAvailable PSScriptAnalyzer |
        Where-Object Version -EQ ([Version]$requiredVersion) |
        Select-Object -First 1
    if (-not $module) {
        throw "PSScriptAnalyzer $requiredVersion was not found. Run scripts/bootstrap-quality-tools.ps1 first."
    }

    Import-Module PSScriptAnalyzer -RequiredVersion $requiredVersion -Force
    $files = @(Get-QualityFiles -Extensions @('.ps1', '.psm1', '.psd1'))
    if ($files.Count -eq 0) {
        Write-Host 'No PowerShell files selected for analysis.'
        return
    }

    $settings = Join-Path $repoRoot 'PSScriptAnalyzerSettings.psd1'
    $findings = @($files | ForEach-Object { Invoke-ScriptAnalyzer -Path $_ -Settings $settings })
    if ($findings.Count -gt 0) {
        $findings | Sort-Object ScriptName, Line, Column | Format-Table -AutoSize
        throw "PSScriptAnalyzer reported $($findings.Count) finding(s)."
    }
}

function Invoke-NativeBuild {
    $msbuild = Resolve-MSBuild
    foreach ($platform in @('x64', 'Win32')) {
        & $msbuild (Join-Path $repoRoot 'EDHM_2DMigoto.sln') `
            -target:Rebuild `
            -property:Configuration=$Configuration `
            -property:Platform=$platform `
            -property:EdhmStrictBuild=true `
            -nodeReuse:false `
            -verbosity:minimal `
            -restore:false

        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed for $Configuration|$platform with exit code $LASTEXITCODE."
        }
    }
}

function Invoke-ClangTidyAnalysis {
    if (-not $CompileDatabase) {
        throw 'ClangTidy mode requires -CompileDatabase pointing to compile_commands.json.'
    }

    $databasePath = [IO.Path]::GetFullPath((Join-Path $repoRoot $CompileDatabase))
    if (-not (Test-Path -LiteralPath $databasePath)) {
        throw "Compilation database not found: $databasePath"
    }

    $database = Get-Content -LiteralPath $databasePath -Raw | ConvertFrom-Json
    if ($database.Count -gt 0 -and $database[0].PSObject.Properties.Name -contains 'arguments') {
        $normalizedDatabase = Join-Path $qualityRoot 'compile_commands/compile_commands.json'
        & (Join-Path $PSScriptRoot 'normalize-compile-commands.ps1') `
            -InputPath $databasePath `
            -OutputPath $normalizedDatabase
        $databasePath = $normalizedDatabase
    }

    $clangTidy = Resolve-Executable -Name 'clang-tidy'
    $selected = @(Get-QualityFiles -Extensions @('.c', '.cc', '.cpp', '.cxx'))
    $selectedSet = @{}
    foreach ($file in $selected) {
        $selectedSet[[IO.Path]::GetFullPath($file).ToLowerInvariant()] = $true
    }

    $database = Get-Content -Raw -LiteralPath $databasePath | ConvertFrom-Json
    $files = @($database | ForEach-Object {
            $path = $_.file
            if (-not [IO.Path]::IsPathRooted($path)) {
                $path = Join-Path $_.directory $path
            }
            [IO.Path]::GetFullPath($path)
        } | Where-Object {
            $selectedSet.ContainsKey($_.ToLowerInvariant())
        } | Sort-Object -Unique)

    $failedFiles = [Collections.Generic.List[string]]::new()
    foreach ($file in $files) {
        & $clangTidy $file `
            "-p=$([IO.Path]::GetDirectoryName($databasePath))" `
            "--config-file=$repoRoot/.clang-tidy" `
            '--extra-arg-before=-Wno-unused-command-line-argument'
        if ($LASTEXITCODE -ne 0) {
            $failedFiles.Add($file)
        }
    }

    if ($failedFiles.Count -gt 0) {
        throw "clang-tidy failed for $($failedFiles.Count) file(s)."
    }
}

Push-Location $repoRoot
try {
    switch ($Mode) {
        'FormatCheck' { Invoke-Format }
        'Format' { Invoke-Format -Apply }
        'PowerShell' { Invoke-PowerShellAnalysis }
        'Build' { Invoke-NativeBuild }
        'ClangTidy' { Invoke-ClangTidyAnalysis }
        'All' {
            Invoke-Format
            Invoke-PowerShellAnalysis
            Invoke-NativeBuild
            if ($CompileDatabase) {
                Invoke-ClangTidyAnalysis
            }
        }
    }
}
finally {
    Pop-Location
}
