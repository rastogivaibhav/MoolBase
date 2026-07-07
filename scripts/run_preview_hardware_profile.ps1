param(
  [string]$BuildDir = "build-preview-profile",
  [string]$ReportDir = "reports/preview-hardware",
  [string]$Config = "Release",
  [string]$ProfileLabel = "",
  [switch]$IntendedHardware,
  [int]$VectorBaselineIncidents = 5000,
  [int]$VectorBaselineQueries = 200,
  [int]$VectorBaselineDim = 64,
  [int]$VectorIndexNodes = 5000,
  [int]$VectorIndexQueries = 200,
  [int]$VectorIndexDim = 32,
  [int]$VectorIndexK = 10,
  [string]$VectorIndex = "auto",
  [double]$VectorIndexMinRecall = 0.999,
  [int]$ExtractionDocs = 100,
  [int]$ExtractionNodesPerDoc = 50,
  [int]$ExtractionQueries = 100,
  [int]$ExtractionDim = 64,
  [string]$ExtractionVectorIndex = "auto",
  [int]$StorageNodes = 20000,
  [int]$StorageQueries = 100,
  [int]$StorageDim = 64,
  [string]$StorageVectorIndex = "auto",
  [int]$BuildJobs = 1,
  [switch]$UseFaiss
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Utf8Transcript = Join-Path $Root "scripts/write_utf8_transcript.ps1"
. $Utf8Transcript
$BuildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
$ReportRoot = if ([System.IO.Path]::IsPathRooted($ReportDir)) { $ReportDir } else { Join-Path $Root $ReportDir }
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunPath = Join-Path $ReportRoot $Stamp
$Summary = Join-Path $RunPath "PREVIEW_HARDWARE_SUMMARY.md"
$HostProfile = Join-Path $RunPath "HOST_PROFILE.json"
$Utf8 = [System.Text.UTF8Encoding]::new($false)

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

function Add-SummaryLine {
  param([string]$Line)
  [System.IO.File]::AppendAllText($Summary, $Line + [Environment]::NewLine, $Utf8)
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
  try {
    Write-Utf8Transcript -OutPath $LogPath -FailureMessage "$Name failed" -Command $Command
    if ($LASTEXITCODE -ne 0) { throw "exit code $LASTEXITCODE" }
    Add-SummaryLine "- status: PASS"
    Add-SummaryLine ""
  } catch {
    Add-SummaryLine "- status: FAIL"
    Add-SummaryLine "- error: $_"
    Add-SummaryLine ""
    Get-Content -LiteralPath $LogPath -ErrorAction SilentlyContinue | Select-Object -Last 80
    throw
  }
}

New-Item -ItemType Directory -Force -Path $RunPath | Out-Null
[System.IO.File]::WriteAllText($Summary, "# GrapheneDB Preview Hardware Profile" + [Environment]::NewLine, $Utf8)
Add-SummaryLine ""
Add-SummaryLine "- timestamp: $Stamp"
Add-SummaryLine "- config: $Config"
Add-SummaryLine "- build_dir: $BuildPath"
Add-SummaryLine "- profile_label: $ProfileLabel"
Add-SummaryLine "- intended_hardware: $([bool]$IntendedHardware)"
Add-SummaryLine "- graphenedb_use_faiss: $([bool]$UseFaiss)"
Add-SummaryLine "- vector_baseline: incidents=$VectorBaselineIncidents queries=$VectorBaselineQueries dim=$VectorBaselineDim"
Add-SummaryLine "- vector_index: nodes=$VectorIndexNodes queries=$VectorIndexQueries dim=$VectorIndexDim k=$VectorIndexK index=$VectorIndex min_recall=$VectorIndexMinRecall"
Add-SummaryLine "- extraction: docs=$ExtractionDocs nodes_per_doc=$ExtractionNodesPerDoc queries=$ExtractionQueries dim=$ExtractionDim index=$ExtractionVectorIndex"
Add-SummaryLine "- storage: nodes=$StorageNodes queries=$StorageQueries dim=$StorageDim index=$StorageVectorIndex"
Add-SummaryLine ""

Invoke-Gate "host profile" "00-host-profile.log" {
  & (Join-Path $Root "scripts/write_host_profile.ps1") `
    -Out $HostProfile `
    -ProfileLabel $ProfileLabel `
    -IntendedHardware $([int][bool]$IntendedHardware) `
    -ApprovedHost 0
}

Invoke-Gate "configure" "01-configure.log" {
  cmake -S $Root -B $BuildPath "-DCMAKE_BUILD_TYPE=${Config}" -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON "-DGRAPHENEDB_USE_FAISS=$(if ($UseFaiss) { 'ON' } else { 'OFF' })"
}

Invoke-Gate "build benchmark targets" "02-build.log" {
  cmake --build $BuildPath --config $Config --target graphenedb_vector_baseline_bench graphenedb_vector_index_recall_bench graphenedb_extraction_ingest_bench graphenedb_rc5_storage_retrieval_bench -j $BuildJobs
}

Invoke-Gate "vector baseline comparison" "03-vector-baseline.log" {
  & (Join-Path $Root "scripts/run_vector_baseline_bench.ps1") `
    -Incidents $VectorBaselineIncidents `
    -Queries $VectorBaselineQueries `
    -Dim $VectorBaselineDim `
    -BuildDir $BuildDir `
    -Out (Join-Path $RunPath "VECTOR_BASELINE_COMPARISON_OUTPUT.txt")
}
Add-SummaryLine "- vector_root_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_BASELINE_COMPARISON_OUTPUT.txt') 'vector_root_hit_rate')"
Add-SummaryLine "- causal_root_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_BASELINE_COMPARISON_OUTPUT.txt') 'causal_root_hit_rate')"
Add-SummaryLine "- causal_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_BASELINE_COMPARISON_OUTPUT.txt') 'causal_p95_ms')"
Add-SummaryLine ""

Invoke-Gate "vector index recall" "04-vector-index.log" {
  & (Join-Path $Root "scripts/run_vector_index_recall_bench.ps1") `
    -Nodes $VectorIndexNodes `
    -Queries $VectorIndexQueries `
    -Dim $VectorIndexDim `
    -K $VectorIndexK `
    -Index $VectorIndex `
    -MinRecall $VectorIndexMinRecall `
    -BuildDir $BuildDir `
    -Out (Join-Path $RunPath "VECTOR_INDEX_RECALL_OUTPUT.txt")
}
Add-SummaryLine "- mean_recall_at_k: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_INDEX_RECALL_OUTPUT.txt') 'mean_recall_at_k')"
Add-SummaryLine "- worst_recall_at_k: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_INDEX_RECALL_OUTPUT.txt') 'worst_recall_at_k')"
Add-SummaryLine "- vector_index_resolved: $(Get-MetricValue (Join-Path $RunPath 'VECTOR_INDEX_RECALL_OUTPUT.txt') 'vector_index')"
Add-SummaryLine ""

Invoke-Gate "extraction ingest performance" "05-extraction.log" {
  if ($UseFaiss) {
    & (Join-Path $Root "scripts/run_extraction_ingest_bench.ps1") `
      -Docs $ExtractionDocs `
      -NodesPerDoc $ExtractionNodesPerDoc `
      -Queries $ExtractionQueries `
      -Dim $ExtractionDim `
      -VectorIndex $ExtractionVectorIndex `
      -BuildDir $BuildDir `
      -Config $Config `
      -Out (Join-Path $RunPath "EXTRACTION_INGEST_OUTPUT.txt") `
      -UseFaiss
  } else {
    & (Join-Path $Root "scripts/run_extraction_ingest_bench.ps1") `
      -Docs $ExtractionDocs `
      -NodesPerDoc $ExtractionNodesPerDoc `
      -Queries $ExtractionQueries `
      -Dim $ExtractionDim `
      -VectorIndex $ExtractionVectorIndex `
      -BuildDir $BuildDir `
      -Config $Config `
      -Out (Join-Path $RunPath "EXTRACTION_INGEST_OUTPUT.txt")
  }
}
Add-SummaryLine "- extraction_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'vector_index_requested')"
Add-SummaryLine "- extraction_vector_index: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'vector_index')"
Add-SummaryLine "- extract_nodes_per_sec: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'extract_nodes_per_sec')"
Add-SummaryLine "- extract_doc_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'extract_doc_p95_ms')"
Add-SummaryLine "- causal_lattice_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'causal_lattice_p95_ms')"
Add-SummaryLine "- causal_root_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'causal_root_hit_rate')"
Add-SummaryLine "- metadata_doc_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'metadata_doc_p95_ms')"
Add-SummaryLine "- avg_lattice_neighbors: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'avg_lattice_neighbors')"
Add-SummaryLine "- reopen_ms: $(Get-MetricValue (Join-Path $RunPath 'EXTRACTION_INGEST_OUTPUT.txt') 'reopen_ms')"
Add-SummaryLine ""

