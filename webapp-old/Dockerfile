FROM oven/bun:1 AS build

WORKDIR /app

COPY package.json bun.lock ./
RUN bun install --frozen-lockfile

COPY . .
RUN bun run build

FROM nginx:1.27-alpine AS release

RUN rm /etc/nginx/conf.d/default.conf
RUN cat <<'EOF' > /etc/nginx/conf.d/default.conf
server {
    listen 8080;
    server_name _;

    root /usr/share/nginx/html;
    index index.html;

    location = / {
        return 302 /ticketeer/app/;
    }

    location = /ticketeer/app {
        return 301 /ticketeer/app/;
    }

    location /ticketeer/app/ {
        try_files $uri $uri/ /ticketeer/app/index.html;
    }
}
EOF

COPY --from=build /app/dist/ /usr/share/nginx/html/ticketeer/app/

EXPOSE 8080

CMD ["nginx", "-g", "daemon off;"]
