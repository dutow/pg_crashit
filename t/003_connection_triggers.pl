# per-backend connection-seconds trigger
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('conn_trig');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
$node->append_conf('postgresql.conf', "restart_after_crash = on");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

sub apply
{
    my ($n, %kv) = @_;
    my $conf = '';
    $conf .= "$_ = '$kv{$_}'\n" for sort keys %kv;
    $n->append_conf('postgresql.conf', $conf);
    $n->reload;
}

# fire once the session has been open >= 1 second
apply($node,
    'crashit.enabled'               => 'on',
    'crashit.action'                => 'backend_fatal',
    'crashit.on_connection_seconds' => '1');

# a session that stays open past the threshold crashes on its next query;
# conn_start is stamped on the first query, so sleep then query again
my ($ret, $out, $err) = $node->psql('postgres',
    'SELECT 1; SELECT pg_sleep(1.5); SELECT 2;');
isnt($ret, 0, 'session crashes once it has been open past the threshold');
is($node->safe_psql('postgres', 'SELECT 1'), '1',
   'server still up after connection_seconds fire');

# a short-lived session (below the threshold) is unaffected
apply($node,
    'crashit.enabled'               => 'on',
    'crashit.action'                => 'backend_fatal',
    'crashit.on_connection_seconds' => '100');
($ret) = $node->psql('postgres', 'SELECT 1; SELECT pg_sleep(1.5); SELECT 2;');
is($ret, 0, 'session below the threshold is not crashed');

$node->stop;
done_testing();
