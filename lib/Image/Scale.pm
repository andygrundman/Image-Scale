package Image::Scale;

use strict;

use constant JPEG_QUALITY              => 90;

use constant IMAGE_SCALE_TYPE_GD       => 0;
use constant IMAGE_SCALE_TYPE_GD_FIXED => 1;
use constant IMAGE_SCALE_TYPE_GM       => 2;
use constant IMAGE_SCALE_TYPE_GM_FIXED => 3;

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Image::Scale', $VERSION);

sub new {
    my $class = shift;
    my $self;
    
    my $file = shift || die "Image::Scale->new requires an image path\n";
    my $opts = shift || {};
    
    if ( ref $file eq 'SCALAR' ) {
        $self = bless {
            data => $file,
            %{$opts},
        };
    }
    else {
        open my $fh, '<', $file || die "Image::Scale couldn't open $file: $!\n";
        binmode $fh;
    
        $self = bless {
            file => $file,
            _fh  => $fh,
            %{$opts},
        };
    }
    
    # XS init, determine the file type and image size
    $self->{_image} = $self->__init();
    
    return $self;
}

sub resize_gd {
    shift->resize( { %{+shift}, type => IMAGE_SCALE_TYPE_GD } );
}

sub resize_gd_fixed_point {
    shift->resize( { %{+shift}, type => IMAGE_SCALE_TYPE_GD_FIXED } );
}

sub resize_gm {
    shift->resize( { %{+shift}, type => IMAGE_SCALE_TYPE_GM } );
}

sub resize_gm_fixed_point {
    shift->resize( { %{+shift}, type => IMAGE_SCALE_TYPE_GM_FIXED } );
}

sub DESTROY {
    my $self = shift;
    
    # XS cleanup
    $self->__cleanup( $self->{_image} );
    
    close $self->{_fh} if $self->{_fh};
}

1;
__END__

=head1 NAME

Image::Scale - Fast, high-quality image resizing

=head1 SYNOPSIS

    use Image::Scale
    
    # Resize to 150 width and save to a file
    my $img = Image::Scale->new('image.jpg');
    $img->resize_gd( { width => 150 } );
    $img->save_jpeg('resized.jpg');

=head1 DESCRIPTION

This module implements several resizing algorithms with a focus on low overhead,
speed and minimal features. Algorithms available are:

  GD's copyResampled
  GD's copyResampled fixed-point (useful on embedded devices/NAS devices)
  GraphicsMagick's assortment of resize filters
  GraphicsMagick's Triangle filter in fixed-point

Supported image formats include JPEG, GIF, PNG, and BMP for input, and
JPEG and PNG for output.

This module came about because we needed to improve the very slow performance of
floating-point resizing algorithms on platforms without a floating-point
unit, such as ARM devices like the SheevaPlug, and the Sparc-based ReadyNAS Duo.
Previously it could take up to 14 seconds to resize using GD on the ReadyNAS but the
conversion to fixed-point with a little assembly code brings this down to the range of 200ms.

Normal platforms will also see improvement, by removing all of the GD overhead this
version of copyResampled is around 3 times faster while also using less memory.

=head1 METHODS

=head2 new( $PATH or \$DATA )

Initialize a new Image::Scale object from PATH, which may be any valid JPEG,
GIF, PNG, or BMP file.

Raw image data may also be passed in as a scalar reference.  Using a file path
is recommended when possible as this is more efficient and requires less memory.

=head2 width()

Returns the width of the original source image.

=head2 height()

Returns the height of the original source image.

=head2 resize_*( \%OPTIONS )

The 4 resize methods available are:

    resize_gd - This is GD's copyResampled algorithm (floating-point)
    resize_gd_fixed_point - copyResampled (fixed-point)
    resize_gm - GraphicsMagick, see below for filter options
    resize_gm_fixed_point - GraphicsMagick, only the Triangle filter is available in fixed-point mode

Options are specified in a hashref:

    width
    height

At least one of width or height are required. If only one is supplied the
image will retain the original aspect ratio.

    filter

For use with resize_gm() only.  Choose from the following filters, sorted in order
from least to most CPU time.  This does not necessarily mean least to best quality, though!
Be sure to do your own comparisons for quality.

    Point
    Box
    Triangle
    Hermite
    Hanning
    Hamming
    Blackman
    Gaussian
    Quadratic
    Cubic
    Catrom
    Mitchell
    Lanczos
    Bessel
    Sinc

If no filter is specified the default is Lanczos if downsizing, and Mitchell for upsizing or
if the image has an alpha channel.

    keep_aspect => 1

Only useful when both width and height are specified. This option will keep the
original aspect ratio of the source as well as center the image when resizing into
a different aspect ratio. For best results, images altered in this way should be
saved as PNG which will automatically add the necessary transparency around the image.

    ignore_exif => 1

By default, if a JPEG image contains an EXIF tag with orientation info, the image will be
rotated accordingly during resizing.  To disable this feature, set ignore_exif => 1.

    memory_limit => $limit_in_bytes

To avoid excess memory growth when resizing images that may be very
large, you can specify this option. If the resize_*() method would result in a
total memory allocation greater than $limit_in_bytes, the method will die.
Be sure to wrap the resize call in an eval when using this option.

=head2 save_jpeg( $PATH, [ $QUALITY ] )

Saves the resized image as a JPEG to PATH. If a quality is not specified, the
quality defaults to 90.

=head2 as_jpeg( [ $QUALITY ] )

Returns the resized JPEG image as scalar data. If a quality is not specified, the
quality defaults to 90.

=head2 save_png( $PATH )

Saves the resized image as a PNG to PATH. Transparency is preserved when saving to PNG.

=head2 as_png()

Returns the resized PNG image as scalar data.

=head1 PERFORMANCE

These numbers were gathered on my 2.4ghz MacBook Pro.

JPEG image, 1425x1425 -> 200x200 (libjpeg v8 with scaling)

    GD copyResampled                        21.9/s
    resize_gm( { filter => 'Triangle' } )   65.7/s
    resize_gd_fixed_point                   67.9/s
    resize_gd                               69.4/s
    resize_gm_fixed_point                   74.5/s

PNG image, 512x768 -> 200x133 (libpng 1.4.3)

    GD copyResampled                        14.7/s
    resize_gm( { filter => 'Triangle' } )   26.2/s
    resize_gm_fixed_point                   27.7/s
    resize_gd                               29.9/s
    resize_gd_fixed_point                   31.9/s

Here are some numbers from a machine without floating-point support.
(Marvell SheevaPlug 1.2ghz ARM9, JPEG 1425x1425 -> 200x200, libjpeg 6 with scaling)

    GD copyResampled                        1.08/s
    resize_gd                               2.16/s
    resize_gm( { filter => 'Triangle' } )   2.85/s
    resize_gd_fixed_point                   7.98/s
    resize_gm_fixed_point                   9.44/s

And finally, from an even slower machine, the 240mhz Sparc ReadyNAS Duo
(JPEG 1425x1425 -> 200x200, libjpeg 6 with scaling)

TODO

=head1 SEE ALSO

TODO

=head1 AUTHOR

Andy Grundman, E<lt>andy@hybridized.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2010 Andy Grundman

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

=cut
