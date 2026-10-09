# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates curl python3 python3-pip python3-venv \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install --break-system-packages "conan>=2.0,<3.0"
WORKDIR /src

# The slow part (building Drogon and its dependencies) is its own layer: it only reruns when conanfile.txt changes.
COPY conanfile.txt ./
RUN conan profile detect --force && conan install . --output-folder=build --build=missing -s build_type=Release

COPY CMakeLists.txt config.json ./
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)" \
    && ./build/util_test

FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libssl3 zlib1g libuuid1 libc-ares2 libsqlite3-0 curl \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /app /data/files /data/tmp && chown -R 10001:10001 /data
WORKDIR /app
COPY --from=builder /src/build/files_backend ./
COPY --from=builder /src/build/config.json ./
USER 10001:10001
EXPOSE 8080
CMD ["./files_backend"]
