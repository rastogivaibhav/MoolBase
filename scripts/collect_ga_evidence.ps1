param(
  [string]$OutDir = "reports/ga-evidence",
  [string]$Config = "developer-preview",
  [string]$GaReadinessDir = "",
  [string]$PackagePath = "",
  [switch]$Archive
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutRoot = Join-Path $Root $OutDir
$BundleDir = Join-Path $OutRoot $Stamp
$ManifestPath = Join-Path $BundleDir "EVIDENCE_MANIFEST.json"
$SummaryPath = Join-Path $BundleDir "EVIDENCE_SUMMARY.md"
$Utf8 = [System.Text.UTF8Encoding]::new($false)

function Resolve-RepoPath {
  param([string]$Path)
  if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
  if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
  return (Join-Path $Root $Path)
}

function Copy-IfExists {
  param(
    [string]$Source,
    [string]$DestinationRelative,
    [string]$Kind,
    [bool]$Required = $false
  )
  $src = Resolve-RepoPath $Source
  $dest = Join-Path $BundleDir $DestinationRelative
  $record = [ordered]@{
    kind = $Kind
    source = $src
    destination = $dest
    required = $Required
    present = $false
  }
  if (Test-Path -LiteralPath $src) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
    Copy-Item -LiteralPath $src -Destination $dest -Recurse -Force
    $record.present = $true
    if ((Test-Path -LiteralPath $dest -PathType Leaf)) {
      $record.sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $dest).Hash.ToLowerInvariant()
      $record.bytes = (Get-Item -LiteralPath $dest).Length
    }
  }
  return $record
}

function Latest-GaRun {
  $runs = Get-ChildItem -LiteralPath (Join-Path $Root "reports/ga-readiness") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
  foreach ($run in $runs) {
    $summary = Join-Path $run.FullName "GA_READINESS_SUMMARY.md"
    if ((Test-Path -LiteralPath $summary) -and ((Get-Content $summary | Where-Object { $_ -ne "" } | Select-Object -Last 1) -eq "PASS")) {
      return $run.FullName
    }
  }
  if ($runs.Count -gt 0) { return $runs[0].FullName }
  return ""
}

function Latest-GaAttemptRun {
  $runs = Get-ChildItem -LiteralPath (Join-Path $Root "reports/ga-readiness") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
  if ($runs.Count -gt 0) { return $runs[0].FullName }
  return ""
}

function Latest-EnterpriseRun {
  $runs = Get-ChildItem -LiteralPath (Join-Path $Root "reports/enterprise-ga") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
  if ($runs.Count -gt 0) { return $runs[0].FullName }
  return ""
}

function Latest-PreviewProfileRun {
  $runs = Get-ChildItem -LiteralPath (Join-Path $Root "reports/preview-hardware") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
  foreach ($run in $runs) {
    $summary = Join-Path $run.FullName "PREVIEW_HARDWARE_SUMMARY.md"
    if ((Test-Path -LiteralPath $summary) -and ((Get-Content $summary | Where-Object { $_ -ne "" } | Select-Object -Last 1) -eq "PASS")) {
      return $run.FullName
    }
  }
  if ($runs.Count -gt 0) { return $runs[0].FullName }
  return ""
}

New-Item -ItemType Directory -Force -Path $BundleDir | Out-Null

if ([string]::IsNullOrWhiteSpace($GaReadinessDir)) {
  $GaReadinessDir = Latest-GaRun
}
$DefaultPackagePath = Join-Path $Root "graphenedb-install-package.zip"
if ([string]::IsNullOrWhiteSpace($PackagePath) -and (Test-Path -LiteralPath $DefaultPackagePath)) {
  $PackagePath = $DefaultPackagePath
}
$GaAttemptDir = Latest-GaAttemptRun
$EnterpriseRunDir = Latest-EnterpriseRun
$PreviewProfileRunDir = Latest-PreviewProfileRun

$records = New-Object System.Collections.Generic.List[object]

$docs = @(
  "README.md",
  "SECURITY.md",
  "LICENSE",
  "CHANGELOG.md",
  "docs/GA_READINESS_SCORECARD.md",
  "docs/NEXT_GA_EXECUTION_PLAN.md",
  "docs/GA_READINESS_VERIFICATION.md",
  "docs/GRAPHENE_LATTICE_MODEL.md",
  "docs/LATTICE_RETRIEVAL.md",
  "docs/EXTRACTION_INGESTION.md",
  "docs/PACKAGING_DISTRIBUTION.md",
  "docs/OPERATIONAL_RECOVERY.md",
  "docs/STORAGE_FORMAT.md",
  "docs/SAFETY_AND_LIMITATIONS.md",
  "docs/PLATFORM_SUPPORT.md",
  "docs/C_API.md",
  "docs/CI_RELEASE_AUTOMATION.md",
  "docs/V1_RC_ACCEPTANCE_REPORT.md",
  "docs/RELEASE_CHECKLIST.md"
)
foreach ($doc in $docs) {
  $records.Add((Copy-IfExists $doc $doc "doc" $true)) | Out-Null
}

