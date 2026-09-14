[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $InputPath,

    [Parameter(Mandatory)]
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'

function ConvertTo-CommandLineArgument {
    param([AllowEmptyString()][string] $Value)

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }

    $builder = [Text.StringBuilder]::new()
    [void] $builder.Append('"')
    $backslashes = 0

    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes++
            continue
        }

        if ($character -eq '"') {
            [void] $builder.Append(('\' * (($backslashes * 2) + 1)))
            [void] $builder.Append('"')
            $backslashes = 0
            continue
        }

        if ($backslashes -gt 0) {
            [void] $builder.Append(('\' * $backslashes))
            $backslashes = 0
        }

        [void] $builder.Append($character)
    }

    if ($backslashes -gt 0) {
        [void] $builder.Append(('\' * ($backslashes * 2)))
    }

    [void] $builder.Append('"')
    return $builder.ToString()
}

function Test-SourceArgument {
    param([string] $Argument)

    return $Argument -match '(?i)\.(c|cc|cpp|cxx)$' -or
    $Argument -match '(?i)^/(Tc|Tp).+\.(c|cc|cpp|cxx)$'
}

function Test-BuildOnlyArgument {
    param([string] $Argument)

    return $Argument -match '(?i)^/(c|FC|FS|GL|Gm-|MP|WX)$' -or
        $Argument -match '(?i)^/(Fd|Fo).+' -or
        $Argument -match '(?i)^/Y[cu].+' -or
        $Argument -match '(?i)^/Fp(?!:).+'
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$analysisIncludeDirectories = @(
    $repositoryRoot
    (Join-Path $repositoryRoot 'BinaryDecompiler')
    (Join-Path $repositoryRoot 'BinaryDecompiler/include')
    (Join-Path $repositoryRoot 'crc32c-hw-1.0.5/include')
    (Join-Path $repositoryRoot 'D3D_Shaders')
    (Join-Path $repositoryRoot 'DirectXTK/Inc')
    (Join-Path $repositoryRoot 'HLSLDecompiler')
    (Join-Path $repositoryRoot 'pcre2')
)
$database = Get-Content -LiteralPath $resolvedInput -Raw | ConvertFrom-Json
$normalized = [Collections.Generic.List[object]]::new()
$seenFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

foreach ($entry in $database) {
    if (-not $entry.directory -or -not $entry.file -or -not $entry.arguments) {
        throw "Invalid compilation database entry: directory, file, and arguments are required."
    }

    $sourcePath = [IO.Path]::GetFullPath((Join-Path $entry.directory $entry.file))
    if (-not $seenFiles.Add($sourcePath)) {
        continue
    }

    # DirectXTK relies on MSVC-specific DirectXMath and WRL behavior that the
    # clang frontend cannot parse reliably. MSVC /W4 /WX and Sonar still cover it.
    $directXtkRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'DirectXTK')) + [IO.Path]::DirectorySeparatorChar
    if ($sourcePath.StartsWith($directXtkRoot, [StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    $arguments = [Collections.Generic.List[string]]::new()
    foreach ($argument in $entry.arguments) {
        if (
            -not (Test-SourceArgument -Argument $argument) -and
            -not (Test-BuildOnlyArgument -Argument $argument)
        ) {
            $arguments.Add([string] $argument)
        }
    }

    foreach ($includeDirectory in $analysisIncludeDirectories) {
        $arguments.Add("/I$includeDirectory")
    }

    $arguments.Add([string] $entry.file)
    $command = ($arguments | ForEach-Object { ConvertTo-CommandLineArgument -Value $_ }) -join ' '
    $normalized.Add(
        [ordered]@{
            directory = [string] $entry.directory
            file      = [string] $entry.file
            command   = $command
        }
    )
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [IO.Path]::GetDirectoryName($resolvedOutput)
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$normalized | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $resolvedOutput -Encoding utf8NoBOM

Write-Output "Normalized $($normalized.Count) compilation commands to $resolvedOutput"

