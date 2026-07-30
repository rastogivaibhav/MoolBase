param(
  [int]$Nodes = 1000000,
  [int]$Queries = 5,
  [int]$Dim = 64,
  [string]$VectorIndex = "auto",
  [string]$BuildDir = "build-release",
  [string]$Config = "Release",
  [switch]$UseFaiss,
  [string]$Out = "reports/RC_STRESS_1M_STORAGE_OUTPUT.txt"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Utf8Transcript = Join-Path $Root "scripts/write_utf8_transcript.ps1"
. $Utf8Transcript
$BuildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }

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

cmake -S $Root -B $BuildPath "-DCMAKE_BUILD_TYPE=${Config}" -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON "-DGRAPHENEDB_USE_FAISS=$(if ($UseFaiss) { 'ON' } else { 'OFF' })"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

cmake --build $BuildPath --target graphenedb_rc_1m_storage_tests --config $Config -j 1
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

$Exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_rc_1m_storage_tests" -ConfigName $Config
Write-Utf8Transcript -OutPath $OutPath -FailureMessage "1M storage gate failed" -Command { & $Exe --nodes $Nodes --queries $Queries --dim $Dim --vector-index $VectorIndex --reopen 1 }
