param(
  [string]$BuildDir = "build-package",
  [string]$InstallDir = "build-package-install",
  [string]$ConsumerBuildDir = "build-package-consumer",
  [string]$Config = "Release",
  [int]$BuildJobs = 1
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildPath = Join-Path $Root $BuildDir
$InstallPath = Join-Path $Root $InstallDir
$ConsumerBuildPath = Join-Path $Root $ConsumerBuildDir
$ConsumerSource = Join-Path $Root "tests/package_consumer"

function Invoke-Checked {
  param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Command)
  if ($Command.Length -gt 1) {
    & $Command[0] @($Command[1..($Command.Length - 1)])
  } else {
    & $Command[0]
  }
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed with exit code ${LASTEXITCODE}: $($Command -join ' ')"
  }
}

foreach ($Path in @($BuildPath, $InstallPath, $ConsumerBuildPath)) {
  $ResolvedRoot = [System.IO.Path]::GetFullPath($Root)
  $ResolvedPath = [System.IO.Path]::GetFullPath($Path)
  if (!$ResolvedPath.StartsWith($ResolvedRoot)) {
    throw "Refusing to remove path outside repo: $ResolvedPath"
  }
  if (Test-Path $ResolvedPath) {
    Remove-Item -LiteralPath $ResolvedPath -Recurse -Force
  }
}

Invoke-Checked cmake -S $Root -B $BuildPath `
  "-DCMAKE_BUILD_TYPE=${Config}" `
  "-DCMAKE_INSTALL_PREFIX=${InstallPath}" `
  -DGRAPHENEDB_BUILD_TESTS=OFF `
  -DGRAPHENEDB_BUILD_BENCH=OFF `
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF

Invoke-Checked cmake --build $BuildPath --config $Config -j $BuildJobs
Invoke-Checked cmake --install $BuildPath --config $Config

Invoke-Checked cmake -S $ConsumerSource -B $ConsumerBuildPath `
  "-DCMAKE_BUILD_TYPE=${Config}" `
  "-DCMAKE_PREFIX_PATH=${InstallPath}"

Invoke-Checked cmake --build $ConsumerBuildPath --config $Config -j $BuildJobs

$Exe = Join-Path $ConsumerBuildPath "graphenedb_package_consumer.exe"
if (!(Test-Path $Exe)) {
  $Exe = Join-Path $ConsumerBuildPath "$Config/graphenedb_package_consumer.exe"
}
if (!(Test-Path $Exe)) {
  $Exe = Join-Path $ConsumerBuildPath "graphenedb_package_consumer"
}

Invoke-Checked $Exe
Write-Output "graphenedb_package_install_verified=true"
