# syntax=docker/dockerfile:1.7

ARG ALPINE_VERSION=3.23
ARG DEPS_IMAGE=ghcr.io/gcca/ticketeer-deps:latest

FROM ${DEPS_IMAGE} AS deps

FROM deps AS build

WORKDIR /src

COPY CMakeLists.txt ./
COPY 3rdparty ./3rdparty
COPY cmd ./cmd
COPY src ./src

RUN cmake -S . -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=23 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    && cmake --build build --target ticketeer ticketeer-create_user

FROM alpine:${ALPINE_VERSION} AS execute

RUN apk add --no-cache \
    c-ares \
    ca-certificates \
    jsoncpp \
    libstdc++ \
    libuuid \
    openssl \
    sqlite-libs \
    zlib

WORKDIR /app

COPY --from=deps /usr/local/lib/ /usr/local/lib/
COPY --from=build /src/build/ticketeer /usr/local/bin/ticketeer
COPY --from=build /src/build/ticketeer-create_user /usr/local/bin/ticketeer-create_user

RUN mkdir db

ENV LD_LIBRARY_PATH=/usr/local/lib \
    DB_URL=/app/db/ticketeer.db \
    UPLOAD_DIR=/app/db/upload

EXPOSE 5521

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -qO- http://127.0.0.1:5521/ticketeer/healthcheck >/dev/null || exit 1

ENTRYPOINT ["ticketeer"]
CMD ["--bind", "0.0.0.0", "--port", "5521"]
