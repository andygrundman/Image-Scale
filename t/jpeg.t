use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 4;

use Image::Scale;

## Basics

# 3-channel RGB
{
    my $im = Image::Scale->new( _f('color_313x234.jpg') );
    
    is( $im->width, 313, 'JPEG width ok' );
    is( $im->height, 234, 'JPEG height ok' );
}

# 1-channel grayscale
{
    my $im = Image::Scale->new( _f('gray_313x234.jpg') );
    
    is( $im->width, 313, 'JPEG grayscale width ok' );
    is( $im->height, 234, 'JPEG grayscale height ok' );
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', shift );
}