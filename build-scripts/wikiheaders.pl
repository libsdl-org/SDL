#!/usr/bin/perl -w

use warnings;
use strict;
use File::Path;
use Text::Wrap;

$Text::Wrap::huge = 'overflow';

my $projectfullname = 'Simple Directmedia Layer';
my $projectshortname = 'SDL';
my $wikisubdir = '';
my $incsubdir = 'include';
my $readmesubdir = undef;
my $apiprefixregex = undef;
my $versionfname = 'include/SDL_version.h';
my $versionmajorregex = '\A\#define\s+SDL_MAJOR_VERSION\s+(\d+)\Z';
my $versionminorregex = '\A\#define\s+SDL_MINOR_VERSION\s+(\d+)\Z';
my $versionmicroregex = '\A\#define\s+SDL_MICRO_VERSION\s+(\d+)\Z';
my $mainincludefname = 'SDL.h';
my $selectheaderregex = '\ASDL.*?\.h\Z';
my $projecturl = 'https://libsdl.org/';
my $wikiurl = 'https://wiki.libsdl.org';
my $bugreporturl = 'https://github.com/libsdl-org/sdlwiki/issues/new';
my $srcpath = undef;
my $wikipath = undef;
my $wikireadmesubdir = 'README';
my $warn_about_missing = 0;
my $copy_direction = 0;
my $optionsfname = undef;
my $wikipreamble = undef;
my $wikiheaderfiletext = 'Defined in %fname%';
my $manpageheaderfiletext = 'Defined in %fname%';
my $headercategoryeval = undef;
my $changeformat = undef;
my $manpath = undef;
my $gitrev = undef;

foreach (@ARGV) {
    $warn_about_missing = 1, next if $_ eq '--warn-about-missing';
    $copy_direction = 1, next if $_ eq '--copy-to-headers';
    $copy_direction = 1, next if $_ eq '--copy-to-header';
    $copy_direction = -1, next if $_ eq '--copy-to-wiki';
    $copy_direction = -2, next if $_ eq '--copy-to-manpages';
    $copy_direction = -3, next if $_ eq '--report-coverage-gaps';
    $copy_direction = -4, next if $_ eq '--copy-to-latex';
    if (/\A--options=(.*)\Z/) {
        $optionsfname = $1;
        next;
    } elsif (/\A--changeformat=(.*)\Z/) {
        $changeformat = $1;
        next;
    } elsif (/\A--manpath=(.*)\Z/) {
        $manpath = $1;
        next;
    } elsif (/\A--rev=(.*)\Z/) {
        $gitrev = $1;
        next;
    }
    $srcpath = $_, next if not defined $srcpath;
    $wikipath = $_, next if not defined $wikipath;
}

my $default_optionsfname = '.wikiheaders-options';
$default_optionsfname = "$srcpath/$default_optionsfname" if defined $srcpath;

if ((not defined $optionsfname) && (-f $default_optionsfname)) {
    $optionsfname = $default_optionsfname;
}

if (defined $optionsfname) {
    open OPTIONS, '<', $optionsfname or die("Failed to open options file '$optionsfname': $!\n");
    while (<OPTIONS>) {
        next if /\A\s*\#/;  # Skip lines that start with (optional whitespace, then) '#' as comments.

        chomp;
        if (/\A(.*?)\=(.*)\Z/) {
            my $key = $1;
            my $val = $2;
            $key =~ s/\A\s+//;
            $key =~ s/\s+\Z//;
            $val =~ s/\A\s+//;
            $val =~ s/\s+\Z//;
            $warn_about_missing = int($val), next if $key eq 'warn_about_missing';
            $srcpath = $val, next if $key eq 'srcpath';
            $wikipath = $val, next if $key eq 'wikipath';
            $apiprefixregex = $val, next if $key eq 'apiprefixregex';
            $projectfullname = $val, next if $key eq 'projectfullname';
            $projectshortname = $val, next if $key eq 'projectshortname';
            $wikisubdir = $val, next if $key eq 'wikisubdir';
            $incsubdir = $val, next if $key eq 'incsubdir';
            $readmesubdir = $val, next if $key eq 'readmesubdir';
            $versionmajorregex = $val, next if $key eq 'versionmajorregex';
            $versionminorregex = $val, next if $key eq 'versionminorregex';
            $versionmicroregex = $val, next if $key eq 'versionmicroregex';
            $versionfname = $val, next if $key eq 'versionfname';
            $mainincludefname = $val, next if $key eq 'mainincludefname';
            $selectheaderregex = $val, next if $key eq 'selectheaderregex';
            $projecturl = $val, next if $key eq 'projecturl';
            $wikiurl = $val, next if $key eq 'wikiurl';
            $bugreporturl = $val, next if $key eq 'bugreporturl';
            $wikipreamble = $val, next if $key eq 'wikipreamble';
            $wikiheaderfiletext = $val, next if $key eq 'wikiheaderfiletext';
            $manpageheaderfiletext = $val, next if $key eq 'manpageheaderfiletext';
            $headercategoryeval = $val, next if $key eq 'headercategoryeval';
        }
    }
    close(OPTIONS);
}

