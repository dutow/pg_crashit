# guardrails: enabled=off is fully inert, invalid values rejected,
# empty action_set with action=random warns and does not crash
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('guardrails');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
$node->append_conf('postgresql.conf', "restart_after_crash = on");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

sub set_conf
{
    my ($n, %kv) = @_;
    my $conf = '';
    $conf .= "$_ = '$kv{$_}'\n" for sort keys %kv;
    $n->append_conf('postgresql.conf', $conf);
    $n->reload;
}

# --- (a) every trigger armed but enabled=off: nothing fires ---
set_conf($node,
    'crashit.enabled'              => 'off',
    'crashit.action'               => 'backend_kill',
    'crashit.victim'               => 'random_backend',
    'crashit.check_interval_ms'    => '200',
    'crashit.on_query_number'      => '1',
    'crashit.on_query_match'       => 'SELECT',
    'crashit.on_statement_count'   => '1',
    'crashit.on_connection_seconds' => '1',
    'crashit.on_uptime_seconds'    => '1',
    'crashit.on_connection_number' => '1');
for my $i (1 .. 5)
{
    is($node->safe_psql('postgres', "SELECT $i"), "$i",
       "enabled=off: query $i runs unaffected");
}
select(undef, undef, undef, 1.5);    # let the bgworker tick several times
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'enabled=off: node still up after bgworker ticks');

# --- (b) invalid values are rejected ---
my ($ret, $out, $err) =
  $node->psql('postgres', "ALTER SYSTEM SET crashit.on_uptime_seconds = '-5'");
isnt($ret, 0, 'negative value is rejected');
($ret, $out, $err) =
  $node->psql('postgres', "ALTER SYSTEM SET crashit.on_uptime_seconds = 'nan'");
isnt($ret, 0, 'non-numeric value is rejected');

# --- (c) action=random with empty action_set warns, does not crash ---
my $logstart = -s $node->logfile;
set_conf($node,
    'crashit.enabled'         => 'on',
    'crashit.action'          => 'random',
    'crashit.action_set'      => '',
    'crashit.on_query_number' => '1');
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'empty action_set: query survives');
ok($node->log_contains(qr/action_set is empty|no valid actions/, $logstart),
   'empty action_set logs a WARNING');

# disarm before teardown
set_conf($node, 'crashit.enabled' => 'off');
$node->stop;
done_testing();
