#!/usr/bin/perl

use strict;

# Needed for custom build of GD with JPEG scaling
use lib qw(
    /Users/andy/dev/Slim/7.6/trunk/server/CPAN
    /Users/andy/dev/Slim/7.6/trunk/server/CPAN/arch/5.10/darwin-thread-multi-2level
);

use Benchmark qw(cmpthese);
use Image::Scale;
use GD;

GD::Image->trueColor(1);

my $path = shift || die "Path required\n";
my $width = shift || die "Width required\n";

warn "Benchmarking $path resized to $width...\n";

my $saves = {};

my $make_gm = sub {
    my ($filter, $type) = @_;
    
    return ( $filter => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gm( { width => $width, filter => $filter } );
        if (!$saves->{"resize_gm_$filter"}++) {
            if ($type eq 'png') {
                $img->save_png("resize_gm_${filter}.png");
            }
            else {
                $img->save_jpeg("resize_gm_${filter}.jpg");
            }
        }
    } );
};

my $png = {
    resize_gd => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd( { width => $width } );
        $img->save_png('resize_gd.png') if !$saves->{resize_gd}++;
    },
    resize_gd_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd_fixed_point( { width => $width } );
        $img->save_png('resize_gd_fixed_point.png') if !$saves->{resize_gd_fixed}++;
    },
    #$make_gm->('Point', 'png'),
    #$make_gm->('Box', 'png'),
    $make_gm->('Triangle', 'png'),
    #$make_gm->('Hermite', 'png'),
    #$make_gm->('Hanning', 'png'),
    #$make_gm->('Hamming', 'png'),
    #$make_gm->('Blackman', 'png'),
    #$make_gm->('Gaussian', 'png'),
    #$make_gm->('Quadratic', 'png'),
    #$make_gm->('Cubic', 'png'),
    #$make_gm->('Catrom', 'png'),
    #$make_gm->('Mitchell', 'png'),
    #$make_gm->('Lanczos', 'png'),
    #$make_gm->('Bessel', 'png'),
    #$make_gm->('Sinc', 'png'),
    gm_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gm_fixed_point( { width => $width } );
        $img->save_png('resize_gm_fixed_point.png') if !$saves->{resize_gm_fixed}++;
    },
    gd_resample => sub {
        my $src = GD::Image->newFromPng($path);
        my $dst = GD::Image->new($width, $width);
        $dst->saveAlpha(1);
        $dst->alphaBlending(0);
        $dst->filledRectangle(0, 0, $width, $width, 0x7f000000);
        $dst->copyResampled(
			$src,
			0, 0,
			0, 0,
			$width, $width,
			$src->width, $src->height
		);
		
		if ( !$saves->{gd_resample}++ ) {
    		open my $fh, '>', 'gd.png';
    		print $fh $dst->png;
    		close $fh;
    	}
	},
};

my $jpg = {
    resize_gd => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd( { width => $width } );
        $img->save_jpeg('resize_gd.jpg') if !$saves->{resize_gd}++;
    },
    resize_gd_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gd_fixed_point( { width => $width } );
        $img->save_jpeg('resize_gd_fixed_point.jpg') if !$saves->{resize_gd_fixed}++;
    },
    #$make_gm->('Point', 'jpg'),
    #$make_gm->('Box', 'jpg'),
    $make_gm->('Triangle', 'jpg'),
    #$make_gm->('Hermite', 'jpg'),
    #$make_gm->('Hanning', 'jpg'),
    #$make_gm->('Hamming', 'jpg'),
    #$make_gm->('Blackman', 'jpg'),
    #$make_gm->('Gaussian', 'jpg'),
    #$make_gm->('Quadratic', 'jpg'),
    #$make_gm->('Cubic', 'jpg'),
    #$make_gm->('Catrom', 'jpg'),
    #$make_gm->('Mitchell', 'jpg'),
    #$make_gm->('Lanczos', 'jpg'),
    #$make_gm->('Bessel', 'jpg'),
    #$make_gm->('Sinc', 'jpg'),
    gm_fixed => sub {
        my $img = Image::Scale->new($path);
        $img->resize_gm_fixed_point( { width => $width } );
        $img->save_jpeg('resize_gm_fixed_point.jpg') if !$saves->{resize_gm_fixed}++;
    },
    gd_resample => sub {
        my $src = GD::Image->newFromJpegScaled($path, $width, $width);
        my $dst = GD::Image->new($width, $width);
        $dst->copyResampled(
			$src,
			0, 0,
			0, 0,
			$width, $width,
			$src->width, $src->height
		);
		
		if (!$saves->{gd_resample}++) {
            open my $fh, '>', 'gd.jpg';
            print $fh $dst->jpeg(90);
            close $fh;
        }
	},
};

cmpthese( -5, $path =~ /png/ ? $png : $jpg );
