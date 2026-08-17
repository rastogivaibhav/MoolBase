FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /work

# Copy entire GrapheneDB source (already on host)
COPY . /work/

# Build GrapheneDB
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Install Python dependencies
RUN pip3 install numpy

# Create output directory
RUN mkdir -p benchmarks/locomo/results

# Run benchmark
ENTRYPOINT ["bash", "-c", "cd /work && python3 benchmarks/locomo/run_benchmark_simulation.py | tee benchmarks/locomo/results/benchmark_report.txt"]
