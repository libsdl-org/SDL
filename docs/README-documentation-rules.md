# Rules for documentation

These are the rules for the care and feeding of wikiheaders.pl.


## No style guide

When adding or editing documentation, we don't (currently) have a style guide
for what it should read like, so try to make it consistent with the rest of
the existing text. It generally should read more like technical reference
manuals and not sound conversational in tone.

Most of these rules are about how to make sure the documentation works on
a _technical_ level, as scripts need to parse it, and there are a few simple
rules we need to obey to cooperate with those scripts.

## The wiki and headers share the same text.

There is a massive Perl script (`build-scripts/wikiheaders.pl`, hereafter
referred to as "wikiheaders") that can read both the wiki and the public
headers, and move changes in one across to the other.

If you prefer to use the wiki, go ahead and edit there. If you prefer to use
your own text editor, or command line tools to batch-process text, etc, you
can [clone the wiki as a git repo](https://github.com/libsdl-org/sdlwiki) and
work locally.


## Don't taunt wikiheaders.

The script isn't magic; it's a massive pile of Regular Expressions and not
a full C or markdown parser. While it isn't _fragile_, if you try to do clever
things, you might confuse it. This is to the benefit of documentation, though,
where we would rather you not do surprising things.


## We _sort of_ write in Doxygen format.

To document a symbol, we use something that looks like Doxygen (and Javadoc)
standard comment format:

```c
/**
 * This is a function that does something.
 *
 * It can be used for frozzling bobbles. Be aware that the Frozulator module
 * _must_ be initialized before calling this.
 *
 * \param frozzlevel The amount of frozzling to perform.
 * \param color What color bobble to frozzle. 0 is red, 1 is green.
 * \returns the number of bobbles that were actually frozzled, -1 on error.
 *
 * \threadsafety Do not call this from two threads at once, or the bobbles
 *               won't all frozzle correctly!
 *
 * \since This function is available since SDL 7.3.1.
 *
 * \sa SDL_DoSomethingElse
 */
extern SDL_DECLSPEC int SDLCALL SDL_DoSomething(int frozzlevel, int color);
```

Note the `/**` at the start of the comment. That's a "Doxygen-style" comment,
and wikiheaders will treat this differently than a comment with one `*`, as
this signifies that this is not just a comment, but _documentation_.

These comments _must_ start in the first column of the line, or wikiheaders
will ignore them, even with the "/**" start (we should improve the script
someday to handle this, but currently this is a requirement).

We do _not_ parse every magic Doxygen tag, and we don't parse them in `@param`
format. The goal here was to mostly coexist with people that might want
to run Doxygen on the SDL headers, not to build Doxygen from scratch. That
being said, compatibility with Doxygen is not a hard requirement here.

wikiheaders uses these specific tags to turn this comment into a (hopefully)
well-formatted wiki page, and also can generate manpages and books in LaTeX
format from it!

Text markup in the headers is _always_ done in Markdown format! But less is
more: try not to markup text more than necessary.


## Doxygen tags we support:

- `\brief one-line description` (Not required, and wikiheaders will remove tag).
- `\param varname description` (One for each function/macro parameter)
- `\returns description` (One for each function, don't use on `void` returns).
- `\sa` (each of these get tucked into a "See Also" section on the wiki)
- `\since This function is available since SDL 3.0.0.` (one per Doxygen comment)
- `\threadsafety description` (one per function/macro).
- `\deprecated description` (one per symbol, if symbol is deprecated!)

Other Doxygen things might exist in the headers, but they aren't understood
by wikiheaders.


## Use Markdown.

The wiki also supports MediaWiki format, but we are transitioning away from it.
The headers always use Markdown. If you're editing the wiki from a git clone,
just make .md files and the wiki will know what to do with them.


## Most things in the headers can be documented.

wikiheaders understands functions, typedefs, structs/unions/enums, `#defines`
... basically most of what makes up a C header. Just slap a Doxygen-style
comment in front of most things and it'll work.


## Defines right below typedefs and functions bind.

Any `#define` directly below a function or non-struct/union/enum typedef is
considered part of that declaration. This happens to work well with how our
headers work, as these defines tend to be bitflags and such that are related
to that symbol.

wikiheaders will include those defines in the syntax section of the wiki
page, and generate stub pages for each define that simply says "please refer
to (The Actual Symbol You Care About)" with a link. It will also pull in
any blank lines and most preprocessor directives for the syntax text, too.

Sometimes an unrelated define, by itself, just happens to be right below one
of these symbols in the header. The easiest way to deal with this is either
to document that define with a Doxygen-style comment, if it makes sense to do
so, or just add a normal C comment right above it if not, so wikiheaders
doesn't bind it to the previous symbol.


## Don't document the `SDL_test*.h` headers.

These are in the public headers but they aren't really considered public APIs.
They live in a separate library that doesn't, or at least probably shouldn't,
ship to end users. As such, we don't want it documented on the wiki.

For now, we do this by not having any Doxygen-style comments in these files.
Please keep it that way! If you want to document these headers, just don't
use the magic two-`*` comment.


## The first line is the summary.

The first line of a piece of documentation is meant to be a succinct
description. This is what Doxygen would call the `\brief` tag. wikiheaders
will split this text out until the first period (end of sentence!), and when
word wrapping, shuffle the overflow into a new paragraph below it.


## Split paragraphs with a blank line.

And don't indent them at all (indenting in Markdown is treated as preformatted
text).

wikiheaders will wordwrap header comments so they fit in 80 columns, so if you
don't leave a blank line between paragraphs, they will smush into a single
block of text when wordwrapping.


## Don't worry about word wrapping.

If you don't word-wrap your header edits perfectly (and you won't, I promise),
wikiheaders will send your change to the wiki, and then to make things match,
send it right back to the headers with correct word wrapping. Since this
happens right after you push your changes, you might as well just write
however you like and assume the system will clean it up for you.


