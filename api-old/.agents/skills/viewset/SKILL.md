---
name: viewset
description: Create a basic CRUD viewset (internal.hpp, handlers.hpp/cpp, routes.hpp) under src/handlers/<namespace>/<viewset>/. Use when adding a new resource endpoint to the API.
argument-hint: <viewset> <namespace> <sql_table>
disable-model-invocation: true
allowed-tools: Read Write Bash Glob Grep
context: fork
---

Create a CRUD viewset for the following arguments:

- Viewset name: $0
- Namespace: $1
- SQL table: $2

## Discover project context first

Before writing any file, read the following to understand the project's conventions:

- Existing handler structure: !`find src/handlers -name "handlers.cpp" | head -3`
- Project namespace (from CMakeLists): !`grep -m1 "^project(" CMakeLists.txt`
- Route prefix (from main entry point): !`grep -m1 "CROW_ROUTE" src/ticketeer-api.cc`
- DB class name: !`grep -rm1 "class Pg\b" src/db/`
- LoginRequired location: !`find src/handlers -name "decorators.hpp"`

Use these to derive:
- The root C++ namespace (e.g. `ticketeer`)
- The route prefix (e.g. `/ticketeer/api/v1`)
- The DB class include path and fully qualified name
- The decorators include path

## Directory

Create all files inside `src/handlers/$1/$0/`.

## Files

### `internal.hpp`

Template `*Impl<DB>` functions. Namespace: `<root>::handlers::$1::$0` (use discovered root namespace).

Implement all six operations:

**ListImpl** — `db.Exec("SELECT id, <fields>, created_at FROM $2 ORDER BY id")`. Build `crow::json::wvalue out` indexing rows with `out[i][...]`. Return 200.

**GetImpl(id)** — `db.ExecParams("SELECT id, <fields>, created_at FROM $2 WHERE id = $1", params, 1)`. Return 404 `{"error":"not_found"}` if `res.nrows() == 0`. Return 200.

**CreateImpl** — Parse JSON body, validate required fields (return 400 if missing). `db.ExecParams("INSERT INTO $2 (...) VALUES (...) RETURNING id", params, N)`. Return 201 `{"id": ...}`.

**UpdateImpl(id)** — Parse JSON body, validate all required fields (return 400). `db.ExecParams("UPDATE $2 SET f1=$1, f2=$2 WHERE id=$N", params, N)`. Return 200.

**PatchImpl(id)** — Parse JSON body (return 400 if not valid JSON). For each patchable field: `std::string field_str; const char *field_ptr = nullptr; if (body.has("field")) { field_str = ...; field_ptr = field_str.c_str(); }`. Use `COALESCE($n, column)` in SQL. Return 200.

**DeleteImpl(id)** — `db.ExecParams("DELETE FROM $2 WHERE id = $1", params, 1)`. Return 204.

---

### `handlers.hpp`

Declarations only. Namespace: `<root>::handlers::$1::$0`. All `[[nodiscard]]`:

```
ListHandler(req)
GetHandler(req, int id)
CreateHandler(req)
UpdateHandler(req, int id)
PatchHandler(req, int id)
DeleteHandler(req, int id)
```

---

### `handlers.cpp`

Include `internal.hpp`, `handlers.hpp`, the discovered DB header, and the discovered decorators header.
Namespace: `<root>::handlers::$1::$0`.

Each handler: instantiate the DB class, check `connected()` → return 503, then wrap with `LoginRequired`:

```cpp
return <decorators_ns>::LoginRequired(req, db, [&] { return *Impl(req, db, ...); });
```

---

### `routes.hpp`

Include `crow_all.h` and `handlers.hpp`. Namespace: `<root>::handlers::$1::$0`.

```cpp
inline void RegisterRoutes(crow::SimpleApp &app)
```

Six routes under `<route_prefix>/$1/$0s` (use discovered route prefix):
- `GET /` → `ListHandler`
- `POST /` → `CreateHandler`
- `GET /<int>` → `GetHandler`
- `PUT /<int>` → `UpdateHandler`
- `PATCH /<int>` → `PatchHandler`
- `DELETE /<int>` → `DeleteHandler`

---

## After creating the files

Read `CMakeLists.txt` and `src/ticketeer-api.cc` before editing them.

1. Add `src/handlers/$1/$0/handlers.cpp` to `add_executable` sources in `CMakeLists.txt`
2. Add the routes header include in the main entry point file
3. Call `<root>::handlers::$1::$0::RegisterRoutes(app)` inside the server setup function

---

## Mandatory conventions

Read `AGENTS.md` if present for full project conventions. Key rules:

- C++23
- `template <class DB>` — never `template <typename DB>`
- PascalCase for all functions and methods (getters exempt)
- No code comments of any kind
- English only — identifiers, strings, SQL
- `const char *params[]` for `ExecParams` — never string interpolation in SQL
- `std::stoll(res.value(i, 0))` for integer id columns in JSON output
- `nullptr` in params array → NULL in PostgreSQL → COALESCE keeps existing value
