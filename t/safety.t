use strict;
use warnings;

use FindBin ();
use Test::More;

use Image::Scale;

sub load_binary {
    my ($path) = @_;
    open my $fh, '<', $path or die "Unable to read $path: $!";
    binmode $fh;
    local $/;
    return scalar <$fh>;
}

my $bmp_path = "$FindBin::Bin/images/bmp/24bit.bmp";
my $bmp = load_binary($bmp_path);

subtest 'offset and length stay inside the supplied input' => sub {
    my $tiny = 'x';
    my $error = '';
    eval { Image::Scale->new(\$tiny, { offset => 2 }) };
    $error = $@;
    like $error, qr/offset is outside/, 'an out-of-range scalar offset is rejected';

    my $im = Image::Scale->new($bmp_path, { length => 54 });
    ok $im, 'a slice containing the complete BMP header can be inspected';
    my $ok;
    {
        local $SIG{__WARN__} = sub { };
        $ok = $im->resize({ width => 20 });
    }
    ok !$ok, 'decoding stops at the declared end of the image slice';
};

subtest 'invalid resize dimensions are rejected before allocation' => sub {
    my $im = Image::Scale->new(\$bmp);
    eval { $im->resize({ width => -1 }) };
    like $@, qr/width must be between 1 and/, 'negative width is rejected';

    eval { $im->resize({ height => 0 }) };
    like $@, qr/height must be between 1 and/, 'explicit zero height is rejected';
};

subtest 'PNG decode errors remain safe on a retry' => sub {
    plan skip_all => 'PNG support not built' if !Image::Scale->png_version();

    my $png = load_binary("$FindBin::Bin/images/png/rgba.png");
    my $truncated = substr($png, 0, 80);
    my $im;
    {
        local $SIG{__WARN__} = sub { };
        $im = Image::Scale->new(\$truncated);
    }
    ok $im, 'truncated input contains enough data for the PNG header';

    my ($first, $second);
    {
        local $SIG{__WARN__} = sub { };
        $first  = $im->resize({ width => 10 });
        $second = $im->resize({ width => 10 });
    }
    ok !$first, 'first decode reports failure';
    ok !$second, 'retry reports failure without dereferencing a destroyed PNG object';
};

subtest 'BMP headers are validated and the pixel offset is honored' => sub {
    my $compressed = $bmp;
    substr($compressed, 30, 4, pack('V', 4));
    my $bad;
    {
        local $SIG{__WARN__} = sub { };
        $bad = Image::Scale->new(\$compressed);
    }
    ok !$bad, 'embedded JPEG/PNG BMP compression is rejected in the header';

    my $pixel_offset = unpack('V', substr($bmp, 10, 4));
    my $with_gap = $bmp;
    my $gap = "not pixel data!!";
    substr($with_gap, $pixel_offset, 0, $gap);
    substr($with_gap, 2, 4, pack('V', length($with_gap)));
    substr($with_gap, 10, 4, pack('V', $pixel_offset + length($gap)));

    my $plain = Image::Scale->new(\$bmp);
    my $gapped = Image::Scale->new(\$with_gap);
    ok $plain && $gapped, 'both ordinary and gapped BMP headers are accepted';
    ok $plain->resize({ width => 31 }), 'ordinary BMP resizes';
    ok $gapped->resize({ width => 31 }), 'BMP with a header-to-pixel gap resizes';

    SKIP: {
        skip 'PNG support not built', 1 if !Image::Scale->png_version();
        is $gapped->as_png(), $plain->as_png(), 'gap bytes are skipped rather than decoded as pixels';
    }
};

subtest 'memory_limit includes GraphicsMagick temporary buffers' => sub {
    my $gd = Image::Scale->new(\$bmp);
    ok $gd->resize_gd({ width => 100, height => 50, memory_limit => 70_000 }),
        'the limit permits the source and output buffers';

    my $gm = Image::Scale->new(\$bmp);
    eval { $gm->resize_gm({ width => 100, height => 50, memory_limit => 70_000 }) };
    like $@, qr/memory_limit exceeded/, 'the additional filter and intermediate buffers are counted';
};

done_testing;