## Things that start with `SDL_` will automatically become wiki links.

wikiheaders knows to turn these into links to other pages, so if you reference
an SDL symbol in the header documentation, you don't need to link to it.
You can optionally wrap the symbol in backticks, and wikiheaders will know to
link the backticked thing. It will not generate links in three-backtick
code/preformatted blocks.


## URLs will automatically become links.

You can use Markdown's `[link markup format](https://example.com/)`, but
sometimes it's clearer to list bare URLs; the URL will be visible on the
wiki page, but also clickable to follow the link. This is up to your judgment
on a case-by-case basis.


## Hide stuff from wikiheaders.

If all else fails, you can block off pieces of the header with this
magic line (whitespace is ignored):

```c
#ifndef SDL_WIKI_DOCUMENTATION_SECTION
```

Everything between this line and the next `#endif` will just be skipped by
wikiheaders. Note that wikiheaders is not a C preprocessor! Don't try to
nest conditionals or use `!defined`.

Just block off sections if you need to. And: you almost never need to.


## Hide stuff from the compiler.

If you need to put something that's only of interest to wikiheaders, the
convention is to put it in a block like this:

```c
#ifdef SDL_WIKI_DOCUMENTATION_SECTION
```

Generally this is used when there's a collection of preprocessor conditionals
to define the same symbol differently in different circumstances. You put
that symbol in this block with some reasonable generic version _and the
Doxygen-style comment_. Because wikiheaders doesn't care about this
preprocessor magic, and the C compiler can be as fancy as it wants, this is
strictly a useful convention.


## Struct/union/enum typedefs must have the name on the first line.

This is because wikiheaders is not a full C parser. Don't write this:

```c
typedef struct
{
    int a;
    int b;
} SDL_MyStruct;
```

...make sure the name is at the start, too:

```c
typedef struct SDL_MyStruct
{
    int a;
    int b;
} SDL_MyStruct;
```

wikiheaders will complain loudly if you don't do this, and exit with an
error message.


## Code examples go in the wiki.

We don't want the headers cluttered up with code examples. These live on the
wiki pages, and wikiheaders knows to not bridge them back to the headers.

Put them in a `## Code Examples` section, and make sure to wrap them in a
three-backtick-c section for formatting purposes. Only write code in C,
please.


## Do you _need_ a code example?

Most code examples aren't actually useful. If your code example is just
`SDL_CreateWindow("Hello SDL", 640, 480, 0);` then just delete it; if all
you're showing is how to call a function in C, it's not a useful code example.
Not all functions need an example. One with complex setup or usage details
might, though!


## Code examples are compiled by GitHub Actions.

On each change to the wiki, there is a script that pulls out all the code
examples into discrete C files and attempts to compile them, and complains
if they don't work.


