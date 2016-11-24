use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More;

use Image::Scale;

eval { require Path::Tiny };
if ( $@ ) {
    plan skip_all => 'Path::Tiny not installed';
}

plan tests => 3;

my $tmpdir = catdir( $FindBin::Bin, 'tmp' );
if ( -d $tmpdir ) {
    File::Path::rmtree($tmpdir);
}
mkdir $tmpdir;

# Test Path::Tiny stringification
SKIP:
{
    skip "Image::Scale not built with libjpeg support", 2 unless Image::Scale->jpeg_version;

    my $im = Image::Scale->new( Path::Tiny::path( _f("jpg/rgb.jpg") ) );
    is( $im->width, 313, "Path::Tiny stringify in new() ok" );

    $im->resize_gd( { width => 50, height => 50 } );

    my $outjpg = _tmp("resized.jpg");
    $im->save_jpeg( Path::Tiny::path($outjpg) );
    ok( -e $outjpg, 'Path::Tiny stringify in save_jpeg() ok' );
}

SKIP:
{
    skip "Image::Scale not built with libpng support", 1 unless Image::Scale->png_version;

    my $im = Image::Scale->new( _f("png/rgb.png") );

    $im->resize_gd( { width => 50, height => 50 } );

    my $outpng = _tmp("resized.png");
    $im->save_png( Path::Tiny::path($outpng) );
    ok( -e $outpng, 'Path::Tiny stringify in save_png() ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'images', shift );
}

sub _tmp {
    return catfile( $tmpdir, shift );
}
