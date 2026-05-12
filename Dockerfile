# syntax=docker/dockerfile:1.7

ARG BASE_IMAGE=ubuntu:latest
ARG BUILD_JOBS=8
ARG DBMATE_IMAGE=ghcr.io/amacneil/dbmate:2.33.0

FROM ${DBMATE_IMAGE} AS dbmate

FROM ${BASE_IMAGE} AS deps

ARG BUILD_JOBS

ENV DEBIAN_FRONTEND=noninteractive \
    PATH=/opt/cmake/bin:${PATH} \
    CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS} \
    CMAKE_PREFIX_PATH=/usr/local \
    CC=clang-23 \
    CXX=clang++-23 \
    LD_LIBRARY_PATH=/usr/local/lib

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      curl \
      git \
      gnupg \
      libc-ares-dev \
      libjsoncpp-dev \
      libsqlite3-dev \
      libssl-dev \
      lsb-release \
      ninja-build \
      pkg-config \
      software-properties-common \
      uuid-dev \
      zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
    && chmod +x /tmp/llvm.sh \
    && /tmp/llvm.sh 23 \
    && "${CC}" --version \
    && "${CXX}" --version \
    && test "$("${CXX}" -dumpversion | cut -d. -f1)" = 23 \
    && rm -f /tmp/llvm.sh \
    && rm -rf /var/lib/apt/lists/*

RUN case "$(uname -m)" in \
      x86_64) cmake_arch=x86_64 ;; \
      aarch64 | arm64) cmake_arch=aarch64 ;; \
      *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;; \
    esac \
    && mkdir -p /opt/cmake \
    && curl -fsSL "https://github.com/Kitware/CMake/releases/download/v4.4.2/cmake-4.4.2-linux-${cmake_arch}.tar.gz" \
       | tar -xz -C /opt/cmake --strip-components=1

WORKDIR /tmp/deps

RUN git clone --depth 1 --branch v1.9.13 --recurse-submodules --shallow-submodules https://github.com/drogonframework/drogon.git \
    && cmake -S drogon -B drogon/build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="${CC}" \
      -DCMAKE_CXX_COMPILER="${CXX}" \
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
    && cmake --build drogon/build --parallel "${BUILD_JOBS}" --target install

FROM deps AS build

WORKDIR /src

COPY CMakeLists.txt ./
COPY 3rdparty ./3rdparty
COPY src ./src

RUN cmake -S . -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="${CC}" \
      -DCMAKE_CXX_COMPILER="${CXX}" \
    && cmake --build build --parallel "${BUILD_JOBS}"

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
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /out/ /
COPY --from=dbmate /usr/local/bin/dbmate /usr/local/bin/dbmate
COPY db/migrations/*.sql /app/migrations/
COPY --chmod=755 entrypoint.sh /usr/local/bin/ticketeer-entrypoint

WORKDIR /app

ENV LD_LIBRARY_PATH=/usr/local/lib \
    TZ=UTC \
    DB_PATH=/app/data/ticketeer.db \
    UPLOAD_DIR=/app/data/upload

EXPOSE 5521

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["curl", "-fsS", "--max-time", "3", "http://127.0.0.1:5521/ticketeer/healthcheck"]

ENTRYPOINT ["/usr/local/bin/ticketeer-entrypoint"]
