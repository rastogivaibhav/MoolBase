param(
  [int]$Nodes = 5000,
  [int]$Queries = 200,
  [int]$Dim = 32,
  [int]$K = 10,
  [string]$Index = "auto",
  [double]$MinRecall = 0.999,
  [string]$BuildDir = "build-release",
  [string]$Out = "reports/VECTOR_INDEX_RECALL_OUTPUT.txt"
)

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptRoot "write_utf8_transcript.ps1")
$Exe = Join-Path $BuildDir "graphenedb_vector_index_recall_bench.exe"

cmake --build $BuildDir --target graphenedb_vector_index_recall_bench --config Release

if (-not (Test-Path $Exe)) {
  throw "benchmark executable not found: $Exe"
}

New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
Write-Utf8Transcript -OutPath $Out -FailureMessage "vector index recall benchmark failed" -Command { & $Exe $Nodes $Queries $Dim $K $Index $MinRecall }
