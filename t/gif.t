use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More;
require Test::NoWarnings;

use Image::Scale;

my $gif_version = Image::Scale->gif_version();
my $png_version = Image::Scale->png_version();

if ($gif_version) {
    plan tests => 17;
}
else {
    plan skip_all => 'Image::Scale not built with giflib/libungif support';
}

my $tmpdir = catdir( $FindBin::Bin, 'tmp' );
if ( -d $tmpdir ) {
    File::Path::rmtree($tmpdir);
}
mkdir $tmpdir;

my @types = qw(
    transparent
    white
    interlaced_256
);

my @resizes = qw(
    resize_gd_fixed_point
);

# width/height
for my $type ( @types ) {    
    my $im = Image::Scale->new( _f("${type}.gif") );
    
    is( $im->width, 160, "GIF $type width ok" );
    is( $im->height, 120, "GIF $type height ok" );
}

# Normal width resize
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 3 if !$png_version;
    
    for my $resize ( @resizes ) {
        for my $type ( @types ) {
            my $outfile = _tmp("${type}_${resize}_w100.png");
        
            my $im = Image::Scale->new( _f("${type}.gif") );
            $im->$resize( { width => 100 } );
            $im->save_png($outfile);
    
            is( _compare( _load($outfile), "${type}_${resize}_w100.png" ), 1, "GIF $type $resize 100 file ok" );
        }
    }
}

# corrupt file
{
    no strict 'subs';
    no warnings;
    
    Test::NoWarnings::clear_warnings();
    
    my $im = Image::Scale->new( _f("corrupt.gif") );

    # Hide stderr
    open OLD_STDERR, '>&', STDERR;
    close STDERR;
    
    my $ok = $im->resize_gd_fixed_point( { width => 50 } );
    
    # Restore stderr
    open STDERR, '>&', OLD_STDERR;
    
    is( $ok, 0, 'GIF corrupt failed resize ok' );
    
    # Test that the correct warning was output
    like( (Test::NoWarnings::warnings())[0]->getMessage, qr/Image::Scale unable to read GIF file/i, 'GIF corrupt error output ok' );
}

# multiple resize calls on same $im object, should throw away previous resize data
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    my $outfile = _tmp("transparent_multiple_resize_gd_fixed_point.png");
    my $im = Image::Scale->new( _f("transparent.gif") );
    $im->resize_gd_fixed_point( { width => 50 } );
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "transparent_multiple_resize_gd_fixed_point.png" ), 1, "GIF multiple resize_gd_fixed_point ok" );
}

# resize from GIF in scalar
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    my $dataref = _load( _f("transparent.gif") );
    
    my $outfile = _tmp("transparent_resize_gd_fixed_point_w100_scalar.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "transparent_resize_gd_fixed_point_w100.png" ), 1, "GIF resize_gd_fixed_point from scalar ok" );
}

# resize multiple from GIF scalar
SKIP:
{
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    my $dataref = _load( _f("transparent.gif") );
    
    my $outfile = _tmp("transparent_multiple_resize_gd_fixed_point_w100_scalar.png");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 150 } );
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "transparent_resize_gd_fixed_point_w100.png" ), 1, "GIF resize_gd_fixed_point multiple from scalar ok" );
}

# offset image in MP3 ID3v2 tag
SKIP:
{
    my $outfile = _tmp("apic_gd_fixed_point_w100.png");
    my $im = Image::Scale->new(
        _f('v2.4-apic-gif-318-5169.mp3'),
        { offset => 318, length => 5169 }
    );
    
    is( $im->width, 160, 'GIF from offset ID3 tag width ok' );
    is( $im->height, 120, 'GIF from offset ID3 tag height ok' );
    
    $im->resize_gd_fixed_point( { width => 100 } );
    
    skip "PNG support not built, skipping file comparison tests", 1 if !$png_version;
    
    $im->save_png($outfile);
    
    is( _compare( _load($outfile), "apic_gd_fixed_point_w100.png" ), 1, "GIF resize_gd_fixed_point from offset ID3 tag ok" );
}

diag("giflib/libungif version: $gif_version");

END {
    File::Path::rmtree($tmpdir);
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', 'gif', shift );
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
    
    my $ref = _load( catfile( $FindBin::Bin, 'ref', 'gif', $path ) );
    
    return $$ref eq $$test;
}
