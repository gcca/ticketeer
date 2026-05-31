# Ticketeer

A helpdesk ticketing system built with C++23 and Drogon. Two roles: **requesters** open and track tickets; **supervisors** manage, assign, and close them. All interaction is server-rendered HTML with htmx fragments — no JavaScript framework.

## Stack

| Layer | Tech      |
|-------|-----------|
| HTTP framework | Drogon 1.9.13 (controllers, middleware, CSP templates) |
| Database | SQLite3 (C API) |
| Logging | quill 11.1.0 (JSON structured) |
| Frontend | daisyUI 5 + Tailwind CSS (browser CDN) + htmx 2.0.10 |
| Migrations | dbmate 2.33.0 |
| Build | CMake 4.3 + Ninja |

## Prerequisites

Build dependencies live in a pre-built Docker base image (`ghcr.io/gcca/ticketeer-deps:latest`) compiled against Alpine 3.23. For local development outside Docker you need the same set installed on your machine:

- CMake ≥ 4.3
- Ninja
- Drogon 1.9.13 (with `drogon_ctl`)
- quill 11.1.0
- SQLite3
- dbmate 2.33.0

On macOS the fastest path is the Docker workflow below.

## Build (local)

```sh
# Configure (run again whenever you add/remove a .csp template file)
cmake -S . -B build -GNinja

# Compile
cmake --build build -j16
```

Binaries produced under `build/`:

| Binary | Purpose |
|--------|---------|
| `ticketeer` | Main HTTP server |
| `ticketeer-user_create` | CLI — create a user |
| `ticketeer-user_list` | CLI — list users |
| `ticketeer-user_password` | CLI — change a password |

### Optional: unit tests

```sh
cmake -S . -B build -DTICKETEER_TEST=ON ...
cmake --build build -j16
ctest --test-dir build
```

## Database setup

Ticketeer uses dbmate to manage migrations. The schema lives in `db/migrations/` and the authoritative snapshot is `db/schema.sql`.

```sh
# Apply migrations (creates db if absent)
export DATABASE_URL="sqlite:data/ticketeer.db"
dbmate up

# Seed initial lookup values (statuses, priorities, settings)
sqlite3 data/ticketeer.db < db/fixtures/init.sql

# Optional: load sample data with 22 users and 51 tickets
sqlite3 data/ticketeer.db < db/fixtures/sample-data.sql
```

## Create the first user

```sh
./build/ticketeer-user_create
```

The CLI prompts for username, password, name, email, and role (`supervisor` or `requester`).

## Run

```sh
./build/ticketeer --bind 127.0.0.1 --port 5521
```

Open `http://127.0.0.1:5521/ticketeer/auth/signin`.

Environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `DB_URL` | `data/ticketeer.db` | SQLite file path |
| `UPLOAD_DIR` | `data/upload` | Attachment storage root |

## Docker

```sh
# Build and start
docker compose up --build -d

# Check health
curl -s http://localhost:5521/ticketeer/healthcheck
```

`docker-compose.yaml` builds the `execute` stage of `Dockerfile`, exposes port `5521`, and persists the SQLite file and uploaded attachments in the `data` named volume. Migrations run automatically on every container start via `docker-entrypoint.sh`.

To rebuild the base dependencies image (only needed when `Dockerfile.deps` changes):

```sh
# Triggers deps.yaml workflow; or build locally:
docker build -f Dockerfile.deps -t ghcr.io/gcca/ticketeer-deps:latest .
```

### Middleware chain

```
All requests  →  Drogon routing
Role routes   →  LogInRequired → Role{Requester,Supervisor}Required → handler
```

### CSP templates

**Important:** after adding or removing a `.csp` file, always re-run `cmake -S . -B build ...` before building — CMake globs at configure time.
