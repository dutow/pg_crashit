# pg_crashit — fault-injection extension
MODULE_big = pg_crashit
OBJS = src/pg_crashit.o src/guc.o src/shmem.o src/action.o \
       src/trigger.o src/hooks.o src/bgworker.o src/funcs.o

EXTENSION = pg_crashit
DATA = pg_crashit--1.0.sql
PGFILEDESC = "pg_crashit - deliberate crash/fault injection for testing"

TAP_TESTS = 1

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
