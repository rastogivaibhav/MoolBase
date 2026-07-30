[CmdletBinding()]
param(
  [string]$ImageTag = "graphenedb:soak-candidate",
  [string]$ReportRoot = "",
  [string]$ApiKey = "",
  [string]$Soak24Name = "graphenedb-soak-24h",
  [string]$Soak72Name = "graphenedb-soak-72h",
  [string]$Soak24Volume = "graphenedb-soak-24h-data",
  [string]$Soak72Volume = "graphenedb-soak-72h-data",
  [int]$Soak24Port = 18080,
  [int]$Soak72Port = 28080,
  [int]$Soak24Seconds = 86400,
  [int]$Soak72Seconds = 259200,
  [int]$Soak24Clients = 8,
  [int]$Soak72Clients = 8,
  [double]$Soak24Rps = 80,
  [double]$Soak72Rps = 80,
  [switch]$KeepContainers,
  [switch]$KeepVolumes
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
  $ReportRoot = Join-Path $Root "reports\container-soaks"
}
if ([string]::IsNullOrWhiteSpace($ApiKey)) {
  $ApiKey = if ($env:GRAPHENEDB_API_KEY) { $env:GRAPHENEDB_API_KEY } else { "soak-test-key" }
}

$Soak24Report = Join-Path $ReportRoot "24h"
$Soak72Report = Join-Path $ReportRoot "72h"
New-Item -ItemType Directory -Force -Path $Soak24Report, $Soak72Report | Out-Null

function Invoke-Docker {
  param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Args)
  & docker @Args
}

function Wait-ContainerHealthy {
  param([string]$Name)
  for ($i = 0; $i -lt 240; $i++) {
    $health = ""
    try {
      $health = (Invoke-Docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' $Name).Trim()
    } catch {
      $health = ""
    }
    if ($health -eq "healthy") {
      return
    }
    if ($health -eq "exited" -or $health -eq "dead") {
      Invoke-Docker logs $Name | Out-Host
      throw "container $Name is not healthy; current state=$health"
    }
    Start-Sleep -Seconds 2
  }
  Invoke-Docker logs $Name | Out-Host
  throw "timed out waiting for container $Name to become healthy"
}

function Collect-Artifacts {
  param(
    [string]$Name,
    [string]$ReportDir
  )
  try {
    Invoke-Docker inspect $Name | Set-Content -Path (Join-Path $ReportDir "container-inspect.json")
    Invoke-Docker logs --timestamps $Name 2>&1 | Set-Content -Path (Join-Path $ReportDir "container.log")
    Invoke-Docker exec $Name /bin/sh -lc 'ls -lah /var/lib/graphenedb' 2>&1 | Set-Content -Path (Join-Path $ReportDir "db-files.txt")
  } catch {
  }
}

function Remove-ContainerAndVolume {
  param(
    [string]$Name,
    [string]$Volume
  )
  if (-not $KeepContainers) {
    try { Invoke-Docker rm -f $Name | Out-Null } catch {}
  }
  if (-not $KeepVolumes) {
    try { Invoke-Docker volume rm $Volume | Out-Null } catch {}
  }
}

function Start-SoakContainer {
  param(
    [string]$Name,
    [string]$Volume,
    [int]$Port
  )

  try { Invoke-Docker rm -f $Name | Out-Null } catch {}
  if (-not $KeepVolumes) {
    try { Invoke-Docker volume rm $Volume | Out-Null } catch {}
  }
  Invoke-Docker volume create $Volume | Out-Null
  Invoke-Docker run -d `
    --name $Name `
    -p "${Port}:8080" `
    -e "GRAPHENEDB_API_KEY=$ApiKey" `
    --mount "source=$Volume,target=/var/lib/graphenedb" `
    --read-only `
    --tmpfs /tmp:size=64m,mode=1777 `
    --cap-drop ALL `
    --security-opt no-new-privileges:true `
    --pids-limit 256 `
    --memory 2g `
    --cpus 2.0 `
    $ImageTag | Out-Null
  Wait-ContainerHealthy -Name $Name
}

function Start-SoakClient {
  param(
    [string]$Name,
    [int]$Port,
    [int]$Seconds,
    [int]$Clients,
    [double]$Rps,
    [string]$ReportDir
  )

  $textPath = Join-Path $ReportDir "server_soak.txt"
  $jsonPath = Join-Path $ReportDir "server_soak.json"
  $errPath = Join-Path $ReportDir "server_soak.err.txt"
  $argList = @(
    (Join-Path $Root "scripts\server_soak_test.py"),
    "--base-url", "http://127.0.0.1:$Port",
    "--api-key", $ApiKey,
    "--container-name", $Name,
    "--seconds", $Seconds,
    "--clients", $Clients,
    "--target-rps", $Rps,
    "--output", $jsonPath
  )
  Start-Process -FilePath python `
    -ArgumentList $argList `
    -WorkingDirectory $Root `
    -RedirectStandardOutput $textPath `
    -RedirectStandardError $errPath `
    -NoNewWindow `
    -PassThru
}

try {
  "Building container image $ImageTag" | Tee-Object -FilePath (Join-Path $ReportRoot "docker-build.txt")
  Invoke-Docker build --pull -t $ImageTag $Root | Tee-Object -FilePath (Join-Path $ReportRoot "docker-build.txt") -Append

  "Starting $Soak24Name on port $Soak24Port" | Write-Host
  Start-SoakContainer -Name $Soak24Name -Volume $Soak24Volume -Port $Soak24Port

  "Starting $Soak72Name on port $Soak72Port" | Write-Host
  Start-SoakContainer -Name $Soak72Name -Volume $Soak72Volume -Port $Soak72Port

  $p24 = Start-SoakClient -Name $Soak24Name -Port $Soak24Port -Seconds $Soak24Seconds -Clients $Soak24Clients -Rps $Soak24Rps -ReportDir $Soak24Report
  $p72 = Start-SoakClient -Name $Soak72Name -Port $Soak72Port -Seconds $Soak72Seconds -Clients $Soak72Clients -Rps $Soak72Rps -ReportDir $Soak72Report

  Wait-Process -Id $p24.Id, $p72.Id
  $failed = @()
  if ($p24.ExitCode -ne 0) { $failed += $Soak24Name }
  if ($p72.ExitCode -ne 0) { $failed += $Soak72Name }

  Collect-Artifacts -Name $Soak24Name -ReportDir $Soak24Report
  Collect-Artifacts -Name $Soak72Name -ReportDir $Soak72Report

  if ($failed.Count -gt 0) {
    throw ("One or more parallel soaks failed: " + ($failed -join ", "))
  }

  "graphenedb_parallel_container_soaks_passed=true" | Write-Host
} finally {
  Collect-Artifacts -Name $Soak24Name -ReportDir $Soak24Report
  Collect-Artifacts -Name $Soak72Name -ReportDir $Soak72Report
  Remove-ContainerAndVolume -Name $Soak24Name -Volume $Soak24Volume
  Remove-ContainerAndVolume -Name $Soak72Name -Volume $Soak72Volume
}
