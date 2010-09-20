use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More;
require Test::NoWarnings;

use Image::Scale;

my $jpeg_version = Image::Scale->jpeg_version();

if ($jpeg_version) {
    plan tests => 112;
}
else {
    plan skip_all => 'Image::Scale not built with libjpeg support';
}

my $tmpdir = catdir( $FindBin::Bin, 'tmp' );
if ( -d $tmpdir ) {
    File::Path::rmtree($tmpdir);
}
mkdir $tmpdir;

my @types = qw(
    rgb
    rgb_progressive
    gray
    gray_progressive
    cmyk
);

my @resizes = qw(
    resize_gd
    resize_gd_fixed_point
    resize_gm
    resize_gm_fixed_point
);

# width/height
for my $type ( @types ) {    
    my $im = Image::Scale->new( _f("${type}.jpg") );
    
    is( $im->width, 313, "JPEG $type width ok" );
    is( $im->height, 234, "JPEG $type height ok" );
}

SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 44
        if $jpeg_version != 62;
        
    # Normal width resize
    for my $resize ( @resizes ) {
        for my $type ( @types ) {
            my $outfile = _tmp("${type}_${resize}_w100.jpg");
        
            my $im = Image::Scale->new( _f("${type}.jpg") );
            $im->$resize( { width => 100 } );
            $im->save_jpeg($outfile);
            my $data = $im->as_jpeg();
            
            # Only perform file comparisons for fixed-point methods, as floating-point
            # is expected to differ on different platforms
            next if $resize !~ /fixed/;
    
            is( _compare( _load($outfile), "${type}_${resize}_w100.jpg" ), 1, "JPEG $type $resize 100 file ok" );
            is( _compare( \$data, "${type}_${resize}_w100.jpg" ), 1, "JPEG $type $resize 100 scalar ok" );
        }
    }

    # square, without keep_aspect
    for my $resize ( @resizes ) {
        for my $type ( @types ) {
            my $outfile = _tmp("${type}_${resize}_50x50.jpg");
        
            my $im = Image::Scale->new( _f("${type}.jpg") );
            $im->$resize( { width => 50, height => 50 } );
            $im->save_jpeg($outfile);
            my $data = $im->as_jpeg();
            
            # Only perform file comparisons for fixed-point methods, as floating-point
            # is expected to differ on different platforms
            next if $resize !~ /fixed/;
        
            is( _compare( _load($outfile), "${type}_${resize}_50x50.jpg" ), 1, "JPEG $type $resize 50x50 square file ok" );
            is( _compare( \$data, "${type}_${resize}_50x50.jpg" ), 1, "JPEG $type $resize 50x50 square scalar ok" );
        }
    }

    # keep_aspect with height padding
    for my $resize ( @resizes ) {
        my $outfile = _tmp("${resize}_50x50_keep_aspect_height.jpg");
    
        my $im = Image::Scale->new( _f("rgb.jpg") );
        $im->$resize( { width => 50, height => 50, keep_aspect => 1 } );
        $im->save_jpeg($outfile);
        
        # Only perform file comparisons for fixed-point methods, as floating-point
        # is expected to differ on different platforms
        next if $resize !~ /fixed/;
    
        is( _compare( _load($outfile), "${resize}_50x50_keep_aspect_height.jpg" ), 1, "JPEG $resize 50x50 keep_aspect file ok" );
    }

    # keep_aspect with width padding
    for my $resize ( @resizes ) {
        my $outfile = _tmp("${resize}_50x50_keep_aspect_width.jpg");
    
        my $im = Image::Scale->new( _f("exif_90_ccw.jpg") );
        $im->$resize( { width => 50, height => 50, ignore_exif => 1, keep_aspect => 1 } );
        $im->save_jpeg($outfile);
        
        # Only perform file comparisons for fixed-point methods, as floating-point
        # is expected to differ on different platforms
        next if $resize !~ /fixed/;
    
        is( _compare( _load($outfile), "${resize}_50x50_keep_aspect_width.jpg" ), 1, "JPEG $resize 50x50 keep_aspect file ok" );
    }
}

