# pg_crashit

Fault injection for PostgreSQL. Crashes a backend or the postmaster on
configurable triggers. For testing crash recovery, HA failover, and client
reconnect logic.

> **This extension breaks your server on purpose. Never load it in production.**

Supports PostgreSQL 16, 17, 18, and master.

## Install

From source:

```sh
make install PG_CONFIG=/path/to/pg_config
```

Or grab a nightly `.deb` for your Ubuntu and PostgreSQL version from the
[`nightly` pre-release](https://github.com/dutow/pg_crashit/releases/tag/nightly):

```sh
sudo apt install ./postgresql-17-crashit_*_amd64.deb
```

Then:

```
# postgresql.conf
shared_preload_libraries = 'pg_crashit'
```

```sql
CREATE EXTENSION pg_crashit;
```

Preloading is required for the automatic triggers; the crash-now SQL functions
work without it.

## Usage

Set a trigger, an action, enable, reload. All GUCs are `PGC_SIGHUP`.

Crash a backend after its 100th query, 10% of the time:

```
crashit.enabled = on
crashit.action = backend_fatal
crashit.on_query_number = 100
crashit.probability = 0.1
```

Kill the postmaster 60 seconds after startup:

```
crashit.enabled = on
crashit.action = postmaster_kill
crashit.on_uptime_seconds = 60
```

Or crash right now (superuser only):

```sql
SELECT crashit_crash_backend();                 -- uses crashit.action
SELECT crashit_crash_backend('backend_fatal');  -- explicit action
SELECT crashit_crash_postmaster();              -- default postmaster_kill
SELECT * FROM crashit_status();                 -- armed rule + counters
SELECT crashit_reset_counters();
```

## Triggers

Zero / empty means inactive. `crashit.trigger_mode` combines active ones with
`any` (OR, default) or `all` (AND).

| GUC | Fires when |
|-----|-----------|
| `crashit.on_query_number` | Nth query in a session |
| `crashit.on_query_match` | query text contains substring |
| `crashit.on_statement_count` | N statements in a session |
| `crashit.on_connection_seconds` | session open ≥ N seconds |
| `crashit.on_uptime_seconds` | server uptime ≥ N seconds |
| `crashit.on_connection_number` | N connections accepted since start |

## Actions

`crashit.action`:

| Action | Effect |
|--------|--------|
| `backend_segv` | SIGSEGV the backend (default) |
| `backend_abort` | `abort()` |
| `backend_kill` | SIGKILL the backend |
| `backend_exit` | `_exit(crashit.exit_code)` |
| `backend_fatal` | FATAL — kills the session, server stays up |
| `backend_panic` | PANIC — takes the whole server down |
| `postmaster_kill` | SIGKILL the postmaster |
| `postmaster_sigquit` | immediate shutdown |
| `random` | pick from `crashit.action_set` |
| `none` | nothing |

## Other knobs

| GUC | Default | Meaning |
|-----|---------|---------|
| `crashit.probability` | `1.0` | chance a satisfied trigger actually fires |
| `crashit.seed` | `0` | PRNG seed, per-process reproducibility (0 = random) |
| `crashit.victim` | `self` | `random_backend` makes uptime/connection triggers kill a random client backend |
| `crashit.check_interval_ms` | `1000` | background worker check interval |
| `crashit.log_before_crash` | `on` | LOG line before acting |

## Tests

```sh
make installcheck PG_CONFIG=/path/to/pg_config
```

## CI

GitHub Actions smoke-test against PGDG PostgreSQL 16–18 on Ubuntu 22.04/24.04,
build PostgreSQL from source (16, 17, 18, master) and run the full TAP test
suite against it, and publish the nightly `.deb` packages.
