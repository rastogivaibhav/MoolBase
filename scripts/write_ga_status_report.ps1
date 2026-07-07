param(
  [string]$Out = "reports/GA_STATUS_REPORT.md"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }
$Scorecard = Join-Path $Root "docs/GA_READINESS_SCORECARD.md"
$ReportLines = New-Object System.Collections.Generic.List[string]

function Add-ReportLine {
  param([string]$Line = "")
  $ReportLines.Add($Line) | Out-Null
  Write-Report
}

function Write-Report {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath) | Out-Null
  $TempOut = "$OutPath.tmp"
  $text = ($ReportLines -join [Environment]::NewLine) + [Environment]::NewLine
  [System.IO.File]::WriteAllText($TempOut, $text)
  Move-Item -Force -LiteralPath $TempOut -Destination $OutPath
}

function Get-LatestDirectory {
  param([string]$RelativePath)
  $path = Join-Path $Root $RelativePath
  $dirs = Get-ChildItem -LiteralPath $path -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
  if ($dirs.Count -gt 0) { return $dirs[0].FullName }
  return ""
}

function Get-LatestPassingGaDirectory {
  $path = Join-Path $Root "reports/ga-readiness"
  $dirs = Get-ChildItem -LiteralPath $path -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
  foreach ($dir in $dirs) {
    $summary = Join-Path $dir.FullName "GA_READINESS_SUMMARY.md"
    if (Test-GaPass $summary) { return $dir.FullName }
  }
  if ($dirs.Count -gt 0) { return $dirs[0].FullName }
  return ""
}

function Get-LatestPassingPreviewDirectory {
  $path = Join-Path $Root "reports/preview-hardware"
  $dirs = Get-ChildItem -LiteralPath $path -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
  foreach ($dir in $dirs) {
    $summary = Join-Path $dir.FullName "PREVIEW_HARDWARE_SUMMARY.md"
    if ((Test-Path -LiteralPath $summary) -and ((Get-Content $summary | Where-Object { $_ -ne "" } | Select-Object -Last 1) -eq "PASS")) {
      return $dir.FullName
    }
  }
  if ($dirs.Count -gt 0) { return $dirs[0].FullName }
  return ""
}

function Get-LatestEvidenceDirectory {
  $path = Join-Path $Root "reports/ga-evidence"
  $dirs = Get-ChildItem -LiteralPath $path -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
  foreach ($dir in $dirs) {
    $manifest = Join-Path $dir.FullName "EVIDENCE_MANIFEST.json"
    if (Test-Path -LiteralPath $manifest) { return $dir.FullName }
  }
  if ($dirs.Count -gt 0) { return $dirs[0].FullName }
  return ""
}

function Test-GaPass {
  param([string]$SummaryPath)
  if (-not (Test-Path -LiteralPath $SummaryPath)) { return $false }
  $lines = Get-Content $SummaryPath | Where-Object { $_ -ne "" }
  return ($lines | Select-Object -Last 1) -eq "PASS"
}

function Get-SectionItems {
  param(
    [string]$Path,
    [string]$Header
  )
  $lines = Get-Content $Path
  $items = New-Object System.Collections.Generic.List[string]
  $inSection = $false
  foreach ($line in $lines) {
    if ($line -eq $Header) {
      $inSection = $true
      continue
    }
    if ($inSection -and $line -match '^## ') { break }
    if (-not $inSection) { continue }
    if ($line -match '^\d+\.\s+(.*)$') {
      $items.Add($Matches[1]) | Out-Null
    } elseif ($line -match '^- (.*)$') {
      $items.Add($Matches[1]) | Out-Null
    }
  }
  return @($items)
}

