#include "pg_crashit.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "postmaster/interrupt.h"
#include "storage/latch.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/timestamp.h"
#include <signal.h>

void
crashit_register_bgworker(void)
{
    BackgroundWorker w;

    memset(&w, 0, sizeof(w));
    w.bgw_flags = BGWORKER_SHMEM_ACCESS;
    w.bgw_start_time = BgWorkerStart_RecoveryFinished;
    w.bgw_restart_time = 5;
    snprintf(w.bgw_library_name, BGW_MAXLEN, "pg_crashit");
    snprintf(w.bgw_function_name, BGW_MAXLEN, "crashit_bgworker_main");
    snprintf(w.bgw_name, BGW_MAXLEN, "pg_crashit worker");
    snprintf(w.bgw_type, BGW_MAXLEN, "pg_crashit");
    w.bgw_notify_pid = 0;
    RegisterBackgroundWorker(&w);
}

static int
sig_for(CrashAction a)
{
    return (a == CRASH_BACKEND_ABORT) ? SIGABRT : SIGKILL;
}

static void
crashit_bgworker_check(void)
{
    bool        any_active = false;
    bool        all_true = true;
    bool        any_true = false;
    CrashAction act;

    if (crashit_on_uptime_seconds > 0)
    {
        long        uptime = 0;
        bool        c;

        if (crashit_shared != NULL)
        {
            TimestampTz start;
            SpinLockAcquire(&crashit_shared->mutex);
            start = crashit_shared->server_start_time;
            SpinLockRelease(&crashit_shared->mutex);
            if (start != 0)
                uptime = (GetCurrentTimestamp() - start) / USECS_PER_SEC;
        }
        c = (uptime >= crashit_on_uptime_seconds);
        any_active = true;
        any_true = any_true || c;
        all_true = all_true && c;
    }

    if (crashit_on_connection_number > 0)
    {
        uint64      conns = 0;
        bool        c;

        if (crashit_shared != NULL)
        {
            SpinLockAcquire(&crashit_shared->mutex);
            conns = crashit_shared->connections_accepted;
            SpinLockRelease(&crashit_shared->mutex);
        }
        c = (conns >= (uint64) crashit_on_connection_number);
        any_active = true;
        any_true = any_true || c;
        all_true = all_true && c;
    }

    if (!any_active)
        return;
    if (!(crashit_trigger_mode == TRIGGER_ALL ? all_true : any_true))
        return;
    if (crashit_random_double() >= crashit_probability)
        return;

    act = crashit_resolve_action();
    if (act == CRASH_NONE)
        return;

    if (act == CRASH_POSTMASTER_KILL || act == CRASH_POSTMASTER_SIGQUIT)
    {
        crashit_perform_action(act, "bgworker trigger");
    }
    else if (crashit_victim == VICTIM_RANDOM_BACKEND)
    {
        int pid = crashit_pick_random_backend_pid();

        if (pid != 0)
        {
            if (crashit_log_before_crash)
                elog(LOG, "pg_crashit: signalling backend pid %d, action %s "
                     "(bgworker trigger), seed=%d",
                     pid, crashit_label_for_action(act), crashit_seed);
            kill(pid, sig_for(act));
        }
        else
            elog(WARNING, "pg_crashit: no live client backend to signal");
    }
    else
    {
        elog(WARNING, "pg_crashit: backend action with victim=self is not "
             "applicable in the background worker");
    }
}

PGDLLEXPORT void
crashit_bgworker_main(Datum arg)
{
    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    pqsignal(SIGTERM, die);
    BackgroundWorkerUnblockSignals();

    for (;;)
    {
        (void) WaitLatch(MyLatch,
                         WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
                         crashit_check_interval_ms, PG_WAIT_EXTENSION);
        ResetLatch(MyLatch);

        if (ConfigReloadPending)
        {
            ConfigReloadPending = false;
            ProcessConfigFile(PGC_SIGHUP);
        }
        CHECK_FOR_INTERRUPTS();

        if (crashit_enabled)
            crashit_bgworker_check();
    }
}
