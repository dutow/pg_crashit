# crashit_status() counters and crashit_reset_counters()
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('status');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

# connections_accepted advances as new backends connect (each new session
# bumps the counter on its first query)
my $c1 = $node->safe_psql('postgres',
    'SELECT connections_accepted FROM crashit_status()');
my $c2 = $node->safe_psql('postgres',
    'SELECT connections_accepted FROM crashit_status()');
cmp_ok($c2, '>', $c1, 'connections_accepted advances across new connections');

# session_query_count reflects the queries issued in a session; the status
# call is itself the Nth query, so a 4-statement batch reports 4
my $sqc = $node->safe_psql('postgres',
       'SELECT 1; SELECT 1; SELECT 1; '
     . 'SELECT session_query_count FROM crashit_status()');
($sqc) = ($sqc =~ /(\d+)\s*$/);    # last line: the status result
is($sqc, '4', 'session_query_count reflects queries in the session');

# reset within one session (a fresh session would re-bump on its first query)
my $after = $node->safe_psql('postgres',
       'SELECT crashit_reset_counters(); '
     . 'SELECT connections_accepted FROM crashit_status()');
($after) = ($after =~ /(\d+)\s*$/);
is($after, '0', 'crashit_reset_counters zeroes connections_accepted');

# status also exposes enabled, action label, uptime and seed
my ($enabled, $action, $uptime, $seed) = split /\|/, $node->safe_psql('postgres',
    'SELECT enabled, action, uptime_seconds >= 0, seed FROM crashit_status()');
is($enabled, 'f', 'status reports enabled=off by default');
is($action, 'backend_segv', 'status reports the configured action label');
is($uptime, 't', 'status reports a non-negative uptime');
is($seed, '0', 'status reports the seed');

$node->stop;
done_testing();
