# GrapheneDB server image for controlled pilots.
# Pin BUILD_IMAGE/RUNTIME_IMAGE to organisation-approved immutable digests in release CI.
ARG BUILD_IMAGE=debian:bookworm-slim
ARG RUNTIME_IMAGE=debian:bookworm-slim

FROM ${BUILD_IMAGE} AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DGRAPHENEDB_BUILD_TESTS=OFF \
      -DGRAPHENEDB_BUILD_BENCH=OFF \
      -DGRAPHENEDB_BUILD_EXAMPLES=OFF \
      -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE" \
      -DCMAKE_EXE_LINKER_FLAGS="-pie -Wl,-z,relro,-z,now" \
 && cmake --build build --target graphenedb_server graphenedb_healthcheck -j2 \
 && strip build/graphenedb_server build/graphenedb_healthcheck

FROM ${RUNTIME_IMAGE}
LABEL org.opencontainers.image.title="GrapheneDB Server" \
      org.opencontainers.image.description="Physical hex-lattice AI memory database server" \
      org.opencontainers.image.licenses="Apache-2.0"
RUN groupadd --system --gid 10001 graphenedb \
 && useradd --system --uid 10001 --gid 10001 --no-create-home --shell /usr/sbin/nologin graphenedb \
 && mkdir -p /var/lib/graphenedb \
 && chown 10001:10001 /var/lib/graphenedb
COPY --from=build --chown=0:0 /src/build/graphenedb_server /usr/local/bin/graphenedb_server
COPY --from=build --chown=0:0 /src/build/graphenedb_healthcheck /usr/local/bin/graphenedb_healthcheck
USER 10001:10001
WORKDIR /var/lib/graphenedb
VOLUME ["/var/lib/graphenedb"]
EXPOSE 8080
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 CMD ["/usr/local/bin/graphenedb_healthcheck", "127.0.0.1", "8080"]
ENTRYPOINT ["/usr/local/bin/graphenedb_server", "/var/lib/graphenedb", "64", "8080", "--bind-address", "0.0.0.0", "--behind-tls-proxy", "--physical-lattice-primary", "--physical-lattice-radius", "600", "--expected-max-nodes", "1000000", "--wal-rotate-bytes", "268435456", "--workers", "8", "--queue-capacity", "1024", "--rate-limit-rps", "200", "--rate-limit-burst", "400"]