# memory_limit
{
    my $im = Image::Scale->new( _f("rgb.jpg") );
    eval { $im->resize( { width => 50, memory_limit => 1000 } ) };
    like( $@, qr/memory_limit exceeded/, 'JPEG memory_limit ok' );
}

# corrupt truncated file but will still resize with a gray area
SKIP:
{
    Test::NoWarnings::clear_warnings();
    
    my $outfile = _tmp("truncated_50.jpg");
    my $im = Image::Scale->new( _f("truncated.jpg") );
    $im->resize_gd_fixed_point( { width => 50 } );
    $im->save_jpeg($outfile);
    
    # Test that the correct warning was output
    like( (Test::NoWarnings::warnings())[0]->getMessage, qr/premature end of/i, 'JPEG corrupt truncated warning output ok' );
    
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 1
        if $jpeg_version != 62;
    
    is( _compare( _load($outfile), "truncated_50.jpg" ), 1, 'JPEG corrupt truncated resize_gd ok' );
}

# corrupt file resulting in a fatal error
{
    Test::NoWarnings::clear_warnings();
    
    my $im = Image::Scale->new( _f("corrupt.jpg") );
    
    is( $im, undef, 'JPEG corrupt failed new() ok' );
    
    # Test that the correct warning was output
    like( (Test::NoWarnings::warnings())[0]->getMessage, qr/Image::Scale libjpeg error: Corrupt JPEG data/i, 'JPEG corrupt warning output ok' );
}

# keep_aspect bgcolor
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 2
        if $jpeg_version != 62;
        
    for my $resize ( @resizes ) {
        my $outfile = _tmp("bgcolor_${resize}.jpg");
    
        my $im = Image::Scale->new( _f("rgb.jpg") );
        $im->$resize( { width => 50, height => 50, keep_aspect => 1, bgcolor => 0x123456 } );
        $im->save_jpeg($outfile);
        
        # Only perform file comparisons for fixed-point methods, as floating-point
        # is expected to differ on different platforms
        next if $resize !~ /fixed/;
    
        is( _compare( _load($outfile), "bgcolor_${resize}.jpg" ), 1, "JPEG bgcolor $resize ok" );
    }
}

# Exif rotation
# Exif with both width/height specified
# Exif with keep_aspect
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 42
        if $jpeg_version != 62;
        
    my @rotations = qw(
        mirror_horiz
        180
        mirror_vert
        mirror_horiz_270_ccw
        90_ccw
        mirror_horiz_90_ccw
        270_ccw
    );
    
    for my $resize ( @resizes ) {
        for my $r ( @rotations ) {
            my $outfile = _tmp("exif_${r}_${resize}_50.jpg");
            my $im = Image::Scale->new( _f("exif_${r}.jpg") );
            $im->$resize( { width => 50 } );
            $im->save_jpeg($outfile);
            
            # Only perform file comparisons for fixed-point methods, as floating-point
            # is expected to differ on different platforms
            next if $resize !~ /fixed/;
        
            is( _compare( _load($outfile), "exif_${r}_${resize}_50.jpg" ), 1, "JPEG EXIF auto-rotation $r $resize width 50 ok" );
        }

        for my $r ( @rotations ) {
            my $outfile = _tmp("exif_${r}_${resize}_50x50.jpg");
            my $im = Image::Scale->new( _f("exif_${r}.jpg") );
            $im->$resize( { width => 50, height => 50 } );
            $im->save_jpeg($outfile);
            
            # Only perform file comparisons for fixed-point methods, as floating-point
            # is expected to differ on different platforms
            next if $resize !~ /fixed/;
        
            is( _compare( _load($outfile), "exif_${r}_${resize}_50x50.jpg" ), 1, "JPEG EXIF auto-rotation $r $resize 50x50 ok" );
        }

        for my $r ( @rotations ) {
            my $outfile = _tmp("exif_${r}_${resize}_50x50_keep_aspect.jpg");
            my $im = Image::Scale->new( _f("exif_${r}.jpg") );
            $im->$resize( { width => 50, height => 50, keep_aspect => 1 } );
            $im->save_jpeg($outfile);
            
            # Only perform file comparisons for fixed-point methods, as floating-point
            # is expected to differ on different platforms
            next if $resize !~ /fixed/;
        
            is( _compare( _load($outfile), "exif_${r}_${resize}_50x50_keep_aspect.jpg" ), 1, "JPEG EXIF auto-rotation $r $resize 50x50 keep_aspect ok" );
        }
    }
}
        
