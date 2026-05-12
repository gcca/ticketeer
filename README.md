# Ticketeer

A helpdesk ticketing system. Roles: **requesters** open and track tickets; **supervisors** manage, assign to **technicians**, and close them. All interaction is server-rendered HTML with htmx fragments — no JavaScript framework.

## Build (local)

```sh
# Configure
cmake -S . -B build

# Compile
cmake --build build -j 16
```

## Database setup

Ticketeer uses dbmate to manage migrations. The schema lives in `db/migrations/` and the authoritative snapshot is `db/schema.sql`.

```sh
# Apply migrations
export DATABASE_URL="sqlite:data/ticketeer.db"
dbmate up

# Seed initial lookup values
sqlite3 data/ticketeer.db < db/fixtures/init.sql

# Optional: load sample data
sqlite3 data/ticketeer.db < db/fixtures/sample-data.sql
```

## Run

```sh
./build/ticketeer --bind 127.0.0.1 --port 5521
```

Open `http://127.0.0.1:5521/ticketeer/auth/signin`.

## Docker

```sh
# Build and run
docker build -t ticketeer .
docker run -p 5521:5521 ticketeer

# Check health
curl -s http://localhost:5521/ticketeer/healthcheck
```
