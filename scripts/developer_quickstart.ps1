[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [int]$Jobs = 2
)

$ErrorActionPreference = "Stop"
$RootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RootDir "build\quickstart"
}

if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    throw "'cmake' is required but was not found in PATH."
}

Write-Host "`n[1/3] Configuring GrapheneDB"
cmake -S $RootDir -B $BuildDir `
    -DGRAPHENEDB_BUILD_TESTS=OFF `
    -DGRAPHENEDB_BUILD_SERVER=OFF `
    -DGRAPHENEDB_BUILD_BENCH=OFF `
    -DGRAPHENEDB_BUILD_EXAMPLES=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "`n[2/3] Building the reasoning demo"
cmake --build $BuildDir --config Release --target graphenedb_hypokosh_runtime_demo --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Demo build failed." }

$Demo = Get-ChildItem -Path $BuildDir -Recurse -File -Filter "graphenedb_hypokosh_runtime_demo.exe" | Select-Object -First 1
if (-not $Demo) {
    throw "graphenedb_hypokosh_runtime_demo.exe was not produced."
}

Write-Host "`n[3/3] Running the demo"
& $Demo.FullName
if ($LASTEXITCODE -ne 0) { throw "Demo execution failed." }

Write-Host "`nGrapheneDB is ready to explore."
Write-Host "Runtime guide: docs/DEVELOPER_QUICKSTART.md"
Write-Host "Installed consumer: examples/installed_consumer"
