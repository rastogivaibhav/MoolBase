[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$ContainerName,
  [Parameter(Mandatory = $true)][string]$ReportDir,
  [int]$IntervalSeconds = 3600
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
$activePath = Join-Path $ReportDir "stats-active.json"
$completeMarker = Join-Path $ReportDir "complete.json"
$statsPath = Join-Path $ReportDir "docker-stats.jsonl"
$pidValue = $PID

$active = [ordered]@{
  container_name = $ContainerName
  status = "running"
  pid = $pidValue
  started_at_utc = ([DateTime]::UtcNow).ToString("o")
  interval_seconds = $IntervalSeconds
}
Write-JsonFile -Path $activePath -Value $active

try {
  while (-not (Test-Path $completeMarker)) {
    $timestamp = [DateTime]::UtcNow.ToString("o")
    $inspectState = ""
    try {
      $inspectState = (& docker inspect --format '{{.State.Status}}|{{if .State.Health}}{{.State.Health.Status}}{{end}}' $ContainerName 2>$null).Trim()
    } catch {
      $inspectState = "missing|"
    }

    $statsRaw = ""
    try {
      $statsRaw = (& docker stats --no-stream --format '{{json .}}' $ContainerName 2>$null).Trim()
    } catch {
      $statsRaw = ""
    }

    $diskUsage = ""
    try {
      $diskUsage = (& docker exec $ContainerName /bin/sh -lc 'du -sb /var/lib/graphenedb 2>/dev/null | cut -f1' 2>$null).Trim()
    } catch {
      $diskUsage = ""
    }

    $payload = [ordered]@{
      timestamp_utc = $timestamp
      container_name = $ContainerName
      inspect_state = $inspectState
      stats = if ($statsRaw) { $statsRaw | ConvertFrom-Json } else { $null }
      data_bytes = if ($diskUsage) { [Int64]$diskUsage } else { $null }
    }
    Add-Content -LiteralPath $statsPath -Value (($payload | ConvertTo-Json -Depth 8 -Compress))

    for ($i = 0; $i -lt $IntervalSeconds; $i += 5) {
      if (Test-Path $completeMarker) {
        break
      }
      Start-Sleep -Seconds ([Math]::Min(5, $IntervalSeconds - $i))
    }
  }
} finally {
  $active.status = "stopped"
  $active.ended_at_utc = ([DateTime]::UtcNow).ToString("o")
  Write-JsonFile -Path $activePath -Value $active
}
