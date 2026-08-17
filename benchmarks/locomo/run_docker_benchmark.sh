#!/bin/bash
# Run LoCoMo benchmark in Docker Desktop

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=========================================="
echo "GrapheneDB LoCoMo Benchmark - Docker Run"
echo "=========================================="
echo ""

# Check if Docker is running
if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker not found. Please install Docker Desktop."
    exit 1
fi

if ! docker ps &> /dev/null; then
    echo "ERROR: Docker daemon not running. Please start Docker Desktop."
    exit 1
fi

echo "[1] Building Docker image..."
docker build \
    -f "$SCRIPT_DIR/Dockerfile" \
    -t graphenedb-locomo-benchmark:latest \
    "$REPO_ROOT"

echo ""
echo "[2] Running benchmark in container..."
docker run \
    --rm \
    -v "$SCRIPT_DIR/results:/work/benchmarks/locomo/results" \
    graphenedb-locomo-benchmark:latest

echo ""
echo "[3] Benchmark complete!"
echo "Results saved to: $SCRIPT_DIR/results/"
echo ""
echo "To view results:"
echo "  cat $SCRIPT_DIR/results/benchmark_report.txt"
