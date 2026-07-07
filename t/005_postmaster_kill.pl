# background worker: postmaster kill / sigquit actions bring the node down
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

# probe until the node stops accepting connections
sub wait_down
{
    my ($n) = @_;
    for (1 .. 100)
    {
        my ($ret) = $n->psql('postgres', 'SELECT 1');
        return 1 if $ret != 0;
        select(undef, undef, undef, 0.2);
    }
    return 0;
}

sub run_case
{
    my ($name, $action) = @_;

    my $node = PostgreSQL::Test::Cluster->new($name);
    $node->init;
    $node->append_conf('postgresql.conf',
        "shared_preload_libraries = 'pg_crashit'");
    $node->append_conf('postgresql.conf', "crashit.enabled = 'on'");
    $node->append_conf('postgresql.conf', "crashit.action = '$action'");
    $node->append_conf('postgresql.conf', "crashit.check_interval_ms = '200'");
    $node->append_conf('postgresql.conf', "crashit.on_uptime_seconds = '1'");
    $node->start;
    $node->safe_psql('postgres', 'CREATE EXTENSION pg_crashit');

    ok(wait_down($node), "$action brings the node down");

    # the postmaster is gone; reconcile framework state so teardown is clean
    $node->{_pid} = undef;
    eval { $node->stop('immediate', fail_ok => 1) };
}

run_case('pm_kill',    'postmaster_kill');
run_case('pm_sigquit', 'postmaster_sigquit');

done_testing();
