package inc::MCGMakeMaker;
use Moose;

extends 'Dist::Zilla::Plugin::MakeMaker::Awesome';

override _build_WriteMakefile_args => sub {
    my ($self) = @_;

    my @INC;
    
    # XXX this code is not executed by the end system, not sure how to fix right now
    my $DEFINES = '';
    #my $DEFINES = '-Wall' unless $^O =~ /sun|solaris/i;
    #$DEFINES .= ' -Wno-unused-value -Wno-format-security' unless $^O =~ /Win32|sun|solaris/i;

    unshift @INC, '-I. -I.. -Isrc -Iinclude';

    my $inc_files = join(' ', glob 'include/*.h');
    my $src_files = join(' ', glob 'src/*.c');

    return +{
        %{ super() },
        INC     => join(' ', @INC),
        LIBS    => '-ljpeg -lpng -lz', # XXX support static or dynamic linking
        DEFINE  => $DEFINES,
        depend  => { 'Scale.c' => "$inc_files $src_files" },
    };
};

__PACKAGE__->meta->make_immutable;