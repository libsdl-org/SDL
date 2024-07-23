#!/usr/bin/perl -w

# Simple DirectMedia Layer
# Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>
#
# This software is provided 'as-is', without any express or implied
# warranty.  In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

use warnings;
use strict;
use File::Basename;
use Cwd qw(abs_path);
use IPC::Open2;

my $examples_dir = abs_path(dirname(__FILE__) . "/../examples");
my $project = undef;
my $emsdk_dir = undef;
my $compile_dir = undef;
my $output_dir = undef;

sub usage {
    die("USAGE: $0 <project_name> <emsdk_dir> <compiler_output_directory> <html_output_directory>\n\n");
}

sub do_system {
    my $cmd = shift;
    $cmd = "exec /bin/bash -c \"$cmd\"";
    print("$cmd\n");
    return system($cmd);
}

sub do_mkdir {
    my $d = shift;
    if ( ! -d $d ) {
        print("mkdir '$d'\n");
        mkdir($d) or die("Couldn't mkdir('$d'): $!\n");
    }
}

sub build_latest {
    # Try to build just the latest without re-running cmake, since that is SLOW.
    print("Building latest version of $project ...\n");
    if (do_system("EMSDK_QUIET=1 source '$emsdk_dir/emsdk_env.sh' && cd '$compile_dir' && ninja") != 0) {
        # Build failed? Try nuking the build dir and running CMake from scratch.
        print("\n\nBuilding latest version of $project FROM SCRATCH ...\n");
        if (do_system("EMSDK_QUIET=1 source '$emsdk_dir/emsdk_env.sh' && rm -rf '$compile_dir' && mkdir '$compile_dir' && cd '$compile_dir' && emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel '$examples_dir/..' && ninja") != 0) {
            die("Failed to build latest version of $project!\n");  # oh well.
        }
    }
}

sub handle_example_dir {
    my $category = shift;
    my $example = shift;

    my @files = ();
    my $files_str = '';
    opendir(my $dh, "$examples_dir/$category/$example") or die("Couldn't opendir '$examples_dir/$category/$example': $!\n");
    my $spc = '';
    while (readdir($dh)) {
        next if not /\.c\Z/;  # only care about .c files.
        my $path = "$examples_dir/$category/$example/$_";
        next if not -f $path;    # only care about files.
        push @files, $path;
        $files_str .= "$spc$path";
        $spc = ' ';
    }
    closedir($dh);

    my $dst = "$output_dir/$category/$example";

    print("Building $category/$example ...\n");

    do_mkdir($dst);

    # !!! FIXME: hardcoded SDL3 references, need to fix this for satellite libraries and SDL2.
    do_system("EMSDK_QUIET=1 source '$emsdk_dir/emsdk_env.sh' && emcc -s USE_SDL=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1gb -s ASSERTIONS=0 -o '$dst/index.js' '-I$examples_dir/../include' $files_str '$compile_dir/libSDL3.a'") == 0
        or die("Failed to build $category/$example!\n");

    my $highlight_cmd = "highlight '--outdir=$dst' --style-outfile=highlight.css --fragment --enclose-pre --stdout --syntax=c '--plug-in=$examples_dir/highlight-plugin.lua'";
    print("$highlight_cmd\n");
    my $pid = open2(my $child_out, my $child_in, $highlight_cmd);

    my $htmlified_source_code = '';
    foreach (@files) {
        my $path = $_;
        open my $srccode, '<', $path or die("Couldn't open '$path': $!\n");
        my $fname = "$path";
        $fname =~ s/\A.*\///;
        print $child_in "/* $fname ... */\n\n";
        while (<$srccode>) {
            print $child_in $_;
        }
        close($srccode);
    }

    close($child_in);

    while (<$child_out>) {
        $htmlified_source_code .= $_;
    }
    close($child_out);

    waitpid($pid, 0);


    my $html = '';
    open my $htmltemplate, '<', "$examples_dir/template.html" or die("Couldn't open '$examples_dir/template.html': $!\n");
    while (<$htmltemplate>) {
        s/\@project_name\@/$project/g;
        s/\@category_name\@/$category/g;
        s/\@example_name\@/$example/g;
        s/\@htmlified_source_code\@/$htmlified_source_code/g;
        $html .= $_;
    }
    close($htmltemplate);

    open my $htmloutput, '>', "$dst/index.html" or die("Couldn't open '$dst/index.html': $!\n");
    print $htmloutput $html;
    close($htmloutput);

    
}

sub handle_category_dir {
    my $category = shift;

    print("Category $category ...\n");

    do_mkdir("$output_dir/$category");

    opendir(my $dh, "$examples_dir/$category") or die("Couldn't opendir '$examples_dir/$category': $!\n");

    while (readdir($dh)) {
        next if ($_ eq '.') || ($_ eq '..');  # obviously skip current and parent entries.
        next if not -d "$examples_dir/$category/$_";   # only care about subdirectories.
        handle_example_dir($category, $_);
    }

    closedir($dh);
}


# Mainline!

foreach (@ARGV) {
    $project = $_, next if not defined $project;
    $emsdk_dir = $_, next if not defined $emsdk_dir;
    $compile_dir = $_, next if not defined $compile_dir;
    $output_dir = $_, next if not defined $output_dir;
    usage();  # too many arguments.
}

usage() if not defined $output_dir;

print("Examples dir: $examples_dir\n");
print("emsdk dir: $emsdk_dir\n");
print("Compile dir: $compile_dir\n");
print("Output dir: $output_dir\n");

do_mkdir($output_dir);

build_latest();

opendir(my $dh, $examples_dir) or die("Couldn't opendir '$examples_dir': $!\n");

while (readdir($dh)) {
    next if ($_ eq '.') || ($_ eq '..');  # obviously skip current and parent entries.
    next if not -d "$examples_dir/$_";   # only care about subdirectories.
    handle_category_dir($_);
}

closedir($dh);

exit(0);  # success!

