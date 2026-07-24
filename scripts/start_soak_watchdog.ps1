[CmdletBinding()]
param(
  [string]$Root = "",
  [int]$IntervalMinutes = 15
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
  $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

$reportRoot = Join-Path $Root "reports\container-soaks-resumable"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
$logPath = Join-Path $reportRoot "watchdog.log"
$scriptPath = Join-Path $Root "scripts\run_resumable_parallel_container_soaks.ps1"

while ($true) {
  try {
    $timestamp = [DateTime]::UtcNow.ToString("o")
    Add-Content -LiteralPath $logPath -Value "[$timestamp] invoking resumable soak orchestrator"
    & powershell.exe -ExecutionPolicy Bypass -File $scriptPath *> (Join-Path $reportRoot "watchdog-last-run.log")
  } catch {
    $timestamp = [DateTime]::UtcNow.ToString("o")
    Add-Content -LiteralPath $logPath -Value "[$timestamp] watchdog error: $($_.Exception.Message)"
  }
  Start-Sleep -Seconds ($IntervalMinutes * 60)
}
