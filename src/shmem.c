#include "pg_crashit.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/timestamp.h"
#include "common/pg_prng.h"

CrashitShared *crashit_shared = NULL;
static pg_prng_state crashit_prng;
static bool crashit_prng_seeded = false;

void
crashit_shmem_request(void)
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(CrashitShared)));
}

void
crashit_shmem_startup(void)
{
    bool found;
    crashit_shared = ShmemInitStruct("pg_crashit",
                                     sizeof(CrashitShared), &found);
    if (!found)
    {
        SpinLockInit(&crashit_shared->mutex);
        crashit_shared->server_start_time = GetCurrentTimestamp();
        crashit_shared->connections_accepted = 0;
        crashit_shared->nbackend_pids = 0;
    }
}

static int
crashit_max_pids(void)
{
    return Min(CRASHIT_MAX_BACKENDS, MaxBackends);
}

static void
crashit_pid_exit_cb(int code, Datum arg)
{
    crashit_unregister_backend_pid();
}

void
crashit_register_backend_pid(void)
{
    int cap = crashit_max_pids();
    if (crashit_shared == NULL) return;
    SpinLockAcquire(&crashit_shared->mutex);
    if (crashit_shared->nbackend_pids < cap)
        crashit_shared->backend_pids[crashit_shared->nbackend_pids++] = MyProcPid;
    SpinLockRelease(&crashit_shared->mutex);
    on_shmem_exit(crashit_pid_exit_cb, (Datum) 0);
}

void
crashit_unregister_backend_pid(void)
{
    if (crashit_shared == NULL) return;
    SpinLockAcquire(&crashit_shared->mutex);
    for (int i = 0; i < crashit_shared->nbackend_pids; i++)
    {
        if (crashit_shared->backend_pids[i] == MyProcPid)
        {
            crashit_shared->backend_pids[i] =
                crashit_shared->backend_pids[--crashit_shared->nbackend_pids];
            break;
        }
    }
    SpinLockRelease(&crashit_shared->mutex);
}

uint64
crashit_bump_connection_count(void)
{
    uint64 v;
    if (crashit_shared == NULL) return 0;
    SpinLockAcquire(&crashit_shared->mutex);
    v = ++crashit_shared->connections_accepted;
    SpinLockRelease(&crashit_shared->mutex);
    return v;
}

int
crashit_pick_random_backend_pid(void)
{
    int pid = 0, n, idx;
    if (crashit_shared == NULL) return 0;
    SpinLockAcquire(&crashit_shared->mutex);
    n = crashit_shared->nbackend_pids;
    if (n > 0)
    {
        idx = (int) (crashit_random_double() * n);
        if (idx >= n) idx = n - 1;
        pid = crashit_shared->backend_pids[idx];
        if (pid == MyProcPid && n > 1)   /* avoid self when alternatives exist */
            pid = crashit_shared->backend_pids[(idx + 1) % n];
    }
    SpinLockRelease(&crashit_shared->mutex);
    return pid;
}

double
crashit_random_double(void)
{
    if (!crashit_prng_seeded)
    {
        if (crashit_seed != 0)
            pg_prng_seed(&crashit_prng, (uint64) crashit_seed);
        else
            pg_prng_seed(&crashit_prng,
                         ((uint64) MyProcPid << 32) ^ (uint64) MyProcPid);
        crashit_prng_seeded = true;
    }
    return pg_prng_double(&crashit_prng);
}
