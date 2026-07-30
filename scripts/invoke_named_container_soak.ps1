[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$JobName,
  [Parameter(Mandatory = $true)][string]$Root,
  [Parameter(Mandatory = $true)][string]$BaseUrl,
  [Parameter(Mandatory = $true)][string]$ApiKey,
  [Parameter(Mandatory = $true)][string]$ContainerName,
  [Parameter(Mandatory = $true)][int]$Seconds,
  [Parameter(Mandatory = $true)][int]$Clients,
  [Parameter(Mandatory = $true)][double]$TargetRps,
  [Parameter(Mandatory = $true)][string]$ReportDir
)

$ErrorActionPreference = "Stop"

function Write-JsonFile {
  param(
    [string]$Path,
    [object]$Value
  )
  $json = $Value | ConvertTo-Json -Depth 8
  Set-Content -LiteralPath $Path -Value $json
}

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$segmentsDir = Join-Path $ReportDir "segments"
New-Item -ItemType Directory -Force -Path $segmentsDir | Out-Null

$segmentId = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$segmentDir = Join-Path $segmentsDir $segmentId
New-Item -ItemType Directory -Force -Path $segmentDir | Out-Null

$activePath = Join-Path $ReportDir "active.json"
$stdoutPath = Join-Path $segmentDir "server_soak.txt"
$stderrPath = Join-Path $segmentDir "server_soak.err.txt"
$jsonPath = Join-Path $segmentDir "server_soak.json"

$startedAt = [DateTime]::UtcNow
$pidValue = $PID
$active = [ordered]@{
  job_name = $JobName
  segment_id = $segmentId
  status = "running"
  pid = $pidValue
  container_name = $ContainerName
  base_url = $BaseUrl
  started_at_utc = $startedAt.ToString("o")
  planned_seconds = $Seconds
  stdout_path = $stdoutPath
  stderr_path = $stderrPath
  json_path = $jsonPath
}
Write-JsonFile -Path $activePath -Value $active

$arguments = @(
  (Join-Path $Root "scripts\server_soak_test.py"),
  "--base-url", $BaseUrl,
  "--api-key", $ApiKey,
  "--container-name", $ContainerName,
  "--seconds", $Seconds,
  "--clients", $Clients,
  "--target-rps", $TargetRps,
  "--output", $jsonPath
)

$process = Start-Process `
  -FilePath python `
  -ArgumentList $arguments `
  -WorkingDirectory $Root `
  -RedirectStandardOutput $stdoutPath `
  -RedirectStandardError $stderrPath `
  -NoNewWindow `
  -Wait `
  -PassThru

$endedAt = [DateTime]::UtcNow
$elapsedSeconds = [Math]::Max(1, [int][Math]::Round(($endedAt - $startedAt).TotalSeconds))
$active.status = if ($process.ExitCode -eq 0) { "completed" } else { "failed" }
$active.ended_at_utc = $endedAt.ToString("o")
$active.elapsed_seconds = $elapsedSeconds
$active.exit_code = $process.ExitCode
Write-JsonFile -Path $activePath -Value $active

exit $process.ExitCode
