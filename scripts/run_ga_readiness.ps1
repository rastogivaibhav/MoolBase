param(
  [string]$BuildDir = "build-ga-readiness",
  [string]$ReportDir = "reports/ga-readiness",
  [string]$Config = "Release",
  [string]$ProfileLabel = "",
  [switch]$ApprovedHost,
  [string]$CtestExclude = "graphenedb_rc_(fuzz|kosh_adapter)_tests",
  [string]$FocusedRegex = "graphenedb_(acid_lattice|lattice)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests",
  [string]$VectorIndexRecallKind = "auto",
  [double]$VectorIndexRecallMin = 0.999,
  [int]$VectorIndexRecallNodes = 5000,
  [int]$VectorIndexRecallQueries = 200,
  [int]$VectorIndexRecallDim = 32,
  [int]$VectorIndexRecallK = 10,
  [int]$ExtractionDocs = 10,
  [int]$ExtractionNodesPerDoc = 20,
  [int]$ExtractionQueries = 10,
  [string]$ExtractionVectorIndex = "auto",
  [int]$StorageNodes = 2000,
  [int]$StorageQueries = 20,
  [int]$Dim = 32,
  [string]$StorageVectorIndex = "auto",
  [int]$BuildJobs = 1,
  [string[]]$ExtractionThreshold = @(),
  [string[]]$StorageThreshold = @(),
  [switch]$UseFaiss,
  [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Utf8Transcript = Join-Path $Root "scripts/write_utf8_transcript.ps1"
. $Utf8Transcript
$BuildPath = Join-Path $Root $BuildDir
$ReportPath = Join-Path $Root $ReportDir
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunPath = Join-Path $ReportPath $Stamp
$Summary = Join-Path $RunPath "GA_READINESS_SUMMARY.md"
$HostProfile = Join-Path $RunPath "HOST_PROFILE.json"
$SummaryLines = New-Object System.Collections.Generic.List[string]

function Add-SummaryLine {
  param([string]$Line = "")
  $SummaryLines.Add($Line) | Out-Null
}

function Write-Summary {
  New-Item -ItemType Directory -Force -Path $RunPath | Out-Null
  $TempSummary = "$Summary.tmp"
  $text = ($SummaryLines -join [Environment]::NewLine) + [Environment]::NewLine
  [System.IO.File]::WriteAllText($TempSummary, $text)
  Move-Item -Force -LiteralPath $TempSummary -Destination $Summary
}

function Invoke-Gate {
  param(
    [string]$Name,
    [string]$LogName,
    [scriptblock]$Command
  )
  $LogPath = Join-Path $RunPath $LogName
  Add-SummaryLine "## $Name"
  Add-SummaryLine "log: ``$LogPath``"
  Write-Summary
  try {
    Write-Utf8Transcript -OutPath $LogPath -FailureMessage "$Name failed" -Command $Command
    Add-SummaryLine "- status: PASS"
    Add-SummaryLine ""
    Write-Summary
  } catch {
    Add-SummaryLine "- status: FAIL"
    Add-SummaryLine "- error: $_"
    Add-SummaryLine ""
    Write-Summary
    Get-Content $LogPath -ErrorAction SilentlyContinue | Select-Object -Last 80
    throw
  }
}

function Get-MetricValue {
  param(
    [string]$Path,
    [string]$Key
  )
  if (-not (Test-Path -LiteralPath $Path)) { return "" }
  $match = Select-String -Path $Path -Pattern ("(?:^|\s)" + [regex]::Escape($Key) + "=([^\s]+)") | Select-Object -First 1
  if ($match) { return $match.Matches[0].Groups[1].Value.Trim() }
  return ""
}

Add-SummaryLine "# GrapheneDB GA Readiness Run"
Add-SummaryLine ""
Add-SummaryLine "- timestamp: $Stamp"
Add-SummaryLine "- config: $Config"
Add-SummaryLine "- profile_label: $ProfileLabel"
Add-SummaryLine "- approved_host: $([bool]$ApprovedHost)"
Add-SummaryLine "- ctest_exclude: $CtestExclude"
Add-SummaryLine "- focused_regex: $FocusedRegex"
Add-SummaryLine "- vector_index_recall_kind: $VectorIndexRecallKind"
Add-SummaryLine "- vector_index_recall_min: $VectorIndexRecallMin"
Add-SummaryLine "- vector_index_recall_nodes: $VectorIndexRecallNodes"
Add-SummaryLine "- vector_index_recall_queries: $VectorIndexRecallQueries"
Add-SummaryLine "- vector_index_recall_dim: $VectorIndexRecallDim"
Add-SummaryLine "- vector_index_recall_k: $VectorIndexRecallK"
Add-SummaryLine "- extraction_docs: $ExtractionDocs"
Add-SummaryLine "- extraction_nodes_per_doc: $ExtractionNodesPerDoc"
Add-SummaryLine "- extraction_queries: $ExtractionQueries"
Add-SummaryLine "- extraction_vector_index: $ExtractionVectorIndex"
Add-SummaryLine "- storage_nodes: $StorageNodes"
Add-SummaryLine "- storage_queries: $StorageQueries"
Add-SummaryLine "- dim: $Dim"
Add-SummaryLine "- storage_vector_index: $StorageVectorIndex"
Add-SummaryLine "- graphenedb_use_faiss: $([bool]$UseFaiss)"
Add-SummaryLine "- extraction_thresholds: $($ExtractionThreshold -join ', ')"
Add-SummaryLine "- storage_thresholds: $($StorageThreshold -join ', ')"
Add-SummaryLine ""
Write-Summary

Invoke-Gate "host profile" "00-host-profile.log" {
  & (Join-Path $Root "scripts/write_host_profile.ps1") `
    -Out $HostProfile `
    -ProfileLabel $ProfileLabel `
    -IntendedHardware 0 `
    -ApprovedHost $([int][bool]$ApprovedHost)
}

Invoke-Gate "configure" "01-configure.log" {
  cmake -S $Root -B $BuildPath `
    "-DCMAKE_BUILD_TYPE=${Config}" `
    -DGRAPHENEDB_BUILD_TESTS=ON `
    -DGRAPHENEDB_BUILD_BENCH=ON `
    -DGRAPHENEDB_BUILD_EXAMPLES=ON `
    "-DGRAPHENEDB_USE_FAISS=$(if ($UseFaiss) { 'ON' } else { 'OFF' })"
}

Invoke-Gate "build" "02-build.log" {
  cmake --build $BuildPath -j $BuildJobs
}

Invoke-Gate "ctest runnable suite" "03-ctest.log" {
  ctest --test-dir $BuildPath -E $CtestExclude --output-on-failure
}

Invoke-Gate "focused acid/crash gates" "04-acid-crash.log" {
  ctest --test-dir $BuildPath -R $FocusedRegex --output-on-failure
}

Invoke-Gate "recovery rehearsal" "04b-recovery-rehearsal.log" {
  & (Join-Path $Root "scripts/run_recovery_rehearsal.ps1") `
    -Cli (Join-Path $BuildPath "graphenedb_cli.exe") `
    -Out (Join-Path $RunPath "RECOVERY_REHEARSAL_OUTPUT.txt")
}

Invoke-Gate "kosh adapter gate" "04c-kosh-adapter.log" {
  & (Join-Path $Root "scripts/run_kosh_adapter_gate.ps1") `
    -BuildDir $BuildDir `
    -Out (Join-Path $Root "reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt")
}

if (!$SkipPackage) {
  Invoke-Gate "package install consumer" "05-package.log" {
    & (Join-Path $Root "scripts/verify_package_install.ps1") `
      -BuildDir "build-ga-package" `
      -InstallDir "build-ga-package-install" `
      -ConsumerBuildDir "build-ga-package-consumer" `
      -Config $Config
  }
}

Invoke-Gate "vector index recall benchmark" "06-vector-index-recall.log" {
  & (Join-Path $BuildPath "graphenedb_vector_index_recall_bench.exe") $VectorIndexRecallNodes $VectorIndexRecallQueries $VectorIndexRecallDim $VectorIndexRecallK $VectorIndexRecallKind $VectorIndexRecallMin
}
Add-SummaryLine "- vector_index_recall_requested: $(Get-MetricValue (Join-Path $RunPath '06-vector-index-recall.log') 'vector_index_requested')"
Add-SummaryLine "- vector_index_recall: $(Get-MetricValue (Join-Path $RunPath '06-vector-index-recall.log') 'vector_index')"
Add-SummaryLine "- vector_index_recall_mean_recall_at_k: $(Get-MetricValue (Join-Path $RunPath '06-vector-index-recall.log') 'mean_recall_at_k')"
Add-SummaryLine ""
Write-Summary

Invoke-Gate "extraction ingest benchmark" "07-extraction-bench.log" {
  & (Join-Path $BuildPath "graphenedb_extraction_ingest_bench.exe") $ExtractionDocs $ExtractionNodesPerDoc $ExtractionQueries $Dim --vector-index $ExtractionVectorIndex
}
Add-SummaryLine "- extraction_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath '07-extraction-bench.log') 'vector_index_requested')"
Add-SummaryLine "- extraction_vector_index: $(Get-MetricValue (Join-Path $RunPath '07-extraction-bench.log') 'vector_index')"
Add-SummaryLine ""
Write-Summary

if ($ExtractionThreshold.Count -gt 0) {
  Invoke-Gate "extraction benchmark thresholds" "07b-extraction-thresholds.log" {
    python (Join-Path $Root "scripts/check_benchmark_thresholds.py") (Join-Path $RunPath "07-extraction-bench.log") @ExtractionThreshold
  }
}

Invoke-Gate "storage retrieval benchmark" "08-storage-bench.log" {
  & (Join-Path $BuildPath "graphenedb_rc5_storage_retrieval_bench.exe") $StorageNodes $StorageQueries $Dim --vector-index $StorageVectorIndex
}
Add-SummaryLine "- storage_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath '08-storage-bench.log') 'vector_index_requested')"
Add-SummaryLine "- storage_vector_index: $(Get-MetricValue (Join-Path $RunPath '08-storage-bench.log') 'vector_index')"
Add-SummaryLine ""
Write-Summary

if ($StorageThreshold.Count -gt 0) {
  Invoke-Gate "storage benchmark thresholds" "08b-storage-thresholds.log" {
    python (Join-Path $Root "scripts/check_benchmark_thresholds.py") (Join-Path $RunPath "08-storage-bench.log") @StorageThreshold
  }
}

Add-SummaryLine "# Final Status"
Add-SummaryLine ""
Add-SummaryLine "PASS"
Write-Summary
Write-Output "graphenedb_ga_readiness_passed=true"
Write-Output $Summary
