#include "pg_crashit.h"
#include "executor/executor.h"
#include "tcop/utility.h"
#include "miscadmin.h"
#include "utils/timestamp.h"

uint64 crashit_session_query_count = 0;
uint64 crashit_session_stmt_count = 0;
static bool crashit_backend_registered = false;
static TimestampTz crashit_conn_start = 0;

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ProcessUtility_hook_type prev_ProcessUtility = NULL;

TimestampTz
crashit_get_conn_start(void)
{
    return crashit_conn_start;
}

static void
crashit_ensure_registered(void)
{
    if (!crashit_backend_registered)
    {
        crashit_register_backend_pid();
        crashit_bump_connection_count();
        crashit_conn_start = GetCurrentTimestamp();
        crashit_backend_registered = true;
    }
}

static void
crashit_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else
        standard_ExecutorStart(queryDesc, eflags);

    crashit_ensure_registered();
    crashit_session_query_count++;
    crashit_session_stmt_count++;
    if (crashit_enabled)
        crashit_backend_check(queryDesc->sourceText);
}

static void
crashit_ProcessUtility(PlannedStmt *pstmt, const char *queryString,
                       bool readOnlyTree, ProcessUtilityContext context,
                       ParamListInfo params, QueryEnvironment *queryEnv,
                       DestReceiver *dest, QueryCompletion *qc)
{
    if (prev_ProcessUtility)
        prev_ProcessUtility(pstmt, queryString, readOnlyTree, context,
                            params, queryEnv, dest, qc);
    else
        standard_ProcessUtility(pstmt, queryString, readOnlyTree, context,
                                params, queryEnv, dest, qc);

    crashit_ensure_registered();
    crashit_session_stmt_count++;
    if (crashit_enabled)
        crashit_backend_check(queryString);
}

void
crashit_install_hooks(void)
{
    prev_ExecutorStart = ExecutorStart_hook;
    ExecutorStart_hook = crashit_ExecutorStart;
    prev_ProcessUtility = ProcessUtility_hook;
    ProcessUtility_hook = crashit_ProcessUtility;
}