sub escLaTeX {
    my $str = shift;
    $str =~ s/([_\#\&\^])/\\$1/g;
    return $str;
}

my $wordwrap_mode = 'mediawiki';
sub wordwrap_atom {   # don't call this directly.
    my $str = shift;
    my $retval = '';

    # wordwrap but leave links intact, even if they overflow.
    if ($wordwrap_mode eq 'mediawiki') {
        while ($str =~ s/(.*?)\s*(\[https?\:\/\/.*?\s+.*?\])\s*//ms) {
            $retval .= fill('', '', $1); # wrap it.
            $retval .= "\n$2\n";  # don't wrap it.
        }
    } elsif ($wordwrap_mode eq 'md') {
        while ($str =~ s/(.*?)\s*(\[.*?\]\(https?\:\/\/.*?\))\s*//ms) {
            $retval .= fill('', '', $1); # wrap it.
            $retval .= "\n$2\n";  # don't wrap it.
        }
    }

    return $retval . fill('', '', $str);
}

sub wordwrap_with_bullet_indent {  # don't call this directly.
    my $bullet = shift;
    my $str = shift;
    my $retval = '';

    #print("WORDWRAP BULLET ('$bullet'):\n\n$str\n\n");

    # You _can't_ (at least with Pandoc) have a bullet item with a newline in
    #  MediaWiki, so _remove_ wrapping!
    if ($wordwrap_mode eq 'mediawiki') {
        $retval = "$bullet$str";
        $retval =~ s/\n/ /gms;
        $retval =~ s/\s+$//gms;
        #print("WORDWRAP BULLET DONE:\n\n$retval\n\n");
        return "$retval\n";
    }

    my $bulletlen = length($bullet);

    # wrap it and then indent each line to be under the bullet.
    $Text::Wrap::columns -= $bulletlen;
    my @wrappedlines = split /\n/, wordwrap_atom($str);
    $Text::Wrap::columns += $bulletlen;

    my $prefix = $bullet;
    my $usual_prefix = ' ' x $bulletlen;

    foreach (@wrappedlines) {
        s/\s*\Z//;
        $retval .= "$prefix$_\n";
        $prefix = $usual_prefix;
    }

    return $retval;
}

sub wordwrap_one_paragraph {  # don't call this directly.
    my $retval = '';
    my $p = shift;
    #print "\n\n\nPARAGRAPH: [$p]\n\n\n";
    if ($p =~ s/\A([\*\-] )//) {  # bullet list, starts with "* " or "- ".
        my $bullet = $1;
        my $item = '';
        my @items = split /\n/, $p;
        foreach (@items) {
            if (s/\A([\*\-] )//) {
                $retval .= wordwrap_with_bullet_indent($bullet, $item);
                $item = '';
            }
            s/\A\s*//;
            $item .= "$_\n";   # accumulate lines until we hit the end or another bullet.
        }
        if ($item ne '') {
            $retval .= wordwrap_with_bullet_indent($bullet, $item);
        }
    } elsif ($p =~ /\A\s*\|.*\|\s*\n/) {  # Markdown table
        $retval = "$p\n";  # don't wrap it (!!! FIXME: but maybe parse by lines until we run out of table...)
    } else {
        $retval = wordwrap_atom($p) . "\n";
    }

    return $retval;
}

sub wordwrap_paragraphs {  # don't call this directly.
    my $str = shift;
    my $retval = '';
    my @paragraphs = split /\n\n/, $str;
    foreach (@paragraphs) {
        next if $_ eq '';
        $retval .= wordwrap_one_paragraph($_);
        $retval .= "\n";
    }
    return $retval;
}

my $wordwrap_default_columns = 76;
sub wordwrap {
    my $str = shift;
    my $columns = shift;

    $columns = $wordwrap_default_columns if not defined $columns;
    $columns += $wordwrap_default_columns if $columns < 0;
    $Text::Wrap::columns = $columns;

    my $retval = '';

    #print("\n\nWORDWRAP:\n\n$str\n\n\n");

    $str =~ s/\A\n+//ms;

    while ($str =~ s/(.*?)(\`\`\`.*?\`\`\`|\<syntaxhighlight.*?\<\/syntaxhighlight\>)//ms) {
        #print("\n\nWORDWRAP BLOCK:\n\n$1\n\n ===\n\n$2\n\n\n");
        $retval .= wordwrap_paragraphs($1); # wrap it.
        $retval .= "$2\n\n";  # don't wrap it.
    }

    $retval .= wordwrap_paragraphs($str);  # wrap what's left.
    $retval =~ s/\n+\Z//ms;

    #print("\n\nWORDWRAP DONE:\n\n$retval\n\n\n");
    return $retval;
}

# This assumes you're moving from Markdown (in the Doxygen data) to Wiki, which
#  is why the 'md' section is so sparse.
sub wikify_chunk {
    my $wikitype = shift;
    my $str = shift;
    my $codelang = shift;
    my $code = shift;

    #print("\n\nWIKIFY CHUNK:\n\n$str\n\n\n");

    if ($wikitype eq 'mediawiki') {
        # convert `code` things first, so they aren't mistaken for other markdown items.
        my $codedstr = '';
        while ($str =~ s/\A(.*?)\`(.*?)\`//ms) {
            my $codeblock = $2;
            $codedstr .= wikify_chunk($wikitype, $1, undef, undef);
            if (defined $apiprefixregex) {
                # Convert obvious API things to wikilinks, even inside `code` blocks.
                $codeblock =~ s/\b($apiprefixregex[a-zA-Z0-9_]+)/[[$1]]/gms;
            }
            $codedstr .= "<code>$codeblock</code>";
        }

        # Convert obvious API things to wikilinks.
        if (defined $apiprefixregex) {
            $str =~ s/\b($apiprefixregex[a-zA-Z0-9_]+)/[[$1]]/gms;
        }

        # Make some Markdown things into MediaWiki...

        # links
        $str =~ s/\[(.*?)\]\((https?\:\/\/.*?)\)/\[$2 $1\]/g;

        # bold+italic
        $str =~ s/\*\*\*(.*?)\*\*\*/'''''$1'''''/gms;

        # bold
        $str =~ s/\*\*(.*?)\*\*/'''$1'''/gms;

        # italic
        $str =~ s/\*(.*?)\*/''$1''/gms;

        # bullets
        $str =~ s/^\- /* /gm;

        $str = $codedstr . $str;

        if (defined $code) {
            $str .= "<syntaxhighlight lang='$codelang'>$code<\/syntaxhighlight>";
        }
    } elsif ($wikitype eq 'md') {
        # convert `code` things first, so they aren't mistaken for other markdown items.
        my $codedstr = '';
        while ($str =~ s/\A(.*?)(\`.*?\`)//ms) {
            my $codeblock = $2;
            $codedstr .= wikify_chunk($wikitype, $1, undef, undef);
            if (defined $apiprefixregex) {
                # Convert obvious API things to wikilinks, even inside `code` blocks,
                # BUT ONLY IF the entire code block is the API thing,
                # So something like "just call `SDL_Whatever`" will become
                # "just call [`SDL_Whatever`](SDL_Whatever)", but
                # "just call `SDL_Whatever(7)`" will not. It's just the safest
                # way to do this without resorting to wrapping things in html <code> tags.
                $codeblock =~ s/\A\`($apiprefixregex[a-zA-Z0-9_]+)\`\Z/[`$1`]($1)/gms;
            }
            $codedstr .= $codeblock;
        }

        # Convert obvious API things to wikilinks.
        if (defined $apiprefixregex) {
            $str =~ s/\b($apiprefixregex[a-zA-Z0-9_]+)/[$1]($1)/gms;
        }

        $str = $codedstr . $str;

        if (defined $code) {
            $str .= "```$codelang\n$code\n```\n";
        }
    }

    #print("\n\nWIKIFY CHUNK DONE:\n\n$str\n\n\n");

    return $str;
}

sub wikify {
    my $wikitype = shift;
    my $str = shift;
    my $retval = '';

    #print("WIKIFY WHOLE:\n\n$str\n\n\n");

    while ($str =~ s/\A(.*?)\`\`\`(.*?)\n(.*?)\n\`\`\`(\n|\Z)//ms) {
        $retval .= wikify_chunk($wikitype, $1, $2, $3);
    }
    $retval .= wikify_chunk($wikitype, $str, undef, undef);

    #print("WIKIFY WHOLE DONE:\n\n$retval\n\n\n");

    return $retval;
}


my $dewikify_mode = 'md';
my $dewikify_manpage_code_indent = 1;

sub dewikify_chunk {
    my $wikitype = shift;
    my $str = shift;
    my $codelang = shift;
    my $code = shift;

    #print("\n\nDEWIKIFY CHUNK:\n\n$str\n\n\n");

    if ($dewikify_mode eq 'md') {
        if ($wikitype eq 'mediawiki') {
            # Doxygen supports Markdown (and it just simply looks better than MediaWiki
            # when looking at the raw headers), so do some conversions here as necessary.

            # Dump obvious wikilinks.
            if (defined $apiprefixregex) {
                $str =~ s/\[\[($apiprefixregex[a-zA-Z0-9_]+)\]\]/$1/gms;
            }

            # links
            $str =~ s/\[(https?\:\/\/.*?)\s+(.*?)\]/\[$2\]\($1\)/g;

            # <code></code> is also popular.  :/
            $str =~ s/\<code>(.*?)<\/code>/`$1`/gms;

            # bold+italic
            $str =~ s/'''''(.*?)'''''/***$1***/gms;

            # bold
            $str =~ s/'''(.*?)'''/**$1**/gms;

            # italic
            $str =~ s/''(.*?)''/*$1*/gms;

            # bullets
            $str =~ s/^\* /- /gm;
        } elsif ($wikitype eq 'md') {
            # Dump obvious wikilinks. The rest can just passthrough.
            if (defined $apiprefixregex) {
                $str =~ s/\[(\`?$apiprefixregex[a-zA-Z0-9_]+\`?)\]\($apiprefixregex[a-zA-Z0-9_]+\)/$1/gms;
            }
        }

        if (defined $code) {
            $str .= "\n```$codelang\n$code\n```\n";
        }
    } elsif ($dewikify_mode eq 'manpage') {
        $str =~ s/\./\\[char46]/gms;  # make sure these can't become control codes.
        if ($wikitype eq 'mediawiki') {
            # Dump obvious wikilinks.
            if (defined $apiprefixregex) {
                $str =~ s/\s*\[\[($apiprefixregex[a-zA-Z0-9_]+)\]\]\s*/\n.BR $1\n/gms;
            }

            # links
            $str =~ s/\[(https?\:\/\/.*?)\s+(.*?)\]/\n.URL "$1" "$2"\n/g;

            # <code></code> is also popular.  :/
            $str =~ s/\s*\<code>(.*?)<\/code>\s*/\n.BR $1\n/gms;

            # bold+italic (this looks bad, just make it bold).
            $str =~ s/\s*'''''(.*?)'''''\s*/\n.B $1\n/gms;

            # bold
            $str =~ s/\s*'''(.*?)'''\s*/\n.B $1\n/gms;

            # italic
            $str =~ s/\s*''(.*?)''\s*/\n.I $1\n/gms;

            # bullets
            $str =~ s/^\* /\n\\\(bu /gm;
        } elsif ($wikitype eq 'md') {
            # Dump obvious wikilinks.
            if (defined $apiprefixregex) {
                $str =~ s/\[(\`?$apiprefixregex[a-zA-Z0-9_]+\`?)\]\($apiprefixregex[a-zA-Z0-9_]+\)/\n.BR $1\n/gms;
            }

            # links
            $str =~ s/\[(.*?)]\((https?\:\/\/.*?)\)/\n.URL "$2" "$1"\n/g;

            # <code></code> is also popular.  :/
            $str =~ s/\s*\`(.*?)\`\s*/\n.BR $1\n/gms;

            # bold+italic (this looks bad, just make it bold).
            $str =~ s/\s*\*\*\*(.*?)\*\*\*\s*/\n.B $1\n/gms;

            # bold
            $str =~ s/\s*\*\*(.*?)\*\*\s*/\n.B $1\n/gms;

            # italic
            $str =~ s/\s*\*(.*?)\*\s*/\n.I $1\n/gms;

            # bullets
            $str =~ s/^\- /\n\\\(bu /gm;
        }

        if (defined $code) {
            $code =~ s/\A\n+//gms;
            $code =~ s/\n+\Z//gms;
            if ($dewikify_manpage_code_indent) {
                $str .= "\n.IP\n"
            } else {
                $str .= "\n.PP\n"
            }
            $str .= ".EX\n$code\n.EE\n.PP\n";
        }
    } elsif ($dewikify_mode eq 'LaTeX') {
        if ($wikitype eq 'mediawiki') {
            # Dump obvious wikilinks.
            if (defined $apiprefixregex) {
                $str =~ s/\s*\[\[($apiprefixregex[a-zA-Z0-9_]+)\]\]/$1/gms;
            }

            # links
            $str =~ s/\[(https?\:\/\/.*?)\s+(.*?)\]/\\href{$1}{$2}/g;

            # <code></code> is also popular.  :/
            $str =~ s/\s*\<code>(.*?)<\/code>/ \\texttt{$1}/gms;

            # bold+italic
            $str =~ s/\s*'''''(.*?)'''''/ \\textbf{\\textit{$1}}/gms;

            # bold
            $str =~ s/\s*'''(.*?)'''/ \\textbf{$1}/gms;

            # italic
            $str =~ s/\s*''(.*?)''/ \\textit{$1}/gms;

            # bullets
            $str =~ s/^\*\s+/  \\item /gm;
        } elsif ($wikitype eq 'md') {
            # Dump obvious wikilinks.
            if (defined $apiprefixregex) {
                $str =~ s/\[(\`?$apiprefixregex[a-zA-Z0-9_]+\`?)\]\($apiprefixregex[a-zA-Z0-9_]+\)/$1/gms;
            }

            # links
            $str =~ s/\[(.*?)]\((https?\:\/\/.*?)\)/\\href{$2}{$1}/g;

            # <code></code> is also popular.  :/
            $str =~ s/\s*\`(.*?)\`/ \\texttt{$1}/gms;

            # bold+italic
            $str =~ s/\s*\*\*\*(.*?)\*\*\*/ \\textbf{\\textit{$1}}/gms;

            # bold
            $str =~ s/\s*\*\*(.*?)\*\*/ \\textbf{$1}/gms;

            # italic
            $str =~ s/\s*\*(.*?)\*/ \\textit{$1}/gms;

            # bullets
            $str =~ s/^\-\s+/  \\item /gm;
        }

        # Wrap bullet lists in itemize blocks...
        $str =~ s/^(\s*\\item .*?)(\n\n|\Z)/\n\\begin{itemize}\n$1$2\n\\end{itemize}\n\n/gms;

        $str = escLaTeX($str);

        if (defined $code) {
            $code =~ s/\A\n+//gms;
            $code =~ s/\n+\Z//gms;

            if (($codelang eq '') || ($codelang eq 'output')) {
                $str .= "\\begin{verbatim}\n$code\n\\end{verbatim}\n";
            } else {
                if ($codelang eq 'c') {
                    $codelang = 'C';
                } elsif ($codelang eq 'c++') {
                    $codelang = 'C++';
                } else {
                    die("Unexpected codelang '$codelang'");
                }
                $str .= "\n\\lstset{language=$codelang}\n";
                $str .= "\\begin{lstlisting}\n$code\n\\end{lstlisting}\n";
            }
        }
    } else {
        die("Unexpected dewikify_mode");
    }

    #print("\n\nDEWIKIFY CHUNK DONE:\n\n$str\n\n\n");

    return $str;
}

sub dewikify {
    my $wikitype = shift;
    my $str = shift;
    return '' if not defined $str;

    #print("DEWIKIFY WHOLE:\n\n$str\n\n\n");

    $str =~ s/\A[\s\n]*\= .*? \=\s*?\n+//ms;
    $str =~ s/\A[\s\n]*\=\= .*? \=\=\s*?\n+//ms;

    my $retval = '';
    if ($wikitype eq 'mediawiki') {
        while ($str =~ s/\A(.*?)<syntaxhighlight lang='?(.*?)'?>(.*?)<\/syntaxhighlight\>//ms) {
            $retval .= dewikify_chunk($wikitype, $1, $2, $3);
        }
    } elsif ($wikitype eq 'md') {
        while ($str =~ s/\A(.*?)\n```(.*?)\n(.*?)\n```\n//ms) {
            $retval .= dewikify_chunk($wikitype, $1, $2, $3);
        }
    }
    $retval .= dewikify_chunk($wikitype, $str, undef, undef);

    #print("DEWIKIFY WHOLE DONE:\n\n$retval\n\n\n");

    return $retval;
}

sub filecopy {
    my $src = shift;
    my $dst = shift;
    my $endline = shift;
    $endline = "\n" if not defined $endline;

    open(COPYIN, '<', $src) or die("Failed to open '$src' for reading: $!\n");
    open(COPYOUT, '>', $dst) or die("Failed to open '$dst' for writing: $!\n");
    while (<COPYIN>) {
        chomp;
        s/[ \t\r\n]*\Z//;
        print COPYOUT "$_$endline";
    }
    close(COPYOUT);
    close(COPYIN);
}

sub usage {
    die("USAGE: $0 <source code git clone path> <wiki git clone path> [--copy-to-headers|--copy-to-wiki|--copy-to-manpages] [--warn-about-missing] [--manpath=<man path>]\n\n");
}

usage() if not defined $srcpath;
usage() if not defined $wikipath;
#usage() if $copy_direction == 0;

if (not defined $manpath) {
    $manpath = "$srcpath/man";
}

my @standard_wiki_sections = (
    'Draft',
    '[Brief]',
    'Deprecated',
    'Header File',
    'Syntax',
    'Function Parameters',
    'Macro Parameters',
    'Fields',
    'Values',
    'Return Value',
    'Remarks',
    'Thread Safety',
    'Version',
    'Code Examples',
    'See Also'
);

# Sections that only ever exist in the wiki and shouldn't be deleted when
#  not found in the headers.
my %only_wiki_sections = (  # The ones don't mean anything, I just need to check for key existence.
    'Draft', 1,
    'Code Examples', 1,
    'Header File', 1
);


my %headers = ();       # $headers{"SDL_audio.h"} -> reference to an array of all lines of text in SDL_audio.h.
my %headersyms = ();   # $headersyms{"SDL_OpenAudio"} -> string of header documentation for SDL_OpenAudio, with comment '*' bits stripped from the start. Newlines embedded!
my %headerdecls = ();
my %headersymslocation = ();   # $headersymslocation{"SDL_OpenAudio"} -> name of header holding SDL_OpenAudio define ("SDL_audio.h" in this case).
my %headersymschunk = ();   # $headersymschunk{"SDL_OpenAudio"} -> offset in array in %headers that should be replaced for this symbol.
my %headersymshasdoxygen = ();   # $headersymshasdoxygen{"SDL_OpenAudio"} -> 1 if there was no existing doxygen for this function.
my %headersymstype = ();   # $headersymstype{"SDL_OpenAudio"} -> 1 (function), 2 (macro), 3 (struct), 4 (enum), 5 (other typedef)
my %headersymscategory = ();   # $headersymscategory{"SDL_OpenAudio"} -> 'Audio' ... this is set with a `/* WIKI CATEGEORY: Audio */` comment in the headers that sets it on all symbols until a new comment changes it. So usually, once at the top of the header file.
my %headercategorydocs = ();   # $headercategorydocs{"Audio"} -> (fake) symbol for this category's documentation. Undefined if not documented.
my %headersymsparaminfo = (); # $headersymsparaminfo{"SDL_OpenAudio"} -> reference to array of parameters, pushed by name, then C type string, repeating. Undef'd if void params, or not a function.
my %headersymsrettype = (); # $headersymsrettype{"SDL_OpenAudio"} -> string of C datatype of return value. Undef'd if not a function.
my %wikitypes = ();  # contains string of wiki page extension, like $wikitypes{"SDL_OpenAudio"} == 'mediawiki'
my %wikisyms = ();  # contains references to hash of strings, each string being the full contents of a section of a wiki page, like $wikisyms{"SDL_OpenAudio"}{"Remarks"}.
my %wikisectionorder = ();   # contains references to array, each array item being a key to a wikipage section in the correct order, like $wikisectionorder{"SDL_OpenAudio"}[2] == 'Remarks'

my %referenceonly = ();  # $referenceonly{"Y"} -> symbol name that this symbol is bound to. This makes wiki pages that say "See X" where "X" is a typedef and "Y" is a define attached to it. These pages are generated in the wiki only and do not bridge to the headers or manpages.

my @coverage_gap = ();  # array of strings that weren't part of documentation, or blank, or basic preprocessor logic. Lets you see what this script is missing!

sub add_coverage_gap {
    if ($copy_direction == -3) {  # --report-coverage-gaps
        my $text = shift;
        my $dent = shift;
        my $lineno = shift;
        return if $text =~ /\A\s*\Z/;  # skip blank lines
        return if $text =~ /\A\s*\#\s*(if|el|endif|include)/; # skip preprocessor floof.
        push @coverage_gap, "$dent:$lineno: $text";
    }
}

sub print_undocumented_section {
    my $fh = shift;
    my $typestr = shift;
    my $typeval = shift;

    print $fh "## $typestr defined in the headers, but not in the wiki\n\n";
    my $header_only_sym = 0;
    foreach (sort keys %headersyms) {
        my $sym = $_;
        if ((not defined $wikisyms{$sym}) && ($headersymstype{$sym} == $typeval)) {
            print $fh "- [$sym]($sym)\n";
            $header_only_sym = 1;
        }
    }
    if (!$header_only_sym) {
        print $fh "(none)\n";
    }
    print $fh "\n";

    if (0) {  # !!! FIXME: this lists things that _shouldn't_ be in the headers, like MigrationGuide, etc, but also we don't know if they're functions, macros, etc at this point (can we parse that from the wiki page, though?)
    print $fh "## $typestr defined in the wiki, but not in the headers\n\n";

    my $wiki_only_sym = 0;
    foreach (sort keys %wikisyms) {
        my $sym = $_;
        if ((not defined $headersyms{$sym}) && ($headersymstype{$sym} == $typeval)) {
            print $fh "- [$sym]($sym)\n";
            $wiki_only_sym = 1;
        }
    }
    if (!$wiki_only_sym) {
        print $fh "(none)\n";
    }
    print $fh "\n";
    }
}

sub strip_fn_declaration_metadata {
    my $decl = shift;
    $decl =~ s/SDL_(PRINTF|SCANF)_FORMAT_STRING\s*//;  # don't want this metadata as part of the documentation.
    $decl =~ s/SDL_ALLOC_SIZE2?\(.*?\)\s*//;  # don't want this metadata as part of the documentation.
    $decl =~ s/SDL_MALLOC\s*//;  # don't want this metadata as part of the documentation.
    $decl =~ s/SDL_(IN|OUT|INOUT)_.*?CAP\s*\(.*?\)\s*//g;  # don't want this metadata as part of the documentation.
    $decl =~ s/\)(\s*SDL_[a-zA-Z_]+(\(.*?\)|))*;/);/; # don't want this metadata as part of the documentation.
    return $decl;
}

sub sanitize_c_typename {
    my $str = shift;
    $str =~ s/\A\s+//;
    $str =~ s/\s+\Z//;
    $str =~ s/const\s*(\*+)/const $1/g;  # one space between `const` and pointer stars: `char const* const *` becomes `char const * const *`.
    $str =~ s/\*\s+\*/**/g;  # drop spaces between pointers: `void * *` becomes `void **`.
    $str =~ s/\s*(\*+)\Z/ $1/;  # one space between pointer stars and what it points to: `void**` becomes `void **`.
    return $str;
}

my $incpath = "$srcpath";
$incpath .= "/$incsubdir" if $incsubdir ne '';

my $wikireadmepath = "$wikipath/$wikireadmesubdir";
my $readmepath = undef;
if (defined $readmesubdir) {
    $readmepath = "$srcpath/$readmesubdir";
}

opendir(DH, $incpath) or die("Can't opendir '$incpath': $!\n");
while (my $d = readdir(DH)) {
    my $dent = $d;
    next if not $dent =~ /$selectheaderregex/;  # just selected headers.
    open(FH, '<', "$incpath/$dent") or die("Can't open '$incpath/$dent': $!\n");

    # You can optionally set a wiki category with Perl code in .wikiheaders-options that gets eval()'d per-header,
    # and also if you put `/* WIKI CATEGORY: blah */` on a line by itself, it'll change the category for any symbols
    # below it in the same file. If no category is set, one won't be added for the symbol (beyond the standard CategoryFunction, etc)
    my $current_wiki_category = undef;
    if (defined $headercategoryeval) {
        $_ = $dent;
        $current_wiki_category = eval($headercategoryeval);
        if (($current_wiki_category eq '') || ($current_wiki_category eq '-')) {
            $current_wiki_category = undef;
        }
        #print("CATEGORY FOR '$dent' IS " . (defined($current_wiki_category) ? "'$current_wiki_category'" : '(undef)') . "\n");
    }

    my @contents = ();
    my $ignoring_lines = 0;
    my $header_comment = -1;
    my $saw_category_doxygen = -1;
    my $lineno = 0;
    while (<FH>) {
        chomp;
        $lineno++;
        my $symtype = 0;  # nothing, yet.
        my $decl;
        my @templines;
        my $str;
        my $has_doxygen = 1;

        # Since a lot of macros are just preprocessor logic spam and not all macros are worth documenting anyhow, we only pay attention to them when they have a Doxygen comment attached.
        # Functions and other things are a different story, though!

        if ($header_comment == -1) {
            $header_comment = /\A\/\*\s*\Z/ ? 1 : 0;
        } elsif (($header_comment == 1) && (/\A\*\/\s*\Z/)) {
            $header_comment = 0;
        }

        if ($ignoring_lines && /\A\s*\#\s*endif\s*\Z/) {
            $ignoring_lines = 0;
            push @contents, $_;
            next;
        } elsif ($ignoring_lines) {
            push @contents, $_;
            next;
        } elsif (/\A\s*\#\s*ifndef\s+SDL_WIKI_DOCUMENTATION_SECTION\s*\Z/) {
            $ignoring_lines = 1;
            push @contents, $_;
            next;
        } elsif (/\A\s*\/\*\s*WIKI CATEGORY:\s*(.*?)\s*\*\/\s*\Z/) {
            $current_wiki_category = (($1 eq '') || ($1 eq '-')) ? undef : $1;
            #print("CATEGORY FOR '$dent' CHANGED TO " . (defined($current_wiki_category) ? "'$current_wiki_category'" : '(undef)') . "\n");
            push @contents, $_;
            next;
        } elsif (/\A\s*extern\s+(SDL_DEPRECATED\s+|)(SDLMAIN_|SDL_)?DECLSPEC/) {  # a function declaration without a doxygen comment?
            $symtype = 1;   # function declaration
            @templines = ();
            $decl = $_;
            $str = '';
            $has_doxygen = 0;
        } elsif (/\A\s*SDL_FORCE_INLINE/) {  # a (forced-inline) function declaration without a doxygen comment?
            $symtype = 1;   # function declaration
            @templines = ();
            $decl = $_;
            $str = '';
            $has_doxygen = 0;
        } elsif (not /\A\/\*\*\s*\Z/) {  # not doxygen comment start?
            push @contents, $_;
            add_coverage_gap($_, $dent, $lineno) if ($header_comment == 0);
            next;
        } else {   # Start of a doxygen comment, parse it out.
            my $is_category_doxygen = 0;

            @templines = ( $_ );
            while (<FH>) {
                chomp;
                $lineno++;
                push @templines, $_;
                last if /\A\s*\*\/\Z/;
                if (s/\A\s*\*\s*\`\`\`/```/) {  # this is a hack, but a lot of other code relies on the whitespace being trimmed, but we can't trim it in code blocks...
                    $str .= "$_\n";
                    while (<FH>) {
                        chomp;
                        $lineno++;
                        push @templines, $_;
                        s/\A\s*\*\s?//;
                        if (s/\A\s*\`\`\`/```/) {
                            $str .= "$_\n";
                            last;
                        } else {
                            $str .= "$_\n";
                        }
                    }
                } else {
                    s/\A\s*\*\s*//;   # Strip off the " * " at the start of the comment line.

                    # To add documentation to Category Pages, the rule is it has to
                    # be the first Doxygen comment in the header, and it must start with `# CategoryX`
                    # (otherwise we'll treat it as documentation for whatever's below it). `X` is
                    # the category name, which doesn't _necessarily_ have to match
                    # $current_wiki_category, but it probably should.
                    #
                    # For compatibility with Doxygen, if there's a `\file` here instead of
                    # `# CategoryName`, we'll accept it and use the $current_wiki_category if set.
                    if ($saw_category_doxygen == -1) {
                        $saw_category_doxygen = defined($current_wiki_category) && /\A\\file\s+/;
                        if ($saw_category_doxygen) {
                            $_ = "# Category$current_wiki_category";
                        } else {
                            $saw_category_doxygen = /\A\# Category/;
                        }
                        $is_category_doxygen = $saw_category_doxygen;
                    }

                    $str .= "$_\n";
                }
            }

            if ($is_category_doxygen) {
                $str =~ s/\s*\Z//;
                $decl = '';
                $symtype = -1;  # not a symbol at all.
            } else {
                $decl = <FH>;
                $lineno++ if defined $decl;
                $decl = '' if not defined $decl;
                chomp($decl);
                if ($decl =~ /\A\s*extern\s+(SDL_DEPRECATED\s+|)(SDLMAIN_|SDL_)?DECLSPEC/) {
                    $symtype = 1;   # function declaration
                } elsif ($decl =~ /\A\s*SDL_FORCE_INLINE/) {
                    $symtype = 1;   # (forced-inline) function declaration
                } elsif ($decl =~ /\A\s*\#\s*define\s+/) {
                    $symtype = 2;   # macro
                } elsif ($decl =~ /\A\s*(typedef\s+|)(struct|union)\s*([a-zA-Z0-9_]*?)\s*(\n|\{|\Z)/) {
                    $symtype = 3;   # struct or union
                } elsif ($decl =~ /\A\s*(typedef\s+|)enum\s*([a-zA-Z0-9_]*?)\s*(\n|\{|\Z)/) {
                    $symtype = 4;   # enum
                } elsif ($decl =~ /\A\s*typedef\s+.*\Z/) {
                    $symtype = 5;   # other typedef
                } else {
                    #print "Found doxygen but no function sig:\n$str\n\n";
                    foreach (@templines) {
                        push @contents, $_;
                        add_coverage_gap($_, $dent, $lineno);
                    }
                    push @contents, $decl;
                    add_coverage_gap($decl, $dent, $lineno);
                    next;
                }
            }
        }

        my @paraminfo = ();
        my $rettype = undef;
        my @decllines = ( $decl );
        my $sym = '';

        if ($symtype == -1) {  # category documentation with no symbol attached.
            @decllines = ();
            if ($str =~ /^#\s*Category(.*?)\s*$/m) {
                $sym = "[category documentation] $1";  # make a fake, unique symbol that's not valid C.
            } else {
                die("Unexpected category documentation line '$str' in '$incpath/$dent' ...?");
            }
            $headercategorydocs{$current_wiki_category} = $sym;
        } elsif ($symtype == 1) {  # a function
            my $is_forced_inline = ($decl =~ /\A\s*SDL_FORCE_INLINE/);

            if ($is_forced_inline) {
                if (not $decl =~ /\)\s*(\{.*|)\s*\Z/) {
                    while (<FH>) {
                        chomp;
                        $lineno++;
                        push @decllines, $_;
                        s/\A\s+//;
                        s/\s+\Z//;
                        $decl .= " $_";
                        last if /\)\s*(\{.*|)\s*\Z/;
                    }
                }
                $decl =~ s/\s*\)\s*(\{.*|)\s*\Z/);/;
            } else {
                if (not $decl =~ /;/) {
                    while (<FH>) {
                        chomp;
                        $lineno++;
                        push @decllines, $_;
                        s/\A\s+//;
                        s/\s+\Z//;
                        $decl .= " $_";
                        last if /;/;
                    }
                }
                $decl =~ s/\s+\);\Z/);/;
                $decl =~ s/\s+;\Z/;/;
            }

            $decl =~ s/\s+\Z//;

            $decl = strip_fn_declaration_metadata($decl);

            my $paramsstr = undef;

            if (!$is_forced_inline && $decl =~ /\A\s*extern\s+(SDL_DEPRECATED\s+|)(SDLMAIN_|SDL_)?DECLSPEC\s+(const\s+|)(unsigned\s+|)(.*?)([\*\s]+)(\*?)\s*SDLCALL\s+(.*?)\s*\((.*?)\);/) {
                $sym = $8;
                $rettype = "$3$4$5$6";
                $paramsstr = $9;
             } elsif ($is_forced_inline && $decl =~ /\A\s*SDL_FORCE_INLINE\s+(SDL_DEPRECATED\s+|)(const\s+|)(unsigned\s+|)(.*?)([\*\s]+)(.*?)\s*\((.*?)\);/) {
                $sym = $6;
                $rettype = "$2$3$4$5";
                $paramsstr = $7;
             } else {
                #print "Found doxygen but no function sig:\n$str\n\n";
                foreach (@templines) {
                    push @contents, $_;
                }
                foreach (@decllines) {
                    push @contents, $_;
                }
                next;
            }

            $rettype = sanitize_c_typename($rettype);

            if ($paramsstr =~ /\(/) {
                die("\n\n$0 FAILURE!\n" .
                    "There's a '(' in the parameters for function '$sym' '$incpath/$dent'.\n" .
                    "This usually means there's a parameter that's a function pointer type.\n" .
                    "This causes problems for wikiheaders.pl and is less readable, too.\n" .
                    "Please put that function pointer into a typedef,\n" .
                    "and use the new type in this function's signature instead!\n\n");
            }

            my @params = split(/,/, $paramsstr);
            my $dotdotdot = 0;
            foreach (@params) {
                my $p = $_;
                $p =~ s/\A\s+//;
                $p =~ s/\s+\Z//;
                if (($p eq 'void') || ($p eq '')) {
                    die("Void parameter in a function with multiple params?! ('$sym' in '$incpath/$dent')") if (scalar(@params) != 1);
                } elsif ($p eq '...') {
                    die("Mutiple '...' params?! ('$sym' in '$incpath/$dent')") if ($dotdotdot);
                    $dotdotdot = 1;
                    push @paraminfo, '...';
                    push @paraminfo, '...';
                } elsif ($p =~ /\A(.*)\s+([a-zA-Z0-9_\*\[\]]+)\Z/) {
                    die("Parameter after '...' param?! ('$sym' in '$incpath/$dent')") if ($dotdotdot);
                    my $t = $1;
                    my $n = $2;
                    if ($n =~ s/\A(\*+)//) {
                        $t .= $1;  # move any `*` that stuck to the name over.
                    }
                    if ($n =~ s/\[\]\Z//) {
                        $t = "$t*";  # move any `[]` that stuck to the name over, as a pointer.
                    }
                    $t = sanitize_c_typename($t);
                    #print("$t\n");
                    #print("$n\n");
                    push @paraminfo, $n;
                    push @paraminfo, $t;
                } else {
                    die("Unexpected parameter '$p' in function '$sym' in '$incpath/$dent'!");
                }
            }

            if (!$is_forced_inline) {  # don't do with forced-inline because we don't want the implementation inserted in the wiki.
                $decl = '';  # rebuild this with the line breaks, since it looks better for syntax highlighting.
                foreach (@decllines) {
                    if ($decl eq '') {
                        $decl = $_;
                        $decl =~ s/\Aextern\s+(SDL_DEPRECATED\s+|)(SDLMAIN_|SDL_)?DECLSPEC\s+(.*?)\s+(\*?)SDLCALL\s+/$3$4 /;
                    } else {
                        my $trimmed = $_;
                        # !!! FIXME: trim space for SDL_DEPRECATED if it was used, too.
                        $trimmed =~ s/\A\s{28}//;  # 28 for shrinking to match the removed "extern SDL_DECLSPEC SDLCALL "
                        $decl .= $trimmed;
                    }
                    $decl .= "\n";
                }
            }

            $decl = strip_fn_declaration_metadata($decl);

            # !!! FIXME: code duplication with typedef processing, below.
            # We assume any `#define`s directly after the function are related to it: probably bitflags for an integer typedef.
            # We'll also allow some other basic preprocessor lines.
            # Blank lines are allowed, anything else, even comments, are not.
            my $blank_lines = 0;
            my $lastpos = tell(FH);
            my $lastlineno = $lineno;
            my $additional_decl = '';
            my $saw_define = 0;
            while (<FH>) {
                chomp;

                $lineno++;

                if (/\A\s*\Z/) {
                    $blank_lines++;
                } elsif (/\A\s*\#\s*(define|if|else|elif|endif)(\s+|\Z)/) {
                    if (/\A\s*\#\s*define\s+([a-zA-Z0-9_]*)/) {
                        $referenceonly{$1} = $sym;
                        $saw_define = 1;
                    } elsif (!$saw_define) {
                        # if the first non-blank thing isn't a #define, assume we're done.
                        seek(FH, $lastpos, 0);  # re-read eaten lines again next time.
                        $lineno = $lastlineno;
                        last;
                    }

                    # update strings now that we know everything pending is to be applied to this declaration. Add pending blank lines and the new text.

                    # At Sam's request, don't list property defines with functions. (See #9440)
                    my $is_property = /\A\s*\#\s*define\s+SDL_PROP_/;
                    if (!$is_property) {
                        if ($blank_lines > 0) {
                            while ($blank_lines > 0) {
                                $additional_decl .= "\n";
                                push @decllines, '';
                                $blank_lines--;
                            }
                        }
                        $additional_decl .= "\n$_";
                        push @decllines, $_;
                        $lastpos = tell(FH);
                    }
                } else {
                    seek(FH, $lastpos, 0);  # re-read eaten lines again next time.
                    $lineno = $lastlineno;
                    last;
                }
            }
            $decl .= $additional_decl;

        } elsif ($symtype == 2) {  # a macro
            if ($decl =~ /\A\s*\#\s*define\s+(.*?)(\(.*?\)|)\s+/) {
                $sym = $1;
            } else {
                #print "Found doxygen but no macro:\n$str\n\n";
                foreach (@templines) {
                    push @contents, $_;
                }
                foreach (@decllines) {
                    push @contents, $_;
                }
                next;
            }

            while ($decl =~ /\\\Z/) {
                my $l = <FH>;
                last if not $l;
                $lineno++;
                chomp($l);
                push @decllines, $l;
                #$l =~ s/\A\s+//;
                $l =~ s/\s+\Z//;
                $decl .= "\n$l";
            }
        } elsif (($symtype == 3) || ($symtype == 4)) {  # struct or union or enum
            my $has_definition = 0;
            if ($decl =~ /\A\s*(typedef\s+|)(struct|union|enum)\s*([a-zA-Z0-9_]*?)\s*(\n|\{|\;|\Z)/) {
                my $ctype = $2;
                my $origsym = $3;
                my $ending = $4;
                $sym = $origsym;
                if ($sym =~ s/\A(.*?)(\s+)(.*?)\Z/$1/) {
                    die("Failed to parse '$origsym' correctly!") if ($sym ne $1);  # Thought this was "typedef struct MySym MySym;" ... it was not.  :(  This is a hack!
                }
                if ($sym eq '') {
                    die("\n\n$0 FAILURE!\n" .
                        "There's a 'typedef $ctype' in $incpath/$dent without a name at the top.\n" .
                        "Instead of `typedef $ctype {} x;`, this should be `typedef $ctype x {} x;`.\n" .
                        "This causes problems for wikiheaders.pl and scripting language bindings.\n" .
                        "Please fix it!\n\n");
                }
                $has_definition = ($ending ne ';');
            } else {
                #print "Found doxygen but no datatype:\n$str\n\n";
                foreach (@templines) {
                    push @contents, $_;
                }
                foreach (@decllines) {
                    push @contents, $_;
                }
                next;
            }

            # This block attempts to find the whole struct/union/enum definition by counting matching brackets. Kind of yucky.
            if ($has_definition) {
                my $started = 0;
                my $brackets = 0;
                my $pending = $decl;

                $decl = '';
                while (!$started || ($brackets != 0)) {
                    foreach my $seg (split(/([{}])/, $pending)) {
                        $decl .= $seg;
                        if ($seg eq '{') {
                            $started = 1;
                            $brackets++;
                        } elsif ($seg eq '}') {
                            die("Something is wrong with header $incpath/$dent while parsing $sym; is a bracket missing?\n") if ($brackets <= 0);
                            $brackets--;
                        }
                    }

                    if (!$started || ($brackets != 0)) {
                        $pending = <FH>;
                        die("EOF/error reading $incpath/$dent while parsing $sym\n") if not $pending;
                        $lineno++;
                        chomp($pending);
                        push @decllines, $pending;
                        $decl .= "\n";
                    }
                }
                # this currently assumes the struct/union/enum ends on the line with the final bracket. I'm not writing a C parser here, fix the header!
            }
        } elsif ($symtype == 5) {  # other typedef
            if ($decl =~ /\A\s*typedef\s+(.*)\Z/) {
                my $tdstr = $1;

                if (not $decl =~ /;/) {
                    while (<FH>) {
                        chomp;
                        $lineno++;
                        push @decllines, $_;
                        s/\A\s+//;
                        s/\s+\Z//;
                        $decl .= " $_";
                        last if /;/;
                    }
                }
                $decl =~ s/\s+(\))?;\Z/$1;/;

                $tdstr =~ s/;\s*\Z//;

                #my $datatype;
                if ($tdstr =~ /\A(.*?)\s*\((.*?)\s*\*\s*(.*?)\)\s*\((.*?)(\))?/) {  # a function pointer type
                    $sym = $3;
                    #$datatype = "$1 ($2 *$sym)($4)";
                } elsif ($tdstr =~ /\A(.*[\s\*]+)(.*?)\s*\Z/) {
                    $sym = $2;
                    #$datatype = $1;
                } else {
                    die("Failed to parse typedef '$tdstr' in $incpath/$dent!\n");  # I'm hitting a C grammar nail with a regexp hammer here, y'all.
                }

                $sym =~ s/\A\s+//;
                $sym =~ s/\s+\Z//;
                #$datatype =~ s/\A\s+//;
                #$datatype =~ s/\s+\Z//;
            } else {
                #print "Found doxygen but no datatype:\n$str\n\n";
                foreach (@templines) {
                    push @contents, $_;
                }
                foreach (@decllines) {
                    push @contents, $_;
                }
                next;
            }

            # We assume any `#define`s directly after the typedef are related to it: probably bitflags for an integer typedef.
            # We'll also allow some other basic preprocessor lines.
            # Blank lines are allowed, anything else, even comments, are not.
            my $blank_lines = 0;
            my $lastpos = tell(FH);
            my $lastlineno = $lineno;
            my $additional_decl = '';
            my $saw_define = 0;
            while (<FH>) {
                chomp;

                $lineno++;

                if (/\A\s*\Z/) {
                    $blank_lines++;
                } elsif (/\A\s*\#\s*(define|if|else|elif|endif)(\s+|\Z)/) {
                    if (/\A\s*\#\s*define\s+([a-zA-Z0-9_]*)/) {
                        $referenceonly{$1} = $sym;
                        $saw_define = 1;
                    } elsif (!$saw_define) {
                        # if the first non-blank thing isn't a #define, assume we're done.
                        seek(FH, $lastpos, 0);  # re-read eaten lines again next time.
                        $lineno = $lastlineno;
                        last;
                    }
                    # update strings now that we know everything pending is to be applied to this declaration. Add pending blank lines and the new text.
                    if ($blank_lines > 0) {
                        while ($blank_lines > 0) {
                            $additional_decl .= "\n";
                            push @decllines, '';
                            $blank_lines--;
                        }
                    }
                    $additional_decl .= "\n$_";
                    push @decllines, $_;
                    $lastpos = tell(FH);
                } else {
                    seek(FH, $lastpos, 0);  # re-read eaten lines again next time.
                    $lineno = $lastlineno;
                    last;
                }
            }
            $decl .= $additional_decl;
        } else {
            die("Unexpected symtype $symtype");
        }

        #print("DECL: [$decl]\n");

        #print("$sym:\n$str\n\n");

        # There might be multiple declarations of a function due to #ifdefs,
        #  and only one of them will have documentation. If we hit an
        #  undocumented one before, delete the placeholder line we left for
        #  it so it doesn't accumulate a new blank line on each run.
        my $skipsym = 0;
        if (defined $headersymshasdoxygen{$sym}) {
            if ($headersymshasdoxygen{$sym} == 0) {  # An undocumented declaration already exists, nuke its placeholder line.
                delete $contents[$headersymschunk{$sym}];  # delete DOES NOT RENUMBER existing elements!
            } else {  # documented function already existed?
                $skipsym = 1;  # don't add this copy to the list of functions.
                if ($has_doxygen) {
                    print STDERR "WARNING: Symbol '$sym' appears to be documented in multiple locations. Only keeping the first one we saw!\n";
                }
                push @contents, join("\n", @decllines) if (scalar(@decllines) > 0);  # just put the existing declation in as-is.
            }
        }

        if (!$skipsym) {
            $headersymscategory{$sym} = $current_wiki_category if defined $current_wiki_category;
            $headersyms{$sym} = $str;
            $headerdecls{$sym} = $decl;
            $headersymslocation{$sym} = $dent;
            $headersymschunk{$sym} = scalar(@contents);
            $headersymshasdoxygen{$sym} = $has_doxygen;
            $headersymstype{$sym} = $symtype;
            $headersymsparaminfo{$sym} = \@paraminfo if (scalar(@paraminfo) > 0);
            $headersymsrettype{$sym} = $rettype if (defined($rettype));
            push @contents, join("\n", @templines);
            push @contents, join("\n", @decllines) if (scalar(@decllines) > 0);
        }

    }
    close(FH);

    $headers{$dent} = \@contents;
}
closedir(DH);


