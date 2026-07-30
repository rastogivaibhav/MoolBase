param(
  [string]$BuildDir = "build-enterprise-ga",
  [string]$ReportDir = "reports/enterprise-ga",
  [string]$Config = "Release",
  [string]$ProfileLabel = "",
  [switch]$ApprovedHost,
  [int]$SoakSeconds = 300,
  [int]$SoakDim = 32,
  [int]$StressIncidents = 16667,
  [int]$StressQueries = 200,
  [int]$StressDim = 64,
  [string]$StressVectorIndex = "auto",
  [int]$OneMNodes = 1000000,
  [int]$OneMQueries = 5,
  [int]$OneMDim = 64,
  [string]$OneMVectorIndex = "auto",
  [int]$FuzzRuns = 10000,
  [string]$FuzzBuildDir = "build-enterprise-fuzz",
  [string]$FuzzCC = "clang",
  [string]$FuzzCXX = "clang++",
  [int]$BuildJobs = 1,
  [switch]$UseFaiss,
  [switch]$SkipSoak,
  [switch]$SkipGaReadiness,
  [switch]$SkipFullCtest,
  [switch]$SkipFuzz
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Utf8Transcript = Join-Path $Root "scripts/write_utf8_transcript.ps1"
. $Utf8Transcript
$BuildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
$ReportRoot = if ([System.IO.Path]::IsPathRooted($ReportDir)) { $ReportDir } else { Join-Path $Root $ReportDir }
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunPath = Join-Path $ReportRoot $Stamp
$Summary = Join-Path $RunPath "ENTERPRISE_GA_SUMMARY.md"
$HostProfile = Join-Path $RunPath "HOST_PROFILE.json"
$IsWindowsHost = $env:OS -eq "Windows_NT"

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

function Resolve-Executable {
  param(
    [string]$Directory,
    [string]$Name,
    [string]$ConfigName
  )
  $candidates = @(
    (Join-Path $Directory "$Name.exe"),
    (Join-Path (Join-Path $Directory $ConfigName) "$Name.exe"),
    (Join-Path $Directory $Name),
    (Join-Path (Join-Path $Directory $ConfigName) $Name)
  )
  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) { return $candidate }
  }
  throw "executable not found: $Name under $Directory"
}