## Unrecognized sections are left alone in the wiki.

A wiki section that starts with `## Section Name` (or `== Section Name ==` in
MediaWiki format) that isn't one of the recognized names will be left alone
by wikiheaders. Recognized sections might get overwritten with new content
from the headers, but the wiki file will not have other sections cleaned out
(this is how Code Examples remain wiki only, for example). You can use this
to add Wiki-specific text, or stuff that doesn't make sense in a header, or
would merely clutter it up.

A possibly-incomplete list of sections that will be overwritten by changes
to the headers:

- The page title line, and the "brief" one-sentence description section.
- "Deprecated"
- "Header File"
- "Syntax"
- "Function Parameters"
- "Macro Parameters"
- "Fields"
- "Values"
- "Return Value"
- "Remarks"
- "Thread Safety"
- "Version"
- "See Also"


## It's okay to repeat yourself.

Each individual piece of documentation becomes a separate page on the wiki, so
small repeated details can just exist in different pieces of documentation. If
it's complicated, it's not unreasonable to say "Please refer to
SDL_SomeOtherFunction for more details" ... wiki users can click right
through, header users can search for the function name.


## The docs directory is bridged to the wiki, too.

You might be reading this document on the wiki! Any `README-*.md` files in
the docs directory are bridged to the wiki, so `docs/README-linux.md` lands
at https://wiki.libsdl.org/SDL3/README/linux ...these are just copied directly
without any further processing by wikiheaders, and changes go in both
directions.


## The wiki can have its own pages, too.

If a page name isn't a symbol that wikiheaders sees in the headers, or a
README in the source's `docs` directory, or a few other exceptions, it'll
assume it's an unrelated wiki page and leave it alone. So feel free to
write any wiki-only pages that make sense and not worry about it junking
up the headers!


## Wiki categories are (mostly) managed automatically.

The wiki will see this pattern as the last thing on a page and treat it as a
list of categories that page belongs to:

```
----
[CategoryStuff](CategoryStuff), [CategoryWhatever](CategoryWhatever)
```

You can use this to simply tag a page as part of a category, and the user can
click directly to see other pages in that category. The wiki will
automatically manage a `Category*` pages that list any tagged pages.

You _should not_ add tags to the public headers. They don't mean anything
there. wikiheaders will add a few tags that make sense when generating wiki
content from the header files, and it will preserve other tags already present
on the page, so if you want to add extra categories to something, tag it on
the wiki itself.

The wiki uses some magic HTML comment tags to decide how to list items on
Category pages and let other content live on the page as well. You can
see an example of this in action at:

https://raw.githubusercontent.com/libsdl-org/sdlwiki/main/SDL3/CategoryEvents.md


## Categorizing the headers.

To put a symbol in a specific category, we use three approaches in SDL:

- Things in the `SDL_test*.h` headers aren't categorized at all (and you
  shouldn't document them!)
- Most files are categorized by header name: we strip off the leading `SDL_`
  and capitalize the first letter of what's left. So everything in SDL_audio.h
  is in the "Audio" category, everything in SDL_video.h is in the "Video"
  category, etc.
- If wikiheaders sees a comment like this on a line by itself...
  ```c
  /* WIKI CATEGORY: Blah */
  ```
  ...then all symbols below that will land in the "Blah" category. We use this
  at the top of a few headers where the simple
  chop-off-SDL_-and-captialize-the-first-letter trick doesn't work well, but
  one could theoretically use this for headers that have some overlap in
  category.


## Category documentation lives in headers.

To document a category (text that lives before the item lists on a wiki
category page), you have to follow a simple rule:

The _first_ Doxygen-style comment in a header must start with:

```
/**
 * # CategoryABC
```

If these conditions aren't met, wikiheaders will assume that documentation
belongs to whatever is below it instead of the Category.

The text of this comment will be added to the appropriate wiki Category page,
at the top, replacing everything in the file until it sees a line that starts
with an HTML comment (`<!--`), or a line that starts with `----`. Everything
after that in the wiki file will be preserved.

Likewise, when bridging _back_ to the headers, if wikiheaders sees one of
these comments, it'll copy the top section of the Category page back into the
comment.

Beyond stripping the initial ` * ` portion off each line, these comments are
treated as pure Markdown. They don't support any Doxygen tags like `\sa` or
`\since`.