opendir(DH, $wikipath) or die("Can't opendir '$wikipath': $!\n");
while (my $d = readdir(DH)) {
    my $dent = $d;
    my $type = '';
    if ($dent =~ /\.(md|mediawiki)\Z/) {
        $type = $1;
    } else {
        next;  # only dealing with wiki pages.
    }

    my $sym = $dent;
    $sym =~ s/\..*\Z//;

    # (There are other pages to ignore, but these are known ones to not bother parsing.)
    # Ignore FrontPage.
    next if $sym eq 'FrontPage';

    open(FH, '<', "$wikipath/$dent") or die("Can't open '$wikipath/$dent': $!\n");

    if ($sym =~ /\ACategory(.*?)\Z/) {  # Special case for Category pages.
        # Find the end of the category documentation in the existing file and append everything else to the new file.
        my $cat = $1;
        my $docstr = '';
        my $notdocstr = '';
        my $docs = 1;
        while (<FH>) {
            chomp;
            if ($docs) {
                $docs = 0 if /\A\-\-\-\-\Z/;  # Hit a footer? We're done.
                $docs = 0 if /\A<!\-\-/;  # Hit an HTML comment? We're done.
            }
            if ($docs) {
                $docstr .= "$_\n";
            } else {
                $notdocstr .= "$_\n";
            }
        }
        close(FH);

        $docstr =~ s/\s*\Z//;

        $sym = "[category documentation] $cat";  # make a fake, unique symbol that's not valid C.
        $wikitypes{$sym} = $type;
        my %sections = ();
        $sections{'Remarks'} = $docstr;
        $sections{'[footer]'} = $notdocstr;
        $wikisyms{$sym} = \%sections;
        my @section_order = ( 'Remarks', '[footer]' );
        $wikisectionorder{$sym} = \@section_order;
        next;
    }

    my $current_section = '[start]';
    my @section_order = ( $current_section );
    my %sections = ();
    $sections{$current_section} = '';

    my $firstline = 1;

    while (<FH>) {
        chomp;
        my $orig = $_;
        s/\A\s*//;
        s/\s*\Z//;

        if ($type eq 'mediawiki') {
            if (defined($wikipreamble) && $firstline && /\A\=\=\=\=\=\= (.*?) \=\=\=\=\=\=\Z/ && ($1 eq $wikipreamble)) {
                $firstline = 0;  # skip this.
                next;
            } elsif (/\A\= (.*?) \=\Z/) {
                $firstline = 0;
                $current_section = ($1 eq $sym) ? '[Brief]' : $1;
                die("Doubly-defined section '$current_section' in '$dent'!\n") if defined $sections{$current_section};
                push @section_order, $current_section;
                $sections{$current_section} = '';
            } elsif (/\A\=\= (.*?) \=\=\Z/) {
                $firstline = 0;
                $current_section = ($1 eq $sym) ? '[Brief]' : $1;
                die("Doubly-defined section '$current_section' in '$dent'!\n") if defined $sections{$current_section};
                push @section_order, $current_section;
                $sections{$current_section} = '';
                next;
            } elsif (/\A\-\-\-\-\Z/) {
                $firstline = 0;
                $current_section = '[footer]';
                die("Doubly-defined section '$current_section' in '$dent'!\n") if defined $sections{$current_section};
                push @section_order, $current_section;
                $sections{$current_section} = '';
                next;
            }
        } elsif ($type eq 'md') {
            if (defined($wikipreamble) && $firstline && /\A\#\#\#\#\#\# (.*?)\Z/ && ($1 eq $wikipreamble)) {
                $firstline = 0;  # skip this.
                next;
            } elsif (/\A\#+ (.*?)\Z/) {
                $firstline = 0;
                $current_section = ($1 eq $sym) ? '[Brief]' : $1;
                die("Doubly-defined section '$current_section' in '$dent'!\n") if defined $sections{$current_section};
                push @section_order, $current_section;
                $sections{$current_section} = '';
                next;
            } elsif (/\A\-\-\-\-\Z/) {
                $firstline = 0;
                $current_section = '[footer]';
                die("Doubly-defined section '$current_section' in '$dent'!\n") if defined $sections{$current_section};
                push @section_order, $current_section;
                $sections{$current_section} = '';
                next;
            }
        } else {
            die("Unexpected wiki file type. Fixme!");
        }

        if ($firstline) {
            $firstline = ($_ ne '');
        }
        if (!$firstline) {
            $sections{$current_section} .= "$orig\n";
        }
    }
    close(FH);

    foreach (keys %sections) {
        $sections{$_} =~ s/\A\n+//;
        $sections{$_} =~ s/\n+\Z//;
        $sections{$_} .= "\n";
    }

    # older section name we used, migrate over from it.
    if (defined $sections{'Related Functions'}) {
        if (not defined $sections{'See Also'}) {
            $sections{'See Also'} = $sections{'Related Functions'};
        }
        delete $sections{'Related Functions'};
    }

    if (0) {
        foreach (@section_order) {
            print("$sym SECTION '$_':\n");
            print($sections{$_});
            print("\n\n");
        }
    }

    $wikitypes{$sym} = $type;
    $wikisyms{$sym} = \%sections;
    $wikisectionorder{$sym} = \@section_order;
}
closedir(DH);

