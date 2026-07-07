# crash-now SQL functions: backend crashes, status, reset, superuser-only
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('crash_now');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
# override the TAP default (restart_after_crash = off) so a crashed backend
# triggers crash recovery instead of taking the whole cluster down
$node->append_conf('postgresql.conf', "restart_after_crash = on");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

# --- backend_fatal terminates the session but leaves the server up ---
my $logstart = -s $node->logfile;
my ($ret, $out, $err) =
  $node->psql('postgres', "SELECT crashit_crash_backend('backend_fatal')");
isnt($ret, 0, 'backend_fatal terminates the calling backend');

is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'server still up after backend_fatal');

# log_before_crash emits a LOG line naming the action
ok($node->log_contains(qr/pg_crashit: performing action backend_fatal/, $logstart),
   'log_before_crash logged the action');

# --- status SRF and reset ---
is($node->safe_psql('postgres', 'SELECT enabled FROM crashit_status()'), 'f',
   'crashit_status reports enabled=off by default');
is($node->safe_psql('postgres',
       'SELECT connections_accepted >= 0 FROM crashit_status()'), 't',
   'crashit_status returns a connections_accepted count');
# reset and read in one session: a fresh session would re-bump the counter on
# its own first query, so verify the reset within the resetting session
my $after = $node->safe_psql('postgres',
       'SELECT crashit_reset_counters(); '
     . 'SELECT connections_accepted FROM crashit_status()');
($after) = ($after =~ /(\d+)\s*$/);   # trailing number (void reset prints a blank line)
is($after, '0', 'crashit_reset_counters zeroes connections_accepted');

# --- functions are superuser-only ---
$node->safe_psql('postgres', 'CREATE ROLE alice LOGIN');
($ret, $out, $err) = $node->psql('postgres',
    "SELECT crashit_crash_backend('backend_fatal')",
    extra_params => [ '-U', 'alice' ]);
isnt($ret, 0, 'non-superuser cannot call crashit_crash_backend');
like($err, qr/permission denied/, 'permission denied for non-superuser');
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'server survived the denied call');

# --- backend_segv crashes the backend and the server recovers ---
$logstart = -s $node->logfile;
($ret, $out, $err) =
  $node->psql('postgres', "SELECT crashit_crash_backend('backend_segv')");
isnt($ret, 0, 'backend_segv crashes the calling backend');
$node->poll_query_until('postgres', 'SELECT 1', '1')
  or die 'server did not recover after backend_segv';
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'server recovered after backend_segv crash');

# --- unknown action is rejected without crashing ---
($ret, $out, $err) =
  $node->psql('postgres', "SELECT crashit_crash_backend('nonsense')");
isnt($ret, 0, 'unknown action rejected');
like($err, qr/unknown crashit action/, 'clear error for unknown action');
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'server up after unknown action error');

$node->stop;
done_testing();
