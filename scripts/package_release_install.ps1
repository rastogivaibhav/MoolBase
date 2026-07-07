param(
  [string]$BuildDir = "build-release-package",
  [string]$InstallDir = "build-release-install",
  [string]$Out = "graphenedb-install-package.zip",
  [string]$Config = "Release",
  [int]$BuildJobs = 1
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildPath = Join-Path $Root $BuildDir
$InstallPath = Join-Path $Root $InstallDir
$OutPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $Root $Out }

cmake -S $Root -B $BuildPath `
  "-DCMAKE_BUILD_TYPE=${Config}" `
  "-DCMAKE_INSTALL_PREFIX=${InstallPath}" `
  -DGRAPHENEDB_BUILD_TESTS=OFF `
  -DGRAPHENEDB_BUILD_BENCH=OFF `
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

cmake --build $BuildPath -j $BuildJobs
if ($LASTEXITCODE -ne 0) { throw "build failed" }

cmake --install $BuildPath
if ($LASTEXITCODE -ne 0) { throw "install failed" }

& (Join-Path $Root "scripts/verify_package_install.ps1") `
  -BuildDir "build-release-package-verify" `
  -InstallDir $InstallDir `
  -ConsumerBuildDir "build-release-package-consumer" `
  -Config $Config `
  -BuildJobs $BuildJobs
if ($LASTEXITCODE -ne 0) { throw "package verification failed" }

if (Test-Path $OutPath) {
  Remove-Item -LiteralPath $OutPath -Force
}
Compress-Archive -Path $InstallPath -DestinationPath $OutPath -Force
& (Join-Path $Root "scripts/write_release_manifest.ps1") `
  -Package $OutPath `
  -InstallDir $InstallPath
Write-Output $OutPath
