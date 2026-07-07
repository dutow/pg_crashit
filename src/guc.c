#include "pg_crashit.h"
#include "utils/guc.h"
#include <limits.h>

bool   crashit_enabled = false;
int    crashit_action = CRASH_BACKEND_SEGV;
char  *crashit_action_set = NULL;
int    crashit_exit_code = 1;
int    crashit_victim = VICTIM_SELF;
int    crashit_on_query_number = 0;
char  *crashit_on_query_match = NULL;
int    crashit_on_connection_seconds = 0;
int    crashit_on_statement_count = 0;
int    crashit_on_uptime_seconds = 0;
int    crashit_on_connection_number = 0;
int    crashit_trigger_mode = TRIGGER_ANY;
double crashit_probability = 1.0;
int    crashit_seed = 0;
int    crashit_check_interval_ms = 1000;
bool   crashit_log_before_crash = true;

static const struct config_enum_entry action_options[] = {
    {"none", CRASH_NONE, false},
    {"backend_segv", CRASH_BACKEND_SEGV, false},
    {"backend_abort", CRASH_BACKEND_ABORT, false},
    {"backend_kill", CRASH_BACKEND_KILL, false},
    {"backend_exit", CRASH_BACKEND_EXIT, false},
    {"backend_fatal", CRASH_BACKEND_FATAL, false},
    {"backend_panic", CRASH_BACKEND_PANIC, false},
    {"postmaster_kill", CRASH_POSTMASTER_KILL, false},
    {"postmaster_sigquit", CRASH_POSTMASTER_SIGQUIT, false},
    {"random", CRASH_RANDOM, false},
    {NULL, 0, false}
};
static const struct config_enum_entry victim_options[] = {
    {"self", VICTIM_SELF, false},
    {"random_backend", VICTIM_RANDOM_BACKEND, false},
    {NULL, 0, false}
};
static const struct config_enum_entry trigger_mode_options[] = {
    {"any", TRIGGER_ANY, false},
    {"all", TRIGGER_ALL, false},
    {NULL, 0, false}
};

void
crashit_define_gucs(void)
{
    DefineCustomBoolVariable("crashit.enabled",
        "Enable rule evaluation (SQL crash-now functions always work).",
        NULL, &crashit_enabled, false,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomEnumVariable("crashit.action",
        "Action to perform when a trigger fires.",
        NULL, &crashit_action, CRASH_BACKEND_SEGV, action_options,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomStringVariable("crashit.action_set",
        "Comma-separated actions drawn from when action = random.",
        NULL, &crashit_action_set, "backend_segv,backend_abort,backend_kill",
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.exit_code",
        "Exit code for the backend_exit action.",
        NULL, &crashit_exit_code, 1, 0, 255,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomEnumVariable("crashit.victim",
        "Which process a backend action targets (self or a random backend).",
        NULL, &crashit_victim, VICTIM_SELF, victim_options,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.on_query_number",
        "Fire on the Nth query in a session (0 = inactive).",
        NULL, &crashit_on_query_number, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomStringVariable("crashit.on_query_match",
        "Fire when the query text contains this substring (empty = inactive).",
        NULL, &crashit_on_query_match, "",
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.on_connection_seconds",
        "Fire once the session has been open this many seconds (0 = inactive).",
        NULL, &crashit_on_connection_seconds, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.on_statement_count",
        "Fire after this many statements in a session (0 = inactive).",
        NULL, &crashit_on_statement_count, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.on_uptime_seconds",
        "Fire once server uptime reaches this many seconds (0 = inactive).",
        NULL, &crashit_on_uptime_seconds, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.on_connection_number",
        "Fire once this many connections have been accepted (0 = inactive).",
        NULL, &crashit_on_connection_number, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomEnumVariable("crashit.trigger_mode",
        "Combine active conditions with any (OR) or all (AND).",
        NULL, &crashit_trigger_mode, TRIGGER_ANY, trigger_mode_options,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomRealVariable("crashit.probability",
        "Probability that a satisfied trigger actually fires.",
        NULL, &crashit_probability, 1.0, 0.0, 1.0,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.seed",
        "PRNG seed for reproducible decisions (0 = per-process nondeterministic).",
        NULL, &crashit_seed, 0, 0, INT_MAX,
        PGC_SIGHUP, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("crashit.check_interval_ms",
        "Background worker evaluation interval.",
        NULL, &crashit_check_interval_ms, 1000, 10, INT_MAX,
        PGC_SIGHUP, GUC_UNIT_MS, NULL, NULL, NULL);

    DefineCustomBoolVariable("crashit.log_before_crash",
        "Emit a LOG line describing the action before performing it.",
        NULL, &crashit_log_before_crash, true,
        PGC_SIGHUP, 0, NULL, NULL, NULL);
}
