[CmdletBinding()]
param(
  [string]$ImageTag = "graphenedb:soak-candidate",
  [string]$ReportRoot = "",
  [string]$ApiKey = "",
  [int]$Soak24Port = 18080,
  [int]$Soak72Port = 28080,
  [int]$Soak24Seconds = 86400,
  [int]$Soak72Seconds = 259200,
  [int]$Soak24Clients = 8,
  [int]$Soak72Clients = 8,
  [double]$Soak24Rps = 80,
  [double]$Soak72Rps = 80,
  [int]$StatsIntervalSeconds = 3600,
  [switch]$RebuildImage,
  [switch]$RegisterResumeTasks
)

$ErrorActionPreference = "Stop"
$mutex = New-Object System.Threading.Mutex($false, "Global\GrapheneDBResumableContainerSoaks")
if (-not $mutex.WaitOne(0)) {
  Write-Host "Another resumable soak orchestrator run is already active."
  exit 0
}

try {
  $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
  if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
    $ReportRoot = Join-Path $Root "reports\container-soaks-resumable"
  }
  if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    $ApiKey = if ($env:GRAPHENEDB_API_KEY) { $env:GRAPHENEDB_API_KEY } else { "soak-test-key" }
  }

  New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
  $StatePath = Join-Path $ReportRoot "state.json"
  $BuildLog = Join-Path $ReportRoot "docker-build.txt"

  function Write-JsonFile {
    param(
      [string]$Path,
      [object]$Value
    )
    $json = $Value | ConvertTo-Json -Depth 10
    Set-Content -LiteralPath $Path -Value $json
  }

  function Read-JsonFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
      return $null
    }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
  }

  function Get-NowUtc {
    return [DateTime]::UtcNow
  }

  function Ensure-ContainerStarted {
    param([pscustomobject]$Job)

    $name = $Job.container_name
    $volume = $Job.volume_name
    $port = [string]$Job.port

    $exists = $false
    try {
      & docker inspect $name *> $null
      $exists = $true
    } catch {
      $exists = $false
    }

    if (-not $exists) {
      try { & docker volume create $volume | Out-Null } catch {}
      & docker run -d `
        --name $name `
        --restart unless-stopped `
        -p "${port}:8080" `
        -e "GRAPHENEDB_API_KEY=$ApiKey" `
        --mount "source=$volume,target=/var/lib/graphenedb" `
        --read-only `
        --tmpfs /tmp:size=64m,mode=1777 `
        --cap-drop ALL `
        --security-opt no-new-privileges:true `
        --pids-limit 256 `
        --memory 2g `
        --cpus 2.0 `
        $ImageTag | Out-Null
    } else {
      $status = (& docker inspect --format '{{.State.Status}}' $name).Trim()
      if ($status -ne "running") {
        & docker start $name | Out-Null
      }
    }

    for ($i = 0; $i -lt 240; $i++) {
      $health = ""
      try {
        $health = (& docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' $name).Trim()
      } catch {
        $health = ""
      }
      if ($health -eq "healthy") {
        return
      }
      Start-Sleep -Seconds 2
    }

    throw "Container $name did not become healthy."
  }

  function Test-ProcessAlive {
    param([Nullable[int]]$ProcessId)
    if (-not $ProcessId) {
      return $false
    }
    try {
      $proc = Get-Process -Id $ProcessId -ErrorAction Stop
      return $null -ne $proc
    } catch {
      return $false
    }
  }

  function Initialize-State {
    $now = Get-NowUtc
    $jobs = @{}
    foreach ($cfg in @(
      @{
        job_name = "24h"
        port = $Soak24Port
        target_seconds = $Soak24Seconds
        clients = $Soak24Clients
        target_rps = $Soak24Rps
        container_name = "graphenedb-soak-24h"
        volume_name = "graphenedb-soak-24h-data"
      },
      @{
        job_name = "72h"
        port = $Soak72Port
        target_seconds = $Soak72Seconds
        clients = $Soak72Clients
        target_rps = $Soak72Rps
        container_name = "graphenedb-soak-72h"
        volume_name = "graphenedb-soak-72h-data"
      }
    )) {
      $reportDir = Join-Path $ReportRoot $cfg.job_name
      New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
      $jobs[$cfg.job_name] = [ordered]@{
        job_name = $cfg.job_name
        port = $cfg.port
        target_seconds = $cfg.target_seconds
        clients = $cfg.clients
        target_rps = $cfg.target_rps
        container_name = $cfg.container_name
        volume_name = $cfg.volume_name
        report_dir = $reportDir
        accumulated_seconds = 0
        failed = $false
        completed = $false
        started_at_utc = $now.ToString("o")
        last_launch_at_utc = $null
        latest_segment_status = $null
      }
    }

    return [ordered]@{
      version = 1
      image_tag = $ImageTag
      api_key = $ApiKey
      stats_interval_seconds = $StatsIntervalSeconds
      created_at_utc = $now.ToString("o")
      jobs = $jobs
    }
  }

  function Ensure-Image {
    param([pscustomobject]$State)
    $buildNeeded = $RebuildImage.IsPresent
    try {
      & docker image inspect $ImageTag *> $null
    } catch {
      $buildNeeded = $true
    }
    if ($buildNeeded) {
      "Building container image $ImageTag" | Tee-Object -FilePath $BuildLog
      & docker build --pull -t $ImageTag $Root | Tee-Object -FilePath $BuildLog -Append
    }
  }

  function Reconcile-ActiveSegment {
    param([pscustomobject]$Job)

    $activePath = Join-Path $Job.report_dir "active.json"
    if (-not (Test-Path $activePath)) {
      return
    }

    $active = Read-JsonFile $activePath
    if ($null -eq $active) {
      Remove-Item -LiteralPath $activePath -Force -ErrorAction SilentlyContinue
      return
    }

    if ($active.status -eq "running" -and (Test-ProcessAlive ([int]$active.pid))) {
      return
    }

    $contribution = 0
    if ($active.PSObject.Properties.Name -contains "elapsed_seconds" -and $active.elapsed_seconds) {
      $contribution = [int]$active.elapsed_seconds
    } else {
      $started = [DateTime]::Parse($active.started_at_utc).ToUniversalTime()
      $contribution = [int][Math]::Min(
        [double]$active.planned_seconds,
        [Math]::Max(1, (Get-NowUtc).Subtract($started).TotalSeconds)
      )
    }

    $Job.accumulated_seconds = [Math]::Min([int]$Job.target_seconds, [int]$Job.accumulated_seconds + $contribution)
    $Job.latest_segment_status = $active.status
    if ($active.status -eq "failed") {
      $Job.failed = $true
    }

    $archiveDir = Join-Path $Job.report_dir "segments"
    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
    $archivePath = Join-Path $archiveDir ("segment-" + $active.segment_id + ".json")
    Move-Item -LiteralPath $activePath -Destination $archivePath -Force

    if ($Job.accumulated_seconds -ge [int]$Job.target_seconds -and -not $Job.failed) {
      $Job.completed = $true
      Write-JsonFile -Path (Join-Path $Job.report_dir "complete.json") -Value @{
        completed_at_utc = (Get-NowUtc).ToString("o")
        accumulated_seconds = $Job.accumulated_seconds
        target_seconds = $Job.target_seconds
      }
    }
  }

  function Ensure-StatsCollector {
    param([pscustomobject]$Job)

    if ($Job.completed -or $Job.failed) {
      return
    }

    $statsActivePath = Join-Path $Job.report_dir "stats-active.json"
    $stats = Read-JsonFile $statsActivePath
    if ($null -ne $stats -and $stats.status -eq "running" -and (Test-ProcessAlive ([int]$stats.pid))) {
      return
    }

    Start-Process `
      -FilePath powershell.exe `
      -ArgumentList @(
        "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $Root "scripts\collect_docker_stats.ps1"),
        "-ContainerName", $Job.container_name,
        "-ReportDir", $Job.report_dir,
        "-IntervalSeconds", $StatsIntervalSeconds
      ) `
      -WorkingDirectory $Root `
      -WindowStyle Hidden | Out-Null
  }

  function Ensure-SoakSegment {
    param([pscustomobject]$Job)

    if ($Job.completed -or $Job.failed) {
      return
    }

    $activePath = Join-Path $Job.report_dir "active.json"
    $active = Read-JsonFile $activePath
    if ($null -ne $active -and $active.status -eq "running" -and (Test-ProcessAlive ([int]$active.pid))) {
      return
    }

    $remaining = [int]$Job.target_seconds - [int]$Job.accumulated_seconds
    if ($remaining -le 0) {
      $Job.completed = $true
      Write-JsonFile -Path (Join-Path $Job.report_dir "complete.json") -Value @{
        completed_at_utc = (Get-NowUtc).ToString("o")
        accumulated_seconds = $Job.accumulated_seconds
        target_seconds = $Job.target_seconds
      }
      return
    }

    Ensure-ContainerStarted -Job $Job
    Ensure-StatsCollector -Job $Job

    Start-Process `
      -FilePath powershell.exe `
      -ArgumentList @(
        "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $Root "scripts\invoke_named_container_soak.ps1"),
        "-JobName", $Job.job_name,
        "-Root", $Root,
        "-BaseUrl", ("http://127.0.0.1:" + $Job.port),
        "-ApiKey", $ApiKey,
        "-ContainerName", $Job.container_name,
        "-Seconds", $remaining,
        "-ReportDir", $Job.report_dir
      ) `
      -WorkingDirectory $Root `
      -WindowStyle Hidden | Out-Null

    $Job.last_launch_at_utc = (Get-NowUtc).ToString("o")
  }

  function Register-ResumeScheduledTasks {
    $scriptPath = Join-Path $Root "scripts\run_resumable_parallel_container_soaks.ps1"
    $taskCommand = "powershell.exe -ExecutionPolicy Bypass -File `"$scriptPath`""
    & schtasks /Create /TN "GrapheneDB Parallel Soaks Resume (Logon)" /SC ONLOGON /TR $taskCommand /RL LIMITED /F | Out-Null
    $start = (Get-Date).AddMinutes(1).ToString("HH:mm")
    & schtasks /Create /TN "GrapheneDB Parallel Soaks Resume (15min)" /SC MINUTE /MO 15 /ST $start /TR $taskCommand /RL LIMITED /F | Out-Null
  }

  $state = Read-JsonFile $StatePath
  if ($null -eq $state) {
    $state = Initialize-State
  }

  if ($RegisterResumeTasks) {
    Register-ResumeScheduledTasks
  }

  Ensure-Image -State $state

  foreach ($jobName in @("24h", "72h")) {
    $job = $state.jobs.$jobName
    Reconcile-ActiveSegment -Job $job
    Ensure-SoakSegment -Job $job
  }

  $state.last_run_at_utc = (Get-NowUtc).ToString("o")
  Write-JsonFile -Path $StatePath -Value $state
  $state | ConvertTo-Json -Depth 10
} finally {
  $mutex.ReleaseMutex() | Out-Null
  $mutex.Dispose()
}
