#!/usr/bin/perl -w

use warnings;
use strict;
use File::Basename;
use Cwd qw(abs_path);

my $wikipath = undef;
foreach (@ARGV) {
    $wikipath = abs_path($_), next if not defined $wikipath;
}

chdir(dirname(__FILE__));
chdir('..');

my @unsorted_releases = ();
open(PIPEFH, '-|', 'git tag -l') or die "Failed to read git release tags: $!\n";

while (<PIPEFH>) {
    chomp;
    if (/\Arelease\-(.*?)\Z/) {
        # Ignore anything that isn't a x.y.0 release.
        # Make sure new APIs are assigned to the next minor version and ignore the patch versions.
        my $ver = $1;
        my @versplit = split /\./, $ver;
        next if (scalar(@versplit) < 1) || ($versplit[0] != 3);  # Ignore anything that isn't an SDL3 release.
        next if (scalar(@versplit) < 3) || ($versplit[2] != 0);

        # Consider this release version.
        push @unsorted_releases, $ver;
    }

}
close(PIPEFH);

#print("\n\nUNSORTED\n");
#foreach (@unsorted_releases) {
#    print "$_\n";
#}

my @releases = sort {
    my @asplit = split /\./, $a;
    my @bsplit = split /\./, $b;
    my $rc;
    for (my $i = 0; $i < scalar(@asplit); $i++) {
        return 1 if (scalar(@bsplit) <= $i);  # a is "2.0.1" and b is "2.0", or whatever.
        my $aseg = $asplit[$i];
        my $bseg = $bsplit[$i];
        $rc = int($aseg) <=> int($bseg);
        return $rc if ($rc != 0);  # found the difference.
    }
    return 0;  # still here? They matched completely?!
} @unsorted_releases;

my $current_release = 'in-development';
my $next_release = '3.0.0';  # valid until we actually ship something.  :)
if (scalar(@releases) > 0) {
    # this happens to work for how SDL versions things at the moment.
    $current_release = $releases[-1];

    my @current_release_segments = split /\./, $current_release;
    @current_release_segments[1] = '' . ($current_release_segments[1] + 2);
    $next_release = join('.', @current_release_segments);
}

#print("\n\nSORTED\n");
#foreach (@releases) {
#    print "$_\n";
#}
#print("\nCURRENT RELEASE: $current_release\n");
#print("NEXT RELEASE: $next_release\n\n");

push @releases, 'HEAD';

my %funcs = ();
foreach my $release (@releases) {
    #print("Checking $release...\n");
    my $tag = ($release eq 'HEAD') ? $release : "release-$release";
    my $blobname = "$tag:src/dynapi/SDL_dynapi_overrides.h";
    open(PIPEFH, '-|', "git show '$blobname'") or die "Failed to read git blob '$blobname': $!\n";
    while (<PIPEFH>) {
        chomp;
        if (/\A\#define\s+(SDL_.*?)\s+SDL_.*?_REAL\Z/) {
            my $fn = $1;
            $funcs{$fn} = $release if not defined $funcs{$fn};
        }
    }
    close(PIPEFH);
}

if (not defined $wikipath) {
    foreach my $release (@releases) {
        foreach my $fn (sort keys %funcs) {
            print("$fn: $funcs{$fn}\n") if $funcs{$fn} eq $release;
        }
    }
} else {
    if (defined $wikipath) {
        chdir($wikipath);
        foreach my $fn (keys %funcs) {
            my $revision = $funcs{$fn};
            $revision = $next_release if $revision eq 'HEAD';
            my $fname = "$fn.md";
            if ( ! -f $fname ) {
                #print STDERR "No such file: $fname\n";
                next;
            }

            my @lines = ();
            open(FH, '<', $fname) or die("Can't open $fname for read: $!\n");
            my $added = 0;
            while (<FH>) {
                chomp;
                if ((/\A\-\-\-\-/) && (!$added)) {
                    push @lines, "## Version";
                    push @lines, "";
                    push @lines, "This function is available since SDL $revision.";
                    push @lines, "";
                    $added = 1;
                }
                push @lines, $_;
                next if not /\A\#\#\s+Version/;
                $added = 1;
                push @lines, "";
                push @lines, "This function is available since SDL $revision.";
                push @lines, "";
                while (<FH>) {
                    chomp;
                    next if not (/\A\#\#\s+/ || /\A\-\-\-\-/);
                    push @lines, $_;
                    last;
                }
            }
            close(FH);

            if (!$added) {
                push @lines, "## Version";
                push @lines, "";
                push @lines, "This function is available since SDL $revision.";
                push @lines, "";
            }

            open(FH, '>', $fname) or die("Can't open $fname for write: $!\n");
            foreach (@lines) {
                print FH "$_\n";
            }
            close(FH);
        }
    }
}

