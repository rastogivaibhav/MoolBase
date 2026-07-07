param(
  [int]$Nodes = 20000,
  [int]$Queries = 100,
  [int]$Dim = 64,
  [string]$VectorIndex = "auto",
  [string]$BuildDir = "build-release",
  [string]$Config = "Release",
  [string]$Out = "reports/RC5_STORAGE_RETRIEVAL_OUTPUT.txt",
  [switch]$UseFaiss
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
if ($LASTEXITCODE -ne 0) {
  throw "storage retrieval benchmark configure failed"
}

cmake --build $BuildPath --target graphenedb_rc5_storage_retrieval_bench --config $Config -j 2
if ($LASTEXITCODE -ne 0) {
  throw "storage retrieval benchmark build failed"
}

$Exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_rc5_storage_retrieval_bench" -ConfigName $Config
Write-Utf8Transcript -OutPath $OutPath -FailureMessage "storage retrieval benchmark failed" -Command { & $Exe $Nodes $Queries $Dim --vector-index $VectorIndex }