$reports = @(
  "reports/RC5_ACID_REPORT.md",
  "reports/RC5_CRASH_MATRIX.md",
  "reports/RC5_STORAGE_RETRIEVAL_PERF.md",
  "reports/EXTRACTION_INGEST_PERF.md",
  "reports/VECTOR_BASELINE_COMPARISON.md",
  "reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt",
  "reports/VECTOR_INDEX_RECALL.md",
  "reports/VECTOR_INDEX_RECALL_OUTPUT.txt",
  "reports/RECOVERY_REHEARSAL.md",
  "reports/RECOVERY_REHEARSAL_OUTPUT.txt",
  "reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt",
  "reports/KOSH_ADAPTER_GATE.md",
  "reports/RELEASE_CANDIDATE_BUNDLE.md",
  "reports/GA_STATUS_REPORT.md",
  "reports/GA_PROGRESS_RC_BUNDLE.md",
  "reports/GA_PROGRESS_GA_HARNESS.md",
  "reports/GA_PROGRESS_FILESYSTEM_FAILURES.md",
  "reports/GA_PROGRESS_EXTRACTION_CONTRACT.md",
  "reports/RELEASE_CANDIDATE_BUNDLE_META.json"
)
foreach ($report in $reports) {
  $records.Add((Copy-IfExists $report $report "report" $false)) | Out-Null
}

if (![string]::IsNullOrWhiteSpace($GaReadinessDir)) {
  $records.Add((Copy-IfExists $GaReadinessDir "ga-readiness/latest" "ga_readiness_run" $false)) | Out-Null
}

if (![string]::IsNullOrWhiteSpace($GaAttemptDir)) {
  $records.Add((Copy-IfExists $GaAttemptDir "ga-readiness/latest-attempt" "ga_readiness_attempt_run" $false)) | Out-Null
}

if (![string]::IsNullOrWhiteSpace($EnterpriseRunDir)) {
  $records.Add((Copy-IfExists $EnterpriseRunDir "enterprise-ga/latest" "enterprise_ga_run" $false)) | Out-Null
}

if (![string]::IsNullOrWhiteSpace($PreviewProfileRunDir)) {
  $records.Add((Copy-IfExists $PreviewProfileRunDir "preview-hardware/latest" "preview_hardware_run" $false)) | Out-Null
}

if (![string]::IsNullOrWhiteSpace($PackagePath)) {
  $records.Add((Copy-IfExists $PackagePath (Join-Path "package" (Split-Path -Leaf $PackagePath)) "package" $false)) | Out-Null
  $records.Add((Copy-IfExists "$PackagePath.sha256" (Join-Path "package" ((Split-Path -Leaf $PackagePath) + ".sha256")) "package_sha256" $false)) | Out-Null
  $records.Add((Copy-IfExists "$PackagePath.manifest.json" (Join-Path "package" ((Split-Path -Leaf $PackagePath) + ".manifest.json")) "package_manifest" $false)) | Out-Null
}

$gitStatusPath = Join-Path $BundleDir "git-status.txt"
try {
  git -C $Root status --short > $gitStatusPath
  $records.Add([ordered]@{
    kind = "git_status"
    source = "git status --short"
    destination = $gitStatusPath
    required = $false
    present = $true
    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $gitStatusPath).Hash.ToLowerInvariant()
    bytes = (Get-Item -LiteralPath $gitStatusPath).Length
  }) | Out-Null
} catch {
  $records.Add([ordered]@{
    kind = "git_status"
    source = "git status --short"
    destination = $gitStatusPath
    required = $false
    present = $false
    error = "$_"
  }) | Out-Null
}

$missingRequired = @($records | Where-Object { $_.required -and -not $_.present })
$missingOptional = @($records | Where-Object { -not $_.required -and -not $_.present })
$missingRequiredSources = @($missingRequired | ForEach-Object { [string]$_['source'] })
$missingOptionalSources = @($missingOptional | ForEach-Object { [string]$_['source'] })
$artifactRecords = @($records | ForEach-Object { $_ })

$manifest = [ordered]@{
  name = "GrapheneDB GA evidence bundle"
  generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
  config = $Config
  root = $Root
  bundle_dir = $BundleDir
  ga_readiness_dir = $GaReadinessDir
  package_path = $PackagePath
  missing_required = $missingRequiredSources
  missing_optional = $missingOptionalSources
  artifacts = $artifactRecords
}

$manifest | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $ManifestPath

[System.IO.File]::WriteAllText($SummaryPath, "# GrapheneDB GA Evidence Bundle" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- generated_at_utc: $($manifest.generated_at_utc)" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- config: $Config" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- bundle_dir: $BundleDir" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- ga_readiness_dir: $GaReadinessDir" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- package_path: $PackagePath" + [Environment]::NewLine, $Utf8)
if ($PackagePath) {
  [System.IO.File]::AppendAllText($SummaryPath, "- package_sha256: $PackagePath.sha256" + [Environment]::NewLine, $Utf8)
  [System.IO.File]::AppendAllText($SummaryPath, "- package_manifest: $PackagePath.manifest.json" + [Environment]::NewLine, $Utf8)
}
[System.IO.File]::AppendAllText($SummaryPath, "- missing_required_count: $($missingRequired.Count)" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "- missing_optional_count: $($missingOptional.Count)" + [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, [Environment]::NewLine, $Utf8)
[System.IO.File]::AppendAllText($SummaryPath, "See `EVIDENCE_MANIFEST.json` for the artifact inventory and hashes." + [Environment]::NewLine, $Utf8)

if ($missingRequired.Count -gt 0) {
  Write-Error "Missing required evidence: $($missingRequiredSources -join ', ')"
}

if ($Archive) {
  $ArchivePath = "$BundleDir.zip"
  if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
  Compress-Archive -LiteralPath $BundleDir -DestinationPath $ArchivePath -Force
  Write-Output "evidence_archive=$ArchivePath"
}

Write-Output "evidence_bundle=$BundleDir"
Write-Output "evidence_manifest=$ManifestPath"
Write-Output "missing_optional_count=$($missingOptional.Count)"

exit 0
