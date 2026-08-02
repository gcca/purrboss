# syntax=docker/dockerfile:1.7

ARG DEBIAN_CODENAME=trixie
ARG BUILD_JOBS=4
ARG CMAKE_VERSION=4.3.2
ARG DBMATE_IMAGE=ghcr.io/amacneil/dbmate:2.33.0
ARG GRPC_HEALTH_PROBE_VERSION=v0.4.52

FROM ${DBMATE_IMAGE} AS dbmate

FROM debian:${DEBIAN_CODENAME}-slim AS deps

ARG BUILD_JOBS
ARG CMAKE_VERSION

ENV CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS} \
    DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        curl \
        git \
        libabsl-dev \
        libc-ares-dev \
        libgrpc++-dev \
        libgrpc-dev \
        libgtest-dev \
        libprotobuf-dev \
        libre2-dev \
        libsqlite3-dev \
        libssl-dev \
        ninja-build \
        pkg-config \
        protobuf-compiler \
        protobuf-compiler-grpc \
        python3 \
        python3-pip \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages --no-cache-dir \
    "cmake==${CMAKE_VERSION}"

FROM deps AS build

ARG BUILD_JOBS
ARG GRPC_HEALTH_PROBE_VERSION
ARG TARGETARCH

WORKDIR /src

RUN case "${TARGETARCH}" in \
        amd64|arm64) grpc_probe_arch="${TARGETARCH}" ;; \
        *) echo "unsupported grpc_health_probe architecture: ${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && curl -fsSLo /usr/local/bin/grpc_health_probe \
        "https://github.com/grpc-ecosystem/grpc-health-probe/releases/download/${GRPC_HEALTH_PROBE_VERSION}/grpc_health_probe-linux-${grpc_probe_arch}" \
    && chmod +x /usr/local/bin/grpc_health_probe

COPY CMakeLists.txt ./
COPY 3rdparty ./3rdparty
COPY cmake ./cmake
COPY protos ./protos
COPY src ./src

RUN cmake -S . -B build -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    && cmake --build build --parallel "${BUILD_JOBS}" --target purrboss

FROM debian:${DEBIAN_CODENAME}-slim AS execute

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcares2 \
        libgrpc++1.51t64 \
        libprotobuf32t64 \
        libre2-11 \
        libsqlite3-0 \
        libssl3t64 \
        libstdc++6 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=dbmate /usr/local/bin/dbmate /usr/local/bin/dbmate
COPY --from=build /usr/local/bin/grpc_health_probe /usr/local/bin/grpc_health_probe
COPY --from=build /src/build/purrboss /usr/local/bin/purrboss
COPY db/migrations/*.sql /app/migrations/
COPY docker-entrypoint.sh /usr/local/bin/purrboss-entrypoint

RUN chmod +x /usr/local/bin/purrboss-entrypoint \
    && mkdir -p /app/data

ENV TZ=UTC \
    PURRBOSS_DBPATH=/app/data/purrboss.db \
    PURRBOSS_PORT=50051 \
    PURRBOSS_DEFAULT_TTL_SECONDS=2592000

EXPOSE 50051

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD grpc_health_probe -addr=127.0.0.1:${PURRBOSS_PORT}

ENTRYPOINT ["purrboss-entrypoint"]
