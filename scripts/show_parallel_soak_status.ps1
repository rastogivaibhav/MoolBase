[CmdletBinding()]
param(
  [string]$ReportRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
  $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
  $ReportRoot = Join-Path $Root "reports\container-soaks-resumable"
}

function Read-JsonFile {
  param([string]$Path)
  if (-not (Test-Path $Path)) {
    return $null
  }
  return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-LastJsonLine {
  param([string]$Path)
  if (-not (Test-Path $Path)) {
    return $null
  }
  $line = Get-Content -LiteralPath $Path | Select-Object -Last 1
  if (-not $line) {
    return $null
  }
  return $line | ConvertFrom-Json
}

function Test-ProcessAlive {
  param([Nullable[int]]$ProcessId)
  if (-not $ProcessId) {
    return $false
  }
  try {
    $null = Get-Process -Id $ProcessId -ErrorAction Stop
    return $true
  } catch {
    return $false
  }
}

function Get-ElapsedSeconds {
  param(
    [object]$Job,
    [object]$Active
  )
  $elapsed = 0
  if ($Job -and $Job.accumulated_seconds) {
    $elapsed += [int]$Job.accumulated_seconds
  }
  if ($Active -and $Active.status -eq "running") {
    $started = [DateTime]::Parse($Active.started_at_utc).ToUniversalTime()
    $elapsed += [int]([DateTime]::UtcNow - $started).TotalSeconds
  }
  return $elapsed
}

function Format-Duration {
  param([int]$Seconds)
  $span = [TimeSpan]::FromSeconds([Math]::Max(0, $Seconds))
  return "{0:00}d {1:00}h {2:00}m {3:00}s" -f $span.Days, $span.Hours, $span.Minutes, $span.Seconds
}

$state = Read-JsonFile (Join-Path $ReportRoot "state.json")
if ($null -eq $state) {
  throw "No soak state found under $ReportRoot"
}

$rows = foreach ($jobName in @("24h", "72h")) {
  $job = $state.jobs.$jobName
  $reportDir = $job.report_dir
  $active = Read-JsonFile (Join-Path $reportDir "active.json")
  $stats = Get-LastJsonLine (Join-Path $reportDir "docker-stats.jsonl")
  $result = $null
  $latestJsonPath = $null

  if ($active -and $active.json_path -and (Test-Path $active.json_path)) {
    $latestJsonPath = $active.json_path
    $result = Read-JsonFile $active.json_path
  } else {
    $segmentsDir = Join-Path $reportDir "segments"
    if (Test-Path $segmentsDir) {
      $latestJsonPath = Get-ChildItem -LiteralPath $segmentsDir -Recurse -Filter server_soak.json |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
      if ($latestJsonPath) {
        $result = Read-JsonFile $latestJsonPath
      }
    }
  }

  $elapsedSeconds = Get-ElapsedSeconds -Job $job -Active $active
  $remainingSeconds = [Math]::Max(0, [int]$job.target_seconds - $elapsedSeconds)
  $segmentRunning = $false
  if ($active -and $active.status -eq "running") {
    $segmentRunning = Test-ProcessAlive ([int]$active.pid)
  }

  [pscustomobject]@{
    Job = $jobName
    Status = if ($job.completed) { "completed" } elseif ($job.failed) { "failed" } elseif ($segmentRunning) { "running" } else { "waiting" }
    Container = $job.container_name
    Port = $job.port
    Elapsed = Format-Duration $elapsedSeconds
    Remaining = Format-Duration $remainingSeconds
    Samples = if ($result) { $result.latency_ms.samples } else { $null }
    P95Ms = if ($result) { [Math]::Round([double]$result.latency_ms.p95, 2) } else { $null }
    P99Ms = if ($result) { [Math]::Round([double]$result.latency_ms.p99, 2) } else { $null }
    Failures = if ($result) { $result.failure_count } else { $null }
    MemUsage = if ($stats -and $stats.stats) { $stats.stats.MemUsage } else { $null }
    Cpu = if ($stats -and $stats.stats) { $stats.stats.CPUPerc } else { $null }
    DataBytes = if ($stats) { $stats.data_bytes } else { $null }
    LatestJson = $latestJsonPath
  }
}

$rows | Format-Table -AutoSize
