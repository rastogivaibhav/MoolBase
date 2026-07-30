param(
  [string]$BuildDir = "build-rc-bundle-ga",
  [string]$GaReportDir = "reports/ga-readiness",
  [string]$EvidenceDir = "reports/ga-evidence",
  [string]$PackageOut = "graphenedb-install-package.zip",
  [string]$Config = "Release",
  [string]$ProfileLabel = "release-candidate-smoke",
  [string]$CtestExclude = "graphenedb_(c_api|rc_(crash|fuzz|kosh_adapter|stress|1m_storage|soak))_tests",
  [string]$FocusedRegex = "graphenedb_(acid_lattice|lattice)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests",
  [int]$VectorIndexRecallNodes = 1000,
  [int]$VectorIndexRecallQueries = 20,
  [int]$VectorIndexRecallDim = 16,
  [int]$VectorIndexRecallK = 5,
  [string]$VectorIndexRecallKind = "auto",
  [double]$VectorIndexRecallMin = 0.999,
  [int]$ExtractionDocs = 3,
  [int]$ExtractionNodesPerDoc = 5,
  [int]$ExtractionQueries = 2,
  [string]$ExtractionVectorIndex = "auto",
  [int]$StorageNodes = 500,
  [int]$StorageQueries = 5,
  [int]$Dim = 16,
  [string]$StorageVectorIndex = "auto",
  [int]$BuildJobs = 1,
  [switch]$UseFaiss
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$PackagePath = if ([System.IO.Path]::IsPathRooted($PackageOut)) { $PackageOut } else { Join-Path $Root $PackageOut }

$gaParams = @{
  BuildDir = $BuildDir
  ReportDir = $GaReportDir
  Config = $Config
  ProfileLabel = $ProfileLabel
  SkipPackage = $true
  CtestExclude = $CtestExclude
  FocusedRegex = $FocusedRegex
  VectorIndexRecallNodes = $VectorIndexRecallNodes
  VectorIndexRecallQueries = $VectorIndexRecallQueries
  VectorIndexRecallDim = $VectorIndexRecallDim
  VectorIndexRecallK = $VectorIndexRecallK
  VectorIndexRecallKind = $VectorIndexRecallKind
  VectorIndexRecallMin = $VectorIndexRecallMin
  ExtractionDocs = $ExtractionDocs
  ExtractionNodesPerDoc = $ExtractionNodesPerDoc
  ExtractionQueries = $ExtractionQueries
  ExtractionVectorIndex = $ExtractionVectorIndex
  StorageNodes = $StorageNodes
  StorageQueries = $StorageQueries
  Dim = $Dim
  StorageVectorIndex = $StorageVectorIndex
  BuildJobs = $BuildJobs
}
if ($UseFaiss) { $gaParams.UseFaiss = $true }
$gaOutput = & (Join-Path $Root "scripts/run_ga_readiness.ps1") @gaParams
if ($LASTEXITCODE -ne 0) { throw "run_ga_readiness failed" }
$gaSummary = ($gaOutput | Select-Object -Last 1).Trim()
$gaSummaryPath = Join-Path (Split-Path -Parent $gaSummary) "GA_READINESS_SUMMARY.md"

$packageOutput = & (Join-Path $Root "scripts/package_release_install.ps1") `
  -BuildDir "build-rc-bundle-package" `
  -InstallDir "build-rc-bundle-install" `
  -Out $PackagePath `
  -Config $Config `
  -BuildJobs $BuildJobs
if ($LASTEXITCODE -ne 0) { throw "package_release_install failed" }
$packageArtifact = ($packageOutput | Select-Object -Last 1).Trim()

$gaRunDir = Split-Path -Parent $gaSummary
function Get-MarkdownFieldValue {
  param(
    [string]$Path,
    [string]$Key
  )
  if (-not (Test-Path -LiteralPath $Path)) { return "" }
  $match = Select-String -Path $Path -Pattern ("^- " + [regex]::Escape($Key) + ":\s*(.+)$") | Select-Object -First 1
  if ($match) { return $match.Matches[0].Groups[1].Value.Trim() }
  return ""
}

$evidenceOutput = & (Join-Path $Root "scripts/collect_ga_evidence.ps1") `
  -OutDir $EvidenceDir `
  -Config "release-candidate" `
  -GaReadinessDir $gaRunDir `
  -PackagePath $packageArtifact `
  -Archive
if ($LASTEXITCODE -ne 0) { throw "collect_ga_evidence failed" }

$statusOutput = & (Join-Path $Root "scripts/write_ga_status_report.ps1")
if ($LASTEXITCODE -ne 0) { throw "write_ga_status_report failed" }
$statusReport = ($statusOutput | Select-Object -Last 1).Trim()
$statusReportPath = Join-Path $Root "reports/GA_STATUS_REPORT.md"

$evidenceBundle = ($evidenceOutput | Where-Object { $_ -like "evidence_bundle=*" } | Select-Object -Last 1)
$evidenceArchive = ($evidenceOutput | Where-Object { $_ -like "evidence_archive=*" } | Select-Object -Last 1)
$evidenceBundlePath = if ($evidenceBundle) { ($evidenceBundle -split "=", 2)[1] } else { "" }
$evidenceArchivePath = if ($evidenceArchive) { ($evidenceArchive -split "=", 2)[1] } else { "" }
$bundleMetaPath = Join-Path $Root "reports/RELEASE_CANDIDATE_BUNDLE_META.json"

$bundleMeta = [ordered]@{
  generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
  profile_label = $ProfileLabel
  status_report = $statusReportPath
  ga_summary = $gaSummaryPath
  evidence_bundle = $evidenceBundlePath
  evidence_archive = $evidenceArchivePath
  package_path = $packageArtifact
  package_sha256 = "$packageArtifact.sha256"
  package_manifest = "$packageArtifact.manifest.json"
  requested = [ordered]@{
    vector_index_recall_kind = $VectorIndexRecallKind
    extraction_vector_index = $ExtractionVectorIndex
    storage_vector_index = $StorageVectorIndex
    use_faiss = [bool]$UseFaiss
  }
  resolved = [ordered]@{
    vector_index_recall = (Get-MarkdownFieldValue $gaSummaryPath "vector_index_recall")
    vector_index_recall_requested = (Get-MarkdownFieldValue $gaSummaryPath "vector_index_recall_requested")
    vector_index_recall_mean_recall_at_k = (Get-MarkdownFieldValue $gaSummaryPath "vector_index_recall_mean_recall_at_k")
    extraction_vector_index = (Get-MarkdownFieldValue $gaSummaryPath "extraction_vector_index")
    extraction_vector_index_requested = (Get-MarkdownFieldValue $gaSummaryPath "extraction_vector_index_requested")
    storage_vector_index = (Get-MarkdownFieldValue $gaSummaryPath "storage_vector_index")
    storage_vector_index_requested = (Get-MarkdownFieldValue $gaSummaryPath "storage_vector_index_requested")
    graphenedb_use_faiss = (Get-MarkdownFieldValue $gaSummaryPath "graphenedb_use_faiss")
  }
}
$bundleMeta | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $bundleMetaPath
python (Join-Path $Root "scripts/validate_release_candidate_bundle_meta.py") $bundleMetaPath
if ($LASTEXITCODE -ne 0) { throw "validate_release_candidate_bundle_meta failed" }

Write-Output "release_candidate_bundle=true"
Write-Output "ga_summary=$gaSummary"
Write-Output "package=$packageArtifact"
Write-Output "bundle_meta=$bundleMetaPath"
Write-Output "status_report=$statusReportPath"
Write-Output $statusReport
if ($evidenceBundle) { Write-Output $evidenceBundle }
if ($evidenceArchive) { Write-Output $evidenceArchive }
