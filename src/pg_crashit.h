#ifndef PG_CRASHIT_H
#define PG_CRASHIT_H

#include "postgres.h"
#include "datatype/timestamp.h"
#include "storage/s_lock.h"

#if PG_VERSION_NUM >= 180000
#define CRASHIT_MODULE_MAGIC \
    PG_MODULE_MAGIC_EXT(.name = "pg_crashit", .version = "1.0")
#else
#define CRASHIT_MODULE_MAGIC PG_MODULE_MAGIC
#endif

typedef enum CrashAction
{
    CRASH_NONE = 0,
    CRASH_BACKEND_SEGV, CRASH_BACKEND_ABORT, CRASH_BACKEND_KILL,
    CRASH_BACKEND_EXIT, CRASH_BACKEND_FATAL, CRASH_BACKEND_PANIC,
    CRASH_POSTMASTER_KILL, CRASH_POSTMASTER_SIGQUIT, CRASH_RANDOM
} CrashAction;

typedef enum CrashVictim { VICTIM_SELF = 0, VICTIM_RANDOM_BACKEND } CrashVictim;
typedef enum TriggerMode  { TRIGGER_ANY = 0, TRIGGER_ALL } TriggerMode;

#define CRASHIT_MAX_BACKENDS 4096

typedef struct CrashitShared
{
    slock_t     mutex;
    TimestampTz server_start_time;
    uint64      connections_accepted;
    int         nbackend_pids;
    int         backend_pids[CRASHIT_MAX_BACKENDS];
} CrashitShared;

/* globals defined in shmem.c */
extern CrashitShared *crashit_shared;

/* GUC-backed rule (defined in guc.c) */
extern bool   crashit_enabled;
extern int    crashit_action;        /* CrashAction */
extern char  *crashit_action_set;
extern int    crashit_exit_code;
extern int    crashit_victim;        /* CrashVictim */
extern int    crashit_on_query_number;
extern char  *crashit_on_query_match;
extern int    crashit_on_connection_seconds;
extern int    crashit_on_statement_count;
extern int    crashit_on_uptime_seconds;
extern int    crashit_on_connection_number;
extern int    crashit_trigger_mode;  /* TriggerMode */
extern double crashit_probability;
extern int    crashit_seed;
extern int    crashit_check_interval_ms;
extern bool   crashit_log_before_crash;

/* per-backend counters (defined in hooks.c) */
extern uint64 crashit_session_query_count;
extern uint64 crashit_session_stmt_count;
extern TimestampTz crashit_get_conn_start(void);

/* entry points used across modules */
extern void crashit_define_gucs(void);                 /* guc.c */
extern void crashit_shmem_request(void);               /* shmem.c */
extern void crashit_shmem_startup(void);               /* shmem.c */
extern void crashit_register_backend_pid(void);        /* shmem.c */
extern void crashit_unregister_backend_pid(void);      /* shmem.c */
extern uint64 crashit_bump_connection_count(void);     /* shmem.c */
extern int  crashit_pick_random_backend_pid(void);     /* shmem.c */
extern double crashit_random_double(void);             /* shmem.c: seeded PRNG */
extern void crashit_install_hooks(void);               /* hooks.c */
extern void crashit_register_bgworker(void);           /* bgworker.c */
#if PG_VERSION_NUM >= 180000
pg_noreturn extern PGDLLEXPORT void crashit_bgworker_main(Datum);  /* bgworker.c */
#else
extern PGDLLEXPORT void crashit_bgworker_main(Datum) pg_attribute_noreturn(); /* bgworker.c */
#endif
extern void crashit_perform_action(CrashAction act, const char *why); /* action.c */
extern CrashAction crashit_resolve_action(void);       /* action.c: expands CRASH_RANDOM */
extern int  crashit_action_from_label(const char *label); /* action.c: CrashAction or -1 */
extern const char *crashit_label_for_action(CrashAction act); /* action.c */
/* evaluated in backend hooks (trigger.c) */
extern void crashit_backend_check(const char *query_text);

#endif  /* PG_CRASHIT_H */
