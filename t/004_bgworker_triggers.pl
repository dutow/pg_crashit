# background worker: uptime / connection_number triggers with random-backend kills
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('bgw_trig');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
$node->append_conf('postgresql.conf', "restart_after_crash = on");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

# reconfigure by file + SIGHUP so no SQL backend is needed (an armed rule would
# otherwise crash the reconfiguring session or the bgworker would kill it)
sub set_conf
{
    my ($n, %kv) = @_;
    my $conf = '';
    $conf .= "$_ = '$kv{$_}'\n" for sort keys %kv;
    $n->append_conf('postgresql.conf', $conf);
    $n->reload;
}
sub disarm { set_conf($_[0], 'crashit.enabled' => 'off'); }

# wait until a background psql session's connection has been killed
sub wait_victim_dead
{
    my ($bg) = @_;
    for (1 .. 100)
    {
        my $ok = eval { $bg->query('SELECT 1'); 1 };
        return 1 if !$ok;
        select(undef, undef, undef, 0.2);
    }
    return 0;
}

# --- uptime trigger + random_backend kill ---
my $bg = $node->background_psql('postgres', on_error_stop => 0, timeout => 20);
$bg->query('SELECT 1');    # register this backend as a live client backend
set_conf($node,
    'crashit.enabled'           => 'on',
    'crashit.action'            => 'backend_kill',
    'crashit.victim'            => 'random_backend',
    'crashit.check_interval_ms' => '200',
    'crashit.on_uptime_seconds' => '1');
ok(wait_victim_dead($bg), 'uptime trigger kills a random client backend');
disarm($node);
eval { $bg->quit };
$node->poll_query_until('postgres', 'SELECT 1', '1')
  or die 'node did not recover after random-backend kill (uptime)';
pass('node recovered after random-backend kill (uptime)');

# --- connection_number trigger + random_backend kill ---
my $bg2 = $node->background_psql('postgres', on_error_stop => 0, timeout => 20);
$bg2->query('SELECT 1');
set_conf($node,
    'crashit.enabled'             => 'on',
    'crashit.action'              => 'backend_kill',
    'crashit.victim'              => 'random_backend',
    'crashit.check_interval_ms'   => '200',
    'crashit.on_uptime_seconds'   => '0',
    'crashit.on_connection_number' => '1');
ok(wait_victim_dead($bg2), 'connection_number trigger kills a random client backend');
disarm($node);
eval { $bg2->quit };
$node->poll_query_until('postgres', 'SELECT 1', '1')
  or die 'node did not recover after random-backend kill (connection_number)';
pass('node recovered after random-backend kill (connection_number)');

# --- enabled=off: worker idles, no kills ---
set_conf($node,
    'crashit.enabled'             => 'off',
    'crashit.action'              => 'backend_kill',
    'crashit.victim'              => 'random_backend',
    'crashit.check_interval_ms'   => '200',
    'crashit.on_connection_number' => '1');
my $bg3 = $node->background_psql('postgres', on_error_stop => 0, timeout => 20);
$bg3->query('SELECT 1');
select(undef, undef, undef, 1.5);    # several bgworker ticks
is($bg3->query('SELECT 42'), '42', 'enabled=off: bgworker does not kill backends');
$bg3->quit;

$node->stop;
done_testing();
