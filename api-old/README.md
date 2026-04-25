# ticketeer-api

Bootstrap the environment and configure `build/`:

```fish
./scripts/bootstrap.fish
```

Build:

```fish
cmake --build build -j14
```

If you want to use the machine core count instead of `14`:

```fish
cmake --build build -j(sysctl -n hw.logicalcpu)
```

Run tests:

```fish
ctest --test-dir build
```

## First Local Run

Apply the schema with:

```fish
./scripts/migrate.fish
```

Start the API:

```fish
./build/ticketeer-api --nothreaded
```

Default runtime values:
- Port: `8000`
- Database connection: `host=localhost dbname=ticketeer`

## Smoke Check

With the API running, verify the exposed endpoints with:

```fish
./scripts/check-calls.fish
```

The script checks:
- `GET /`
- `POST /ticketeer/api/v1/auth/signin`

It currently tries a valid sign-in with `gcca/gcca`, so that user must exist in the local database if you expect a `200`.

## Useful Binaries

After building with the bootstrap defaults:
- `build/ticketeer-api`
- `build/ticketeer_cmd_create-user`
- `build/ticketeer-api-test`
