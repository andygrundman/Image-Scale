#!/usr/bin/perl

# Test resizing on all image/audio files found within a directory tree
# Also checks for large memory use

use strict;
use lib qw(blib/lib blib/arch);

$|++;

use Audio::Scan 0.84;
use Image::Scale;
use File::Find;
use Proc::ProcessTable;

use constant MEMORY_LIMIT   => 1024 * 1024;
use constant LEAK_THRESHOLD => 5;

my $check_leaks = 1;

my $dir = shift || die "Usage: $0 directory\n";

my $t = Proc::ProcessTable->new( cache_ttys => 1 );
my $before;
$check_leaks && ($before = size_of());

find( {
    wanted => sub {
        # Skip ._* files OSX leaves around
        return if /\._/;
        
        my $ok;
        
        if ( Audio::Scan->is_supported($File::Find::name) ) {
            $ok = scan_audio($File::Find::name);
        }
        elsif ( /\.(jpg|gif|png|bmp)$/i ) {
            # Image file
            $ok = scan_image($File::Find::name);
        }
        else {
            return;
        }
        
        if ($check_leaks && $ok) {
            my $after = size_of();
            if ( ($after - $before) > LEAK_THRESHOLD ) {
                print " *** grew " . ($after - $before) . " KB\n";
            }
            $before = $after;
        }        
    },
    no_chdir => 1,
}, $dir );

sub scan_audio {
    my $file = shift;
    
    my $offset;
    my $length;
    
    local $ENV{AUDIO_SCAN_NO_ARTWORK} = 1;
    
    my $s = Audio::Scan->scan_tags($file);
    my $tags = $s->{tags};
    
    # MP3, other files with ID3v2
	if ( my $pic = $tags->{APIC} ) {
		if ( ref $pic->[0] eq 'ARRAY' ) {
			# multiple images, return image with lowest image_type value
			$length = ( sort { $a->[1] <=> $b->[1] } @{$pic} )[0]->[3];
			$offset = ( sort { $a->[1] <=> $b->[1] } @{$pic} )[0]->[4];
		}
		else {
		    $length = $pic->[3];
		    $offset = $pic->[4];
		}
		
		if ( $length && !$offset ) {
		    warn "No offset for image length $length: $file\n";
		    return;
	    }
	}
	
	# FLAC/Ogg picture block
	elsif ( $tags->{ALLPICTURES} ) {
		$length = ( sort { $a->{picture_type} <=> $b->{picture_type} } @{ $tags->{ALLPICTURES} } )[0]->{image_data};
		$offset = ( sort { $a->{picture_type} <=> $b->{picture_type} } @{ $tags->{ALLPICTURES} } )[0]->{offset};
	}
	
	# ALAC/M4A
	elsif ( $tags->{COVR} ) {
		$length = $tags->{COVR};
		$offset = $tags->{COVR_offset};
	}
	
	# WMA
    elsif ( my $pic = $tags->{'WM/Picture'} ) {
		if ( ref $pic eq 'ARRAY' ) {
			# return image with lowest image_type value
			$length = ( sort { $a->{image_type} <=> $b->{image_type} } @{$pic} )[0]->{image};
			$offset = ( sort { $a->{image_type} <=> $b->{image_type} } @{$pic} )[0]->{offset};
		}
		else {
		    $length = $pic->{image};
		    $offset = $pic->{offset};		}
	}
	
	# APE
	elsif ( $tags->{'COVER ART (FRONT)'} ) {
		$length = $tags->{'COVER ART (FRONT)'};
		$offset = $tags->{'COVER ART (FRONT)_offset'};
	}
	
	return if !$length && !$offset;
	
	print "$file ($length @ $offset)... ";
	
	# resize
	my $im = Image::Scale->new( $file, { offset => $offset, length => $length } );
	if ($im) {
	    my $ok;
	    print $im->width . 'x' . $im->height . ' ';
	    $ok = eval { $im->resize_gd_fixed_point( { width => 100, memory_limit => MEMORY_LIMIT } ) };
	    if ($@) {
	        print "resize failed ($@)";
	        sleep 5;
        }
        elsif (!$ok) {
            print "resize failed";
        }
        else {
            print "ok";
        }
    }
    else {
        print "read failed";
        sleep 5;
    }
	
	print "\n";
	
	return 1;
}

sub scan_image {
    my $file = shift;
    
    print "$file... ";
    
    my $im = Image::Scale->new($file);
	if ($im) {
	    my $ok;
	    print $im->width . 'x' . $im->height . ' ';
	    $ok = eval { $im->resize_gd_fixed_point( { width => 100, memory_limit => MEMORY_LIMIT } ); };
	    if ($@) {
	        print "resize failed ($@)";
	        sleep 5;
        }
        elsif (!$ok) {
            print "resize failed";
        }
        else {
            print "ok";
        }
    }
    else {
        print "read failed";
        sleep 5;
    }
    
    print "\n";
    
    return 1;
}

sub size_of {
    my $pid = shift || $$;
    
    foreach my $p ( @{ $t->table } ) {
        if ( $p->pid == $pid ) {
            return $p->rss;
        }
    }
    
    die "Pid $pid not found?";
}
