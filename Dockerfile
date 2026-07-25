# GrapheneDB server image for controlled pilots.
# The build base is pinned. The shipped runtime is scratch plus only the
# dynamic libraries required by the two GrapheneDB executables.
ARG BUILD_IMAGE=debian:bookworm-slim@sha256:7b140f374b289a7c2befc338f42ebe6441b7ea838a042bbd5acbfca6ec875818

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
 && strip build/graphenedb_server build/graphenedb_healthcheck \
 && mkdir -p \
      /runtime/etc \
      /runtime/lib/x86_64-linux-gnu \
      /runtime/lib64 \
      /runtime/usr/local/bin \
      /runtime/var/lib/graphenedb \
 && install -m 0755 build/graphenedb_server /runtime/usr/local/bin/graphenedb_server \
 && install -m 0755 build/graphenedb_healthcheck /runtime/usr/local/bin/graphenedb_healthcheck \
 && cp -L /lib/x86_64-linux-gnu/libstdc++.so.6 /runtime/lib/x86_64-linux-gnu/ \
 && cp -L /lib/x86_64-linux-gnu/libm.so.6 /runtime/lib/x86_64-linux-gnu/ \
 && cp -L /lib/x86_64-linux-gnu/libgcc_s.so.1 /runtime/lib/x86_64-linux-gnu/ \
 && cp -L /lib/x86_64-linux-gnu/libc.so.6 /runtime/lib/x86_64-linux-gnu/ \
 && cp -L /lib64/ld-linux-x86-64.so.2 /runtime/lib64/ \
 && printf '%s\n' 'graphenedb:x:10001:10001:GrapheneDB:/var/lib/graphenedb:/sbin/nologin' \
      > /runtime/etc/passwd \
 && printf '%s\n' 'graphenedb:x:10001:' > /runtime/etc/group \
 && chown -R 10001:10001 /runtime/var/lib/graphenedb

FROM scratch
LABEL org.opencontainers.image.title="GrapheneDB Server" \
      org.opencontainers.image.description="Physical hex-lattice AI memory database server" \
      org.opencontainers.image.licenses="Apache-2.0"
COPY --from=build /runtime/ /
USER 10001:10001
WORKDIR /var/lib/graphenedb
VOLUME ["/var/lib/graphenedb"]
EXPOSE 8080
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 CMD ["/usr/local/bin/graphenedb_healthcheck", "127.0.0.1", "8080"]
ENTRYPOINT ["/usr/local/bin/graphenedb_server", "/var/lib/graphenedb", "64", "8080", "--bind-address", "0.0.0.0", "--behind-tls-proxy", "--physical-lattice-primary", "--physical-lattice-radius", "600", "--expected-max-nodes", "1000000", "--wal-rotate-bytes", "268435456", "--workers", "8", "--queue-capacity", "1024", "--rate-limit-rps", "200", "--rate-limit-burst", "400"]
