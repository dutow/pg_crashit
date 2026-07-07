#include "pg_crashit.h"
#include "miscadmin.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char *label;
    CrashAction act;
} ActionLabel;

static const ActionLabel action_labels[] = {
    {"none", CRASH_NONE},
    {"backend_segv", CRASH_BACKEND_SEGV},
    {"backend_abort", CRASH_BACKEND_ABORT},
    {"backend_kill", CRASH_BACKEND_KILL},
    {"backend_exit", CRASH_BACKEND_EXIT},
    {"backend_fatal", CRASH_BACKEND_FATAL},
    {"backend_panic", CRASH_BACKEND_PANIC},
    {"postmaster_kill", CRASH_POSTMASTER_KILL},
    {"postmaster_sigquit", CRASH_POSTMASTER_SIGQUIT},
    {"random", CRASH_RANDOM},
    {NULL, CRASH_NONE}
};

int
crashit_action_from_label(const char *label)
{
    if (label == NULL)
        return -1;
    for (int i = 0; action_labels[i].label != NULL; i++)
        if (strcmp(action_labels[i].label, label) == 0)
            return (int) action_labels[i].act;
    return -1;
}

const char *
crashit_label_for_action(CrashAction act)
{
    for (int i = 0; action_labels[i].label != NULL; i++)
        if (action_labels[i].act == act)
            return action_labels[i].label;
    return "unknown";
}

static CrashAction
crashit_pick_from_action_set(void)
{
    char       *buf;
    char       *tok;
    char       *saveptr;
    CrashAction pool[32];
    int         n = 0;
    int         idx;

    if (crashit_action_set == NULL || crashit_action_set[0] == '\0')
    {
        elog(WARNING, "pg_crashit: action=random but action_set is empty");
        return CRASH_NONE;
    }

    buf = pstrdup(crashit_action_set);
    for (tok = strtok_r(buf, ",", &saveptr); tok != NULL;
         tok = strtok_r(NULL, ",", &saveptr))
    {
        int     a;
        size_t  len;

        while (*tok == ' ')
            tok++;
        len = strlen(tok);
        while (len > 0 && tok[len - 1] == ' ')
            tok[--len] = '\0';
        if (*tok == '\0')
            continue;

        a = crashit_action_from_label(tok);
        if (a < 0 || a == CRASH_RANDOM || a == CRASH_NONE)
            continue;               /* skip invalid / non-terminal actions */
        if (n < (int) lengthof(pool))
            pool[n++] = (CrashAction) a;
    }
    pfree(buf);

    if (n == 0)
    {
        elog(WARNING, "pg_crashit: action_set contains no valid actions");
        return CRASH_NONE;
    }

    idx = (int) (crashit_random_double() * n);
    if (idx >= n)
        idx = n - 1;
    return pool[idx];
}

CrashAction
crashit_resolve_action(void)
{
    if (crashit_action != CRASH_RANDOM)
        return (CrashAction) crashit_action;
    return crashit_pick_from_action_set();
}

void
crashit_perform_action(CrashAction act, const char *why)
{
    if (act == CRASH_RANDOM)
        act = crashit_resolve_action();
    if (act == CRASH_NONE)
        return;

    if (crashit_log_before_crash)
        elog(LOG, "pg_crashit: performing action %s (%s), seed=%d",
             crashit_label_for_action(act), why ? why : "", crashit_seed);

    switch (act)
    {
        case CRASH_BACKEND_SEGV:
            {
                volatile int *p = NULL;
                *p = 1;             /* deliberate */
                break;
            }
        case CRASH_BACKEND_ABORT:
            abort();
            break;
        case CRASH_BACKEND_KILL:
            kill(MyProcPid, SIGKILL);
            break;
        case CRASH_BACKEND_EXIT:
            _exit(crashit_exit_code);
            break;
        case CRASH_BACKEND_FATAL:
            elog(FATAL, "pg_crashit: induced FATAL (%s)", why ? why : "");
            break;
        case CRASH_BACKEND_PANIC:
            elog(PANIC, "pg_crashit: induced PANIC (%s)", why ? why : "");
            break;
        case CRASH_POSTMASTER_KILL:
            kill(PostmasterPid, SIGKILL);
            break;
        case CRASH_POSTMASTER_SIGQUIT:
            kill(PostmasterPid, SIGQUIT);
            break;
        default:
            break;
    }
}
