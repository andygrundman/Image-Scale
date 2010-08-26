use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More tests => (2 * 5) + ((5 * 4) * 2) + ((5 * 4) * 2) + ((5 * 4) * 2) + 3;
require Test::NoWarnings;

use Image::Scale;

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

# Normal width resize
for my $resize ( @resizes ) {
    for my $type ( @types ) {
        my $outfile = _tmp("${type}_${resize}_w100.jpg");
        
        my $im = Image::Scale->new( _f("${type}.jpg") );
        $im->$resize( { width => 100 } );
        $im->save_jpeg($outfile);
        my $data = $im->as_jpeg();
    
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
        
        is( _compare( _load($outfile), "${type}_${resize}_50x50.jpg" ), 1, "JPEG $type $resize 50x50 square file ok" );
        is( _compare( \$data, "${type}_${resize}_50x50.jpg" ), 1, "JPEG $type $resize 50x50 square scalar ok" );
    }
}

# keep_aspect with padding
# XXX gm keep_aspect not implemented yet
for my $resize ( @resizes ) {
    for my $type ( @types ) {
        my $outfile = _tmp("${type}_${resize}_50x50_keep_aspect.jpg");
        
        my $im = Image::Scale->new( _f("${type}.jpg") );
        $im->$resize( { width => 50, height => 50, keep_aspect => 1 } );
        $im->save_jpeg($outfile);
        my $data = $im->as_jpeg();
        
        #is( _compare( _load($outfile), "${type}_${resize}_50x50_keep_aspect.jpg" ), 1, "JPEG $type $resize 50x50 keep_aspect file ok" );
        #is( _compare( \$data, "${type}_${resize}_50x50_keep_aspect.jpg" ), 1, "JPEG $type $resize 50x50 keep_aspect scalar ok" );
    }
}

# memory_limit
{
    my $im = Image::Scale->new( _f("rgb.jpg") );
    eval { $im->resize( { width => 50, memory_limit => 1000 } ) };
    like( $@, qr/memory_limit exceeded/, 'JPEG memory_limit ok' );
}

# corrupt truncated file
{
    Test::NoWarnings::clear_warnings();
    
    my $outfile = _tmp("truncated_50.jpg");
    my $im = Image::Scale->new( _f("truncated.jpg") );
    $im->resize_gd( { width => 50 } );
    $im->save_jpeg($outfile);
    
    # Test that the correct warning was output
    like( (Test::NoWarnings::warnings())[0]->getMessage, qr/Image::Scale error: Premature end of JPEG file/, 'JPEG corrupt truncated warning output ok' );
    
    is( _compare( _load($outfile), "truncated_50.jpg" ), 1, 'JPEG corrupt truncated resize_gd ok' );
}

# XXX keep_aspect bgcolor

# Exif rotation
{
    my @rotations = qw(
        mirror_horiz
        180
        mirror_vert
        mirror_horiz_270_ccw
        90_ccw
        mirror_horiz_90_ccw
        270_ccw
    );
    
    for my $r ( @rotations ) {
        my $outfile = _tmp("exif_${r}_50.jpg");
        my $im = Image::Scale->new( _f("exif_${r}.jpg") );
        $im->resize( { width => 50 } );
        $im->save_jpeg($outfile);
        
        # XXX broken if rotated...
    }
}
        
# XXX Exif with ignore_exif
# XXX Exif with keep_aspect, make sure padding is calculated correctly for orientation >= 5

END {
    #File::Path::rmtree($tmpdir);
}

sub _f {    
    return catfile( $FindBin::Bin, 'images', shift );
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
    
    my $ref = _load( catfile( $FindBin::Bin, 'ref', $path ) );
    
    return $$ref eq $$test;
}

sub _out {
    my $ref = shift;
    
    open my $fh, '>', 'out.jpg';
    binmode $fh;
    print $fh $$ref;
    close $fh;
}