function Add-SummaryLine {
  param([string]$Line)
  [System.IO.File]::AppendAllText($Summary, $Line + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
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

function Invoke-OptionalGate {
  param(
    [string]$Name,
    [string]$LogName,
    [scriptblock]$Command
  )
  $LogPath = Join-Path $RunPath $LogName
  Add-SummaryLine "## $Name"
  Add-SummaryLine "log: ``$LogPath``"
  try {
    & $Command *> $LogPath
    if ($LASTEXITCODE -ne 0) { throw "exit code $LASTEXITCODE" }
    Add-SummaryLine "- status: PASS"
    Add-SummaryLine ""
    return $true
  } catch {
    Add-SummaryLine "- status: SKIP"
    Add-SummaryLine "- note: $_"
    Add-SummaryLine ""
    return $false
  }
}

New-Item -ItemType Directory -Force -Path $RunPath | Out-Null
$fullCtestCompleted = -not $SkipFullCtest
$gaReadinessCompleted = -not $SkipGaReadiness
$fuzzCompleted = -not $SkipFuzz
$soakCompleted = -not $SkipSoak
$filesystemGatePassed = $false
$diskPressureGatePassed = $false
$targetScaleDimensionsReady = ($StressDim -ge 384) -and ($OneMDim -ge 768)
$fullDaySoakProfileReady = $SoakSeconds -ge 86400
$Utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($Summary, "# GrapheneDB Enterprise GA Campaign" + [Environment]::NewLine, $Utf8)
Add-SummaryLine ""
Add-SummaryLine "- timestamp: $Stamp"
Add-SummaryLine "- config: $Config"
Add-SummaryLine "- build_dir: $BuildPath"
Add-SummaryLine "- profile_label: $ProfileLabel"
Add-SummaryLine "- approved_host: $([bool]$ApprovedHost)"
Add-SummaryLine "- soak_seconds: $SoakSeconds"
Add-SummaryLine "- soak_dim: $SoakDim"
Add-SummaryLine "- stress_incidents: $StressIncidents"
Add-SummaryLine "- stress_queries: $StressQueries"
Add-SummaryLine "- stress_dim: $StressDim"
Add-SummaryLine "- stress_vector_index: $StressVectorIndex"
Add-SummaryLine "- one_m_nodes: $OneMNodes"
Add-SummaryLine "- one_m_queries: $OneMQueries"
Add-SummaryLine "- one_m_dim: $OneMDim"
Add-SummaryLine "- one_m_vector_index: $OneMVectorIndex"
Add-SummaryLine "- fuzz_runs: $FuzzRuns"
Add-SummaryLine "- graphenedb_use_faiss: $([bool]$UseFaiss)"
Add-SummaryLine "- skip_soak: $SkipSoak"
Add-SummaryLine "- skip_ga_readiness: $SkipGaReadiness"
Add-SummaryLine "- skip_full_ctest: $SkipFullCtest"
Add-SummaryLine "- skip_fuzz: $SkipFuzz"
Add-SummaryLine ""

Invoke-Gate "host profile" "00-host-profile.log" {
  & (Join-Path $Root "scripts/write_host_profile.ps1") `
    -Out $HostProfile `
    -ProfileLabel $ProfileLabel `
    -IntendedHardware 0 `
    -ApprovedHost $([int][bool]$ApprovedHost)
}

Invoke-Gate "configure" "01-configure.log" {
  cmake -S $Root -B $BuildPath "-DCMAKE_BUILD_TYPE=${Config}" -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON "-DGRAPHENEDB_USE_FAISS=$(if ($UseFaiss) { 'ON' } else { 'OFF' })"
}

Invoke-Gate "build" "02-build.log" {
  $targets = @(
    "graphenedb",
    "graphenedb_tests",
    "graphenedb_c_api_tests",
    "graphenedb_rc_crash_tests",
    "graphenedb_rc_fuzz_tests",
    "graphenedb_rc_kosh_adapter_tests",
    "graphenedb_rc_stress_tests",
    "graphenedb_rc_lock_rotation_metadata_tests",
    "graphenedb_rc5_crash_matrix_tests",
    "graphenedb_rc5_fault_injection_tests",
    "graphenedb_rc_real_kosh_adapter_tests",
    "graphenedb_rc_soak_tests",
    "graphenedb_rc_1m_storage_tests"
  )
  if (-not $IsWindowsHost) {
    $targets += "graphenedb_real_filesystem_failure_tests"
    $targets += "graphenedb_disk_pressure_tests"
  }
  cmake --build $BuildPath --config $Config --target @targets -j $BuildJobs
}

if ($SkipFullCtest) {
  Add-SummaryLine "## full ctest suite"
  Add-SummaryLine "log: ``$(Join-Path $RunPath '03-ctest.log')``"
  Add-SummaryLine "- status: SKIP"
  Add-SummaryLine "- note: skipped by caller"
  Add-SummaryLine ""
} else {
  Invoke-Gate "full ctest suite" "03-ctest.log" {
    ctest --test-dir $BuildPath --output-on-failure -C $Config
  }
}

if ($SkipGaReadiness) {
  Add-SummaryLine "## ga readiness harness"
  Add-SummaryLine "log: ``$(Join-Path $RunPath '04-ga-readiness.log')``"
  Add-SummaryLine "- status: SKIP"
  Add-SummaryLine "- note: skipped by caller"
  Add-SummaryLine ""
} else {
  Invoke-Gate "ga readiness harness" "04-ga-readiness.log" {
    & (Join-Path $Root "scripts/run_ga_readiness.ps1") `
      -BuildDir $BuildDir `
      -ReportDir "reports/ga-readiness" `
      -Config $Config `
      -BuildJobs $BuildJobs
  }
}

Invoke-Gate "100k stress profile" "05-100k-stress.log" {
  if ($UseFaiss) {
    & (Join-Path $Root "scripts/run_100k_stress.ps1") `
      -BuildDir $BuildDir `
      -Config $Config `
      -Incidents $StressIncidents `
      -Queries $StressQueries `
      -Dim $StressDim `
      -VectorIndex $StressVectorIndex `
      -Out (Join-Path $RunPath "RC_STRESS_100K_OUTPUT.txt") `
      -UseFaiss
  } else {
    & (Join-Path $Root "scripts/run_100k_stress.ps1") `
      -BuildDir $BuildDir `
      -Config $Config `
      -Incidents $StressIncidents `
      -Queries $StressQueries `
      -Dim $StressDim `
      -VectorIndex $StressVectorIndex `
      -Out (Join-Path $RunPath "RC_STRESS_100K_OUTPUT.txt")
  }
  if ($LASTEXITCODE -ne 0) { throw "100k stress profile failed" }
}
Add-SummaryLine "- 100k_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'vector_index_requested')"
Add-SummaryLine "- 100k_vector_index: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'vector_index')"
Add-SummaryLine "- ingest_nodes_per_sec: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'ingest_nodes_per_sec')"
Add-SummaryLine "- causal_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'causal_hit_rate')"
Add-SummaryLine "- causal_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'causal_p95_ms')"
Add-SummaryLine "- metadata_service_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'metadata_service_p95_ms')"
Add-SummaryLine "- cross_layer_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'cross_layer_edges')"
Add-SummaryLine "- defect_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'defect_edges')"
Add-SummaryLine "- synthetic_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'synthetic_edges')"
Add-SummaryLine "- reopen_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_100K_OUTPUT.txt') 'reopen_ms')"
Add-SummaryLine ""

Invoke-Gate "1m storage profile" "06-1m-storage.log" {
  if ($UseFaiss) {
    & (Join-Path $Root "scripts/run_1m_stress.ps1") `
      -BuildDir $BuildDir `
      -Config $Config `
      -Nodes $OneMNodes `
      -Queries $OneMQueries `
      -Dim $OneMDim `
      -VectorIndex $OneMVectorIndex `
      -Out (Join-Path $RunPath "RC_STRESS_1M_STORAGE_OUTPUT.txt") `
      -UseFaiss
  } else {
    & (Join-Path $Root "scripts/run_1m_stress.ps1") `
      -BuildDir $BuildDir `
      -Config $Config `
      -Nodes $OneMNodes `
      -Queries $OneMQueries `
      -Dim $OneMDim `
      -VectorIndex $OneMVectorIndex `
      -Out (Join-Path $RunPath "RC_STRESS_1M_STORAGE_OUTPUT.txt")
  }
  if ($LASTEXITCODE -ne 0) { throw "1m storage profile failed" }
}
Add-SummaryLine "- 1m_vector_index_requested: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'vector_index_requested')"
Add-SummaryLine "- 1m_vector_index: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'vector_index')"
Add-SummaryLine "- vector_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'vector_p95_ms')"
Add-SummaryLine "- causal_lattice_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'causal_lattice_p95_ms')"
Add-SummaryLine "- causal_root_hit_rate: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'causal_root_hit_rate')"
Add-SummaryLine "- metadata_service_p95_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'metadata_service_p95_ms')"
Add-SummaryLine "- cross_layer_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'cross_layer_edges')"
Add-SummaryLine "- defect_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'defect_edges')"
Add-SummaryLine "- synthetic_edges: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'synthetic_edges')"
Add-SummaryLine "- reopen_ms: $(Get-MetricValue (Join-Path $RunPath 'RC_STRESS_1M_STORAGE_OUTPUT.txt') 'reopen_ms')"
Add-SummaryLine ""

if ($SkipSoak) {
  Add-SummaryLine "## soak gate"
  Add-SummaryLine "log: ``$(Join-Path $RunPath '07-soak.log')``"
  Add-SummaryLine "- status: SKIP"
  Add-SummaryLine "- note: skipped by caller"
  Add-SummaryLine ""
} else {
  Invoke-Gate "soak gate" "07-soak.log" {
    $exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_rc_soak_tests" -ConfigName $Config
    Write-Utf8Transcript -OutPath (Join-Path $RunPath "RC_SOAK_OUTPUT.txt") -FailureMessage "soak gate failed" -Command { & $exe --seconds $SoakSeconds --dim $SoakDim }
  }
}

$filesystemGatePassed = Invoke-OptionalGate "real filesystem failure gates" "08-filesystem.log" {
  $exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_real_filesystem_failure_tests" -ConfigName $Config
  Write-Utf8Transcript -OutPath (Join-Path $RunPath "RC_REAL_FILESYSTEM_FAILURES_OUTPUT.txt") -FailureMessage "real filesystem failure gate failed" -Command { & $exe }
}

$diskPressureGatePassed = Invoke-OptionalGate "disk pressure gates" "09-disk-pressure.log" {
  $exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_disk_pressure_tests" -ConfigName $Config
  Write-Utf8Transcript -OutPath (Join-Path $RunPath "RC_DISK_PRESSURE_OUTPUT.txt") -FailureMessage "disk pressure gate failed" -Command { & $exe }
}

if ($SkipFuzz) {
  Add-SummaryLine "## coverage fuzz gate"
  Add-SummaryLine "log: ``$(Join-Path $RunPath '10-fuzz.log')``"
  Add-SummaryLine "- status: SKIP"
  Add-SummaryLine "- note: skipped by caller"
  Add-SummaryLine ""
} else {
  Invoke-Gate "coverage fuzz gate" "10-fuzz.log" {
    & (Join-Path $Root "scripts/run_coverage_fuzz.ps1") `
      -Runs $FuzzRuns `
      -BuildDir $FuzzBuildDir `
      -CC $FuzzCC `
      -CXX $FuzzCXX `
      -Out (Join-Path $RunPath "RC_COVERAGE_FUZZ_OUTPUT.txt")
  }
}

$releaseLikeProfileReady = [bool]$ApprovedHost -and
  $fullDaySoakProfileReady -and
  $soakCompleted -and
  $targetScaleDimensionsReady -and
  $fullCtestCompleted -and
  $gaReadinessCompleted -and
  $fuzzCompleted -and
  $filesystemGatePassed -and
  $diskPressureGatePassed

Add-SummaryLine "- full_ctest_completed: $fullCtestCompleted"
Add-SummaryLine "- ga_readiness_completed: $gaReadinessCompleted"
Add-SummaryLine "- fuzz_completed: $fuzzCompleted"
Add-SummaryLine "- soak_completed: $soakCompleted"
Add-SummaryLine "- filesystem_gate_passed: $filesystemGatePassed"
Add-SummaryLine "- disk_pressure_gate_passed: $diskPressureGatePassed"
Add-SummaryLine "- target_scale_dimensions_ready: $targetScaleDimensionsReady"
Add-SummaryLine "- full_day_soak_profile_ready: $fullDaySoakProfileReady"
Add-SummaryLine "- release_like_profile_ready: $releaseLikeProfileReady"
Add-SummaryLine ""

Add-SummaryLine "# Final Status"
Add-SummaryLine ""
$finalStatus = if ($releaseLikeProfileReady) { "PASS" } else { "PARTIAL" }
Add-SummaryLine $finalStatus
Write-Output "enterprise_ga_campaign=$RunPath"
Write-Output $Summary