function Read-JsonFile {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  try {
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
  } catch {
    return $null
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

function Get-CtestFailedTests {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return @() }
  $tests = New-Object System.Collections.Generic.List[string]
  foreach ($line in Get-Content -LiteralPath $Path) {
    if ($line -match '^\d+:(.+)$') {
      $tests.Add($Matches[1].Trim()) | Out-Null
    }
  }
  return @($tests | Select-Object -Unique)
}

function Test-CtestSucceeded {
  param([string]$SuccessPath, [string]$FailurePath)
  if (-not (Test-Path -LiteralPath $SuccessPath)) { return $false }
  if (-not (Test-Path -LiteralPath $FailurePath)) { return $true }
  $successTime = (Get-Item -LiteralPath $SuccessPath).LastWriteTimeUtc
  $failureTime = (Get-Item -LiteralPath $FailurePath).LastWriteTimeUtc
  return $successTime -gt $failureTime
}

function Filter-PendingItems {
  param(
    [string[]]$Items,
    [bool]$ApprovedHost,
    [bool]$IntendedHardware
  )
  $result = New-Object System.Collections.Generic.List[string]
  foreach ($item in $Items) {
    if ($ApprovedHost -and $item -eq "Run the GA readiness harness on a host that does not block freshly built executables.") { continue }
    if ($ApprovedHost -and $item -eq "Run the full default GA readiness harness on an approved build host without local policy exclusions.") { continue }
    if ($IntendedHardware -and $item -eq "Publish preserved benchmark reports for the intended developer-preview hardware profile.") { continue }
    $result.Add($item) | Out-Null
  }
  return @($result)
}

$latestGaDir = Get-LatestPassingGaDirectory
$latestGaAttemptDir = Get-LatestDirectory "reports/ga-readiness"
$latestEvidenceDir = Get-LatestEvidenceDirectory
$latestEnterpriseDir = Get-LatestDirectory "reports/enterprise-ga"
$latestPreviewProfileDir = Get-LatestPassingPreviewDirectory
$latestGaSummary = if ($latestGaDir) { Join-Path $latestGaDir "GA_READINESS_SUMMARY.md" } else { "" }
$latestGaAttemptSummary = if ($latestGaAttemptDir) { Join-Path $latestGaAttemptDir "GA_READINESS_SUMMARY.md" } else { "" }
$latestEvidenceManifest = if ($latestEvidenceDir) { Join-Path $latestEvidenceDir "EVIDENCE_MANIFEST.json" } else { "" }
$latestEnterpriseSummary = if ($latestEnterpriseDir) { Join-Path $latestEnterpriseDir "ENTERPRISE_GA_SUMMARY.md" } else { "" }
$latestPreviewProfileSummary = if ($latestPreviewProfileDir) { Join-Path $latestPreviewProfileDir "PREVIEW_HARDWARE_SUMMARY.md" } else { "" }
$latestEnterprise100kOutput = if ($latestEnterpriseDir) { Join-Path $latestEnterpriseDir "RC_STRESS_100K_OUTPUT.txt" } else { "" }
$latestEnterprise1mOutput = if ($latestEnterpriseDir) { Join-Path $latestEnterpriseDir "RC_STRESS_1M_STORAGE_OUTPUT.txt" } else { "" }
$latestPreviewVectorRecallOutput = if ($latestPreviewProfileDir) { Join-Path $latestPreviewProfileDir "VECTOR_INDEX_RECALL_OUTPUT.txt" } else { "" }
$latestPreviewExtractionOutput = if ($latestPreviewProfileDir) { Join-Path $latestPreviewProfileDir "EXTRACTION_INGEST_OUTPUT.txt" } else { "" }
$latestPreviewStorageOutput = if ($latestPreviewProfileDir) { Join-Path $latestPreviewProfileDir "RC5_STORAGE_RETRIEVAL_OUTPUT.txt" } else { "" }
$latestGaHostProfile = if ($latestGaDir) { Join-Path $latestGaDir "HOST_PROFILE.json" } else { "" }
$latestPreviewHostProfile = if ($latestPreviewProfileDir) { Join-Path $latestPreviewProfileDir "HOST_PROFILE.json" } else { "" }
$latestGaAttemptHostProfile = if ($latestGaAttemptDir) { Join-Path $latestGaAttemptDir "HOST_PROFILE.json" } else { "" }
$latestEnterpriseHostProfile = if ($latestEnterpriseDir) { Join-Path $latestEnterpriseDir "HOST_PROFILE.json" } else { "" }
$gaHostProfile = Read-JsonFile $latestGaHostProfile
$previewHostProfile = Read-JsonFile $latestPreviewHostProfile
$gaAttemptHostProfile = Read-JsonFile $latestGaAttemptHostProfile
$enterpriseHostProfile = Read-JsonFile $latestEnterpriseHostProfile
$approvedHost = [bool]($gaHostProfile -and $gaHostProfile.approved_host)
$intendedPreviewHardware = [bool]($previewHostProfile -and $previewHostProfile.intended_hardware)
$enterpriseApprovedHost = [bool]($enterpriseHostProfile -and $enterpriseHostProfile.approved_host)
$enterprise100kDim = 0
$enterprise1mDim = 0
if (Test-Path -LiteralPath $latestEnterprise100kOutput) { $enterprise100kDim = [int](Get-MetricValue $latestEnterprise100kOutput 'dim') }
if (Test-Path -LiteralPath $latestEnterprise1mOutput) { $enterprise1mDim = [int](Get-MetricValue $latestEnterprise1mOutput 'dim') }
$enterpriseRichHarnessPresent = (Test-Path -LiteralPath $latestEnterprise100kOutput) -and (Test-Path -LiteralPath $latestEnterprise1mOutput)
$enterpriseTargetDimReady = ($enterprise100kDim -ge 384) -and ($enterprise1mDim -ge 768)
$gaProfileLabel = if ($gaHostProfile) { [string]$gaHostProfile.profile_label } else { "" }
$previewProfileLabel = if ($previewHostProfile) { [string]$previewHostProfile.profile_label } else { "" }
$gaAttemptProfileLabel = if ($gaAttemptHostProfile) { [string]$gaAttemptHostProfile.profile_label } else { "" }
$enterpriseProfileLabel = if ($enterpriseHostProfile) { [string]$enterpriseHostProfile.profile_label } else { "" }

$package = Join-Path $Root "graphenedb-install-package.zip"
$packageSha = "$package.sha256"
$packageManifest = "$package.manifest.json"
$bundleMeta = Join-Path $Root "reports/RELEASE_CANDIDATE_BUNDLE_META.json"
$recoveryOutput = Join-Path $Root "reports/RECOVERY_REHEARSAL_OUTPUT.txt"
$koshOutput = Join-Path $Root "reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt"
$vectorRecallOutput = Join-Path $Root "reports/VECTOR_INDEX_RECALL_OUTPUT.txt"
$ctestFailedLog = Join-Path $Root "build-release/Testing/Temporary/LastTestsFailed.log"
$ctestLastLog = Join-Path $Root "build-release/Testing/Temporary/LastTest.log"
$rcBundleReport = Join-Path $Root "reports/GA_PROGRESS_RC_BUNDLE.md"
$harnessReport = Join-Path $Root "reports/GA_PROGRESS_GA_HARNESS.md"
$filesystemReport = Join-Path $Root "reports/GA_PROGRESS_FILESYSTEM_FAILURES.md"
$extractionReport = Join-Path $Root "reports/GA_PROGRESS_EXTRACTION_CONTRACT.md"
$ctestBlockedTests = Get-CtestFailedTests $ctestFailedLog
if (Test-CtestSucceeded $ctestLastLog $ctestFailedLog) { $ctestBlockedTests = @() }

$localEvidence = @(
  [ordered]@{ name = "Latest GA readiness summary"; present = (Test-Path $latestGaSummary); detail = $latestGaSummary; pass = (Test-GaPass $latestGaSummary) },
  [ordered]@{ name = "Latest GA evidence manifest"; present = (Test-Path $latestEvidenceManifest); detail = $latestEvidenceManifest; pass = (Test-Path $latestEvidenceManifest) },
  [ordered]@{ name = "Install package archive"; present = (Test-Path $package); detail = $package; pass = (Test-Path $package) },
  [ordered]@{ name = "Install package sha256"; present = (Test-Path $packageSha); detail = $packageSha; pass = (Test-Path $packageSha) },
  [ordered]@{ name = "Install package manifest"; present = (Test-Path $packageManifest); detail = $packageManifest; pass = (Test-Path $packageManifest) },
  [ordered]@{ name = "Release candidate bundle metadata"; present = (Test-Path $bundleMeta); detail = $bundleMeta; pass = (Test-Path $bundleMeta) },
  [ordered]@{ name = "Recovery rehearsal output"; present = (Test-Path $recoveryOutput); detail = $recoveryOutput; pass = ((Test-Path $recoveryOutput) -and ((Get-Content $recoveryOutput -Raw) -match 'recovery_rehearsal_passed=true')) },
  [ordered]@{ name = "Kosh adapter gate output"; present = (Test-Path $koshOutput); detail = $koshOutput; pass = ((Test-Path $koshOutput) -and ((Get-Content $koshOutput -Raw) -match 'rc_real_kosh_adapter_gate_passed=true')) },
  [ordered]@{ name = "Vector index recall output"; present = (Test-Path $vectorRecallOutput); detail = $vectorRecallOutput; pass = ((Test-Path $vectorRecallOutput) -and ((Get-Content $vectorRecallOutput -Raw) -match 'mean_recall_at_k=')) },
  [ordered]@{ name = "RC bundle progress report"; present = (Test-Path $rcBundleReport); detail = $rcBundleReport; pass = (Test-Path $rcBundleReport) },
  [ordered]@{ name = "GA harness progress report"; present = (Test-Path $harnessReport); detail = $harnessReport; pass = (Test-Path $harnessReport) },
  [ordered]@{ name = "Filesystem failure progress report"; present = (Test-Path $filesystemReport); detail = $filesystemReport; pass = (Test-Path $filesystemReport) },
  [ordered]@{ name = "Extraction contract progress report"; present = (Test-Path $extractionReport); detail = $extractionReport; pass = (Test-Path $extractionReport) }
)

$mustFix = Filter-PendingItems -Items (Get-SectionItems $Scorecard "## Must-fix before enterprise GA") -ApprovedHost $approvedHost -IntendedHardware $intendedPreviewHardware
$shouldFix = Filter-PendingItems -Items (Get-SectionItems $Scorecard "## Should-fix before public developer preview") -ApprovedHost $approvedHost -IntendedHardware $intendedPreviewHardware
$mustFixCount = @($mustFix).Count
$shouldFixCount = @($shouldFix).Count
$passedLocal = @($localEvidence | Where-Object { $_.pass }).Count
$totalLocal = $localEvidence.Count

Add-ReportLine "# GrapheneDB GA Status Report"
Add-ReportLine "- generated_at_utc: $((Get-Date).ToUniversalTime().ToString("o"))"
Add-ReportLine "- local_evidence_passed: $passedLocal/$totalLocal"
Add-ReportLine "- latest_ga_readiness_dir: $latestGaDir"
Add-ReportLine "- latest_ga_attempt_dir: $latestGaAttemptDir"
Add-ReportLine "- latest_ga_evidence_dir: $latestEvidenceDir"
Add-ReportLine "- latest_enterprise_ga_dir: $latestEnterpriseDir"
Add-ReportLine "- latest_preview_hardware_dir: $latestPreviewProfileDir"
Add-ReportLine
Add-ReportLine "## Locally Evidenced"
foreach ($item in $localEvidence) {
  $status = if ($item.pass) { "PASS" } elseif ($item.present) { "PARTIAL" } else { "MISSING" }
  Add-ReportLine "- [$status] $($item.name): ``$($item.detail)``"
}
if ($ctestBlockedTests.Count -gt 0) {
  Add-ReportLine
  Add-ReportLine "## Local CTest Blockers"
  Add-ReportLine "- latest_failed_log: ``$ctestFailedLog``"
  foreach ($test in $ctestBlockedTests) {
    Add-ReportLine "- $test"
  }
}
Add-ReportLine
Add-ReportLine "## Enterprise Campaign"
if ($latestEnterpriseSummary) {
  $enterpriseStatus = if (Test-GaPass $latestEnterpriseSummary) { "PASS" } elseif (Test-Path -LiteralPath $latestEnterpriseSummary) { "PARTIAL" } else { "MISSING" }
  Add-ReportLine "- [$enterpriseStatus] Latest enterprise campaign summary: ``$latestEnterpriseSummary``"
} else {
  Add-ReportLine "- [MISSING] Latest enterprise campaign summary: ``$latestEnterpriseSummary``"
}
Add-ReportLine "- [$(if ($enterpriseHostProfile) { if ($enterpriseApprovedHost) { 'PASS' } else { 'PARTIAL' } } else { 'MISSING' })] Host profile: ``$latestEnterpriseHostProfile``"
if ($enterpriseHostProfile) {
  Add-ReportLine "- approved_host: $enterpriseApprovedHost"
  Add-ReportLine "- profile_label: $enterpriseProfileLabel"
}
Add-ReportLine "- rich_workload_harness_present: $enterpriseRichHarnessPresent"
Add-ReportLine "- target_scale_dimensions_ready: $enterpriseTargetDimReady"
if (Test-Path -LiteralPath $latestEnterprise100kOutput) {
  Add-ReportLine "- 100k_dim: $enterprise100kDim"
  Add-ReportLine "- 100k_vector_index_requested: $(Get-MetricValue $latestEnterprise100kOutput 'vector_index_requested')"
  Add-ReportLine "- 100k_vector_index: $(Get-MetricValue $latestEnterprise100kOutput 'vector_index')"
  Add-ReportLine "- 100k_ingest_nodes_per_sec: $(Get-MetricValue $latestEnterprise100kOutput 'ingest_nodes_per_sec')"
  Add-ReportLine "- 100k_causal_hit_rate: $(Get-MetricValue $latestEnterprise100kOutput 'causal_hit_rate')"
  Add-ReportLine "- 100k_causal_p95_ms: $(Get-MetricValue $latestEnterprise100kOutput 'causal_p95_ms')"
  Add-ReportLine "- 100k_cross_layer_edges: $(Get-MetricValue $latestEnterprise100kOutput 'cross_layer_edges')"
}
if (Test-Path -LiteralPath $latestEnterprise1mOutput) {
  Add-ReportLine "- 1m_dim: $enterprise1mDim"
  Add-ReportLine "- 1m_vector_index_requested: $(Get-MetricValue $latestEnterprise1mOutput 'vector_index_requested')"
  Add-ReportLine "- 1m_vector_index: $(Get-MetricValue $latestEnterprise1mOutput 'vector_index')"
  Add-ReportLine "- 1m_causal_root_hit_rate: $(Get-MetricValue $latestEnterprise1mOutput 'causal_root_hit_rate')"
  Add-ReportLine "- 1m_causal_lattice_p95_ms: $(Get-MetricValue $latestEnterprise1mOutput 'causal_lattice_p95_ms')"
  Add-ReportLine "- 1m_metadata_service_p95_ms: $(Get-MetricValue $latestEnterprise1mOutput 'metadata_service_p95_ms')"
  Add-ReportLine "- 1m_cross_layer_edges: $(Get-MetricValue $latestEnterprise1mOutput 'cross_layer_edges')"
}
if (Test-Path -LiteralPath $latestEnterpriseSummary) {
  Add-ReportLine "- full_ctest_completed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'full_ctest_completed')"
  Add-ReportLine "- ga_readiness_completed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'ga_readiness_completed')"
  Add-ReportLine "- fuzz_completed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'fuzz_completed')"
  Add-ReportLine "- soak_completed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'soak_completed')"
  Add-ReportLine "- filesystem_gate_passed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'filesystem_gate_passed')"
  Add-ReportLine "- disk_pressure_gate_passed: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'disk_pressure_gate_passed')"
  Add-ReportLine "- full_day_soak_profile_ready: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'full_day_soak_profile_ready')"
  Add-ReportLine "- release_like_profile_ready: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'release_like_profile_ready')"
  Add-ReportLine "- graphenedb_use_faiss: $(Get-MarkdownFieldValue $latestEnterpriseSummary 'graphenedb_use_faiss')"
}
Add-ReportLine
Add-ReportLine "## GA Host Attestation"
if ($gaHostProfile) {
  Add-ReportLine "- [$(if ($approvedHost) { 'PASS' } else { 'PARTIAL' })] Host profile: ``$latestGaHostProfile``"
  Add-ReportLine "- approved_host: $approvedHost"
  Add-ReportLine "- profile_label: $gaProfileLabel"
} else {
  Add-ReportLine "- [MISSING] Host profile: ``$latestGaHostProfile``"
}
Add-ReportLine
Add-ReportLine "## Latest Passing GA Benchmarks"
if (Test-Path -LiteralPath $latestGaSummary) {
  Add-ReportLine "- graphenedb_use_faiss: $(Get-MarkdownFieldValue $latestGaSummary 'graphenedb_use_faiss')"
  Add-ReportLine "- vector_index_recall_requested: $(Get-MarkdownFieldValue $latestGaSummary 'vector_index_recall_requested')"
  Add-ReportLine "- vector_index_recall: $(Get-MarkdownFieldValue $latestGaSummary 'vector_index_recall')"
  Add-ReportLine "- vector_index_recall_mean_recall_at_k: $(Get-MarkdownFieldValue $latestGaSummary 'vector_index_recall_mean_recall_at_k')"
  Add-ReportLine "- extraction_vector_index_requested: $(Get-MarkdownFieldValue $latestGaSummary 'extraction_vector_index_requested')"
  Add-ReportLine "- extraction_vector_index: $(Get-MarkdownFieldValue $latestGaSummary 'extraction_vector_index')"
  Add-ReportLine "- storage_vector_index_requested: $(Get-MarkdownFieldValue $latestGaSummary 'storage_vector_index_requested')"
  Add-ReportLine "- storage_vector_index: $(Get-MarkdownFieldValue $latestGaSummary 'storage_vector_index')"
} else {
  Add-ReportLine "- [MISSING] Latest passing GA summary: ``$latestGaSummary``"
}
Add-ReportLine
Add-ReportLine "## Latest GA Attempt"
if ($latestGaAttemptSummary) {
  $attemptStatus = if (Test-GaPass $latestGaAttemptSummary) { "PASS" } elseif (Test-Path -LiteralPath $latestGaAttemptSummary) { "PARTIAL" } else { "MISSING" }
  Add-ReportLine "- [$attemptStatus] Latest attempted GA summary: ``$latestGaAttemptSummary``"
} else {
  Add-ReportLine "- [MISSING] Latest attempted GA summary: ``$latestGaAttemptSummary``"
}
if ($gaAttemptHostProfile) {
  Add-ReportLine "- [PARTIAL] Latest attempted GA host profile: ``$latestGaAttemptHostProfile``"
  Add-ReportLine "- profile_label: $gaAttemptProfileLabel"
  Add-ReportLine "- approved_host: $([bool]$gaAttemptHostProfile.approved_host)"
} else {
  Add-ReportLine "- [MISSING] Latest attempted GA host profile: ``$latestGaAttemptHostProfile``"
}
Add-ReportLine
Add-ReportLine "## Preview Hardware Profile"
if ($latestPreviewProfileSummary) {
  $previewStatus = if (Test-GaPass $latestPreviewProfileSummary) { "PASS" } elseif (Test-Path -LiteralPath $latestPreviewProfileSummary) { "PARTIAL" } else { "MISSING" }
  Add-ReportLine "- [$previewStatus] Latest preview hardware summary: ``$latestPreviewProfileSummary``"
} else {
  Add-ReportLine "- [MISSING] Latest preview hardware summary: ``$latestPreviewProfileSummary``"
}
Add-ReportLine "- [$(if ($previewHostProfile) { if ($intendedPreviewHardware) { 'PASS' } else { 'PARTIAL' } } else { 'MISSING' })] Host profile: ``$latestPreviewHostProfile``"
if ($previewHostProfile) {
  Add-ReportLine "- intended_hardware: $intendedPreviewHardware"
  Add-ReportLine "- profile_label: $previewProfileLabel"
}
if (Test-Path -LiteralPath $latestPreviewProfileSummary) {
  Add-ReportLine "- graphenedb_use_faiss: $(Get-MarkdownFieldValue $latestPreviewProfileSummary 'graphenedb_use_faiss')"
}
if (Test-Path -LiteralPath $latestPreviewVectorRecallOutput) {
  Add-ReportLine "- preview_vector_index_requested: $(Get-MetricValue $latestPreviewVectorRecallOutput 'vector_index_requested')"
  Add-ReportLine "- preview_vector_index: $(Get-MetricValue $latestPreviewVectorRecallOutput 'vector_index')"
  Add-ReportLine "- preview_mean_recall_at_k: $(Get-MetricValue $latestPreviewVectorRecallOutput 'mean_recall_at_k')"
}
if (Test-Path -LiteralPath $latestPreviewExtractionOutput) {
  Add-ReportLine "- extraction_vector_index_requested: $(Get-MetricValue $latestPreviewExtractionOutput 'vector_index_requested')"
  Add-ReportLine "- extraction_vector_index: $(Get-MetricValue $latestPreviewExtractionOutput 'vector_index')"
}
if (Test-Path -LiteralPath $latestPreviewStorageOutput) {
  Add-ReportLine "- storage_vector_index_requested: $(Get-MetricValue $latestPreviewStorageOutput 'vector_index_requested')"
  Add-ReportLine "- storage_vector_index: $(Get-MetricValue $latestPreviewStorageOutput 'vector_index')"
}
Add-ReportLine
Add-ReportLine "## Pending Counts"
Add-ReportLine "- public_developer_preview: $shouldFixCount"
Add-ReportLine "- enterprise_ga: $mustFixCount"
Add-ReportLine
Add-ReportLine "## Pending For Public Developer Preview"
foreach ($item in $shouldFix) {
  Add-ReportLine "- $item"
}
Add-ReportLine
Add-ReportLine "## Pending For Enterprise GA"
foreach ($item in $mustFix) {
  Add-ReportLine "- $item"
}
Add-ReportLine
Add-ReportLine "## Summary"
Add-ReportLine "GrapheneDB has strong local evidence for controlled-pilot and release-candidate readiness, but enterprise GA is still blocked on long-running soak/fuzz, target-host/full-filesystem campaigns, target-scale performance, live integration decisions, and release governance."

Write-Output "ga_status_report=$OutPath"
