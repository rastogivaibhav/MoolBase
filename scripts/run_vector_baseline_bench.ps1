param(
  [int]$Incidents = 5000,
  [int]$Queries = 200,
  [int]$Dim = 64,
  [string]$BuildDir = "build-release",
  [string]$Out = "reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt"
)

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptRoot "write_utf8_transcript.ps1")
$Exe = Join-Path $BuildDir "graphenedb_vector_baseline_bench.exe"

cmake --build $BuildDir --target graphenedb_vector_baseline_bench --config Release

if (-not (Test-Path $Exe)) {
  throw "benchmark executable not found: $Exe"
}

New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
Write-Utf8Transcript -OutPath $Out -FailureMessage "vector baseline benchmark failed" -Command { & $Exe $Incidents $Queries $Dim }
