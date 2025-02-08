# Versioning

## Since 2.23.0

SDL follows an "odd/even" versioning policy, similar to GLib, GTK, Flatpak
and older versions of the Linux kernel:

* The major version (first part) increases when backwards compatibility
    is broken, which will happen infrequently.

* If the minor version (second part) is divisible by 2
    (for example 2.24.x, 2.26.x), this indicates a version of SDL that
    is believed to be stable and suitable for production use.

    * In stable releases, the patchlevel or micro version (third part)
        indicates bugfix releases. Bugfix releases should not add or
        remove ABI, so the ".0" release (for example 2.24.0) should be
        forwards-compatible with all the bugfix releases from the
        same cycle (for example 2.24.1).

    * The minor version increases when new API or ABI is added, or when
        other significant changes are made. Newer minor versions are
        backwards-compatible, but not fully forwards-compatible.
        For example, programs built against SDL 2.24.x should work fine
        with SDL 2.26.x, but programs built against SDL 2.26.x will not
        necessarily work with 2.24.x.

* If the minor version (second part) is not divisible by 2
    (for example 2.23.x, 2.25.x), this indicates a development prerelease
    of SDL that is not suitable for stable software distributions.
    Use with caution.

    * The patchlevel or micro version (third part) increases with
        each prerelease.

    * Each prerelease might add new API and/or ABI.

    * Prereleases are backwards-compatible with older stable branches.
        For example, 2.25.x will be backwards-compatible with 2.24.x.

    * Prereleases are not guaranteed to be backwards-compatible with
        each other. For example, new API or ABI added in 2.25.1
        might be removed or changed in 2.25.2.
        If this would be a problem for you, please do not use prereleases.

    * Only upgrade to a prerelease if you can guarantee that you will
        promptly upgrade to the stable release that follows it.
        For example, do not upgrade to 2.23.x unless you will be able to
        upgrade to 2.24.0 when it becomes available.

    * Software distributions that have a freeze policy (in particular Linux
        distributions with a release cycle, such as Debian and Fedora)
        should usually only package stable releases, and not prereleases.

## Before 2.23.0

Older versions of SDL followed a similar policy, but instead of the
odd/even rule applying to the minor version, it applied to the patchlevel
(micro version, third part). For example, 2.0.22 was a stable release
and 2.0.21 was a prerelease.
