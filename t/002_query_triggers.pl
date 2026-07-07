# per-backend query triggers: query_number, query_match (incl. utility),
# statement_count, trigger_mode, and probability+seed reproducibility
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $node = PostgreSQL::Test::Cluster->new('query_trig');
$node->init;
$node->append_conf('postgresql.conf', "shared_preload_libraries = 'pg_crashit'");
$node->append_conf('postgresql.conf', "restart_after_crash = on");
$node->start;
$node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

# Apply a batch of crashit GUCs (all PGC_SIGHUP) by writing the config file and
# reloading via SIGHUP.  This avoids reconfiguring through a SQL backend, which
# an already-armed rule would crash mid-statement.  Callers always pass a full
# batch (%off + overrides) so every key is reset each time (last value wins).
sub apply
{
    my ($n, %kv) = @_;
    my $conf = '';
    $conf .= "$_ = '$kv{$_}'\n" for sort keys %kv;
    $n->append_conf('postgresql.conf', $conf);
    $n->reload;
}

# defaults used across tests, reset the ones a given test does not use
my %off = (
    'crashit.enabled'               => 'on',
    'crashit.action'                => 'backend_fatal',
    'crashit.trigger_mode'          => 'any',
    'crashit.probability'           => '1.0',
    'crashit.seed'                  => '0',
    'crashit.on_query_number'       => '0',
    'crashit.on_statement_count'    => '0',
    'crashit.on_query_match'        => '',
    'crashit.on_connection_seconds' => '0',
);

# --- on_query_number fires on the Nth query ---
apply($node, %off, 'crashit.on_query_number' => '3');
my ($ret) = $node->psql('postgres', 'SELECT 1; SELECT 2;');
is($ret, 0, 'two queries survive when on_query_number=3');
($ret) = $node->psql('postgres', 'SELECT 1; SELECT 2; SELECT 3; SELECT 4;');
isnt($ret, 0, 'third query fires on_query_number=3');
is($node->safe_psql('postgres', 'SELECT 1'), '1', 'server up after query_number fire');

# --- on_query_match: substring match, including a utility statement ---
apply($node, %off, 'crashit.on_query_match' => 'CRASHIT_MARKER');
($ret) = $node->psql('postgres', "SELECT 'no such token'");
is($ret, 0, 'non-matching query does not fire');
($ret) = $node->psql('postgres', "SELECT 'CRASHIT_MARKER'");
isnt($ret, 0, 'matching query fires on_query_match');

apply($node, %off, 'crashit.on_query_match' => 'DROP TABLE');
$node->safe_psql('postgres', 'CREATE TABLE victim(x int)');
($ret) = $node->psql('postgres', 'DROP TABLE victim');
isnt($ret, 0, 'utility statement matched via ProcessUtility hook');
is($node->safe_psql('postgres', 'SELECT 1'), '1', 'server up after utility match');

# --- on_statement_count counts utility statements too ---
apply($node, %off, 'crashit.on_statement_count' => '1');
($ret) = $node->psql('postgres', "SET application_name = 'x'");
isnt($ret, 0, 'a single utility statement fires on_statement_count=1');

# --- trigger_mode=all requires all active conditions ---
apply($node, %off,
    'crashit.trigger_mode'    => 'all',
    'crashit.on_query_number' => '1',
    'crashit.on_query_match'  => 'FIREME');
($ret) = $node->psql('postgres', 'SELECT 1');       # number ok, match no
is($ret, 0, 'trigger_mode=all does not fire when only one condition holds');
($ret) = $node->psql('postgres', "SELECT 'FIREME'"); # both hold
isnt($ret, 0, 'trigger_mode=all fires when all conditions hold');

# --- probability + seed reproducibility ---
apply($node, %off,
    'crashit.on_query_number' => '1',
    'crashit.probability'     => '0.5',
    'crashit.seed'            => '42');

sub fire_point
{
    my ($n) = @_;
    my $sql = join('', map { "SELECT $_;" } (1 .. 80));
    my (undef, $out) =
      $n->psql('postgres', $sql, extra_params => [ '-At' ]);
    my @rows = grep { /^\d+$/ } split /\n/, $out;
    return scalar(@rows);
}
my $fp1 = fire_point($node);
my $fp2 = fire_point($node);
cmp_ok($fp1, '<', 80, 'probability trigger fires within the sequence');
is($fp1, $fp2, 'same seed reproduces the same fire point');

# --- enabled=off makes everything inert ---
apply($node, %off, 'crashit.enabled' => 'off',
    'crashit.on_query_number' => '1');
($ret) = $node->psql('postgres', 'SELECT 1; SELECT 2; SELECT 3;');
is($ret, 0, 'enabled=off: no trigger fires');

$node->stop;
done_testing();
