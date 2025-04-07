# Versioning

## Since 3.2.0

SDL follows an "odd/even" versioning policy, similar to GLib, GTK, Flatpak
and older versions of the Linux kernel:

* If the minor version (second part) and the patch version (third part) is
    divisible by 2 (for example 3.2.6, 3.4.0), this indicates a version of
    SDL that is believed to be stable and suitable for production use.

    * In stable releases, the patchlevel or micro version (third part)
        indicates bugfix releases. Bugfix releases may add small changes
        to the ABI, so newer patch versions are backwards-compatible but
        not fully forwards-compatible. For example, programs built against
        SDL 3.2.0 should work fine with SDL 3.2.8, but programs built against
        SDL 3.2.8 may not work with 3.2.0.

    * The minor version increases when significant changes are made that
        require longer development or testing time, e.g. major new functionality,
        or revamping support for a platform. Newer minor versions are
        backwards-compatible, but not fully forwards-compatible. For example,
        programs built against SDL 3.2.x should work fine with SDL 3.4.x,
        but programs built against SDL 3.4.x may not work with 3.2.x.

* If the minor version (second part) or patch version (third part) is not
    divisible by 2 (for example 3.2.9, 3.3.x), this indicates a development
    prerelease of SDL that is not suitable for stable software distributions.
    Use with caution.

    * The patchlevel or micro version (third part) increases with each prerelease.

    * Prereleases are backwards-compatible with older stable branches.
        For example, programs built against SDL 3.2.x should work fine with
        SDL 3.3.x, but programs built against SDL 3.3.x may not work with 3.2.x.

    * Prereleases are not guaranteed to be backwards-compatible with each other.
        For example, new API or ABI added in 3.3.0 might be removed or changed in
        3.3.1. If this would be a problem for you, please do not use prereleases.

    * Only use a prerelease if you can guarantee that you will promptly upgrade
        to the stable release that follows it. For example, do not use 3.3.x
        unless you will be able to upgrade to 3.4.0 when it becomes available.

    * Software distributions that have a freeze policy (in particular Linux
        distributions with a release cycle, such as Debian and Fedora)
        should only package stable releases, and not prereleases.

