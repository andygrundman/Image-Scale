#!/usr/bin/perl

use strict;
use lib qw(blib/arch blib/lib);
#use Time::HiRes qw(sleep);
use Data::Dump qw(dump);
use Devel::Peek;
use File::Slurp qw(read_file);
use Image::Scale;

my $path = shift;
my $height = shift;

# From file
if (0) {
    my $img = Image::Scale->new($path);
    
    #$img->resize_gd( { height => $height, width => $height, keep_aspect => 1 } );
    $img->resize_gd_fixed_point( { height => $height } );
    #$img->resize_gm( { height => $height, width => $height, keep_aspect => 1 } );
    #$img->resize_gm_fixed_point( { height => $height, width => $height, keep_aspect => 1 } );

    #$img->save_jpeg('resized.jpg');
    
    #open my $fh, '>', 'resized.jpg';
    #print $fh $img->as_jpeg();
    #close $fh;
    
    $img->save_png('resized.png');

    #open my $fh, '>', 'resized.png';
    #print $fh $img->as_png();
    #close $fh;
}

# From data
if (1) {
    my $data = read_file($path);
    
    my $img = Image::Scale->new(\$data);
    
    $img->resize_gd_fixed_point( { height => $height } );
    
    $img->save_png('resized.png');
}
    