#include "pg_crashit.h"
#include "utils/timestamp.h"
#include <string.h>

void
crashit_backend_check(const char *query_text)
{
    bool any_active = false;
    bool all_true = true;
    bool any_true = false;

#define CONSIDER(active, cond) \
    do { \
        if (active) { \
            bool c = (cond); \
            any_active = true; \
            any_true = any_true || c; \
            all_true = all_true && c; \
        } \
    } while (0)

    CONSIDER(crashit_on_query_number > 0,
             crashit_session_query_count >= (uint64) crashit_on_query_number);

    CONSIDER(crashit_on_statement_count > 0,
             crashit_session_stmt_count >= (uint64) crashit_on_statement_count);

    CONSIDER(crashit_on_query_match != NULL && crashit_on_query_match[0] != '\0',
             query_text != NULL &&
             strstr(query_text, crashit_on_query_match) != NULL);

    if (crashit_on_connection_seconds > 0)
    {
        long elapsed = 0;
        TimestampTz start = crashit_get_conn_start();
        if (start != 0)
            elapsed = (GetCurrentTimestamp() - start) / USECS_PER_SEC;
        CONSIDER(true, elapsed >= crashit_on_connection_seconds);
    }

#undef CONSIDER

    if (!any_active)
        return;

    if (crashit_trigger_mode == TRIGGER_ALL ? all_true : any_true)
    {
        if (crashit_random_double() < crashit_probability)
            crashit_perform_action(crashit_resolve_action(), "backend trigger");
    }
}
