FROM debian:bookworm-slim AS graphenedb-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake ninja-build python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
RUN cmake -S /src -B /build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DGRAPHENEDB_BUILD_TESTS=OFF \
      -DGRAPHENEDB_BUILD_SERVER=ON \
      -DGRAPHENEDB_BUILD_BENCH=OFF \
      -DGRAPHENEDB_BUILD_EXAMPLES=OFF \
    && cmake --build /build --target graphenedb_server --parallel 2

FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    EVIDENCE_LAB_PUBLIC_MODE=true \
    GRAPHENEDB_SERVER_BINARY=/opt/graphenedb/bin/graphenedb_server

RUN groupadd --gid 10001 evidence-lab \
    && useradd --create-home --uid 10001 --gid 10001 evidence-lab
WORKDIR /app

COPY apps/evidence_lab/gateway/pyproject.toml /app/pyproject.toml
COPY apps/evidence_lab/gateway/evidence_lab /app/evidence_lab
RUN pip install --no-cache-dir .

COPY --from=graphenedb-build /build/graphenedb_server /opt/graphenedb/bin/graphenedb_server
RUN chown -R evidence-lab:evidence-lab /app /opt/graphenedb \
    && chmod 0555 /opt/graphenedb/bin/graphenedb_server \
    && chmod -R go-w /app /opt/graphenedb

USER 10001:10001
ENTRYPOINT ["python", "-m", "evidence_lab.worker_entrypoint"]
