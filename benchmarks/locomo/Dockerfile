FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    make \
    python3 \
    python3-pip \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /work

# Clone GrapheneDB (use master branch)
RUN git clone https://github.com/rastogivaibhav/graphenedb_v1.git /work && \
    git checkout master

# Build GrapheneDB
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    cd ..

# Install Python dependencies for benchmark
RUN pip3 install numpy

# Create benchmark output directory
RUN mkdir -p benchmarks/locomo/results

# Run benchmark and save results
ENTRYPOINT ["bash", "-c", "python3 benchmarks/locomo/run_benchmark_simulation.py | tee benchmarks/locomo/results/benchmark_report.txt"]
