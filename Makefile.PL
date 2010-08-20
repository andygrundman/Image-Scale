use strict;

# XXX better static build support

use Config;
use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;

my (@INC, @LIBPATH, @LIBS);
my $MYEXTLIB;

my $DEFINES = '-O2'; # I tested -funroll-loops, it is slightly slower
$DEFINES .= ' -Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security -Winline' unless $^O =~ /Win32|sun|solaris/i;
unshift @INC, '-I. -I.. -Isrc -Iinclude';

if ( $^O !~ /Win32/ ) {
    push @LIBS, '-ljpeg -lpng -lz -lgif';
    
    # Check for Sparc ReadyNAS to enable ASM code
    if ( -e '/etc/raidiator_version' ) {
        if ( $Config{archname} eq 'sparc-linux' ) {
            $DEFINES .= ' -DPADRE';
        }
    }
}

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
    PREREQ_PM         => {},
    ABSTRACT_FROM     => 'lib/Image/Scale.pm',
    AUTHOR            => 'Andy Grundman <andy@hybridized.org>',
    INC               => join(' ', @INC),
    LIBS              => [ join(' ', @LIBPATH, @LIBS) ],
    DEFINE            => $DEFINES,
    MYEXTLIB          => $MYEXTLIB,
    depend            => { 'Scale.c' => "$inc_files $src_files" },
);
