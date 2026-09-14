[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectKey,

    [ValidateNotNullOrEmpty()]
    [string] $Branch = "main",

    [ValidateNotNullOrEmpty()]
    [string] $SonarHostUrl = "https://sonarcloud.io"
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
$acceptedIssues = [System.Collections.Generic.List[string]]::new()

do {
    $query = [System.Web.HttpUtility]::ParseQueryString("")
    $query["componentKeys"] = $ProjectKey
    $query["branch"] = $Branch
    $query["issueStatuses"] = "ACCEPTED"
    $query["p"] = $page.ToString([Globalization.CultureInfo]::InvariantCulture)
    $query["ps"] = $pageSize.ToString([Globalization.CultureInfo]::InvariantCulture)

    $response = Invoke-RestMethod `
        -Uri "$apiRoot/api/issues/search?$($query.ToString())" `
        -Headers $headers `
        -Method Get

    foreach ($issue in $response.issues) {
        $acceptedIssues.Add($issue.key)
    }

    $page++
} while ($acceptedIssues.Count -lt $response.paging.total)

if ($acceptedIssues.Count -eq 0) {
    Write-Host "No accepted issues remain for $ProjectKey on $Branch."
    exit 0
}

Write-Host "Found $($acceptedIssues.Count) accepted issues for $ProjectKey on $Branch."

$batchSize = 100
for ($offset = 0; $offset -lt $acceptedIssues.Count; $offset += $batchSize) {
    $lastIndex = [Math]::Min($offset + $batchSize - 1, $acceptedIssues.Count - 1)
    $batch = $acceptedIssues.GetRange($offset, $lastIndex - $offset + 1)

    if ($PSCmdlet.ShouldProcess("$($batch.Count) Sonar issues", "Reopen")) {
        $requestBody = @{
            issues            = $batch -join ","
            do_transition     = "reopen"
            comment           = "Reopened by the repository reconciliation workflow so the current clean analysis can close obsolete accepted debt."
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
$verifyQuery["issueStatuses"] = "ACCEPTED"
$verifyQuery["ps"] = "1"
$remaining = Invoke-RestMethod `
    -Uri "$apiRoot/api/issues/search?$($verifyQuery.ToString())" `
    -Headers $headers `
    -Method Get

if ($remaining.total -ne 0) {
    throw "$($remaining.total) accepted Sonar issues remain after reconciliation."
}

Write-Host "Reopened $($acceptedIssues.Count) accepted issues. Run a fresh main analysis to close obsolete records."