Invoke-Gate "storage retrieval performance" "06-storage.log" {
  if ($UseFaiss) {
    & (Join-Path $Root "scripts/run_rc5_storage_retrieval_bench.ps1") `
      -Nodes $StorageNodes `
      -Queries $StorageQueries `
      -Dim $StorageDim `
      -VectorIndex $StorageVectorIndex `
      -BuildDir $BuildDir `
      -Config $Config `
      -Out (Join-Path $RunPath "RC5_STORAGE_RETRIEVAL_OUTPUT.txt") `
      -UseFaiss
  } else {
    & (Join-Path $Root "scripts/run_rc5_storage_retrieval_bench.ps1") `
      -Nodes $StorageNodes `
      -Queries $StorageQueries `
      -Dim $StorageDim `
      -VectorIndex $StorageVectorIndex `
      -BuildDir $BuildDir `
      -Config $Config `
      -Out (Join-Path $RunPath "RC5_STORAGE_RETRIEVAL_OUTPUT.txt")
  }
}
Add-SummaryLine "- storage_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'vector_index_requested')"
Add-SummaryLine "- storage_vector_index: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'vector_index')"
Add-SummaryLine "- ingest_nodes_per_sec: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'ingest_nodes_per_sec')"
Add-SummaryLine "- vector_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'vector_p95_ms')"
Add-SummaryLine "- causal_lattice_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'causal_lattice_p95_ms')"
Add-SummaryLine "- causal_root_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'causal_root_hit_rate')"
Add-SummaryLine "- metadata_service_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'metadata_service_p95_ms')"
Add-SummaryLine "- avg_lattice_neighbors: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'avg_lattice_neighbors')"
Add-SummaryLine "- cross_layer_edges: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'cross_layer_edges')"
Add-SummaryLine "- reopen_ms: $(Get-MetricValue (Join-Path $RunPath 'RC5_STORAGE_RETRIEVAL_OUTPUT.txt') 'reopen_ms')"
Add-SummaryLine ""

Add-SummaryLine "# Final Status"
Add-SummaryLine ""
Add-SummaryLine "PASS"
Write-Output "preview_hardware_profile=$RunPath"
Write-Output $Summary
