use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More tests => 1;
require Test::NoWarnings;

use Image::Scale;

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

# Normal width resize
for my $resize ( @resizes ) {
    for my $type ( @types ) {
        my $outfile = _tmp("${type}_${resize}_w50.png");
        
        my $im = Image::Scale->new( _f("${type}.bmp") );
        $im->$resize( { width => 50 } );
        $im->save_png($outfile);
    
        #is( _compare( _load($outfile), "${type}_${resize}_w50.png" ), 1, "BMP $type $resize 50 file ok" );
    }
}

# XXX flipped image (negative height)

END {
    #File::Path::rmtree($tmpdir);
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
