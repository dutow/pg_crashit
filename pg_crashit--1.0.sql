-- pg_crashit 1.0

CREATE FUNCTION crashit_crash_backend(action text DEFAULT NULL)
    RETURNS void AS 'MODULE_PATHNAME', 'crashit_crash_backend'
    LANGUAGE C VOLATILE;
REVOKE ALL ON FUNCTION crashit_crash_backend(text) FROM PUBLIC;

CREATE FUNCTION crashit_crash_postmaster(action text DEFAULT NULL)
    RETURNS void AS 'MODULE_PATHNAME', 'crashit_crash_postmaster'
    LANGUAGE C VOLATILE;
REVOKE ALL ON FUNCTION crashit_crash_postmaster(text) FROM PUBLIC;

CREATE FUNCTION crashit_status(
    OUT enabled bool, OUT action text, OUT uptime_seconds int8,
    OUT connections_accepted int8, OUT session_query_count int8, OUT seed int4)
    RETURNS record AS 'MODULE_PATHNAME', 'crashit_status'
    LANGUAGE C VOLATILE;
REVOKE ALL ON FUNCTION crashit_status() FROM PUBLIC;

CREATE FUNCTION crashit_reset_counters() RETURNS void
    AS 'MODULE_PATHNAME', 'crashit_reset_counters' LANGUAGE C VOLATILE;
REVOKE ALL ON FUNCTION crashit_reset_counters() FROM PUBLIC;
