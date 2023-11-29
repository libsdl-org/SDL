# Contributing to SDL

We appreciate your interest in contributing to SDL, this document will describe how to report bugs, contribute code or ideas or edit documentation.

**Table Of Contents**

-   [Filing a GitHub issue](#filing-a-github-issue)
    -   [Reporting a bug](#reporting-a-bug)
    -   [Suggesting enhancements](#suggesting-enhancements)
-   [Contributing code](#contributing-code)
    -   [Forking the project](#forking-the-project)
    -   [Following the style guide](#following-the-style-guide)
    -   [Running the tests](#running-the-tests)
    -   [Opening a pull request](#opening-a-pull-request)
-   [Contributing to the documentation](#contributing-to-the-documentation)
    -   [Editing a function documentation](#editing-a-function-documentation)
    -   [Editing the wiki](#editing-the-wiki)

## Filing a GitHub issue

### Reporting a bug

If you think you have found a bug and would like to report it, here are the steps you should take:

-   Before opening a new issue, ensure your bug has not already been reported on the [GitHub Issues page](https://github.com/libsdl-org/SDL/issues).
-   On the issue tracker, click on [New Issue](https://github.com/libsdl-org/SDL/issues/new).
-   Include details about your environment, such as your Operating System and SDL version.
-   If possible, provide a small example that reproduces your bug.

### Suggesting enhancements

If you want to suggest changes for the project, here are the steps you should take:

-   Check if the suggestion has already been made on:
    -   the [issue tracker](https://github.com/libsdl-org/SDL/issues);
    -   the [discourse forum](https://discourse.libsdl.org/);
    -   or if a [pull request](https://github.com/libsdl-org/SDL/pulls) already exists.
-   On the issue tracker, click on [New Issue](https://github.com/libsdl-org/SDL/issues/new).
-   Describe what change you would like to happen.

## Contributing code

This section will cover how the process of forking the project, making a change and opening a pull request.

### Forking the project

The first step consists in making a fork of the project, this is only necessary for the first contribution.

Head over to https://github.com/libsdl-org/SDL and click on the `Fork` button in the top right corner of your screen, you may leave the fields unchanged and click `Create Fork`.

You will be redirected to your fork of the repository, click the green `Code` button and copy the git clone link.

If you had already forked the repository, you may update it from the web page using the `Fetch upstream` button.

### Following the style guide

Code formatting is done using a custom `.clang-format` file, you can learn more about how to run it [here](https://clang.llvm.org/docs/ClangFormat.html).

Some legacy code may not be formatted, as such avoid formatting the whole file at once and only format around your changes.

For your commit message to be properly displayed on GitHub, it should contain:

-   A short description of the commit of 50 characters or less on the first line.
-   If necessary, add a blank line followed by a long description, each line should be 72 characters or less.

For example:

```
Fix crash in SDL_FooBar.

This addresses the issue #123456 by making sure Foo was successful
before calling Bar.
```

### Running the tests

Tests allow you to verify if your changes did not break any behaviour, here are the steps to follow:

-   Before pushing, run the `testautomation` suite on your machine, there should be no more failing tests after your change than before.
-   After pushing to your fork, Continuous Integration (GitHub Actions) will ensure compilation and tests still pass on other systems.

### Opening a pull request

-   Head over to your fork's GitHub page.
-   Click on the `Contribute` button and `Open Pull Request`.
-   Fill out the pull request template.
-   If any changes are requested, you can add new commits to your fork and they will be automatically added to the pull request.

## Contributing to the documentation

### Editing a function documentation

The wiki documentation for API functions is synchronised from the headers' doxygen comments. As such, all modifications to syntax; function parameters; return value; version; related functions should be done in the header directly.

### Editing the wiki

Other changes to the wiki should done directly from https://wiki.libsdl.org/  ... Just click the "edit" link at the bottom of any page!
