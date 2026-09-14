[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectKey,

    [ValidateNotNullOrEmpty()]
    [string] $Branch = "main",

    [ValidateNotNullOrEmpty()]
    [string] $SonarHostUrl = "https://sonarcloud.io",

    [ValidateSet("reopen", "resolve")]
    [string] $Transition = "reopen",

    [ValidateRange(0, [int]::MaxValue)]
    [int] $ExpectedCount = 0
)

$ErrorActionPreference = "Stop"

$token = $env:SONAR_TOKEN
if ([string]::IsNullOrWhiteSpace($token)) {
    throw "SONAR_TOKEN is required."
}

$headers = @{
    Authorization = "Bearer $token"
}
$apiRoot = $SonarHostUrl.TrimEnd("/")
$pageSize = 500
$page = 1
$sourceStatus = if ($Transition -eq "reopen") { "ACCEPTED" } else { "OPEN" }
$issuesToChange = [System.Collections.Generic.List[string]]::new()

do {
    $query = [System.Web.HttpUtility]::ParseQueryString("")
    $query["componentKeys"] = $ProjectKey
    $query["branch"] = $Branch
    $query["issueStatuses"] = $sourceStatus
    $query["p"] = $page.ToString([Globalization.CultureInfo]::InvariantCulture)
    $query["ps"] = $pageSize.ToString([Globalization.CultureInfo]::InvariantCulture)

    $response = Invoke-RestMethod `
        -Uri "$apiRoot/api/issues/search?$($query.ToString())" `
        -Headers $headers `
        -Method Get

    foreach ($issue in $response.issues) {
        $issuesToChange.Add($issue.key)
    }

    $page++
} while ($issuesToChange.Count -lt $response.paging.total)

if ($issuesToChange.Count -ne $ExpectedCount) {
    throw "Expected $ExpectedCount $sourceStatus issues, but found $($issuesToChange.Count)."
}

if ($issuesToChange.Count -eq 0) {
    Write-Host "No $sourceStatus issues require the $Transition transition for $ProjectKey on $Branch."
    exit 0
}

Write-Host "Found $($issuesToChange.Count) $sourceStatus issues for $ProjectKey on $Branch."

$batchSize = 100
for ($offset = 0; $offset -lt $issuesToChange.Count; $offset += $batchSize) {
    $lastIndex = [Math]::Min($offset + $batchSize - 1, $issuesToChange.Count - 1)
    $batch = $issuesToChange.GetRange($offset, $lastIndex - $offset + 1)

    if ($PSCmdlet.ShouldProcess("$($batch.Count) Sonar issues", $Transition)) {
        $comment = if ($Transition -eq "reopen") {
            "Reopened by the repository reconciliation workflow so a current clean analysis can evaluate historical accepted debt."
        }
        else {
            "Resolved by the repository reconciliation workflow after a cache-free branch analysis confirmed zero current issues."
        }
        $requestBody = @{
            issues            = $batch -join ","
            do_transition     = $Transition
            comment           = $comment
            sendNotifications = "false"
        }

        Invoke-RestMethod `
            -Uri "$apiRoot/api/issues/bulk_change" `
            -Headers $headers `
            -Method Post `
            -ContentType "application/x-www-form-urlencoded" `
            -Body $requestBody | Out-Null
    }
}

$verifyQuery = [System.Web.HttpUtility]::ParseQueryString("")
$verifyQuery["componentKeys"] = $ProjectKey
$verifyQuery["branch"] = $Branch
$verifyQuery["issueStatuses"] = $sourceStatus
$verifyQuery["ps"] = "1"
$remaining = Invoke-RestMethod `
    -Uri "$apiRoot/api/issues/search?$($verifyQuery.ToString())" `
    -Headers $headers `
    -Method Get

if ($remaining.total -ne 0) {
    throw "$($remaining.total) $sourceStatus Sonar issues remain after reconciliation."
}

Write-Host "Applied $Transition to $($issuesToChange.Count) $sourceStatus issues."
