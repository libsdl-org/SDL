#!/usr/bin/perl -w

# Add source files and headers to Xcode and Visual Studio projects.
# THIS IS NOT ROBUST, THIS IS JUST RYAN AVOIDING RUNNING BETWEEN
# THREE COMPUTERS AND A BUNCH OF DEVELOPMENT ENVIRONMENTS TO ADD
# A STUPID FILE TO THE BUILD.


use warnings;
use strict;
use File::Basename;


my %xcode_references = ();
sub generate_xcode_id {
    my @chars = ('0'..'9', 'A'..'F');
    my $str;

    do {
        my $len = 16;
        $str = '0000';  # start and end with '0000' so we know we added it.
        while ($len--) {
            $str .= $chars[rand @chars]
        };
        $str .= '0000';  # start and end with '0000' so we know we added it.
    } while (defined($xcode_references{$str}));

    $xcode_references{$str} = 1;  # so future calls can't generate this one.

    return $str;
}

sub process_xcode {
    my $addpath = shift;
    my $pbxprojfname = shift;
    my $lineno;

    %xcode_references = ();

    my $addfname = basename($addpath);
    my $addext = '';
    if ($addfname =~ /\.(.*?)\Z/) {
        $addext = $1;
    }

    my $is_public_header = ($addpath =~ /\Ainclude\/SDL3\//) ? 1 : 0;
    my $filerefpath = $is_public_header ? "SDL3/$addfname" : $addfname;

    my $srcs_or_headers = '';
    my $addfiletype = '';

    if ($addext eq 'c') {
        $srcs_or_headers = 'Sources';
        $addfiletype = 'sourcecode.c.c';
    } elsif ($addext eq 'm') {
        $srcs_or_headers = 'Sources';
        $addfiletype = 'sourcecode.c.objc';
    } elsif ($addext eq 'h') {
        $srcs_or_headers = 'Headers';
        $addfiletype = 'sourcecode.c.h';
    } else {
        die("Unexpected file extension '$addext'\n");
    }

    my $fh;

    open $fh, '<', $pbxprojfname or die("Failed to open '$pbxprojfname': $!\n");
    chomp(my @pbxproj = <$fh>);
    close($fh);

    # build a table of all ids, in case we duplicate one by some miracle.
    $lineno = 0;
    foreach (@pbxproj) {
        $lineno++;

        # like "F3676F582A7885080091160D /* SDL3.dmg */ = {"
        if (/\A\t\t([A-F0-9]{24}) \/\* (.*?) \*\/ \= \{\Z/) {
            $xcode_references{$1} = $2;
        }
    }

    # build out of a tree of PBXGroup items.
    my %pbxgroups = ();
    my $thispbxgroup;
    my $pbxgroup_children;
    my $pbxgroup_state = 0;
    my $pubheaders_group_hash = '';
    my $libsrc_group_hash = '';
    $lineno = 0;
    foreach (@pbxproj) {
        $lineno++;
        if ($pbxgroup_state == 0) {
            $pbxgroup_state++ if /\A\/\* Begin PBXGroup section \*\/\Z/;
        } elsif ($pbxgroup_state == 1) {
            # like "F3676F582A7885080091160D /* SDL3.dmg */ = {"
            if (/\A\t\t([A-F0-9]{24}) \/\* (.*?) \*\/ \= \{\Z/) {
                my %newhash = ();
                $pbxgroups{$1} = \%newhash;
                $thispbxgroup = \%newhash;
                $pubheaders_group_hash = $1 if $2 eq 'Public Headers';
                $libsrc_group_hash = $1 if $2 eq 'Library Source';
                $pbxgroup_state++;
            } elsif (/\A\/\* End PBXGroup section \*\/\Z/) {
                last;
            } else {
                die("Expected pbxgroup obj on '$pbxprojfname' line $lineno\n");
            }
        } elsif ($pbxgroup_state == 2) {
            if (/\A\t\t\tisa \= PBXGroup;\Z/) {
                $pbxgroup_state++;
            } else {
                die("Expected pbxgroup obj's isa field on '$pbxprojfname' line $lineno\n");
            }
        } elsif ($pbxgroup_state == 3) {
            if (/\A\t\t\tchildren \= \(\Z/) {
                my %newhash = ();
                $$thispbxgroup{'children'} = \%newhash;
                $pbxgroup_children = \%newhash;
                $pbxgroup_state++;
            } else {
                die("Expected pbxgroup obj's children field on '$pbxprojfname' line $lineno\n");
            }
        } elsif ($pbxgroup_state == 4) {
            if (/\A\t\t\t\t([A-F0-9]{24}) \/\* (.*?) \*\/,\Z/) {
                $$pbxgroup_children{$1} = $2;
            } elsif (/\A\t\t\t\);\Z/) {
                $pbxgroup_state++;
            } else {
                die("Expected pbxgroup obj's children element on '$pbxprojfname' line $lineno\n");
            }
        } elsif ($pbxgroup_state == 5) {
            if (/\A\t\t\t(.*?) \= (.*?);\Z/) {
                $$thispbxgroup{$1} = $2;
            } elsif (/\A\t\t\};\Z/) {
                $pbxgroup_state = 1;
            } else {
                die("Expected pbxgroup obj field on '$pbxprojfname' line $lineno\n");
            }
        } else {
            die("bug in this script.");
        }
    }

    die("Didn't see PBXGroup section in '$pbxprojfname'. Bug?\n") if $pbxgroup_state == 0;
    die("Didn't see Public Headers PBXGroup in '$pbxprojfname'. Bug?\n") if $pubheaders_group_hash eq '';
    die("Didn't see Library Source PBXGroup in '$pbxprojfname'. Bug?\n") if $libsrc_group_hash eq '';

    # Some debug log dumping...
    if (0) {
        foreach (keys %pbxgroups) {
            my $k = $_;
            my $g = $pbxgroups{$k};
            print("$_:\n");
            foreach (keys %$g) {
                print("  $_:\n");
                if ($_ eq 'children') {
                    my $kids = $$g{$_};
                    foreach (keys %$kids) {
                        print("    $_ -> " . $$kids{$_} . "\n");
                    }
                } else {
                    print('    ' . $$g{$_} . "\n");
                }
            }
            print("\n");
        }
    }

    # Get some unique IDs for our new thing.
    my $fileref = generate_xcode_id();
    my $buildfileref = generate_xcode_id();


    # Figure out what group to insert this into (or what groups to make)
    my $add_to_group_fileref = $fileref;
    my $add_to_group_addfname = $addfname;
    my $newgrptext = '';
    my $grphash = '';
    if ($is_public_header) {
        $grphash = $pubheaders_group_hash;  # done!
    } else  {
        $grphash = $libsrc_group_hash;
        my @splitpath = split(/\//, dirname($addpath));
        if ($splitpath[0] eq 'src') {
            shift @splitpath;
        }
        while (my $elem = shift(@splitpath)) {
            my $g = $pbxgroups{$grphash};
            my $kids = $$g{'children'};
            my $found = 0;
            foreach (keys %$kids) {
                my $hash = $_;
                my $fname = $$kids{$hash};
                if (uc($fname) eq uc($elem)) {
                    $grphash = $hash;
                    $found = 1;
                    last;
                }
            }
            unshift(@splitpath, $elem), last if (not $found);
        }

        if (@splitpath) {  # still elements? We need to build groups.
            my $newgroupref = generate_xcode_id();

            $add_to_group_fileref = $newgroupref;
            $add_to_group_addfname = $splitpath[0];

            while (my $elem = shift(@splitpath)) {
                my $lastelem = @splitpath ? 0 : 1;
                my $childhash = $lastelem ? $fileref : generate_xcode_id();
                my $childpath = $lastelem ? $addfname : $splitpath[0];
                $newgrptext .= "\t\t$newgroupref /* $elem */ = {\n";
                $newgrptext .= "\t\t\tisa = PBXGroup;\n";
                $newgrptext .= "\t\t\tchildren = (\n";
                $newgrptext .= "\t\t\t\t$childhash /* $childpath */,\n";
                $newgrptext .= "\t\t\t);\n";
                $newgrptext .= "\t\t\tpath = $elem;\n";
                $newgrptext .= "\t\t\tsourceTree = \"<group>\";\n";
                $newgrptext .= "\t\t};\n";
                $newgroupref = $childhash;
            }
        }
    }

    my $tmpfname = "$pbxprojfname.tmp";
    open $fh, '>', $tmpfname or die("Failed to open '$tmpfname': $!\n");

    my $add_to_this_group = 0;
    $pbxgroup_state = 0;
    $lineno = 0;
    foreach (@pbxproj) {
        $lineno++;
        if ($pbxgroup_state == 0) {
            # Drop in new references at the end of their sections...
            if (/\A\/\* End PBXBuildFile section \*\/\Z/) {
                print $fh "\t\t$buildfileref /* $addfname in $srcs_or_headers */ = {isa = PBXBuildFile; fileRef = $fileref /* $addfname */;";
                if ($is_public_header) {
                    print $fh " settings = {ATTRIBUTES = (Public, ); };";
                }
                print $fh " };\n";
            } elsif (/\A\/\* End PBXFileReference section \*\/\Z/) {
                print $fh "\t\t$fileref /* $addfname */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = $addfiletype; name = $addfname; path = $filerefpath; sourceTree = \"<group>\"; };\n";
            } elsif (/\A\/\* Begin PBXGroup section \*\/\Z/) {
                $pbxgroup_state = 1;
            } elsif (/\A\/\* Begin PBXSourcesBuildPhase section \*\/\Z/) {
                $pbxgroup_state = 5;
            }
        } elsif ($pbxgroup_state == 1) {
            if (/\A\t\t([A-F0-9]{24}) \/\* (.*?) \*\/ \= \{\Z/) {
                $pbxgroup_state++;
                $add_to_this_group = $1 eq $grphash;
            } elsif (/\A\/\* End PBXGroup section \*\/\Z/) {
                print $fh $newgrptext;
                $pbxgroup_state = 0;
            }
        } elsif ($pbxgroup_state == 2) {
            if (/\A\t\t\tchildren \= \(\Z/) {
                $pbxgroup_state++;
            }
        } elsif ($pbxgroup_state == 3) {
            if (/\A\t\t\t\);\Z/) {
                if ($add_to_this_group) {
                    print $fh "\t\t\t\t$add_to_group_fileref /* $add_to_group_addfname */,\n";
                }
                $pbxgroup_state++;
            }
        } elsif ($pbxgroup_state == 4) {
            if (/\A\t\t\};\Z/) {
                $add_to_this_group = 0;
            }
            $pbxgroup_state = 1;
        } elsif ($pbxgroup_state == 5) {
            if (/\A\t\t\t\);\Z/) {
                if ($srcs_or_headers eq 'Sources') {
                    print $fh "\t\t\t\t$buildfileref /* $addfname in $srcs_or_headers */,\n";
                }
                $pbxgroup_state = 0;
            }
        }

        print $fh "$_\n";
    }

    close($fh);
    rename($tmpfname, $pbxprojfname);
}

my %visualc_references = ();
sub generate_visualc_id {   # these are just standard Windows GUIDs.
    my @chars = ('0'..'9', 'a'..'f');
    my $str;

    do {
        my $len = 24;
        $str = '0000';  # start and end with '0000' so we know we added it.
        while ($len--) {
            $str .= $chars[rand @chars]
        };
        $str .= '0000';  # start and end with '0000' so we know we added it.
        $str =~ s/\A(........)(....)(....)(............)\Z/$1-$2-$3-$4/;  # add dashes in the appropriate places.
    } while (defined($visualc_references{$str}));

    $visualc_references{$str} = 1;  # so future calls can't generate this one.

    return $str;
}


sub process_visualstudio {
    my $addpath = shift;
    my $vcxprojfname = shift;
    my $lineno;

    %visualc_references = ();

    my $is_public_header = ($addpath =~ /\Ainclude\/SDL3\//) ? 1 : 0;

    my $addfname = basename($addpath);
    my $addext = '';
    if ($addfname =~ /\.(.*?)\Z/) {
        $addext = $1;
    }

    my $isheader = 0;
    if ($addext eq 'c') {
        $isheader = 0;
    } elsif ($addext eq 'm') {
        return;  # don't add Objective-C files to Visual Studio projects!
    } elsif ($addext eq 'h') {
        $isheader = 1;
    } else {
        die("Unexpected file extension '$addext'\n");
    }

    my $fh;

    open $fh, '<', $vcxprojfname or die("Failed to open '$vcxprojfname': $!\n");
    chomp(my @vcxproj = <$fh>);
    close($fh);

    my $vcxgroup_state;

    my $rawaddvcxpath = "$addpath";
    $rawaddvcxpath =~ s/\//\\/g;

    # Figure out relative path from vcxproj file...
    my $addvcxpath = '';
    my @subdirs = split(/\//, $vcxprojfname);
    pop @subdirs;
    foreach (@subdirs) {
        $addvcxpath .= "..\\";
    }
    $addvcxpath .= $rawaddvcxpath;

    my $prevname = undef;

    my $tmpfname;

    $tmpfname = "$vcxprojfname.tmp";
    open $fh, '>', $tmpfname or die("Failed to open '$tmpfname': $!\n");

    my $added = 0;

    $added = 0;
    $vcxgroup_state = 0;
    $prevname = undef;
    $lineno = 0;
    foreach (@vcxproj) {
        $lineno++;
        if ($vcxgroup_state == 0) {
            if (/\A  \<ItemGroup\>\Z/) {
                $vcxgroup_state = 1;
                $prevname = undef;
            }
        } elsif ($vcxgroup_state == 1) {
            if (/\A    \<ClInclude .*\Z/) {
                $vcxgroup_state = 2 if $isheader;
            } elsif (/\A    \<ClCompile .*\Z/) {
                $vcxgroup_state = 3 if not $isheader;
            } elsif (/\A  \<\/ItemGroup\>\Z/) {
                $vcxgroup_state = 0;
                $prevname = undef;
            }
        }

        # Don't do elsif, we need to check this line again.
        if ($vcxgroup_state == 2) {
            if (/\A    <ClInclude Include="(.*?)" \/\>\Z/) {
                my $nextname = $1;
                if ((not $added) && (((not defined $prevname) || (uc($prevname) lt uc($addvcxpath))) && (uc($nextname) gt uc($addvcxpath)))) {
                    print $fh "    <ClInclude Include=\"$addvcxpath\" />\n";
                    $vcxgroup_state = 0;
                    $added = 1;
                }
                $prevname = $nextname;
            } elsif (/\A  \<\/ItemGroup\>\Z/) {
                if ((not $added) && ((not defined $prevname) || (uc($prevname) lt uc($addvcxpath)))) {
                    print $fh "    <ClInclude Include=\"$addvcxpath\" />\n";
                    $vcxgroup_state = 0;
                    $added = 1;
                }
            }
        } elsif ($vcxgroup_state == 3) {
            if (/\A    <ClCompile Include="(.*?)" \/\>\Z/) {
                my $nextname = $1;
                if ((not $added) && (((not defined $prevname) || (uc($prevname) lt uc($addvcxpath))) && (uc($nextname) gt uc($addvcxpath)))) {
                    print $fh "    <ClCompile Include=\"$addvcxpath\" />\n";
                    $vcxgroup_state = 0;
                    $added = 1;
                }
                $prevname = $nextname;
            } elsif (/\A  \<\/ItemGroup\>\Z/) {
                if ((not $added) && ((not defined $prevname) || (uc($prevname) lt uc($addvcxpath)))) {
                    print $fh "    <ClCompile Include=\"$addvcxpath\" />\n";
                    $vcxgroup_state = 0;
                    $added = 1;
                }
            }
        }

        print $fh "$_\n";
    }

    close($fh);
    rename($tmpfname, $vcxprojfname);

    my $vcxfiltersfname = "$vcxprojfname.filters";
    open $fh, '<', $vcxfiltersfname or die("Failed to open '$vcxfiltersfname': $!\n");
    chomp(my @vcxfilters = <$fh>);
    close($fh);

    my $newgrptext = '';
    my $filter = '';
    if ($is_public_header) {
        $filter = 'API Headers';
    } else {
        $filter = lc(dirname($addpath));
        $filter =~ s/\Asrc\///; # there's no filter for the base "src/" dir, where SDL.c and friends live.
        $filter =~ s/\//\\/g;
        $filter = '' if $filter eq 'src';

        if ($filter ne '') {
            # see if the filter already exists, otherwise add it.
            my %existing_filters = ();
            my $current_filter = '';
            my $found = 0;
            foreach (@vcxfilters) {
                # These lines happen to be unique, so we don't have to parse down to find this section.
                if (/\A    \<Filter Include\="(.*?)"\>\Z/) {
                    $current_filter = lc($1);
                    if ($current_filter eq $filter) {
                        $found = 1;
                    }
                } elsif (/\A      \<UniqueIdentifier\>\{(.*?)\}\<\/UniqueIdentifier\>\Z/) {
                    $visualc_references{$1} = $current_filter;  # gather up existing GUIDs to avoid duplicates.
                    $existing_filters{$current_filter} = $1;
                }
            }

            if (not $found) {  # didn't find it? We need to build filters.
                my $subpath = '';
                my @splitpath = split(/\\/, $filter);
                while (my $elem = shift(@splitpath)) {
                    $subpath .= "\\" if ($subpath ne '');
                    $subpath .= $elem;
                    if (not $existing_filters{$subpath}) {
                        my $newgroupref = generate_visualc_id();
                        $newgrptext .= "    <Filter Include=\"$subpath\">\n";
                        $newgrptext .= "      <UniqueIdentifier>{$newgroupref}</UniqueIdentifier>\n";
                        $newgrptext .= "    </Filter>\n"
                    }
                }
            }
        }
    }

    $tmpfname = "$vcxfiltersfname.tmp";
    open $fh, '>', $tmpfname or die("Failed to open '$tmpfname': $!\n");

    $added = 0;
    $vcxgroup_state = 0;
    $prevname = undef;
    $lineno = 0;
    foreach (@vcxfilters) {
        $lineno++;

        # We cheat here, because these lines are unique, we don't have to fully parse this file.
        if ($vcxgroup_state == 0) {
            if (/\A    \<Filter Include\="(.*?)"\>\Z/) {
                if ($newgrptext ne '') {
                    $vcxgroup_state = 1;
                    $prevname = undef;
                }
            } elsif (/\A    \<ClInclude .*\Z/) {
                if ($isheader) {
                    $vcxgroup_state = 2;
                    $prevname = undef;
                }
            } elsif (/\A    \<ClCompile .*\Z/) {
                if (not $isheader) {
                    $vcxgroup_state = 3;
                    $prevname = undef;
                }
            }
        }

        # Don't do elsif, we need to check this line again.
        if ($vcxgroup_state == 1) {
            if (/\A  \<\/ItemGroup\>\Z/) {
                print $fh $newgrptext;
                $newgrptext = '';
                $vcxgroup_state = 0;
            }
        } elsif ($vcxgroup_state == 2) {
            if (/\A    <ClInclude Include="(.*?)"/) {
                my $nextname = $1;
                if ((not $added) && (((not defined $prevname) || (uc($prevname) lt uc($addvcxpath))) && (uc($nextname) gt uc($addvcxpath)))) {
                    print $fh "    <ClInclude Include=\"$addvcxpath\"";
                    if ($filter ne '') {
                        print $fh ">\n";
                        print $fh "      <Filter>$filter</Filter>\n";
                        print $fh "    </ClInclude>\n";
                    } else {
                        print $fh " />\n";
                    }
                    $added = 1;
                }
                $prevname = $nextname;
            } elsif (/\A  \<\/ItemGroup\>\Z/) {
                if ((not $added) && ((not defined $prevname) || (uc($prevname) lt uc($addvcxpath)))) {
                    print $fh "    <ClInclude Include=\"$addvcxpath\"";
                    if ($filter ne '') {
                        print $fh ">\n";
                        print $fh "      <Filter>$filter</Filter>\n";
                        print $fh "    </ClInclude>\n";
                    } else {
                        print $fh " />\n";
                    }
                    $added = 1;
                }
                $vcxgroup_state = 0;
            }
        } elsif ($vcxgroup_state == 3) {
            if (/\A    <ClCompile Include="(.*?)"/) {
                my $nextname = $1;
                if ((not $added) && (((not defined $prevname) || (uc($prevname) lt uc($addvcxpath))) && (uc($nextname) gt uc($addvcxpath)))) {
                    print $fh "    <ClCompile Include=\"$addvcxpath\"";
                    if ($filter ne '') {
                        print $fh ">\n";
                        print $fh "      <Filter>$filter</Filter>\n";
                        print $fh "    </ClCompile>\n";
                    } else {
                        print $fh " />\n";
                    }
                    $added = 1;
                }
                $prevname = $nextname;
            } elsif (/\A  \<\/ItemGroup\>\Z/) {
                if ((not $added) && ((not defined $prevname) || (uc($prevname) lt uc($addvcxpath)))) {
                    print $fh "    <ClCompile Include=\"$addvcxpath\"";
                    if ($filter ne '') {
                        print $fh ">\n";
                        print $fh "      <Filter>$filter</Filter>\n";
                        print $fh "    </ClCompile>\n";
                    } else {
                        print $fh " />\n";
                    }
                    $added = 1;
                }
                $vcxgroup_state = 0;
            }
        }

        print $fh "$_\n";
    }

    close($fh);
    rename($tmpfname, $vcxfiltersfname);
}


# Mainline!

chdir(dirname($0));  # assumed to be in build-scripts
chdir('..');  # head to root of source tree.

foreach (@ARGV) {
    s/\A\.\///;  # Turn "./path/to/file.txt" into "path/to/file.txt"
    my $arg = $_;
    process_xcode($arg, 'Xcode/SDL/SDL.xcodeproj/project.pbxproj');
    process_visualstudio($arg, 'VisualC/SDL/SDL.vcxproj');
    process_visualstudio($arg, 'VisualC-GDK/SDL/SDL.vcxproj');
    process_visualstudio($arg, 'VisualC-WinRT/SDL-UWP.vcxproj');
}

print("Done. Please run `git diff` and make sure this looks okay!\n");

exit(0);

