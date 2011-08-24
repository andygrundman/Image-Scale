use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More;
require Test::NoWarnings;

use Image::Scale;

### PNG tests pass with both 1.4.3 and 1.2.44, don't need to skip any

my $png_version = Image::Scale->png_version();

if ($png_version) {
    plan tests => 46;
}
else {
    plan skip_all => 'Image::Scale not built with libpng support';
}

my $tmpdir = catdir( $FindBin::Bin, 'tmp' );
if ( -d $tmpdir ) {
    File::Path::rmtree($tmpdir);
}
mkdir $tmpdir;

my @types = qw(
    rgb
    rgba
    rgba_interlaced
    rgba16
    gray
    gray_alpha
    gray_interlaced
    palette
    palette_alpha
);

# We don't need to test all resizes, JPEG can do that
my @resizes = qw(
    resize_gd_fixed_point
);

# width/height
for my $type ( @types ) {    
    my $im = Image::Scale->new( _f("${type}.png") );
    
    is( $im->width, 160, "PNG $type width ok" );
    is( $im->height, 120, "PNG $type height ok" );
}

# Normal width resize
for my $resize ( @resizes ) {
    for my $type ( @types ) {
        my $outfile = _tmp("${type}_${resize}_w100.png");
        
        my $im = Image::Scale->new( _f("${type}.png") );
        $im->$resize( { width => 100 } );
        $im->save_png($outfile);
        my $data = $im->as_png();
    
        is( _compare( _load($outfile), "${type}_${resize}_w100.png" ), 1, "PNG $type $resize 100 file ok" );
        is( _compare( \$data, "${type}_${resize}_w100.png" ), 1, "PNG $type $resize 100 scalar ok" );
    }
}

# XXX palette_bkgd

# corrupt files from PNG test suite
# x00n0g01 - empty 0x0 grayscale file 
# xcrn0g04 - added cr bytes
{
    Test::NoWarnings::clear_warnings();

    my $im = Image::Scale->new( _f("x00n0g01.png") );
    
    # Test that $im is undef when new fails
    ok( !defined $im, 'new() returns undef on error ok' );
    
    # Test that the correct warning was output
    my @warnings = Test::NoWarnings::warnings();
    like( $warnings[0]->getMessage, qr/zero in IHDR/, 'PNG corrupt warning 1 output ok' );
}

{
    Test::NoWarnings::clear_warnings();

    my $im = Image::Scale->new( _f("xcrn0g04.png") );
    
    # This file won't be seen as PNG, so generates a generic unknown warning
    my @warnings = Test::NoWarnings::warnings();
    like( $warnings[0]->getMessage, qr/Image::Scale unknown file type/, 'PNG corrupt header ok' );
}

# XXX test for valid header but error during image_png_load()

# multiple resize calls on same $im object, should throw away previous resize data
{
    for my $resize ( @resizes ) {
        my $outfile = _tmp("rgba_multiple_${resize}.png");
        my $im = Image::Scale->new( _f("rgba.png") );
        $im->$resize( { width => 50 } );
        $im->$resize( { width => 75 } );
        $im->save_png($outfile);
        
        is( _compare( _load($outfile), "rgba_multiple_${resize}.png" ), 1, "PNG multiple resize $resize ok" );
    }
}

# resize from PNG in scalar
{
    my $dataref = _load( _f("rgba.png") );
    
    my $outfile = _tmp("rgba_resize_gd_fixed_point_w100.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "rgba_resize_gd_fixed_point_w100.png" ), 1, "PNG resize_gd_fixed_point from scalar ok" );
}

# resize multiple from PNG scalar
{
    my $dataref = _load( _f("rgba.png") );
    
    my $outfile = _tmp("rgba_resize_gd_fixed_point_w100.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 150 } );
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "rgba_resize_gd_fixed_point_w100.png" ), 1, "PNG resize_gd_fixed_point multiple from scalar ok" );
}

# offset image in MP3 ID3v2 tag
{
    my $outfile = _tmp("apic_gd_fixed_point_w50.png");
    my $im = Image::Scale->new(
        _f('v2.4-apic-png-350-58618.mp3'),
        { offset => 350, length => 58618 }
    );
    
    is( $im->width, 320, 'PNG from offset ID3 tag width ok' );
    is( $im->height, 240, 'PNG from offset ID3 tag height ok' );
    
    $im->resize_gd_fixed_point( { width => 50 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "apic_gd_fixed_point_w50.png" ), 1, "PNG resize_gd_fixed_point from offset ID3 tag ok" );
}

# 1-height image that would previously try to resize to 0-height
{
    my $dataref = _load( _f("height1.png") );
    
    my $outfile = _tmp("height1_resize_gd_fixed_point_w100.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "height1_resize_gd_fixed_point_w100.png" ), 1, "PNG 1-height resize ok" );
}

diag("libpng version: $png_version");

END {
    File::Path::rmtree($tmpdir);
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', 'png', shift );
}

sub _tmp {
    return catfile( $tmpdir, shift );
}

sub _load {
    my $path = shift;
    
    open my $fh, '<', $path or die "Cannot open $path";
    binmode $fh;
    my $data = do { local $/; <$fh> };
    close $fh;
    
    return \$data;
}    

sub _compare {
    my ( $test, $path ) = @_;
    
    my $ref = _load( catfile( $FindBin::Bin, 'ref', 'png', $path ) );
    
    return $$ref eq $$test;
}
