use strict;

use File::Path ();
use File::Spec::Functions;
use FindBin ();
use Test::More tests => 9;
require Test::NoWarnings;

use Image::Scale;

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
for my $resize ( @resizes ) {
    for my $type ( @types ) {
        my $outfile = _tmp("${type}_${resize}_w100.png");
        
        my $im = Image::Scale->new( _f("${type}.gif") );
        $im->$resize( { width => 100 } );
        $im->save_png($outfile);
    
        is( _compare( _load($outfile), "${type}_${resize}_w100.png" ), 1, "GIF $type $resize 100 file ok" );
    }
}

# XXX corrupt file

# XXX offset image in MP3 ID3v2 tag

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
