use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More tests => 30;
require Test::NoWarnings;

use Image::Scale;

my $png_version = Image::Scale->png_version();

my $tmpdir = catdir( $FindBin::Bin, 'tmp' );
if ( -d $tmpdir ) {
    File::Path::rmtree($tmpdir);
}
mkdir $tmpdir;

my @types = qw(
    1bit
    4bit
    8bit
    16bit_555
    16bit_565
    24bit
    32bit
    32bit_alpha
);

# XXX 4bit_rle, 8bit_os2, 8bit_rle

# We don't need to test all resizes, JPEG can do that
my @resizes = qw(
    resize_gd_fixed_point
);

# width/height
for my $type ( @types ) {    
    my $im = Image::Scale->new( _f("${type}.bmp") );
    
    is( $im->width, 127, "BMP $type width ok" );
    is( $im->height, 64, "BMP $type height ok" );
}

SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 8 if !$png_version;
        
    # Normal width resize
    for my $resize ( @resizes ) {
        for my $type ( @types ) {
            my $outfile = _tmp("${type}_${resize}_w127.png");
        
            my $im = Image::Scale->new( _f("${type}.bmp") );
            $im->$resize( { width => 127 } );
            $im->save_png($outfile);
    
            is( _compare( _load($outfile), "${type}_${resize}_w127.png" ), 1, "BMP $type $resize 127 file ok" );
        }
    }
}

# XXX flipped image (negative height)

# multiple resize calls on same $im object, should throw away previous resize data
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
        
    my $outfile = _tmp("24bit_multiple_resize_gd_fixed_point.png");
    my $im = Image::Scale->new( _f("24bit.bmp") );
    $im->resize_gd_fixed_point( { width => 50 } );
    $im->resize_gd_fixed_point( { width => 127 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "24bit_multiple_resize_gd_fixed_point.png" ), 1, "BMP multiple resize_gd_fixed_point ok" );
}

# resize from BMP in scalar
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    my $dataref = _load( _f("24bit.bmp") );
    
    my $outfile = _tmp("24bit_resize_gd_fixed_point_w127_scalar.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 127 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "24bit_resize_gd_fixed_point_w127.png" ), 1, "BMP resize_gd_fixed_point from scalar ok" );
}

# resize multiple from BMP scalar
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    my $dataref = _load( _f("24bit.bmp") );
    
    my $outfile = _tmp("24bit_multiple_resize_gd_fixed_point_w127_scalar.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 150 } );
    $im->resize_gd_fixed_point( { width => 127 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "24bit_resize_gd_fixed_point_w127.png" ), 1, "BMP resize_gd_fixed_point multiple from scalar ok" );
}

# offset image in MP3 ID3v2 tag
SKIP:
{
    my $outfile = _tmp("apic_gd_fixed_point_w127.png");
    my $im = Image::Scale->new(
        _f('v2.4-apic-bmp-318-24632.mp3'),
        { offset => 318, length => 24632 }
    );
    
    is( $im->width, 127, 'BMP from offset ID3 tag width ok' );
    is( $im->height, 64, 'BMP from offset ID3 tag height ok' );
    
    $im->resize_gd_fixed_point( { width => 127 } );
    
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "apic_gd_fixed_point_w127.png" ), 1, "BMP resize_gd_fixed_point from offset ID3 tag ok" );
}

END {
    File::Path::rmtree($tmpdir);
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', 'bmp', shift );
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
    
    my $ref = _load( catfile( $FindBin::Bin, 'ref', 'bmp', $path ) );
    
    return $$ref eq $$test;
}
