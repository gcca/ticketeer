# syntax=docker/dockerfile:1.7

ARG ALPINE_VERSION=3.23
ARG DEPS_IMAGE=ghcr.io/gcca/ticketeer-deps:latest
ARG DBMATE_IMAGE=ghcr.io/amacneil/dbmate:2.33.0

FROM ${DBMATE_IMAGE} AS dbmate

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
    && cmake --build build --parallel "$(nproc)" --target ticketeer ticketeer-user_create ticketeer-user_list ticketeer-user_password

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
COPY --from=dbmate /usr/local/bin/dbmate /usr/local/bin/dbmate
COPY --from=build /src/build/ticketeer /usr/local/bin/ticketeer
COPY --from=build /src/build/ticketeer-user_create /usr/local/bin/ticketeer-user_create
COPY --from=build /src/build/ticketeer-user_list /usr/local/bin/ticketeer-user_list
COPY --from=build /src/build/ticketeer-user_password /usr/local/bin/ticketeer-user_password
COPY db/migrations/*.sql /app/migrations/
COPY docker-entrypoint.sh /usr/local/bin/ticketeer-entrypoint

RUN chmod +x /usr/local/bin/ticketeer-entrypoint \
    && mkdir data

ENV LD_LIBRARY_PATH=/usr/local/lib \
    TZ=UTC \
    DB_URL=/app/data/ticketeer.db \
    UPLOAD_DIR=/app/data/upload

EXPOSE 5521

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -qO- http://127.0.0.1:5521/ticketeer/healthcheck >/dev/null || exit 1

ENTRYPOINT ["ticketeer-entrypoint"]
