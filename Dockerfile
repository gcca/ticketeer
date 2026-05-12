# syntax=docker/dockerfile:1.7

ARG BASE_IMAGE=ubuntu:latest
ARG BUILD_JOBS=4
ARG CMAKE_VERSION=4.4.2
ARG DROGON_VERSION=1.9.13

FROM ${BASE_IMAGE} AS deps

ARG BUILD_JOBS
ARG CMAKE_VERSION
ARG DROGON_VERSION

ENV DEBIAN_FRONTEND=noninteractive \
    PATH=/opt/cmake/bin:${PATH} \
    CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS} \
    CMAKE_PREFIX_PATH=/usr/local \
    LD_LIBRARY_PATH=/usr/local/lib

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      curl \
      git \
      libc-ares-dev \
      libjsoncpp-dev \
      libsqlite3-dev \
      libssl-dev \
      ninja-build \
      pkg-config \
      uuid-dev \
      zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN case "$(uname -m)" in \
      x86_64) cmake_arch=x86_64 ;; \
      aarch64 | arm64) cmake_arch=aarch64 ;; \
      *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;; \
    esac \
    && mkdir -p /opt/cmake \
    && curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${cmake_arch}.tar.gz" \
       | tar -xz -C /opt/cmake --strip-components=1

WORKDIR /tmp/deps

RUN git clone --depth 1 --branch "v${DROGON_VERSION}" --recurse-submodules --shallow-submodules https://github.com/drogonframework/drogon.git \
    && cmake -S drogon -B drogon/build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DBUILD_SHARED_LIBS=ON \
      -DBUILD_CTL=ON \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTING=OFF \
      -DBUILD_DOC=OFF \
      -DBUILD_ORM=OFF \
      -DBUILD_BROTLI=OFF \
      -DBUILD_YAML_CONFIG=OFF \
      -DUSE_SUBMODULE=ON \
    && cmake --build drogon/build --parallel 8 --target install

FROM deps AS build

WORKDIR /src

COPY CMakeLists.txt ./
COPY 3rdparty ./3rdparty
COPY cmd ./cmd
COPY src ./src

RUN cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 8

RUN mkdir -p /out/usr/local/bin /out/app/data \
    && cp build/bin/* /out/usr/local/bin/ \
    && for bin in /out/usr/local/bin/*; do \
         ldd "$bin" 2>/dev/null; \
       done \
       | sed -nE 's#^[[:space:]]*([^[:space:]]+)[[:space:]]=>[[:space:]]*(/[^[:space:]]+).*#\1|\2#p; s#^[[:space:]]*(/[^[:space:]]+)[[:space:]].*#\1|\1#p' \
       | sort -u > /tmp/libs.txt \
    && while IFS='|' read -r name path; do \
         real="$(realpath "$path")" \
         && mkdir -p "/out$(dirname "$real")" \
         && cp -Lf "$real" "/out$real" \
         && base="$(basename "$name")" \
         && [ "$base" = "$(basename "$real")" ] \
         || ln -sf "$(basename "$real")" "/out$(dirname "$real")/$base"; \
       done < /tmp/libs.txt

FROM ${BASE_IMAGE} AS execute

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /out/ /
COPY db/migrations/*.sql /app/migrations/

WORKDIR /app

ENV LD_LIBRARY_PATH=/usr/local/lib \
    TZ=UTC \
    DB_URL=/app/data/ticketeer.db \
    UPLOAD_DIR=/app/data/upload

EXPOSE 5521

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["/usr/local/bin/ticketeer-healthcheck"]

ENTRYPOINT ["/usr/local/bin/ticketeer-entrypoint"]
