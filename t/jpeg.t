use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 8;

use Image::Scale;

# XXX reusable functions for each format type

# 3-channel RGB
{
    my $im = Image::Scale->new( _f('rgb_313x234.jpg') );
    
    is( $im->width, 313, 'JPEG RGB width ok' );
    is( $im->height, 234, 'JPEG RGB height ok' );
}


sub _f {    
    return catfile( $FindBin::Bin, 'images', shift );
}