delete $wikisyms{"Undocumented"};

{
    my $path = "$wikipath/Undocumented.md";
    open(my $fh, '>', $path) or die("Can't open '$path': $!\n");

    print $fh "# Undocumented\n\n";
    print_undocumented_section($fh, 'Functions', 1);
    #print_undocumented_section($fh, 'Macros', 2);

    close($fh);
}

if ($warn_about_missing) {
    foreach (keys %wikisyms) {
        my $sym = $_;
        if (not defined $headersyms{$sym}) {
            print STDERR "WARNING: $sym defined in the wiki but not the headers!\n";
        }
    }

    foreach (keys %headersyms) {
        my $sym = $_;
        if (not defined $wikisyms{$sym}) {
            print STDERR "WARNING: $sym defined in the headers but not the wiki!\n";
        }
    }
}

if ($copy_direction == 1) {  # --copy-to-headers
    my %changed_headers = ();

    $dewikify_mode = 'md';
    $wordwrap_mode = 'md';   # the headers use Markdown format.

    foreach (keys %headersyms) {
        my $sym = $_;
        next if not defined $wikisyms{$sym};  # don't have a page for that function, skip it.
        my $symtype = $headersymstype{$sym};
        my $wikitype = $wikitypes{$sym};
        my $sectionsref = $wikisyms{$sym};
        my $remarks = $sectionsref->{'Remarks'};
        my $returns = $sectionsref->{'Return Value'};
        my $threadsafety = $sectionsref->{'Thread Safety'};
        my $version = $sectionsref->{'Version'};
        my $related = $sectionsref->{'See Also'};
        my $deprecated = $sectionsref->{'Deprecated'};
        my $brief = $sectionsref->{'[Brief]'};
        my $addblank = 0;
        my $str = '';

        my $params = undef;
        my $paramstr = undef;

        if ($symtype == -1) {  # category documentation block.
            # nothing to be done here.
        } elsif (($symtype == 1) || (($symtype == 5))) {  # we'll assume a typedef (5) with a \param is a function pointer typedef.
            $params = $sectionsref->{'Function Parameters'};
            $paramstr = '\param';
        } elsif ($symtype == 2) {
            $params = $sectionsref->{'Macro Parameters'};
            $paramstr = '\param';
        } elsif ($symtype == 3) {
            $params = $sectionsref->{'Fields'};
            $paramstr = '\field';
        } elsif ($symtype == 4) {
            $params = $sectionsref->{'Values'};
            $paramstr = '\value';
        } else {
            die("Unexpected symtype $symtype");
        }

        $headersymshasdoxygen{$sym} = 1;  # Added/changed doxygen for this header.

        $brief = dewikify($wikitype, $brief);
        $brief =~ s/\A(.*?\.) /$1\n/;  # \brief should only be one sentence, delimited by a period+space. Split if necessary.
        my @briefsplit = split /\n/, $brief;
        $brief = shift @briefsplit;

        if (defined $remarks) {
            $remarks = join("\n", @briefsplit) . dewikify($wikitype, $remarks);
        }

        if (defined $brief) {
            $str .= "\n" if $addblank; $addblank = 1;
            $str .= wordwrap($brief) . "\n";
        }

        if (defined $remarks) {
            $str .= "\n" if $addblank; $addblank = 1;
            $str .= wordwrap($remarks) . "\n";
        }

        if (defined $deprecated) {
            # !!! FIXME: lots of code duplication in all of these.
            $str .= "\n" if $addblank; $addblank = 1;
            my $v = dewikify($wikitype, $deprecated);
            my $whitespacelen = length("\\deprecated") + 1;
            my $whitespace = ' ' x $whitespacelen;
            $v = wordwrap($v, -$whitespacelen);
            my @desclines = split /\n/, $v;
            my $firstline = shift @desclines;
            $str .= "\\deprecated $firstline\n";
            foreach (@desclines) {
                $str .= "${whitespace}$_\n";
            }
        }

        if (defined $params) {
            $str .= "\n" if $addblank; $addblank = (defined $returns) ? 0 : 1;
            my @lines = split /\n/, dewikify($wikitype, $params);
            if ($wikitype eq 'mediawiki') {
                die("Unexpected data parsing MediaWiki table") if (shift @lines ne '{|');  # Dump the '{|' start
                while (scalar(@lines) >= 3) {
                    my $c_datatype = shift @lines;
                    my $name = shift @lines;
                    my $desc = shift @lines;
                    my $terminator;  # the '|-' or '|}' line.

                    if (($desc eq '|-') or ($desc eq '|}') or (not $desc =~ /\A\|/)) {  # we seem to be out of cells, which means there was no datatype column on this one.
                        $terminator = $desc;
                        $desc = $name;
                        $name = $c_datatype;
                        $c_datatype = '';
                    } else {
                        $terminator = shift @lines;
                    }

                    last if ($terminator ne '|-') and ($terminator ne '|}');  # we seem to have run out of table.
                    $name =~ s/\A\|\s*//;
                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    $desc =~ s/\A\|\s*//;
                    #print STDERR "SYM: $sym   CDATATYPE: $c_datatype  NAME: $name   DESC: $desc TERM: $terminator\n";
                    my $whitespacelen = length($name) + 8;
                    my $whitespace = ' ' x $whitespacelen;
                    $desc = wordwrap($desc, -$whitespacelen);
                    my @desclines = split /\n/, $desc;
                    my $firstline = shift @desclines;
                    $str .= "$paramstr $name $firstline\n";
                    foreach (@desclines) {
                        $str .= "${whitespace}$_\n";
                    }
                }
            } elsif ($wikitype eq 'md') {
                my $l;
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A(\s*\|)?\s*\|\s*\|\s*\|\s*\Z/);
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A\s*(\|\s*\-*\s*)?\|\s*\-*\s*\|\s*\-*\s*\|\s*\Z/);
                while (scalar(@lines) >= 1) {
                    $l = shift @lines;
                    my $name;
                    my $desc;
                    if ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        # c datatype is $1, but we don't care about it here.
                        $name = $2;
                        $desc = $3;
                    } elsif ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        $name = $1;
                        $desc = $2;
                    } else {
                        last;  # we seem to have run out of table.
                    }

                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    #print STDERR "SYM: $sym   NAME: $name   DESC: $desc\n";
                    my $whitespacelen = length($name) + 8;
                    my $whitespace = ' ' x $whitespacelen;
                    $desc = wordwrap($desc, -$whitespacelen);
                    my @desclines = split /\n/, $desc;
                    my $firstline = shift @desclines;
                    $str .= "$paramstr $name $firstline\n";
                    foreach (@desclines) {
                        $str .= "${whitespace}$_\n";
                    }
                }
            } else {
                die("write me");
            }
        }

        if (defined $returns) {
            $str .= "\n" if $addblank; $addblank = 1;
            my $r = dewikify($wikitype, $returns);
            $r =~ s/\A\(.*?\)\s*//;  # Chop datatype in parentheses off the front.
            my $retstr = "\\returns";
            if ($r =~ s/\AReturn(s?)\s+//) {
                $retstr = "\\return$1";
            }

            my $whitespacelen = length($retstr) + 1;
            my $whitespace = ' ' x $whitespacelen;
            $r = wordwrap($r, -$whitespacelen);
            my @desclines = split /\n/, $r;
            my $firstline = shift @desclines;
            $str .= "$retstr $firstline\n";
            foreach (@desclines) {
                $str .= "${whitespace}$_\n";
            }
        }

        if (defined $threadsafety) {
            # !!! FIXME: lots of code duplication in all of these.
            $str .= "\n" if $addblank; $addblank = 1;
            my $v = dewikify($wikitype, $threadsafety);
            my $whitespacelen = length("\\threadsafety") + 1;
            my $whitespace = ' ' x $whitespacelen;
            $v = wordwrap($v, -$whitespacelen);
            my @desclines = split /\n/, $v;
            my $firstline = shift @desclines;
            $str .= "\\threadsafety $firstline\n";
            foreach (@desclines) {
                $str .= "${whitespace}$_\n";
            }
        }

        if (defined $version) {
            # !!! FIXME: lots of code duplication in all of these.
            $str .= "\n" if $addblank; $addblank = 1;
            my $v = dewikify($wikitype, $version);
            my $whitespacelen = length("\\since") + 1;
            my $whitespace = ' ' x $whitespacelen;
            $v = wordwrap($v, -$whitespacelen);
            my @desclines = split /\n/, $v;
            my $firstline = shift @desclines;
            $str .= "\\since $firstline\n";
            foreach (@desclines) {
                $str .= "${whitespace}$_\n";
            }
        }

        if (defined $related) {
            # !!! FIXME: lots of code duplication in all of these.
            $str .= "\n" if $addblank; $addblank = 1;
            my $v = dewikify($wikitype, $related);
            my @desclines = split /\n/, $v;
            foreach (@desclines) {
                s/\(\)\Z//;  # Convert "SDL_Func()" to "SDL_Func"
                s/\[\[(.*?)\]\]/$1/;  # in case some wikilinks remain.
                s/\[(.*?)\]\(.*?\)/$1/;  # in case some wikilinks remain.
                s/\A\/*//;
                s/\A\s*[\:\*\-]\s*//;
                s/\A\s+//;
                s/\s+\Z//;
                $str .= "\\sa $_\n";
            }
        }

        my $header = $headersymslocation{$sym};
        my $contentsref = $headers{$header};
        my $chunk = $headersymschunk{$sym};

        my @lines = split /\n/, $str;

        my $addnewline = (($chunk > 0) && ($$contentsref[$chunk-1] ne '')) ? "\n" : '';

        my $output = "$addnewline/**\n";
        foreach (@lines) {
            chomp;
            s/\s*\Z//;
            if ($_ eq '') {
                $output .= " *\n";
            } else {
                $output .= " * $_\n";
            }
        }
        $output .= " */";

        #print("$sym:\n[$output]\n\n");

        $$contentsref[$chunk] = $output;
        #$$contentsref[$chunk+1] = $headerdecls{$sym};

        $changed_headers{$header} = 1;
    }

    foreach (keys %changed_headers) {
        my $header = $_;

        # this is kinda inefficient, but oh well.
        my @removelines = ();
        foreach (keys %headersymslocation) {
            my $sym = $_;
            next if $headersymshasdoxygen{$sym};
            next if $headersymslocation{$sym} ne $header;
            # the index of the blank line we put before the function declaration in case we needed to replace it with new content from the wiki.
            push @removelines, $headersymschunk{$sym};
        }

        my $contentsref = $headers{$header};
        foreach (@removelines) {
            delete $$contentsref[$_];  # delete DOES NOT RENUMBER existing elements!
        }

        my $path = "$incpath/$header.tmp";
        open(FH, '>', $path) or die("Can't open '$path': $!\n");
        foreach (@$contentsref) {
            print FH "$_\n" if defined $_;
        }
        close(FH);
        rename($path, "$incpath/$header") or die("Can't rename '$path' to '$incpath/$header': $!\n");
    }

    if (defined $readmepath) {
        if ( -d $wikireadmepath ) {
            mkdir($readmepath);  # just in case
            opendir(DH, $wikireadmepath) or die("Can't opendir '$wikireadmepath': $!\n");
            while (readdir(DH)) {
                my $dent = $_;
                if ($dent =~ /\A(.*?)\.md\Z/) {  # we only bridge Markdown files here.
                    next if $1 eq 'FrontPage';
                    filecopy("$wikireadmepath/$dent", "$readmepath/README-$dent", "\n");
                }
            }
            closedir(DH);
        }
    }

} elsif ($copy_direction == -1) { # --copy-to-wiki

    if (defined $changeformat) {
        $dewikify_mode = $changeformat;
        $wordwrap_mode = $changeformat;
    }

    foreach (keys %headersyms) {
        my $sym = $_;
        next if not $headersymshasdoxygen{$sym};
        next if $sym =~ /\A\[category documentation\]/;   # not real symbols, we handle this elsewhere.
        my $symtype = $headersymstype{$sym};
        my $origwikitype = defined $wikitypes{$sym} ? $wikitypes{$sym} : 'md';  # default to MarkDown for new stuff.
        my $wikitype = (defined $changeformat) ? $changeformat : $origwikitype;
        die("Unexpected wikitype '$wikitype'") if (($wikitype ne 'mediawiki') and ($wikitype ne 'md') and ($wikitype ne 'manpage'));

        #print("$sym\n"); next;

        $wordwrap_mode = $wikitype;

        my $raw = $headersyms{$sym};  # raw doxygen text with comment characters stripped from start/end and start of each line.
        next if not defined $raw;
        $raw =~ s/\A\s*\\brief\s+//;  # Technically we don't need \brief (please turn on JAVADOC_AUTOBRIEF if you use Doxygen), so just in case one is present, strip it.

        my @doxygenlines = split /\n/, $raw;
        my $brief = '';
        while (@doxygenlines) {
            last if $doxygenlines[0] =~ /\A\\/;  # some sort of doxygen command, assume we're past the general remarks.
            last if $doxygenlines[0] =~ /\A\s*\Z/;  # blank line? End of paragraph, done.
            my $l = shift @doxygenlines;
            chomp($l);
            $l =~ s/\A\s*//;
            $l =~ s/\s*\Z//;
            $brief .= "$l ";
        }

        $brief =~ s/\s+\Z//;
        $brief =~ s/\A(.*?\.) /$1\n\n/;  # \brief should only be one sentence, delimited by a period+space. Split if necessary.
        my @briefsplit = split /\n/, $brief;

        next if not defined $briefsplit[0];  # No brief text? Probably a bogus Doxygen comment, skip it.

        $brief = wikify($wikitype, shift @briefsplit) . "\n";
        @doxygenlines = (@briefsplit, @doxygenlines);

        my $remarks = '';
        while (@doxygenlines) {
            last if $doxygenlines[0] =~ /\A\\/;  # some sort of doxygen command, assume we're past the general remarks.
            my $l = shift @doxygenlines;
            $remarks .= "$l\n";
        }

        #print("REMARKS:\n\n $remarks\n\n");

        $remarks = wordwrap(wikify($wikitype, $remarks));
        $remarks =~ s/\A\s*//;
        $remarks =~ s/\s*\Z//;

        my $decl = $headerdecls{$sym};

        my $syntax = '';
        if ($wikitype eq 'mediawiki') {
            $syntax = "<syntaxhighlight lang='c'>\n$decl</syntaxhighlight>\n";
        } elsif ($wikitype eq 'md') {
            $decl =~ s/\n+\Z//;
            $syntax = "```c\n$decl\n```\n";
        } else { die("Expected wikitype '$wikitype'"); }

        my %sections = ();
        $sections{'[Brief]'} = $brief;  # include this section even if blank so we get a title line.
        $sections{'Remarks'} = "$remarks\n" if $remarks ne '';
        $sections{'Syntax'} = $syntax;

        my %params = ();  # have to parse these and build up the wiki tables after, since Markdown needs to know the length of the largest string.  :/
        my @paramsorder = ();
        my $fnsigparams = $headersymsparaminfo{$sym};
        my $has_returns = 0;

        while (@doxygenlines) {
            my $l = shift @doxygenlines;
            # We allow param/field/value interchangeably, even if it doesn't make sense. The next --copy-to-headers will correct it anyhow.
            if ($l =~ /\A\\(param|field|value)\s+(.*?)\s+(.*)\Z/) {
                my $arg = $2;
                my $desc = $3;
                while (@doxygenlines) {
                    my $subline = $doxygenlines[0];
                    $subline =~ s/\A\s*//;
                    last if $subline =~ /\A\\/;  # some sort of doxygen command, assume we're past this thing.
                    shift @doxygenlines;  # dump this line from the array; we're using it.
                    if ($subline eq '') {  # empty line, make sure it keeps the newline char.
                        $desc .= "\n";
                    } else {
                        $desc .= " $subline";
                    }
                }

                $desc =~ s/[\s\n]+\Z//ms;

                if (0) {  # !!! FIXME: disabled because it's not currently suitable for general use, but for manually inspecting the output, it can be useful.
                    if (($desc =~ /\A[A-Z]/) && (not $desc =~ /\ASDL_/)) {
                        print STDERR "WARNING: $sym\'s '\\param $arg' text starts with a capital letter: '$desc'. Fixing.\n";
                        $desc = lcfirst($desc);
                    }
                }

                if (not $desc =~ /[\.\!]\Z/) {
                    print STDERR "WARNING: $sym\'s '\\param $arg' text doesn't end with punctuation: '$desc'. Fixing.\n";
                    $desc .= '.';
                }

                # Validate this param.
                if (defined($params{$arg})) {
                    print STDERR "WARNING: Symbol '$sym' has multiple '\\param $arg' declarations! Only keeping the first one!\n";
                } elsif (defined $fnsigparams) {
                    my $found = 0;
                    for (my $i = 0; $i < scalar(@$fnsigparams); $i += 2) {
                        $found = 1, last if (@$fnsigparams[$i] eq $arg);
                    }
                    if (!$found) {
                        print STDERR "WARNING: Symbol '$sym' has a '\\param $arg' for a param that doesn't exist. It will be removed!\n";
                    }
                }

                # We need to know the length of the longest string to make Markdown tables, so we just store these off until everything is parsed.
                $params{$arg} = $desc;
                push @paramsorder, $arg;
            } elsif ($l =~ /\A\\r(eturns?)\s+(.*)\Z/) {
                $has_returns = 1;
                # !!! FIXME: complain if this isn't a function or macro.
                my $retstr = "R$1";  # "Return" or "Returns"
                my $desc = $2;

                while (@doxygenlines) {
                    my $subline = $doxygenlines[0];
                    $subline =~ s/\A\s*//;
                    last if $subline =~ /\A\\/;  # some sort of doxygen command, assume we're past this thing.
                    shift @doxygenlines;  # dump this line from the array; we're using it.
                    if ($subline eq '') {  # empty line, make sure it keeps the newline char.
                        $desc .= "\n";
                    } else {
                        $desc .= " $subline";
                    }
                }
                $desc =~ s/[\s\n]+\Z//ms;

                if (0) {  # !!! FIXME: disabled because it's not currently suitable for general use, but for manually inspecting the output, it can be useful.
                    if (($desc =~ /\A[A-Z]/) && (not $desc =~ /\ASDL_/)) {
                        print STDERR "WARNING: $sym\'s '\\returns' text starts with a capital letter: '$desc'. Fixing.\n";
                        $desc = lcfirst($desc);
                    }
                }

                if (not $desc =~ /[\.\!]\Z/) {
                    print STDERR "WARNING: $sym\'s '\\returns' text doesn't end with punctuation: '$desc'. Fixing.\n";
                    $desc .= '.';
                }

                # Make sure the \returns info is valid.
                my $rettype = $headersymsrettype{$sym};
                die("Don't have a rettype for '$sym' for some reason!") if (($symtype == 1) && (not defined($rettype)));
                if (defined($sections{'Return Value'})) {
                    print STDERR "WARNING: Symbol '$sym' has multiple '\\return' declarations! Only keeping the first one!\n";
                } elsif (($symtype != 1) && ($symtype != 2) && ($symtype != 5)) {  # !!! FIXME: if 5, make sure it's a function pointer typedef!
                    print STDERR "WARNING: Symbol '$sym' has a '\\return' declaration but isn't a function or macro! Removing it!\n";
                } elsif (($symtype == 1) && ($headersymsrettype{$sym} eq 'void')) {
                    print STDERR "WARNING: Function '$sym' has a '\\returns' declaration but function returns void! Removing it!\n";
                } else {
                    my $rettypestr = defined($rettype) ? ('(' . wikify($wikitype, $rettype) . ') ') : '';
                    $sections{'Return Value'} = wordwrap("$rettypestr$retstr ". wikify($wikitype, $desc)) . "\n";
                }
            } elsif ($l =~ /\A\\deprecated\s+(.*)\Z/) {
                my $desc = $1;
                while (@doxygenlines) {
                    my $subline = $doxygenlines[0];
                    $subline =~ s/\A\s*//;
                    last if $subline =~ /\A\\/;  # some sort of doxygen command, assume we're past this thing.
                    shift @doxygenlines;  # dump this line from the array; we're using it.
                    if ($subline eq '') {  # empty line, make sure it keeps the newline char.
                        $desc .= "\n";
                    } else {
                        $desc .= " $subline";
                    }
                }
                $desc =~ s/[\s\n]+\Z//ms;
                $sections{'Deprecated'} = wordwrap(wikify($wikitype, $desc)) . "\n";
            } elsif ($l =~ /\A\\since\s+(.*)\Z/) {
                my $desc = $1;
                while (@doxygenlines) {
                    my $subline = $doxygenlines[0];
                    $subline =~ s/\A\s*//;
                    last if $subline =~ /\A\\/;  # some sort of doxygen command, assume we're past this thing.
                    shift @doxygenlines;  # dump this line from the array; we're using it.
                    if ($subline eq '') {  # empty line, make sure it keeps the newline char.
                        $desc .= "\n";
                    } else {
                        $desc .= " $subline";
                    }
                }
                $desc =~ s/[\s\n]+\Z//ms;
                $sections{'Version'} = wordwrap(wikify($wikitype, $desc)) . "\n";
            } elsif ($l =~ /\A\\threadsafety\s+(.*)\Z/) {
                my $desc = $1;
                while (@doxygenlines) {
                    my $subline = $doxygenlines[0];
                    $subline =~ s/\A\s*//;
                    last if $subline =~ /\A\\/;  # some sort of doxygen command, assume we're past this thing.
                    shift @doxygenlines;  # dump this line from the array; we're using it.
                    if ($subline eq '') {  # empty line, make sure it keeps the newline char.
                        $desc .= "\n";
                    } else {
                        $desc .= " $subline";
                    }
                }
                $desc =~ s/[\s\n]+\Z//ms;
                $sections{'Thread Safety'} = wordwrap(wikify($wikitype, $desc)) . "\n";
            } elsif ($l =~ /\A\\sa\s+(.*)\Z/) {
                my $sa = $1;
                $sa =~ s/\(\)\Z//;  # Convert "SDL_Func()" to "SDL_Func"
                $sections{'See Also'} = '' if not defined $sections{'See Also'};
                if ($wikitype eq 'mediawiki') {
                    $sections{'See Also'} .= ":[[$sa]]\n";
                } elsif ($wikitype eq 'md') {
                    $sections{'See Also'} .= "- [$sa]($sa)\n";
                } else { die("Expected wikitype '$wikitype'"); }
            }
        }

        if (($symtype == 1) && ($headersymsrettype{$sym} ne 'void') && !$has_returns) {
            print STDERR "WARNING: Function '$sym' has a non-void return type but no '\\returns' declaration\n";
        }

        # Make sure %params is in the same order as the actual function signature and add C datatypes...
        my $params_has_c_datatype = 0;
        my @final_params = ();
        if (($symtype == 1) && (defined($headersymsparaminfo{$sym}))) {  # is a function and we have param info for it...
            my $fnsigparams = $headersymsparaminfo{$sym};
            for (my $i = 0; $i < scalar(@$fnsigparams); $i += 2) {
                my $paramname = @$fnsigparams[$i];
                my $paramdesc = $params{$paramname};
                if (defined($paramdesc)) {
                    push @final_params, $paramname;             # name
                    push @final_params, @$fnsigparams[$i+1];    # C datatype
                    push @final_params, $paramdesc;             # description
                    $params_has_c_datatype = 1 if (defined(@$fnsigparams[$i+1]));
                } else {
                    print STDERR "WARNING: Symbol '$sym' is missing a '\\param $paramname' declaration!\n";
                }
            }
        } else {
            foreach (@paramsorder) {
                my $paramname = $_;
                my $paramdesc = $params{$paramname};
                if (defined($paramdesc)) {
                    push @final_params, $_;
                    push @final_params, undef;
                    push @final_params, $paramdesc;
                }
            }
        }

        my $hfiletext = $wikiheaderfiletext;
        $hfiletext =~ s/\%fname\%/$headersymslocation{$sym}/g;
        $sections{'Header File'} = "$hfiletext\n";

        # Make sure this ends with a double-newline.
        $sections{'See Also'} .= "\n" if defined $sections{'See Also'};

        if (0) {  # !!! FIXME: this was a useful hack, but this needs to be generalized if we're going to do this always.
            # Plug in a \since section if one wasn't listed.
            if (not defined $sections{'Version'}) {
                my $symtypename;
                if ($symtype == 1) {
                    $symtypename = 'function';
                } elsif ($symtype == 2) {
                    $symtypename = 'macro';
                } elsif ($symtype == 3) {
                    $symtypename = 'struct';
                } elsif ($symtype == 4) {
                    $symtypename = 'enum';
                } elsif ($symtype == 5) {
                    $symtypename = 'datatype';
                } else {
                    die("Unexpected symbol type $symtype!");
                }
                my $str = "This $symtypename is available since SDL 3.0.0.";
                $sections{'Version'} = wordwrap(wikify($wikitype, $str)) . "\n";
            }
        }

        # We can build the wiki table now that we have all the data.
        if (scalar(@final_params) > 0) {
            my $str = '';
            if ($wikitype eq 'mediawiki') {
                while (scalar(@final_params) > 0) {
                    my $arg = shift @final_params;
                    my $c_datatype = shift @final_params;
                    my $desc = wikify($wikitype, shift @final_params);
                    $c_datatype = '' if not defined $c_datatype;
                    $str .= ($str eq '') ? "{|\n" : "|-\n";
                    $str .= "|$c_datatype\n" if $params_has_c_datatype;
                    $str .= "|'''$arg'''\n";
                    $str .= "|$desc\n";
                }
                $str .= "|}\n";
            } elsif ($wikitype eq 'md') {
                my $longest_arg = 0;
                my $longest_c_datatype = 0;
                my $longest_desc = 0;
                my $which = 0;
                foreach (@final_params) {
                    if ($which == 0) {
                        my $len = length($_);
                        $longest_arg = $len if ($len > $longest_arg);
                        $which = 1;
                    } elsif ($which == 1) {
                        if (defined($_)) {
                            my $len = length(wikify($wikitype, $_));
                            $longest_c_datatype = $len if ($len > $longest_c_datatype);
                        }
                        $which = 2;
                    } else {
                        my $len = length(wikify($wikitype, $_));
                        $longest_desc = $len if ($len > $longest_desc);
                        $which = 0;
                    }
                }

                # Markdown tables are sort of obnoxious.
                my $c_datatype_cell;
                $c_datatype_cell = ($longest_c_datatype > 0) ? ('| ' . (' ' x ($longest_c_datatype)) . ' ') : '';
                $str .= $c_datatype_cell . '| ' . (' ' x ($longest_arg+4)) . ' | ' . (' ' x $longest_desc) . " |\n";
                $c_datatype_cell = ($longest_c_datatype > 0) ? ('| ' . ('-' x ($longest_c_datatype)) . ' ') : '';
                $str .= $c_datatype_cell . '| ' . ('-' x ($longest_arg+4)) . ' | ' . ('-' x $longest_desc) . " |\n";

                while (@final_params) {
                    my $arg = shift @final_params;
                    my $c_datatype = shift @final_params;
                    $c_datatype_cell = '';
                    if ($params_has_c_datatype) {
                        $c_datatype = defined($c_datatype) ? wikify($wikitype, $c_datatype) : '';
                        $c_datatype_cell = ($longest_c_datatype > 0) ? ("| $c_datatype " . (' ' x ($longest_c_datatype - length($c_datatype)))) : '';
                    }
                    my $desc = wikify($wikitype, shift @final_params);
                    $str .= $c_datatype_cell . "| **$arg** " . (' ' x ($longest_arg - length($arg))) . "| $desc" . (' ' x ($longest_desc - length($desc))) . " |\n";
                }
            } else {
                die("Unexpected wikitype!");  # should have checked this elsewhere.
            }
            $sections{'Function Parameters'} = $str;
        }

        my $path = "$wikipath/$sym.${wikitype}.tmp";
        open(FH, '>', $path) or die("Can't open '$path': $!\n");

        my $sectionsref = $wikisyms{$sym};

        foreach (@standard_wiki_sections) {
            # drop sections we either replaced or removed from the original wiki's contents.
            if (not defined $only_wiki_sections{$_}) {
                delete($$sectionsref{$_});
            }
        }

        my $wikisectionorderref = $wikisectionorder{$sym};

        # Make sure there's a footer in the wiki that puts this function in CategoryAPI...
        if (not $$sectionsref{'[footer]'}) {
            $$sectionsref{'[footer]'} = '';
            push @$wikisectionorderref, '[footer]';
        }

        # If changing format, convert things that otherwise are passed through unmolested.
        if (defined $changeformat) {
            if (($dewikify_mode eq 'md') and ($origwikitype eq 'mediawiki')) {
                $$sectionsref{'[footer]'} =~ s/\[\[(Category[a-zA-Z0-9_]+)\]\]/[$1]($1)/g;
            } elsif (($dewikify_mode eq 'mediawiki') and ($origwikitype eq 'md')) {
                $$sectionsref{'[footer]'} =~ s/\[(Category[a-zA-Z0-9_]+)\]\(.*?\)/[[$1]]/g;
            }

            foreach (keys %only_wiki_sections) {
                my $sect = $_;
                if (defined $$sectionsref{$sect}) {
                    $$sectionsref{$sect} = wikify($wikitype, dewikify($origwikitype, $$sectionsref{$sect}));
                }
            }
        }

        if ($symtype != -1) {  # Don't do these in category documentation block
            my $footer = $$sectionsref{'[footer]'};

            my $symtypename;
            if ($symtype == 1) {
                $symtypename = 'Function';
            } elsif ($symtype == 2) {
                $symtypename = 'Macro';
            } elsif ($symtype == 3) {
                $symtypename = 'Struct';
            } elsif ($symtype == 4) {
                $symtypename = 'Enum';
            } elsif ($symtype == 5) {
                $symtypename = 'Datatype';
            } else {
                die("Unexpected symbol type $symtype!");
            }

            my $symcategory = $headersymscategory{$sym};
            if ($wikitype eq 'mediawiki') {
                $footer =~ s/\[\[CategoryAPI\]\],?\s*//g;
                $footer =~ s/\[\[CategoryAPI${symtypename}\]\],?\s*//g;
                $footer =~ s/\[\[Category${symcategory}\]\],?\s*//g if defined $symcategory;
                $footer = "[[CategoryAPI]], [[CategoryAPI$symtypename]]" . (defined $symcategory ? ", [[Category$symcategory]]" : '') . (($footer eq '') ? "\n" : ", $footer");
            } elsif ($wikitype eq 'md') {
                $footer =~ s/\[CategoryAPI\]\(CategoryAPI\),?\s*//g;
                $footer =~ s/\[CategoryAPI${symtypename}\]\(CategoryAPI${symtypename}\),?\s*//g;
                $footer =~ s/\[Category${symcategory}\]\(Category${symcategory}\),?\s*//g if defined $symcategory;
                $footer = "[CategoryAPI](CategoryAPI), [CategoryAPI$symtypename](CategoryAPI$symtypename)" . (defined $symcategory ? ", [Category$symcategory](Category$symcategory)" : '') . (($footer eq '') ? '' : ', ') . $footer;
            } else { die("Unexpected wikitype '$wikitype'"); }
            $$sectionsref{'[footer]'} = $footer;

            if (defined $wikipreamble) {
                my $wikified_preamble = wikify($wikitype, $wikipreamble);
                if ($wikitype eq 'mediawiki') {
                    print FH "====== $wikified_preamble ======\n";
                } elsif ($wikitype eq 'md') {
                    print FH "###### $wikified_preamble\n";
                } else { die("Unexpected wikitype '$wikitype'"); }
            }
        }

        my $prevsectstr = '';
        my @ordered_sections = (@standard_wiki_sections, defined $wikisectionorderref ? @$wikisectionorderref : ());  # this copies the arrays into one.
        foreach (@ordered_sections) {
            my $sect = $_;
            next if $sect eq '[start]';
            next if (not defined $sections{$sect} and not defined $$sectionsref{$sect});
            my $section = defined $sections{$sect} ? $sections{$sect} : $$sectionsref{$sect};

            if ($sect eq '[footer]') {
                # Make sure previous section ends with two newlines.
                if (substr($prevsectstr, -1) ne "\n") {
                    print FH "\n\n";
                } elsif (substr($prevsectstr, -2) ne "\n\n") {
                    print FH "\n";
                }
                print FH "----\n";   # It's the same in Markdown and MediaWiki.
            } elsif ($sect eq '[Brief]') {
                if ($wikitype eq 'mediawiki') {
                    print FH  "= $sym =\n\n";
                } elsif ($wikitype eq 'md') {
                    print FH "# $sym\n\n";
                } else { die("Unexpected wikitype '$wikitype'"); }
            } else {
                my $sectname = $sect;
                if ($sectname eq 'Function Parameters') {  # We use this same table for different things depending on what we're documenting, so rename it now.
                    if (($symtype == 1) || ($symtype == 5)) {  # function (or typedef, in case it's a function pointer type).
                    } elsif ($symtype == 2) {  # macro
                        $sectname = 'Macro Parameters';
                    } elsif ($symtype == 3) {  # struct/union
                        $sectname = 'Fields';
                    } elsif ($symtype == 4) {  # enum
                        $sectname = 'Values';
                    } else {
                        die("Unexpected symtype $symtype");
                    }
                }

                if ($symtype != -1) {  # Not for category documentation block
                    if ($wikitype eq 'mediawiki') {
                        print FH  "\n== $sectname ==\n\n";
                    } elsif ($wikitype eq 'md') {
                        print FH "\n## $sectname\n\n";
                    } else { die("Unexpected wikitype '$wikitype'"); }
                }
            }

            my $sectstr = defined $sections{$sect} ? $sections{$sect} : $$sectionsref{$sect};
            print FH $sectstr;

            $prevsectstr = $sectstr;

            # make sure these don't show up twice.
            delete($sections{$sect});
            delete($$sectionsref{$sect});
        }

        print FH "\n\n";
        close(FH);

        if (defined $changeformat and ($origwikitype ne $wikitype)) {
            system("cd '$wikipath' ; git mv '$_.${origwikitype}' '$_.${wikitype}'");
            unlink("$wikipath/$_.${origwikitype}");
        }

        rename($path, "$wikipath/$_.${wikitype}") or die("Can't rename '$path' to '$wikipath/$_.${wikitype}': $!\n");
    }

    # Write out simple redirector pages if they don't already exist.
    foreach (keys %referenceonly) {
        my $sym = $_;
        my $refersto = $referenceonly{$sym};
        my $path = "$wikipath/$sym.md";  # we only do Markdown for these.
        next if (-f $path);  # don't overwrite if it already exists. Delete the file if you need a rebuild!
        open(FH, '>', $path) or die("Can't open '$path': $!\n");

        if (defined $wikipreamble) {
            my $wikified_preamble = wikify('md', $wikipreamble);
            print FH "###### $wikified_preamble\n";
        }

        print FH "# $sym\n\nPlease refer to [$refersto]($refersto) for details.\n\n";
        print FH "----\n";
        print FH "[CategoryAPI](CategoryAPI), [CategoryAPIMacro](CategoryAPIMacro)\n\n";

        close(FH);
    }

    # Write out Category pages...
    foreach (keys %headercategorydocs) {
        my $cat = $_;
        my $sym = $headercategorydocs{$cat};  # fake symbol
        my $raw = $headersyms{$sym};  # raw doxygen text with comment characters stripped from start/end and start of each line.
        my $wikitype = defined($wikitypes{$sym}) ? $wikitypes{$sym} : 'md';
        my $path = "$wikipath/Category$cat.$wikitype";

        $raw = wordwrap(wikify($wikitype, $raw));

        my $tmppath = "$path.tmp";
        open(FH, '>', $tmppath) or die("Can't open '$tmppath': $!\n");
        print FH "$raw\n\n";

        if (! -f $path) {  # Doesn't exist at all? Write out a template file.
            # If writing from scratch, it's always a Markdown file.
            die("Unexpected wikitype '$wikitype'!") if $wikitype ne 'md';
            print FH <<__EOF__

<!-- END CATEGORY DOCUMENTATION -->

## Functions

<!-- DO NOT HAND-EDIT CATEGORY LISTS, THEY ARE AUTOGENERATED AND WILL BE OVERWRITTEN, BASED ON TAGS IN INDIVIDUAL PAGE FOOTERS. EDIT THOSE INSTEAD. -->
<!-- BEGIN CATEGORY LIST: Category$cat, CategoryAPIFunction -->
<!-- END CATEGORY LIST -->

## Datatypes

<!-- DO NOT HAND-EDIT CATEGORY LISTS, THEY ARE AUTOGENERATED AND WILL BE OVERWRITTEN, BASED ON TAGS IN INDIVIDUAL PAGE FOOTERS. EDIT THOSE INSTEAD. -->
<!-- BEGIN CATEGORY LIST: Category$cat, CategoryAPIDatatype -->
<!-- END CATEGORY LIST -->

## Structs

<!-- DO NOT HAND-EDIT CATEGORY LISTS, THEY ARE AUTOGENERATED AND WILL BE OVERWRITTEN, BASED ON TAGS IN INDIVIDUAL PAGE FOOTERS. EDIT THOSE INSTEAD. -->
<!-- BEGIN CATEGORY LIST: Category$cat, CategoryAPIStruct -->
<!-- END CATEGORY LIST -->

## Enums

<!-- DO NOT HAND-EDIT CATEGORY LISTS, THEY ARE AUTOGENERATED AND WILL BE OVERWRITTEN, BASED ON TAGS IN INDIVIDUAL PAGE FOOTERS. EDIT THOSE INSTEAD. -->
<!-- BEGIN CATEGORY LIST: Category$cat, CategoryAPIEnum -->
<!-- END CATEGORY LIST -->

## Macros

<!-- DO NOT HAND-EDIT CATEGORY LISTS, THEY ARE AUTOGENERATED AND WILL BE OVERWRITTEN, BASED ON TAGS IN INDIVIDUAL PAGE FOOTERS. EDIT THOSE INSTEAD. -->
<!-- BEGIN CATEGORY LIST: Category$cat, CategoryAPIMacro -->
<!-- END CATEGORY LIST -->

----
[CategoryAPICategory](CategoryAPICategory)

__EOF__
;
        } else {
            my $endstr = $wikisyms{$sym}->{'[footer]'};
            if (defined($endstr)) {
                print FH $endstr;
            }
        }

        close(FH);
        rename($tmppath, $path) or die("Can't rename '$tmppath' to '$path': $!\n");
    }

    # Write out READMEs...
    if (defined $readmepath) {
        if ( -d $readmepath ) {
            mkdir($wikireadmepath);  # just in case
            opendir(DH, $readmepath) or die("Can't opendir '$readmepath': $!\n");
            while (my $d = readdir(DH)) {
                my $dent = $d;
                if ($dent =~ /\AREADME\-(.*?\.md)\Z/) {  # we only bridge Markdown files here.
                    my $wikifname = $1;
                    next if $wikifname eq 'FrontPage.md';
                    filecopy("$readmepath/$dent", "$wikireadmepath/$wikifname", "\n");
                }
            }
            closedir(DH);

            my @pages = ();
            opendir(DH, $wikireadmepath) or die("Can't opendir '$wikireadmepath': $!\n");
            while (my $d = readdir(DH)) {
                my $dent = $d;
                if ($dent =~ /\A(.*?)\.(mediawiki|md)\Z/) {
                    my $wikiname = $1;
                    next if $wikiname eq 'FrontPage';
                    push @pages, $wikiname;
                }
            }
            closedir(DH);

            open(FH, '>', "$wikireadmepath/FrontPage.md") or die("Can't open '$wikireadmepath/FrontPage.md': $!\n");
            print FH "# All READMEs available here\n\n";
            foreach (sort @pages) {
                my $wikiname = $_;
                print FH "- [$wikiname]($wikiname)\n";
            }
            close(FH);
        }
    }

} elsif ($copy_direction == -2) { # --copy-to-manpages
    # This only takes from the wiki data, since it has sections we omit from the headers, like code examples.

    File::Path::make_path("$manpath/man3");

    $dewikify_mode = 'manpage';
    $wordwrap_mode = 'manpage';

    my $introtxt = '';
    if (0) {
    open(FH, '<', "$srcpath/LICENSE.txt") or die("Can't open '$srcpath/LICENSE.txt': $!\n");
    while (<FH>) {
        chomp;
        $introtxt .= ".\\\" $_\n";
    }
    close(FH);
    }

    if (!$gitrev) {
        $gitrev = `cd "$srcpath" ; git rev-list HEAD~..`;
        chomp($gitrev);
    }

    # !!! FIXME
    open(FH, '<', "$srcpath/$versionfname") or die("Can't open '$srcpath/$versionfname': $!\n");
    my $majorver = 0;
    my $minorver = 0;
    my $microver = 0;
    while (<FH>) {
        chomp;
        if (/$versionmajorregex/) {
            $majorver = int($1);
        } elsif (/$versionminorregex/) {
            $minorver = int($1);
        } elsif (/$versionmicroregex/) {
            $microver = int($1);
        }
    }
    close(FH);
    my $fullversion = "$majorver.$minorver.$microver";

    foreach (keys %headersyms) {
        my $sym = $_;
        next if not defined $wikisyms{$sym};  # don't have a page for that function, skip it.
        next if $sym =~ /\A\[category documentation\]/;   # not real symbols
        my $symtype = $headersymstype{$sym};
        my $wikitype = $wikitypes{$sym};
        my $sectionsref = $wikisyms{$sym};
        my $remarks = $sectionsref->{'Remarks'};
        my $params = $sectionsref->{'Function Parameters'};
        my $returns = $sectionsref->{'Return Value'};
        my $version = $sectionsref->{'Version'};
        my $threadsafety = $sectionsref->{'Thread Safety'};
        my $related = $sectionsref->{'See Also'};
        my $examples = $sectionsref->{'Code Examples'};
        my $deprecated = $sectionsref->{'Deprecated'};
        my $headerfile = $manpageheaderfiletext;
        $headerfile =~ s/\%fname\%/$headersymslocation{$sym}/g;
        $headerfile .= "\n";

        my $mansection;
        my $mansectionname;
        if (($symtype == 1) || ($symtype == 2)) {  # functions or macros
            $mansection = '3';
            $mansectionname = 'FUNCTIONS';
        } elsif (($symtype >= 3) && ($symtype <= 5)) {  # struct/union/enum/typedef
            $mansection = '3type';
            $mansectionname = 'DATATYPES';
        } else {
            die("Unexpected symtype $symtype");
        }

        my $brief = $sectionsref->{'[Brief]'};
        my $decl = $headerdecls{$sym};
        my $str = '';

        $brief = "$brief";
        $brief =~ s/\A[\s\n]*\= .*? \=\s*?\n+//ms;
        $brief =~ s/\A[\s\n]*\=\= .*? \=\=\s*?\n+//ms;
        $brief =~ s/\A(.*?\.) /$1\n/;  # \brief should only be one sentence, delimited by a period+space. Split if necessary.
        my @briefsplit = split /\n/, $brief;
        $brief = shift @briefsplit;
        $brief = dewikify($wikitype, $brief);

        if (defined $remarks) {
            $remarks = dewikify($wikitype, join("\n", @briefsplit) . $remarks);
        }

        $str .= $introtxt;

        $str .= ".\\\" This manpage content is licensed under Creative Commons\n";
        $str .= ".\\\"  Attribution 4.0 International (CC BY 4.0)\n";
        $str .= ".\\\"   https://creativecommons.org/licenses/by/4.0/\n";
        $str .= ".\\\" This manpage was generated from ${projectshortname}'s wiki page for $sym:\n";
        $str .= ".\\\"   $wikiurl/$sym\n";
        $str .= ".\\\" Generated with SDL/build-scripts/wikiheaders.pl\n";
        $str .= ".\\\"  revision $gitrev\n" if $gitrev ne '';
        $str .= ".\\\" Please report issues in this manpage's content at:\n";
        $str .= ".\\\"   $bugreporturl\n";
        $str .= ".\\\" Please report issues in the generation of this manpage from the wiki at:\n";
        $str .= ".\\\"   https://github.com/libsdl-org/SDL/issues/new?title=Misgenerated%20manpage%20for%20$sym\n";
        $str .= ".\\\" $projectshortname can be found at $projecturl\n";

        # Define a .URL macro. The "www.tmac" thing decides if we're using GNU roff (which has a .URL macro already), and if so, overrides the macro we just created.
        # This wizadry is from https://web.archive.org/web/20060102165607/http://people.debian.org/~branden/talks/wtfm/wtfm.pdf
        $str .= ".de URL\n";
        $str .= '\\$2 \(laURL: \\$1 \(ra\\$3' . "\n";
        $str .= "..\n";
        $str .= '.if \n[.g] .mso www.tmac' . "\n";

        $str .= ".TH $sym $mansection \"$projectshortname $fullversion\" \"$projectfullname\" \"$projectshortname$majorver $mansectionname\"\n";
        $str .= ".SH NAME\n";

        $str .= "$sym";
        $str .= " \\- $brief" if (defined $brief);
        $str .= "\n";

        if (defined $deprecated) {
            $str .= ".SH DEPRECATED\n";
            $str .= dewikify($wikitype, $deprecated) . "\n";
        }

        if (defined $headerfile) {
            $str .= ".SH HEADER FILE\n";
            $str .= dewikify($wikitype, $headerfile) . "\n";
        }

        $str .= ".SH SYNOPSIS\n";
        $str .= ".nf\n";
        $str .= ".B #include \\(dq$mainincludefname\\(dq\n";
        $str .= ".PP\n";

        my @decllines = split /\n/, $decl;
        foreach (@decllines) {
            $str .= ".BI \"$_\n";
        }
        $str .= ".fi\n";

        if (defined $remarks) {
            $str .= ".SH DESCRIPTION\n";
            $str .= $remarks . "\n";
        }

        if (defined $params) {
            if (($symtype == 1) || ($symtype == 5)) {
                $str .= ".SH FUNCTION PARAMETERS\n";
            } elsif ($symtype == 2) {  # macro
                $str .= ".SH MACRO PARAMETERS\n";
            } elsif ($symtype == 3) {  # struct/union
                $str .= ".SH FIELDS\n";
            } elsif ($symtype == 4) {  # enum
                $str .= ".SH VALUES\n";
            } else {
                die("Unexpected symtype $symtype");
            }

            my @lines = split /\n/, $params;
            if ($wikitype eq 'mediawiki') {
                die("Unexpected data parsing MediaWiki table") if (shift @lines ne '{|');  # Dump the '{|' start
                while (scalar(@lines) >= 3) {
                    my $c_datatype = shift @lines;
                    my $name = shift @lines;
                    my $desc = shift @lines;
                    my $terminator;  # the '|-' or '|}' line.

                    if (($desc eq '|-') or ($desc eq '|}') or (not $desc =~ /\A\|/)) {  # we seem to be out of cells, which means there was no datatype column on this one.
                        $terminator = $desc;
                        $desc = $name;
                        $name = $c_datatype;
                        $c_datatype = '';
                    } else {
                        $terminator = shift @lines;
                    }

                    last if ($terminator ne '|-') and ($terminator ne '|}');  # we seem to have run out of table.
                    $name =~ s/\A\|\s*//;
                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    $desc =~ s/\A\|\s*//;
                    $desc = dewikify($wikitype, $desc);
                    #print STDERR "SYM: $sym   CDATATYPE: $c_datatype  NAME: $name   DESC: $desc TERM: $terminator\n";

                    $str .= ".TP\n";
                    $str .= ".I $name\n";
                    $str .= "$desc\n";
                }
            } elsif ($wikitype eq 'md') {
                my $l;
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A(\s*\|)?\s*\|\s*\|\s*\|\s*\Z/);
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A\s*(\|\s*\-*\s*)?\|\s*\-*\s*\|\s*\-*\s*\|\s*\Z/);
                while (scalar(@lines) >= 1) {
                    $l = shift @lines;
                    my $name;
                    my $desc;
                    if ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        # c datatype is $1, but we don't care about it here.
                        $name = $2;
                        $desc = $3;
                    } elsif ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        $name = $1;
                        $desc = $2;
                    } else {
                        last;  # we seem to have run out of table.
                    }

                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    $desc = dewikify($wikitype, $desc);

                    $str .= ".TP\n";
                    $str .= ".I $name\n";
                    $str .= "$desc\n";
                }
            } else {
                die("write me");
            }
        }

        if (defined $returns) {
            $returns = dewikify($wikitype, $returns);
            $returns =~ s/\A\(.*?\)\s*//;  # Chop datatype in parentheses off the front.
            $str .= ".SH RETURN VALUE\n";
            $str .= "$returns\n";
        }

        if (defined $examples) {
            $str .= ".SH CODE EXAMPLES\n";
            $dewikify_manpage_code_indent = 0;
            $str .= dewikify($wikitype, $examples) . "\n";
            $dewikify_manpage_code_indent = 1;
        }

        if (defined $threadsafety) {
            $str .= ".SH THREAD SAFETY\n";
            $str .= dewikify($wikitype, $threadsafety) . "\n";
        }

        if (defined $version) {
            $str .= ".SH AVAILABILITY\n";
            $str .= dewikify($wikitype, $version) . "\n";
        }

        if (defined $related) {
            $str .= ".SH SEE ALSO\n";
            # !!! FIXME: lots of code duplication in all of these.
            my $v = dewikify($wikitype, $related);
            my @desclines = split /\n/, $v;
            my $nextstr = '';
            foreach (@desclines) {
                s/\(\)\Z//;  # Convert "SDL_Func()" to "SDL_Func"
                s/\[\[(.*?)\]\]/$1/;  # in case some wikilinks remain.
                s/\[(.*?)\]\(.*?\)/$1/;  # in case some wikilinks remain.
                s/\A\*\s*\Z//;
                s/\A\/*//;
                s/\A\.BR\s+//;  # dewikify added this, but we want to handle it.
                s/\A\.I\s+//;  # dewikify added this, but we want to handle it.
                s/\A\s*[\:\*\-]\s*//;
                s/\A\s+//;
                s/\s+\Z//;
                next if $_ eq '';
                my $seealso_symtype = $headersymstype{$_};
                my $seealso_mansection = '3';
                if (defined($seealso_symtype) && ($seealso_symtype >= 3) && ($seealso_symtype <= 5)) {  # struct/union/enum/typedef
                    $seealso_mansection = '3type';
                }
                $str .= "$nextstr.BR $_ ($seealso_mansection)";
                $nextstr = ",\n";
            }
            $str .= "\n";
        }

        if (0) {
        $str .= ".SH COPYRIGHT\n";
        $str .= "This manpage is licensed under\n";
        $str .= ".UR https://creativecommons.org/licenses/by/4.0/\n";
        $str .= "Creative Commons Attribution 4.0 International (CC BY 4.0)\n";
        $str .= ".UE\n";
        $str .= ".PP\n";
        $str .= "This manpage was generated from\n";
        $str .= ".UR $wikiurl/$sym\n";
        $str .= "${projectshortname}'s wiki\n";
        $str .= ".UE\n";
        $str .= "using SDL/build-scripts/wikiheaders.pl";
        $str .= " revision $gitrev" if $gitrev ne '';
        $str .= ".\n";
        $str .= "Please report issues in this manpage at\n";
        $str .= ".UR $bugreporturl\n";
        $str .= "our bugtracker!\n";
        $str .= ".UE\n";
        }

        my $path = "$manpath/man3/$_.$mansection";
        my $tmppath = "$path.tmp";
        open(FH, '>', $tmppath) or die("Can't open '$tmppath': $!\n");
        print FH $str;
        close(FH);
        rename($tmppath, $path) or die("Can't rename '$tmppath' to '$path': $!\n");
    }

} elsif ($copy_direction == -4) { # --copy-to-latex
    # This only takes from the wiki data, since it has sections we omit from the headers, like code examples.

    print STDERR "\n(The --copy-to-latex code is known to not be ready for serious use; send patches, not bug reports, please.)\n\n";

    $dewikify_mode = 'LaTeX';
    $wordwrap_mode = 'LaTeX';

    # !!! FIXME: code duplication with --copy-to-manpages section.

    my $introtxt = '';
    if (0) {
    open(FH, '<', "$srcpath/LICENSE.txt") or die("Can't open '$srcpath/LICENSE.txt': $!\n");
    while (<FH>) {
        chomp;
        $introtxt .= ".\\\" $_\n";
    }
    close(FH);
    }

    if (!$gitrev) {
        $gitrev = `cd "$srcpath" ; git rev-list HEAD~..`;
        chomp($gitrev);
    }

    # !!! FIXME
    open(FH, '<', "$srcpath/$versionfname") or die("Can't open '$srcpath/$versionfname': $!\n");
    my $majorver = 0;
    my $minorver = 0;
    my $microver = 0;
    while (<FH>) {
        chomp;
        if (/$versionmajorregex/) {
            $majorver = int($1);
        } elsif (/$versionminorregex/) {
            $minorver = int($1);
        } elsif (/$versionmicroregex/) {
            $microver = int($1);
        }
    }
    close(FH);
    my $fullversion = "$majorver.$minorver.$microver";

    my $latex_fname = "$srcpath/$projectshortname.tex";
    my $latex_tmpfname = "$latex_fname.tmp";
    open(TEXFH, '>', "$latex_tmpfname") or die("Can't open '$latex_tmpfname' for writing: $!\n");

    print TEXFH <<__EOF__
\\documentclass{book}

\\usepackage{listings}
\\usepackage{color}
\\usepackage{hyperref}

\\definecolor{dkgreen}{rgb}{0,0.6,0}
\\definecolor{gray}{rgb}{0.5,0.5,0.5}
\\definecolor{mauve}{rgb}{0.58,0,0.82}

\\setcounter{secnumdepth}{0}

\\lstset{frame=tb,
  language=C,
  aboveskip=3mm,
  belowskip=3mm,
  showstringspaces=false,
  columns=flexible,
  basicstyle={\\small\\ttfamily},
  numbers=none,
  numberstyle=\\tiny\\color{gray},
  keywordstyle=\\color{blue},
  commentstyle=\\color{dkgreen},
  stringstyle=\\color{mauve},
  breaklines=true,
  breakatwhitespace=true,
  tabsize=3
}

\\begin{document}
\\frontmatter

\\title{$projectfullname $majorver.$minorver.$microver Reference Manual}
\\author{The $projectshortname Developers}
\\maketitle

\\mainmatter

__EOF__
;

    # !!! FIXME: Maybe put this in the book intro?  print TEXFH $introtxt;

    # Sort symbols by symbol type, then alphabetically.
    my @headersymskeys = sort {
        my $symtypea = $headersymstype{$a};
        my $symtypeb = $headersymstype{$b};
        $symtypea = 3 if ($symtypea > 3);
        $symtypeb = 3 if ($symtypeb > 3);
        my $rc = $symtypea <=> $symtypeb;
        if ($rc == 0) {
            $rc = lc($a) cmp lc($b);
        }
        return $rc;
    } keys %headersyms;

    my $current_symtype = 0;
    my $current_chapter = '';

    foreach (@headersymskeys) {
        my $sym = $_;
        next if not defined $wikisyms{$sym};  # don't have a page for that function, skip it.
        next if $sym =~ /\A\[category documentation\]/;   # not real symbols.
        my $symtype = $headersymstype{$sym};
        my $wikitype = $wikitypes{$sym};
        my $sectionsref = $wikisyms{$sym};
        my $remarks = $sectionsref->{'Remarks'};
        my $params = $sectionsref->{'Function Parameters'};
        my $returns = $sectionsref->{'Return Value'};
        my $version = $sectionsref->{'Version'};
        my $threadsafety = $sectionsref->{'Thread Safety'};
        my $related = $sectionsref->{'See Also'};
        my $examples = $sectionsref->{'Code Examples'};
        my $deprecated = $sectionsref->{'Deprecated'};
        my $headerfile = $manpageheaderfiletext;
        $headerfile =~ s/\%fname\%/$headersymslocation{$sym}/g;
        $headerfile .= "\n";

        my $brief = $sectionsref->{'[Brief]'};
        my $decl = $headerdecls{$sym};
        my $str = '';

        if ($current_symtype != $symtype) {
            my $newchapter = '';
            if ($symtype == 1) {
                $newchapter = 'Functions';
            } elsif ($symtype == 2) {
                $newchapter = 'Macros';
            } else {
                $newchapter = 'Datatypes';
            }

            if ($current_chapter ne $newchapter) {
                $str .= "\n\n\\chapter{$projectshortname $newchapter}\n\n\\clearpage\n\n";
                $current_chapter = $newchapter;
            }
            $current_symtype = $symtype;
        }

        $brief = "$brief";
        $brief =~ s/\A[\s\n]*\= .*? \=\s*?\n+//ms;
        $brief =~ s/\A[\s\n]*\=\= .*? \=\=\s*?\n+//ms;
        $brief =~ s/\A(.*?\.) /$1\n/;  # \brief should only be one sentence, delimited by a period+space. Split if necessary.
        my @briefsplit = split /\n/, $brief;
        $brief = shift @briefsplit;
        $brief = dewikify($wikitype, $brief);

        if (defined $remarks) {
            $remarks = dewikify($wikitype, join("\n", @briefsplit) . $remarks);
        }

        my $escapedsym = escLaTeX($sym);
        $str .= "\\hypertarget{$sym}{%\n\\section{$escapedsym}\\label{$sym}}\n\n";
        $str .= $brief if (defined $brief);
        $str .= "\n\n";

        if (defined $deprecated) {
            $str .= "\\subsection{Deprecated}\n\n";
            $str .= dewikify($wikitype, $deprecated) . "\n";
        }

        if (defined $headerfile) {
            $str .= "\\subsection{Header File}\n\n";
            $str .= dewikify($wikitype, $headerfile) . "\n";
        }

        $str .= "\\subsection{Syntax}\n\n";
        $str .= "\\begin{lstlisting}\n$decl\n\\end{lstlisting}\n";

        if (defined $params) {
            if (($symtype == 1) || ($symtype == 5)) {
                $str .= "\\subsection{Function Parameters}\n\n";
            } elsif ($symtype == 2) {  # macro
                $str .= "\\subsection{Macro Parameters}\n\n";
            } elsif ($symtype == 3) {  # struct/union
                $str .= "\\subsection{Fields}\n\n";
            } elsif ($symtype == 4) {  # enum
                $str .= "\\subsection{Values}\n\n";
            } else {
                die("Unexpected symtype $symtype");
            }

            $str .= "\\begin{center}\n";
            $str .= "    \\begin{tabular}{ | l | p{0.75\\textwidth} |}\n";
            $str .= "    \\hline\n";

            # !!! FIXME: this table parsing has gotten complicated and is pasted three times in this file; move it to a subroutine!
            my @lines = split /\n/, $params;
            if ($wikitype eq 'mediawiki') {
                die("Unexpected data parsing MediaWiki table") if (shift @lines ne '{|');  # Dump the '{|' start
                while (scalar(@lines) >= 3) {
                    my $name = shift @lines;
                    my $desc = shift @lines;
                    my $terminator = shift @lines;  # the '|-' or '|}' line.
                    last if ($terminator ne '|-') and ($terminator ne '|}');  # we seem to have run out of table.
                    $name =~ s/\A\|\s*//;
                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    $name = escLaTeX($name);
                    $desc =~ s/\A\|\s*//;
                    $desc = dewikify($wikitype, $desc);
                    #print STDERR "FN: $sym   NAME: $name   DESC: $desc TERM: $terminator\n";
                    $str .= "    \\textbf{$name} & $desc \\\\ \\hline\n";
                }
            } elsif ($wikitype eq 'md') {
                my $l;
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A(\s*\|)?\s*\|\s*\|\s*\|\s*\Z/);
                $l = shift @lines;
                die("Unexpected data parsing Markdown table") if (not $l =~ /\A\s*(\|\s*\-*\s*)?\|\s*\-*\s*\|\s*\-*\s*\|\s*\Z/);
                while (scalar(@lines) >= 1) {
                    $l = shift @lines;
                    my $name;
                    my $desc;
                    if ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        # c datatype is $1, but we don't care about it here.
                        $name = $2;
                        $desc = $3;
                    } elsif ($l =~ /\A\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*\Z/) {
                        $name = $1;
                        $desc = $2;
                    } else {
                        last;  # we seem to have run out of table.
                    }

                    $name =~ s/\A\*\*(.*?)\*\*/$1/;
                    $name =~ s/\A\'\'\'(.*?)\'\'\'/$1/;
                    $name = escLaTeX($name);
                    $desc = dewikify($wikitype, $desc);
                    $str .= "    \\textbf{$name} & $desc \\\\ \\hline\n";
                }
            } else {
                die("write me");
            }

            $str .= "    \\end{tabular}\n";
            $str .= "\\end{center}\n";
        }

        if (defined $returns) {
            $returns = dewikify($wikitype, $returns);
            $returns =~ s/\A\(.*?\)\s*//;  # Chop datatype in parentheses off the front.
            $str .= "\\subsection{Return Value}\n\n";
            $str .= "$returns\n";
        }

        if (defined $remarks) {
            $str .= "\\subsection{Remarks}\n\n";
            $str .= $remarks . "\n";
        }

        if (defined $examples) {
            $str .= "\\subsection{Code Examples}\n\n";
            $dewikify_manpage_code_indent = 0;
            $str .= dewikify($wikitype, $examples) . "\n";
            $dewikify_manpage_code_indent = 1;
        }

        if (defined $threadsafety) {
            $str .= "\\subsection{Thread Safety}\n\n";
            $str .= dewikify($wikitype, $threadsafety) . "\n";
        }

        if (defined $version) {
            $str .= "\\subsection{Version}\n\n";
            $str .= dewikify($wikitype, $version) . "\n";
        }

        if (defined $related) {
            $str .= "\\subsection{See Also}\n\n";
            $str .= "\\begin{itemize}\n";
            # !!! FIXME: lots of code duplication in all of these.
            my $v = dewikify($wikitype, $related);
            my @desclines = split /\n/, $v;
            my $nextstr = '';
            foreach (@desclines) {
                s/\(\)\Z//;  # Convert "SDL_Func()" to "SDL_Func"
                s/\[\[(.*?)\]\]/$1/;  # in case some wikilinks remain.
                s/\[(.*?)\]\(.*?\)/$1/;  # in case some wikilinks remain.
                s/\A\*\s*\Z//;
                s/\A\s*\\item\s*//;
                s/\A\/*//;
                s/\A\s*[\:\*\-]\s*//;
                s/\A\s+//;
                s/\s+\Z//;
                next if $_ eq '';
                next if $_ eq '\begin{itemize}';
                next if $_ eq '\end{itemize}';
                $str .= "    \\item $_\n";
            }
            $str .= "\\end{itemize}\n";
            $str .= "\n";
        }

        # !!! FIXME: Maybe put copyright in the book intro?
        if (0) {
        $str .= ".SH COPYRIGHT\n";
        $str .= "This manpage is licensed under\n";
        $str .= ".UR https://creativecommons.org/licenses/by/4.0/\n";
        $str .= "Creative Commons Attribution 4.0 International (CC BY 4.0)\n";
        $str .= ".UE\n";
        $str .= ".PP\n";
        $str .= "This manpage was generated from\n";
        $str .= ".UR $wikiurl/$sym\n";
        $str .= "${projectshortname}'s wiki\n";
        $str .= ".UE\n";
        $str .= "using SDL/build-scripts/wikiheaders.pl";
        $str .= " revision $gitrev" if $gitrev ne '';
        $str .= ".\n";
        $str .= "Please report issues in this manpage at\n";
        $str .= ".UR $bugreporturl\n";
        $str .= "our bugtracker!\n";
        $str .= ".UE\n";
        }

        $str .= "\\clearpage\n\n";

        print TEXFH $str;
    }

    print TEXFH "\\end{document}\n\n";
    close(TEXFH);
    rename($latex_tmpfname, $latex_fname) or die("Can't rename '$latex_tmpfname' to '$latex_fname': $!\n");

} elsif ($copy_direction == -3) { # --report-coverage-gaps
    foreach (@coverage_gap) {
        print("$_\n");
    }
}

# end of wikiheaders.pl ...

