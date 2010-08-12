use strict;
use warnings;

use File::Spec;
use FindBin ();
use Test::More;

if ( !-e "$FindBin::Bin/../MANIFEST.SKIP" ) {
    plan skip_all => 'Critic test only for developers.';
}
else {
    eval { require Test::Perl::Critic };

    SKIP: {
        if ( $@ ) {
            plan tests => 1;
            skip( 'You must install Test::Perl::Critic to run 04critic.t', 1 );
        }

        my $rcfile = File::Spec->catfile( 't', '04critic.rc' );
        Test::Perl::Critic->import( -profile => $rcfile );
        all_critic_ok();
    }
}
