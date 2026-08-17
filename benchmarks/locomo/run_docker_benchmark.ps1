# Run LoCoMo benchmark in Docker Desktop (Windows)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommandPath
$RepoRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "GrapheneDB LoCoMo Benchmark - Docker Run" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Docker is installed
try {
    $DockerVersion = docker version --format "{{.Server.Version}}" 2>$null
    if (-not $DockerVersion) {
        throw "Docker not running"
    }
    Write-Host "[OK] Docker is running (version: $DockerVersion)" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Docker not found or not running." -ForegroundColor Red
    Write-Host "Please install and start Docker Desktop for Windows." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[1] Building Docker image..." -ForegroundColor Yellow

$BuildCmd = @(
    "build",
    "-f", "$ScriptDir/Dockerfile",
    "-t", "graphenedb-locomo-benchmark:latest",
    $RepoRoot
)

& docker @BuildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Docker build failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[2] Running benchmark in container..." -ForegroundColor Yellow

# Create results directory if it doesn't exist
$ResultsDir = Join-Path $ScriptDir "results"
if (-not (Test-Path $ResultsDir)) {
    New-Item -ItemType Directory -Path $ResultsDir | Out-Null
}

$RunCmd = @(
    "run",
    "--rm",
    "-v", "$($ResultsDir):/work/benchmarks/locomo/results",
    "graphenedb-locomo-benchmark:latest"
)

& docker @RunCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Benchmark execution failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[3] Benchmark complete!" -ForegroundColor Green
Write-Host "Results saved to: $ResultsDir" -ForegroundColor Green
Write-Host ""
Write-Host "To view results:" -ForegroundColor Cyan
Write-Host "  cat $ResultsDir/benchmark_report.txt"
