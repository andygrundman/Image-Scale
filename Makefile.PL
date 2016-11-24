use strict;

use 5.008;

use Config;
use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

my (@INC, @LIBPATH, @LIBS);
my ($JPEG, $PNG, $GIF, $MYEXTLIB);
my ($JPEG_VERSION, $PNG_VERSION, $GIF_VERSION);

my ($help, $jpeg_inc, $jpeg_lib, $png_inc, $png_lib, $gif_inc, $gif_lib);
my ($jpeg_static, $png_static, $gif_static);

my $DEFINES = '-O2';
$DEFINES .= ' -Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security -Winline -ffast-math' unless $^O =~ /Win32|sun|solaris/i;

if ( $^O !~ /Win32/i ) {
    # Check for Sparc ReadyNAS to enable ASM code
    if ( -e '/etc/raidiator_version' ) {
        if ( $Config{archname} eq 'sparc-linux' ) {
            $DEFINES .= ' -DPADRE';
        }
    }

    push @LIBPATH, '-L/usr/local/lib';
}

my $result = GetOptions(
    "help|h"               => \$help,
    "with-jpeg-includes=s" => \$jpeg_inc,
    "with-jpeg-libs=s"     => \$jpeg_lib,
    "with-jpeg-static"     => \$jpeg_static,
    "with-png-includes=s"  => \$png_inc,
    "with-png-libs=s"      => \$png_lib,
    "with-png-static"      => \$png_static,
    "with-gif-includes=s"  => \$gif_inc,
    "with-gif-libs=s"      => \$gif_lib,
    "with-gif-static"      => \$gif_static,
);

if ($help || !$result) {
    print STDERR <<END;
Usage: perl Makefile.PL [options]

Configure Image::Scale module.

Options:
    --with-jpeg-includes    Path to directory containing jpeglib.h
    --with-jpeg-libs        Path to directory containing libjpeg
    --with-jpeg-static      Statically link libjpeg
    --with-png-includes     Path to directory containing png.h
    --with-png-libs         Path to directory containing libpng
    --with-png-static       Statically link libpng
    --with-gif-includes     Path to directory containing gif_lib.h
    --with-gif-libs         Path to directory containing libgif
    --with-gif-static       Statically link libgif

END

    exit 0;
}

my @check = qw(/usr/include /usr/local/include /opt/local/include);

# Check in environment-supplied locations before standard ones
unshift @check, split /:/, $ENV{INCLUDE};

if ( $^O =~ /Win32/i ) {
    push @check, '../libjpeg-turbo-1.1.1';
    push @check, '../lpng143';
    push @check, '../giflib-4.1.6/lib';
}

# Look for libjpeg
for my $incdir ( $jpeg_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'jpeglib.h') ) {
        $JPEG = 1;
        $jpeg_inc = $incdir;

        unshift @INC, '-I' . $jpeg_inc;
        if ($jpeg_lib) {
            unshift @LIBPATH, '-L' . $jpeg_lib;
        }

        if ($jpeg_static) {
            if ($jpeg_lib) {
                $MYEXTLIB .= $jpeg_lib . '/' . 'libjpeg$(LIB_EXT) ';
            }
            else {
                $MYEXTLIB .= $jpeg_inc . '/../lib/libjpeg$(LIB_EXT) ';
            }
        }
        else {
            push @LIBS, '-ljpeg';
        }

        if ( $^O =~ /Win32/i ) {
            $JPEG_VERSION = _re_find( catfile($incdir, 'build', 'jconfig.h'), qr/JPEG_LIB_VERSION\s+(\d+)/ ) || 'unknown';
        }
        else {
            $JPEG_VERSION = _re_find( catfile($incdir, 'jpeglib.h'), qr/JPEG_LIB_VERSION\s+(\d+)/ ) || 'unknown';
            if ( $JPEG_VERSION eq 'unknown' ) {
                # Also check jconfig.h as it contains the version when using libjpeg-turbo
                if ( -d catdir($incdir, 'x86_64-linux-gnu') ) { # jconfig.h header is sometimes installed in a subdir
                    $JPEG_VERSION = _re_find( catfile($incdir, 'x86_64-linux-gnu', 'jconfig.h'), qr/JPEG_LIB_VERSION\s+(\d+)/ ) || 'unknown';
                }
                else {
                    $JPEG_VERSION = _re_find( catfile($incdir, 'jconfig.h'), qr/JPEG_LIB_VERSION\s+(\d+)/ ) || 'unknown';
                }
            }
        }

        $DEFINES .= " -DHAVE_JPEG -DJPEG_VERSION=$JPEG_VERSION";

        last;
    }
}

# Look for libpng
for my $incdir ( $png_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'png.h') ) {
        $PNG = 1;
        $png_inc = $incdir;

        unshift @INC, '-I' . $png_inc;
        if ($png_lib) {
            unshift @LIBPATH, '-L' . $png_lib;
        }

        if ($png_static) {
            if ($png_lib) {
                $MYEXTLIB .= $png_lib . '/' . 'libpng$(LIB_EXT) ';
            }
            else {
                $MYEXTLIB .= $png_inc . '/../lib/libpng$(LIB_EXT) ';
            }

            push @LIBS, '-lz';
        }
        else {
            push @LIBS, '-lpng -lz';
        }

        $PNG_VERSION = _re_find( catfile($incdir, 'png.h'), qr/PNG_LIBPNG_VER_STRING\s+"([\d\.]+)"/ ) || 'unknown';

        $DEFINES .= " -DHAVE_PNG -DPNG_VERSION=$PNG_VERSION";

        last;
    }
}