# Exif with ignore_exif
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 1
        if $jpeg_version != 62;
        
    my $outfile = _tmp("exif_ignore_50.jpg");
    my $im = Image::Scale->new( _f("exif_90_ccw.jpg") );
    $im->resize_gd_fixed_point( { width => 50, ignore_exif => 1 } );
    $im->save_jpeg($outfile);
    
    is( _compare( _load($outfile), "exif_ignore_50.jpg" ), 1, "JPEG EXIF ignore_exif ok" );
}

# multiple resize calls on same $im object, should throw away previous resize data
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 2
        if $jpeg_version != 62;
        
    for my $resize ( @resizes ) {
        my $outfile = _tmp("rgb_multiple_${resize}.jpg");
        my $im = Image::Scale->new( _f("rgb.jpg") );
        $im->$resize( { width => 50 } );
        $im->$resize( { width => 75 } );
        $im->save_jpeg($outfile);
        
        # Only perform file comparisons for fixed-point methods, as floating-point
        # is expected to differ on different platforms
        next if $resize !~ /fixed/;
        
        is( _compare( _load($outfile), "rgb_multiple_${resize}.jpg" ), 1, "JPEG multiple resize $resize ok" );
    }
}

# resize from JPEG in scalar
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 1
        if $jpeg_version != 62;
        
    my $dataref = _load( _f("rgb.jpg") );
    
    my $outfile = _tmp("rgb_resize_gd_fixed_point_w100.jpg");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_jpeg($outfile);
    
    is( _compare( _load($outfile), "rgb_resize_gd_fixed_point_w100.jpg" ), 1, "JPEG resize_gd_fixed_point from scalar ok" );
}

# resize multiple from JPEG scalar
SKIP:
{
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 1
        if $jpeg_version != 62;
        
    my $dataref = _load( _f("rgb.jpg") );
    
    my $outfile = _tmp("rgb_resize_gd_fixed_point_w100.jpg");
    my $im = Image::Scale->new($dataref);
    $im->resize_gd_fixed_point( { width => 150 } );
    $im->resize_gd_fixed_point( { width => 100 } );
    $im->save_jpeg($outfile);
    
    is( _compare( _load($outfile), "rgb_resize_gd_fixed_point_w100.jpg" ), 1, "JPEG resize_gd_fixed_point multiple from scalar ok" );
}

# offset image in MP3 ID3v2 tag
SKIP:
{
    my $outfile = _tmp("apic_gd_fixed_point_w50.jpg");
    my $im = Image::Scale->new(
        _f('v2.4-apic-jpg-351-2103.mp3'),
        { offset => 351, length => 2103 }
    );
    
    is( $im->width, 192, 'JPEG from offset ID3 tag width ok' );
    is( $im->height, 256, 'JPEG from offset ID3 tag height ok' );
    
    $im->resize_gd_fixed_point( { width => 50 } );
    $im->save_jpeg($outfile);
    
    skip "libjpeg version is $jpeg_version, skipping file comparison tests (they require v62)", 1
        if $jpeg_version != 62;
    
    is( _compare( _load($outfile), "apic_gd_fixed_point_w50.jpg" ), 1, "JPEG resize_gd_fixed_point from offset ID3 tag ok" );
}

# Exif tag larger than 4K
{
    my $im = Image::Scale->new( _f('large-exif.jpg') );
    is( $im->width, 200, 'JPEG large Exif ok' );
}

# XXX fatal errors during compression, will this ever actually happen?

# XXX progressive JPEG with/without memory_limit

diag("libjpeg version: $jpeg_version");

END {
    File::Path::rmtree($tmpdir);
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', 'jpg', shift );
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
    
    my $ref = _load( catfile( $FindBin::Bin, 'ref', 'jpg', $path ) );
    
    return $$ref eq $$test;
}
