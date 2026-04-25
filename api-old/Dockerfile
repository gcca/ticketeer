FROM python:3.13-slim-bookworm AS quill-builder

ARG QUILL_VERSION=11.1.0

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
 && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir cmake==4.3.1 ninja

WORKDIR /tmp

RUN curl -fsSL "https://github.com/odygrd/quill/archive/refs/tags/v${QUILL_VERSION}.tar.gz" -o quill.tar.gz \
 && tar -xzf quill.tar.gz \
 && cmake -S "quill-${QUILL_VERSION}" -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUILL_BUILD_EXAMPLES=OFF \
    -DQUILL_BUILD_TESTS=OFF \
    -DQUILL_BUILD_BENCHMARKS=OFF \
 && cmake --build build --parallel \
 && cmake --install build --prefix /opt/quill

FROM python:3.13-slim-bookworm AS builder

ARG CMAKE_CXX_FLAGS="-march=native -flto"

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    libasio-dev \
    libpq-dev \
    pkg-config \
 && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir cmake==4.3.1 ninja

COPY --from=quill-builder /opt/quill /opt/quill

WORKDIR /src

COPY CMakeLists.txt ./
COPY cmake ./cmake
COPY 3rdparty ./3rdparty
COPY src ./src

RUN cmake -S . -B build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -G Ninja \
    -DCMAKE_CXX_STANDARD=23 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" \
    -DCMAKE_PREFIX_PATH=/opt/quill \
    -DTICKETEER_API_COMMANDS=OFF \
    -DTICKETEER_API_TESTS=OFF \
 && cmake --build build --target ticketeer-api --parallel

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    libpq5 \
    libstdc++6 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/build/ticketeer-api /usr/local/bin/ticketeer-api

EXPOSE 8000

ENTRYPOINT ["/usr/local/bin/ticketeer-api"]