# Look for giflib
for my $incdir ( $gif_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'gif_lib.h') ) {
        $gif_inc = $incdir;

        unshift @INC, '-I' . $gif_inc;
        if ($gif_lib) {
            unshift @LIBPATH, '-L' . $gif_lib;
        }

        if ($gif_static) {
            if ($gif_lib) {
                $MYEXTLIB .= $gif_lib . '/' . "libgif\$(LIB_EXT) ";
            }
            else {
                $MYEXTLIB .= $gif_inc . "/../lib/libgif\$(LIB_EXT) ";
            }
        }
        else {
            push @LIBS, "-lgif";
        }

        my $GIF_41    = _re_find( catfile($incdir, 'gif_lib.h'), qr/GIF_LIB_VERSION " Version (4\.1), "/ );
        my $GIF_MAJOR = _re_find( catfile($incdir, 'gif_lib.h'), qr/GIFLIB_MAJOR (\d+)/ );
        my $GIF_MINOR = _re_find( catfile($incdir, 'gif_lib.h'), qr/GIFLIB_MINOR (\d+)/ );

        if ( $GIF_MAJOR ) {
            $GIF_VERSION = "${GIF_MAJOR}.${GIF_MINOR}";
            if ( $GIF_VERSION >= 4.2 ) {
                $GIF = 1;
                $DEFINES .= " -DHAVE_GIF -DGIF_VERSION=$GIF_VERSION";

                # giflib >= 5.1 changes DGifClose()
                if ( $GIF_VERSION >= 5.1 ) {
                    $DEFINES .= " -DGIFLIB_API_51";
                }

                # giflib >= 5.0 change DGifOpen(), GifErrorString(), GifAddExtensionBlock() and struct SavedImage
                if ( $GIF_VERSION >= 5.0 ) {
                    $DEFINES .= " -DGIFLIB_API_50";
                }
            }
        }
        elsif ( $GIF_41 ) {
            $GIF = 1;
            $GIF_VERSION = '4.1';
            $DEFINES .= ' -DHAVE_GIF -DGIF_VERSION=4.1 -DGIFLIB_API_41';
        }

        last;
    }
}

# Require at least one of jpeg or png
if ( !$JPEG && !$PNG ) {
    my $err = "Image::Scale requires at least one of libjpeg or libpng, please install one or both and try again.\n";

    if ( $^O =~ /Win32/i ) {
        $err .= "\nPlease read the README.win32 file for build instructions on Windows.\n";
    }

    warn $err;

    exit 0;
}

unshift @INC, '-I. -I.. -Isrc -Iinclude';

print "\n";
print "Image::Scale will be built with:\n\n";
print "  JPEG support:     " . ( $JPEG ? "yes ($jpeg_inc, libjpeg version $JPEG_VERSION)" : "no, install libjpeg (v8 recommended)" ) . "\n";
print "  PNG support:      " . ( $PNG ? "yes ($png_inc, libpng version $PNG_VERSION)" : "no, install libpng (v1.4.x recommended)" ) . "\n";
print "  GIF support:      " . ( $GIF ? "yes ($gif_inc, giflib version $GIF_VERSION)" : "no, install giflib 4.2 or higher" ) . "\n";
print "\n";

if ( $^O =~ /Win32/i ) {
    *MY::postamble = sub {};

    if ($JPEG) {
        push @INC, '-I../libjpeg-turbo-1.1.1/build'; # for jconfig.h
        $MYEXTLIB .= '../libjpeg-turbo-1.1.1/build/jpeg-static.lib ';
    }

    if ($PNG) {
        push @INC, '-I../zlib';
        $MYEXTLIB .= '../lpng143/libpng.lib ../zlib/zlib.lib ';
    }

    if ($GIF) {
        $MYEXTLIB .= '../giflib-4.1.6/windows/Release/giflib.lib ';
    }
}

my $inc_files = join(' ', glob 'include/*.h');
my $src_files = join(' ', glob 'src/*.c');

WriteMakefile(
    NAME              => 'Image::Scale',
    VERSION_FROM      => 'lib/Image/Scale.pm',
    PREREQ_PM         => { 'Test::NoWarnings' => 0 },
    ABSTRACT_FROM     => 'lib/Image/Scale.pm',
    AUTHOR            => 'Andy Grundman <andy@hybridized.org>',
    INC               => join(' ', @INC),
    LIBS              => [ join(' ', @LIBPATH, @LIBS) ],
    DEFINE            => $DEFINES,
    MYEXTLIB          => $MYEXTLIB,
    depend            => { 'Scale.c' => "$inc_files $src_files" },
    META_MERGE => {
        'meta-spec' => { version => 2 },
        resources => {
            repository => {
                type => 'git',
                url  => 'https://github.com/andygrundman/Image-Scale.git',
                web  => 'https://github.com/andygrundman/Image-Scale',
            },
        },
    },
    MIN_PERL_VERSION => 5.008,
    LICENSE => 'gpl_2',
);

sub _re_find {
    my ( $file, $re ) = @_;

    my $result;

    open my $fh, '<', $file or die "Unable to read $file\n";
    while ( my $line = <$fh> ) {
        if ( $line =~ $re ) {
            $result = $1;
            last;
        }
    }
    close $fh;

    return $result;
}
