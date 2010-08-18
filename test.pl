#!/usr/bin/perl

use strict;
use lib qw(blib/arch blib/lib);
#use Time::HiRes qw(sleep);
use Data::Dump qw(dump);
use Devel::Peek;
use Image::Scale;

my $path = shift;
my $height = shift;

{
    my $img = Image::Scale->new($path);
    
    #$img->resize_gd( { height => $height, width => $height, keep_aspect => 1 } );
    #$img->resize_gd_fixed_point( { height => $height, width => $height, keep_aspect => 1 } );
    #$img->resize_gm( { height => $height, width => $height, keep_aspect => 1 } );
    $img->resize_gm_fixed_point( { height => $height, width => $height, keep_aspect => 1 } );

    #$img->save_jpeg('resized.jpg');
    
    #open my $fh, '>', 'resized.jpg';
    #print $fh $img->as_jpeg();
    #close $fh;
    
    $img->save_png('resized.png');

    #open my $fh, '>', 'resized.png';
    #print $fh $img->as_png();
    #close $fh;
}
