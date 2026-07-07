#include "pg_crashit.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "utils/guc.h"

CRASHIT_MODULE_MAGIC;

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

static void
crashit_shmem_request_cb(void)
{
    if (prev_shmem_request_hook)
        prev_shmem_request_hook();
    crashit_shmem_request();
}

static void
crashit_shmem_startup_cb(void)
{
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();
    crashit_shmem_startup();
}

void
_PG_init(void)
{
    crashit_define_gucs();
    MarkGUCPrefixReserved("crashit");

    if (!process_shared_preload_libraries_in_progress)
        return;   /* SQL "crash now" funcs still work without preload */

    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = crashit_shmem_request_cb;
    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = crashit_shmem_startup_cb;

    crashit_install_hooks();
    crashit_register_bgworker();
}
