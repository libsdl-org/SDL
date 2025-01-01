#!/usr/bin/perl -w

# Simple DirectMedia Layer
# Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
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
use File::Copy;
use Cwd qw(abs_path);
use IPC::Open2;

my $examples_dir = abs_path(dirname(__FILE__) . "/../examples");
my $project = undef;
my $emsdk_dir = undef;
my $compile_dir = undef;
my $cmake_flags = undef;
my $output_dir = undef;

sub usage {
    die("USAGE: $0 <project_name> <emsdk_dir> <compiler_output_directory> <cmake_flags> <html_output_directory>\n\n");
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

sub do_copy {
    my $src = shift;
    my $dst = shift;
    print("cp '$src' '$dst'\n");
    copy($src, $dst) or die("Failed to copy '$src' to '$dst': $!\n");
}

sub build_latest {
    # Try to build just the latest without re-running cmake, since that is SLOW.
    print("Building latest version of $project ...\n");
    if (do_system("EMSDK_QUIET=1 source '$emsdk_dir/emsdk_env.sh' && cd '$compile_dir' && ninja") != 0) {
        # Build failed? Try nuking the build dir and running CMake from scratch.
        print("\n\nBuilding latest version of $project FROM SCRATCH ...\n");
        if (do_system("EMSDK_QUIET=1 source '$emsdk_dir/emsdk_env.sh' && rm -rf '$compile_dir' && mkdir '$compile_dir' && cd '$compile_dir' && emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel $cmake_flags '$examples_dir/..' && ninja") != 0) {
            die("Failed to build latest version of $project!\n");  # oh well.
        }
    }
}

sub get_category_description {
    my $category = shift;
    my $retval = ucfirst($category);

    if (open(my $fh, '<', "$examples_dir/$category/description.txt")) {
        $retval = <$fh>;
        chomp($retval);
        close($fh);
    }

    return $retval;
}

sub get_categories {
    my @categories = ();

    if (open(my $fh, '<', "$examples_dir/categories.txt")) {
        while (<$fh>) {
            chomp;
            s/\A\s+//;
            s/\s+\Z//;
            next if $_ eq '';
            next if /\A\#/;
            push @categories, $_;
        }
        close($fh);
    } else {
        opendir(my $dh, $examples_dir) or die("Couldn't opendir '$examples_dir': $!\n");
        foreach my $dir (sort readdir $dh) {
            next if ($dir eq '.') || ($dir eq '..');  # obviously skip current and parent entries.
            next if not -d "$examples_dir/$dir";   # only care about subdirectories.
            push @categories, $dir;
        }
        closedir($dh);
    }

    return @categories;
}

sub get_examples_for_category {
    my $category = shift;

    my @examples = ();

    opendir(my $dh, "$examples_dir/$category") or die("Couldn't opendir '$examples_dir/$category': $!\n");
    foreach my $dir (sort readdir $dh) {
        next if ($dir eq '.') || ($dir eq '..');  # obviously skip current and parent entries.
        next if not -d "$examples_dir/$category/$dir";   # only care about subdirectories.

        push @examples, $dir;
    }
    closedir($dh);

    return @examples;
}

sub handle_example_dir {
    my $category = shift;
    my $example = shift;

    my @files = ();
    my $files_str = '';
    opendir(my $dh, "$examples_dir/$category/$example") or die("Couldn't opendir '$examples_dir/$category/$example': $!\n");
    my $spc = '';
    while (readdir($dh)) {
        my $path = "$examples_dir/$category/$example/$_";
        next if not -f $path;    # only care about files.
        push @files, $path if /\.[ch]\Z/;  # add .c and .h files to source code.
        if (/\.c\Z/) {  # only care about .c files for compiling.
            $files_str .= "$spc$path";
            $spc = ' ';
        }
    }
    closedir($dh);

    my $dst = "$output_dir/$category/$example";

    print("Building $category/$example ...\n");

    my $basefname = "$example";
    $basefname =~ s/\A\d+\-//;
    $basefname = "$category-$basefname";
    my $jsfname = "$basefname.js";
    my $wasmfname = "$basefname.wasm";
    my $thumbnailfname = 'thumbnail.png';
    my $onmouseoverfname = 'onmouseover.webp';
    my $jssrc = "$compile_dir/examples/$jsfname";
    my $wasmsrc = "$compile_dir/examples/$wasmfname";
    my $thumbnailsrc = "$examples_dir/$category/$example/$thumbnailfname";
    my $onmouseoversrc = "$examples_dir/$category/$example/$onmouseoverfname";
    my $jsdst = "$dst/$jsfname";
    my $wasmdst = "$dst/$wasmfname";
    my $thumbnaildst = "$dst/$thumbnailfname";
    my $onmouseoverdst = "$dst/$onmouseoverfname";

    my $description = '';
    my $has_paragraph = 0;
    if (open(my $readmetxth, '<', "$examples_dir/$category/$example/README.txt")) {
        while (<$readmetxth>) {
            chomp;
            s/\A\s+//;
            s/\s+\Z//;
            if (($_ eq '') && ($description ne '')) {
                $has_paragraph = 1;
            } else {
                if ($has_paragraph) {
                    $description .= "\n<br/>\n<br/>\n";
                    $has_paragraph = 0;
                }
                $description .= "$_ ";
            }
        }
        close($readmetxth);
        $description =~ s/\s+\Z//;
    }

    do_mkdir($dst);
    do_copy($jssrc, $jsdst);
    do_copy($wasmsrc, $wasmdst);
    do_copy($thumbnailsrc, $thumbnaildst) if ( -f $thumbnailsrc );
    do_copy($onmouseoversrc, $onmouseoverdst) if ( -f $onmouseoversrc );

    my $highlight_cmd = "highlight '--outdir=$dst' --style-outfile=highlight.css --fragment --enclose-pre --stdout --syntax=c '--plug-in=$examples_dir/highlight-plugin.lua'";
    print("$highlight_cmd\n");
    my $pid = open2(my $child_out, my $child_in, $highlight_cmd);

    my $htmlified_source_code = '';
    foreach (sort(@files)) {
        my $path = $_;
        open my $srccode, '<', $path or die("Couldn't open '$path': $!\n");
        my $fname = "$path";
        $fname =~ s/\A.*\///;
        print $child_in "/* $fname ... */\n\n";
        while (<$srccode>) {
            print $child_in $_;
        }
        print $child_in "\n\n\n";
        close($srccode);
    }

    close($child_in);

    while (<$child_out>) {
        $htmlified_source_code .= $_;
    }
    close($child_out);

    waitpid($pid, 0);

    my $other_examples_html = "<ul>";
    foreach my $example (get_examples_for_category($category)) {
        $other_examples_html .= "<li><a href='/$project/$category/$example'>$category/$example</a></li>";
    }
    $other_examples_html .= "</ul>";

    my $category_description = get_category_description($category);
    my $preview_image = get_example_thumbnail($project, $category, $example);

    my $html = '';
    open my $htmltemplate, '<', "$examples_dir/template.html" or die("Couldn't open '$examples_dir/template.html': $!\n");
    while (<$htmltemplate>) {
        s/\@project_name\@/$project/g;
        s/\@category_name\@/$category/g;
        s/\@category_description\@/$category_description/g;
        s/\@example_name\@/$example/g;
        s/\@javascript_file\@/$jsfname/g;
        s/\@htmlified_source_code\@/$htmlified_source_code/g;
        s/\@description\@/$description/g;
        s/\@preview_image\@/$preview_image/g;
        s/\@other_examples_html\@/$other_examples_html/g;
        $html .= $_;
    }
    close($htmltemplate);

    open my $htmloutput, '>', "$dst/index.html" or die("Couldn't open '$dst/index.html': $!\n");
    print $htmloutput $html;
    close($htmloutput);
}

sub get_example_thumbnail {
    my $project = shift;
    my $category = shift;
    my $example = shift;

    if ( -f "$examples_dir/$category/$example/thumbnail.png" ) {
        return "/$project/$category/$example/thumbnail.png";
    } elsif ( -f "$examples_dir/$category/thumbnail.png" ) {
        return "/$project/$category/thumbnail.png";
    }

    return "/$project/thumbnail.png";
}

sub generate_example_thumbnail {
    my $project = shift;
    my $category = shift;
    my $example = shift;

    my $example_no_num = "$example";
    $example_no_num =~ s/\A\d+\-//;

    my $example_image_url = get_example_thumbnail($project, $category, $example);

    my $example_mouseover_html = '';
    if ( -f "$examples_dir/$category/$example/onmouseover.webp" ) {
        $example_mouseover_html = "onmouseover=\"this.src='/$project/$category/$example/onmouseover.webp'\" onmouseout=\"this.src='$example_image_url';\"";
    } elsif ( -f "$examples_dir/$category/onmouseover.webp" ) {
        $example_mouseover_html = "onmouseover=\"this.src='/$project/$category/onmouseover.webp'\" onmouseout=\"this.src='$example_image_url';\"";
    }

    return "
        <a href='/$project/$category/$example'>
          <div>
            <img src='$example_image_url' $example_mouseover_html />
            <div>$example_no_num</div>
          </div>
        </a>"
    ;
}

sub generate_example_thumbnails_for_category {
    my $project = shift;
    my $category = shift;
    my @examples = get_examples_for_category($category);
    my $retval = '';
    foreach my $example (@examples) {
        $retval .= generate_example_thumbnail($project, $category, $example);
    }
    return $retval;
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

    my $examples_list_html = generate_example_thumbnails_for_category($project, $category);

    my $dst = "$output_dir/$category";

    do_copy("$examples_dir/$category/thumbnail.png", "$dst/thumbnail.png") if ( -f "$examples_dir/$category/thumbnail.png" );
    do_copy("$examples_dir/$category/onmouseover.webp", "$dst/onmouseover.webp") if ( -f "$examples_dir/$category/onmouseover.webp" );

    my $category_description = get_category_description($category);
    my $preview_image = "/$project/thumbnail.png";
    if ( -f "$examples_dir/$category/thumbnail.png" ) {
        $preview_image = "/$project/$category/thumbnail.png";
    }

    # write category page
    my $html = '';
    open my $htmltemplate, '<', "$examples_dir/template-category.html" or die("Couldn't open '$examples_dir/template-category.html': $!\n");
    while (<$htmltemplate>) {
        s/\@project_name\@/$project/g;
        s/\@category_name\@/$category/g;
        s/\@category_description\@/$category_description/g;
        s/\@examples_list_html\@/$examples_list_html/g;
        s/\@preview_image\@/$preview_image/g;
        $html .= $_;
    }
    close($htmltemplate);

    open my $htmloutput, '>', "$dst/index.html" or die("Couldn't open '$dst/index.html': $!\n");
    print $htmloutput $html;
    close($htmloutput);
}


# Mainline!

foreach (@ARGV) {
    $project = $_, next if not defined $project;
    $emsdk_dir = $_, next if not defined $emsdk_dir;
    $compile_dir = $_, next if not defined $compile_dir;
    $cmake_flags = $_, next if not defined $cmake_flags;
    $output_dir = $_, next if not defined $output_dir;
    usage();  # too many arguments.
}

usage() if not defined $output_dir;

print("Examples dir: $examples_dir\n");
print("emsdk dir: $emsdk_dir\n");
print("Compile dir: $compile_dir\n");
print("CMake flags: $cmake_flags\n");
print("Output dir: $output_dir\n");

do_system("rm -rf '$output_dir'");
do_mkdir($output_dir);

build_latest();

do_copy("$examples_dir/template.css", "$output_dir/examples.css");
do_copy("$examples_dir/template-placeholder.png", "$output_dir/thumbnail.png");

opendir(my $dh, $examples_dir) or die("Couldn't opendir '$examples_dir': $!\n");

while (readdir($dh)) {
    next if ($_ eq '.') || ($_ eq '..');  # obviously skip current and parent entries.
    next if not -d "$examples_dir/$_";   # only care about subdirectories.
    # !!! FIXME: this needs to generate a preview page for all the categories.
    handle_category_dir($_);
}

closedir($dh);

# write homepage
my $homepage_list_html = "";
foreach my $category (get_categories()) {
    my $category_description = get_category_description($category);
    $homepage_list_html .= "<h2>$category_description</h2>";
    $homepage_list_html .= "<div class='list'>";
    $homepage_list_html .= generate_example_thumbnails_for_category($project, $category);
    $homepage_list_html .= "</div>";
}

my $preview_image = "/$project/thumbnail.png";

my $dst = "$output_dir/";
my $html = '';
open my $htmltemplate, '<', "$examples_dir/template-homepage.html" or die("Couldn't open '$examples_dir/template-category.html': $!\n");
while (<$htmltemplate>) {
    s/\@project_name\@/$project/g;
    s/\@homepage_list_html\@/$homepage_list_html/g;
    s/\@preview_image\@/$preview_image/g;
    $html .= $_;
}
close($htmltemplate);

open my $htmloutput, '>', "$dst/index.html" or die("Couldn't open '$dst/index.html': $!\n");
print $htmloutput $html;
close($htmloutput);

print("All examples built successfully!\n");
exit(0);  # success!
