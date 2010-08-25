use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 2 * 5;

use Image::Scale;

my @types = qw(
    rgb
    rgb_progressive
    gray
    gray_progressive
    cmyk
);    

for my $type ( @types ) {
    my $im = Image::Scale->new( _f("$type.jpg") );
    
    is( $im->width, 313, "JPEG $type width ok" );
    is( $im->height, 234, "JPEG $type height ok" );
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', shift );
}