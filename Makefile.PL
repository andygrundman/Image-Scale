use strict;

# XXX better static build support

use Config;
use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

my (@INC, @LIBPATH, @LIBS);
my ($JPEG, $PNG, $GIF, $MYEXTLIB);
my ($JPEG_VERSION, $PNG_VERSION, $GIF_VERSION);

my ($help, $jpeg_inc, $jpeg_lib, $png_inc, $png_lib, $gif_inc, $gif_lib);
my ($jpeg_static, $png_static, $gif_static);

my $DEFINES = '-O2'; # I tested -funroll-loops, it is slightly slower
$DEFINES .= ' -Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security -Winline' unless $^O =~ /Win32|sun|solaris/i;

if ( $^O !~ /Win32/ ) { 
    # Check for Sparc ReadyNAS to enable ASM code
    if ( -e '/etc/raidiator_version' ) {
        if ( $Config{archname} eq 'sparc-linux' ) {
            $DEFINES .= ' -DPADRE';
        }
    }
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

if ( $^O =~ /Win32/ ) {
    # XXX add include paths to @check
}

# Look for libjpeg
for my $incdir ( $jpeg_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'jpeglib.h') ) {
        $JPEG = 1;
        $jpeg_inc = $incdir;
        
        push @INC, '-I' . $jpeg_inc;
        if ($jpeg_lib) {
            push @LIBPATH, '-L' . $jpeg_lib;
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
        
        $JPEG_VERSION = _re_find( catfile($incdir, 'jpeglib.h'), qr/JPEG_LIB_VERSION\s+(\d+)/ ) || 'unknown';
        
        $DEFINES .= " -DHAVE_JPEG -DJPEG_VERSION=$JPEG_VERSION";
        
        last;
    }
}

# Look for libpng
for my $incdir ( $png_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'png.h') ) {
        $PNG = 1;
        $png_inc = $incdir;
        
        push @INC, '-I' . $png_inc;
        if ($png_lib) {
            push @LIBPATH, '-L' . $png_lib;
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

# Look for libgif
for my $incdir ( $gif_inc, @check ) {
    if ( $incdir && -e catfile($incdir, 'gif_lib.h') ) {
        $GIF = 1;
        $gif_inc = $incdir;
        
        push @INC, '-I' . $gif_inc;
        if ($gif_lib) {
            push @LIBPATH, '-L' . $gif_lib;
        }
        
        if ($gif_static) {
            if ($gif_lib) {
                $MYEXTLIB .= $gif_lib . '/' . 'libgif$(LIB_EXT) ';
            }
            else {
                $MYEXTLIB .= $gif_inc . '/../lib/libgif$(LIB_EXT) ';
            }
        }
        else {
            push @LIBS, '-lgif';
        }
        
        $GIF_VERSION = _re_find( catfile($incdir, 'gif_lib.h'), qr/GIF_LIB_VERSION\s+" Version ([\d\.]+)/ ) || 'unknown';
        
        $DEFINES .= " -DHAVE_GIF -DGIF_VERSION=$GIF_VERSION";
        
        last;
    }
}

# Require at least one of jpeg or png
if ( !$JPEG && !$PNG ) {
    die "Image::Scale requires at least one of libjpeg or libpng, please install one or both and try again.\n";
}

unshift @INC, '-I. -I.. -Isrc -Iinclude';

print "\n";
print "Image::Scale will be built with:\n\n";
print "  JPEG support:     " . ( $JPEG ? "yes ($jpeg_inc, version $JPEG_VERSION)" : "no, install libjpeg (v8 recommended)" ) . "\n";
print "  PNG support:      " . ( $PNG ? "yes ($png_inc, version $PNG_VERSION)" : "no, install libpng (v1.4.x recommended)" ) . "\n";
print "  GIF support:      " . ( $GIF ? "yes ($gif_inc, version $GIF_VERSION)" : "no, install giflib" ) . "\n";
print "\n";

if ( $^O =~ /Win32/ ) {
    *MY::postamble = sub {};
    # XXX Add libjpeg(-turbo?) and libpng
    $MYEXTLIB .= 'win32/zlib.lib ';
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
