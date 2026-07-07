#include "pg_crashit.h"
#include "access/htup_details.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

PG_FUNCTION_INFO_V1(crashit_crash_backend);
PG_FUNCTION_INFO_V1(crashit_crash_postmaster);
PG_FUNCTION_INFO_V1(crashit_status);
PG_FUNCTION_INFO_V1(crashit_reset_counters);

static CrashAction
action_from_arg(FunctionCallInfo fcinfo, CrashAction dflt)
{
    char       *label;
    int         a;

    if (PG_ARGISNULL(0))
        return dflt;

    label = text_to_cstring(PG_GETARG_TEXT_PP(0));
    a = crashit_action_from_label(label);
    if (a < 0)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("unknown crashit action \"%s\"", label)));
    pfree(label);
    return (CrashAction) a;
}

Datum
crashit_crash_backend(PG_FUNCTION_ARGS)
{
    CrashAction act = action_from_arg(fcinfo, (CrashAction) crashit_action);

    crashit_perform_action(act, "crashit_crash_backend()");
    PG_RETURN_VOID();
}

Datum
crashit_crash_postmaster(PG_FUNCTION_ARGS)
{
    /* default to a postmaster action so the function honours its name */
    CrashAction act = action_from_arg(fcinfo, CRASH_POSTMASTER_KILL);

    crashit_perform_action(act, "crashit_crash_postmaster()");
    PG_RETURN_VOID();
}

Datum
crashit_status(PG_FUNCTION_ARGS)
{
    TupleDesc   tupdesc;
    Datum       values[6];
    bool        nulls[6] = {false, false, false, false, false, false};
    HeapTuple   tuple;
    TimestampTz start = 0;
    uint64      conns = 0;
    int64       uptime = 0;

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("function returning record called in context "
                        "that cannot accept type record")));
    tupdesc = BlessTupleDesc(tupdesc);

    if (crashit_shared != NULL)
    {
        SpinLockAcquire(&crashit_shared->mutex);
        start = crashit_shared->server_start_time;
        conns = crashit_shared->connections_accepted;
        SpinLockRelease(&crashit_shared->mutex);
    }
    if (start != 0)
        uptime = (GetCurrentTimestamp() - start) / USECS_PER_SEC;

    values[0] = BoolGetDatum(crashit_enabled);
    values[1] = CStringGetTextDatum(crashit_label_for_action((CrashAction) crashit_action));
    values[2] = Int64GetDatum(uptime);
    values[3] = Int64GetDatum((int64) conns);
    values[4] = Int64GetDatum((int64) crashit_session_query_count);
    values[5] = Int32GetDatum(crashit_seed);

    tuple = heap_form_tuple(tupdesc, values, nulls);
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

Datum
crashit_reset_counters(PG_FUNCTION_ARGS)
{
    if (crashit_shared != NULL)
    {
        SpinLockAcquire(&crashit_shared->mutex);
        crashit_shared->connections_accepted = 0;
        SpinLockRelease(&crashit_shared->mutex);
    }
    PG_RETURN_VOID();
}
