param(
  [string]$BuildDir = "build-enterprise-target-scale",
  [string]$ReportDir = "reports/enterprise-ga",
  [string]$Config = "Release",
  [string]$ProfileLabel = "target-scale-approved",
  [string]$ApprovedHost = "1",
  [int]$SoakSeconds = 86400,
  [int]$SoakDim = 384,
  [int]$StressIncidents = 16667,
  [int]$StressQueries = 500,
  [int]$StressDim = 384,
  [string]$StressVectorIndex = "auto",
  [int]$OneMNodes = 1000000,
  [int]$OneMQueries = 100,
  [int]$OneMDim = 768,
  [string]$OneMVectorIndex = "auto",
  [int]$FuzzRuns = 500000,
  [string]$FuzzBuildDir = "build-enterprise-target-scale-fuzz",
  [string]$FuzzCC = "clang",
  [string]$FuzzCXX = "clang++",
  [switch]$UseFaiss,
  [switch]$SkipSoak,
  [switch]$SkipGaReadiness,
  [switch]$SkipFullCtest,
  [switch]$SkipFuzz
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$approved = @("1", "true", "yes", "on") -contains $ApprovedHost.ToLowerInvariant()

$args = @(
  "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $Root "scripts/run_enterprise_ga_campaign.ps1"),
  "-BuildDir", $BuildDir,
  "-ReportDir", $ReportDir,
  "-Config", $Config,
  "-ProfileLabel", $ProfileLabel,
  "-SoakSeconds", $SoakSeconds,
  "-SoakDim", $SoakDim,
  "-StressIncidents", $StressIncidents,
  "-StressQueries", $StressQueries,
  "-StressDim", $StressDim,
  "-StressVectorIndex", $StressVectorIndex,
  "-OneMNodes", $OneMNodes,
  "-OneMQueries", $OneMQueries,
  "-OneMDim", $OneMDim,
  "-OneMVectorIndex", $OneMVectorIndex,
  "-FuzzRuns", $FuzzRuns,
  "-FuzzBuildDir", $FuzzBuildDir,
  "-FuzzCC", $FuzzCC,
  "-FuzzCXX", $FuzzCXX
)

if ($approved) { $args += "-ApprovedHost" }
if ($UseFaiss) { $args += "-UseFaiss" }
if ($SkipSoak) { $args += "-SkipSoak" }
if ($SkipGaReadiness) { $args += "-SkipGaReadiness" }
if ($SkipFullCtest) { $args += "-SkipFullCtest" }
if ($SkipFuzz) { $args += "-SkipFuzz" }

& powershell @args
if ($LASTEXITCODE -ne 0) {
  throw "target-scale enterprise profile failed"
}
