param(
  [int]$Runs = 1000,
  [string]$BuildDir = "build-fuzz",
  [string]$Config = "RelWithDebInfo",
  [string]$CC = "clang",
  [string]$CXX = "clang++",
  [string]$Out = "reports/RC_COVERAGE_FUZZ_OUTPUT.txt"
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

if (-not (Get-Command $CC -ErrorAction SilentlyContinue)) {
  throw "C compiler not found: $CC"
}
if (-not (Get-Command $CXX -ErrorAction SilentlyContinue)) {
  throw "C++ compiler not found: $CXX"
}

$isWindowsHost = $env:OS -eq "Windows_NT"
$targetTriple = (& $CXX -dumpmachine 2>$null | Select-Object -First 1).Trim()
if ($isWindowsHost -and $targetTriple -like "*windows-gnu*") {
  throw "libFuzzer is not supported by the current Clang target '$targetTriple'. Use a libFuzzer-capable Clang toolchain on Linux/WSL or a supported Windows Clang setup."
}

$oldCc = $env:CC
$oldCxx = $env:CXX
try {
  $env:CC = $CC
  $env:CXX = $CXX
  cmake -S $Root -B $BuildPath "-DCMAKE_BUILD_TYPE=${Config}" -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_EXAMPLES=OFF -DGRAPHENEDB_BUILD_FUZZERS=ON
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

  cmake --build $BuildPath --target graphenedb_wal_fuzzer --config $Config -j 2
  if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
} finally {
  $env:CC = $oldCc
  $env:CXX = $oldCxx
}

$Exe = Resolve-Executable -Directory $BuildPath -Name "graphenedb_wal_fuzzer" -ConfigName $Config
Write-Utf8Transcript -OutPath $OutPath -FailureMessage "coverage fuzz gate failed" -Command { & $Exe "-runs=$Runs" }
