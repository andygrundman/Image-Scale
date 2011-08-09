#!/usr/bin/perl

use strict;

use Benchmark::Dumb qw(:all);
use Image::Scale;
use GD;

GD::Image->trueColor(1);

my $path = shift || die "Path required\n";
my $width = shift || die "Width required\n";
my $saves = {};

warn "Benchmarking $path resized to $width...\n";

my $jpg = $path =~ /jpe?g/i ? 1 : 0;

my $tests = {
    resize_gd => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd( { width => $width } );
        my $out = $jpg ? $img->as_jpeg() : $img->as_png();
        if ( !$saves->{resize_gd}++ ) {
            open my $fh, '>', 'resize_gd.' . ($jpg ? 'jpg' : 'png');
            print $fh $out;
            close $fh;
        }
    },
    resize_gd_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd_fixed_point( { width => $width } );
        my $out = $jpg ? $img->as_jpeg() : $img->as_png();
        if ( !$saves->{resize_gd_fixed}++ ) {
            open my $fh, '>', 'resize_gd_fixed_point.' . ($jpg ? 'jpg' : 'png');
            print $fh $out;
            close $fh;
        }
    },
    resize_gm => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gm( { width => $width, filter => 'Triangle' } );
        my $out = $jpg ? $img->as_jpeg() : $img->as_png();
        if (!$saves->{"resize_gm"}++) {
            open my $fh, '>', "resize_gm." . ($jpg ? 'jpg' : 'png');
            print $fh $out;
            close $fh;
        }
    },
    resize_gm_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gm_fixed_point( { width => $width } );
        my $out = $jpg ? $img->as_jpeg() : $img->as_png();
        
        if ( !$saves->{resize_gm_fixed}++ ) {
            open my $fh, '>', 'resize_gm_fixed_point.' . ($jpg ? 'jpg' : 'png');
            print $fh $out;
            close $fh;
        }
    },
};

if ( ($jpg && GD::Image->can('newFromJpeg')) || (!$jpg && GD::Image->can('newFromPng')) ) {
    $tests->{gd_resample} = sub {
        my $src = $jpg ? GD::Image->newFromJpeg($path) : GD::Image->newFromPng($path);
        my $dst = GD::Image->new($width, $width);
        $dst->copyResampled(
			$src,
			0, 0,
			0, 0,
			$width, $width,
			$src->width, $src->height
		);

		my $out = $jpg ? $dst->jpeg(90) : $dst->png;

		if (!$saves->{gd_resample}++) {
            open my $fh, '>', 'gd.' . ($jpg ? 'jpg' : 'png');
            print $fh $out;
            close $fh;
        }
	};
}

cmpthese( 0.005, $tests );
