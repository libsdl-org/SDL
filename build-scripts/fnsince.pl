#!/usr/bin/perl -w

use warnings;
use strict;
use File::Basename;

chdir(dirname(__FILE__));
chdir('..');

my @unsorted_releases = ();
open(PIPEFH, '-|', 'git tag -l') or die "Failed to read git release tags: $!\n";

while (<PIPEFH>) {
    chomp;
    if (/\Arelease\-(.*?)\Z/) {
        push @unsorted_releases, $1;
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

#print("\n\nSORTED\n");
#foreach (@releases) {
#    print "$_\n";
#}

push @releases, 'HEAD';

my %funcs = ();
foreach my $release (@releases) {
    #print("Checking $release...\n");
    next if ($release eq '2.0.0') || ($release eq '2.0.1');  # no dynapi before 2.0.2
    my $assigned_release = ($release eq '2.0.2') ? '2.0.0' : $release;  # assume everything in 2.0.2--first with dynapi--was there since 2.0.0. We'll fix it up later.
    my $tag = ($release eq 'HEAD') ? $release : "release-$release";
    my $blobname = "$tag:src/dynapi/SDL_dynapi_overrides.h";
    open(PIPEFH, '-|', "git show '$blobname'") or die "Failed to read git blob '$blobname': $!\n";
    while (<PIPEFH>) {
        chomp;
        if (/\A\#define\s+(SDL_.*?)\s+SDL_.*?_REAL\Z/) {
            my $fn = $1;
            $funcs{$fn} = $assigned_release if not defined $funcs{$fn};
        }
    }
    close(PIPEFH);
}

# Fixup the handful of functions that were added in 2.0.1 and 2.0.2 that we
#  didn't have dynapi revision data about...
$funcs{'SDL_GetSystemRAM'} = '2.0.1';
$funcs{'SDL_GetBasePath'} = '2.0.1';
$funcs{'SDL_GetPrefPath'} = '2.0.1';
$funcs{'SDL_UpdateYUVTexture'} = '2.0.1';
$funcs{'SDL_GL_GetDrawableSize'} = '2.0.1';

$funcs{'SDL_GetAssertionHandler'} = '2.0.2';
$funcs{'SDL_GetDefaultAssertionHandler'} = '2.0.2';
$funcs{'SDL_AtomicAdd'} = '2.0.2';
$funcs{'SDL_AtomicGet'} = '2.0.2';
$funcs{'SDL_AtomicGetPtr'} = '2.0.2';
$funcs{'SDL_AtomicSet'} = '2.0.2';
$funcs{'SDL_AtomicSetPtr'} = '2.0.2';
$funcs{'SDL_HasAVX'} = '2.0.2';
$funcs{'SDL_GameControllerAddMappingsFromRW'} = '2.0.2';
$funcs{'SDL_acos'} = '2.0.2';
$funcs{'SDL_asin'} = '2.0.2';
$funcs{'SDL_vsscanf'} = '2.0.2';
$funcs{'SDL_DetachThread'} = '2.0.2';
$funcs{'SDL_GL_ResetAttributes'} = '2.0.2';

foreach my $release (@releases) {
    foreach my $fn (sort keys %funcs) {
        print("$fn: $funcs{$fn}\n") if $funcs{$fn} eq $release;
    }
}

