# Release checklist

When changing the version, run `build-scripts/update-version.sh X Y Z`,
where `X Y Z` are the major version, minor version, and patch level. So
`2 28 1` means "change the version to 2.28.1". This script does much of the
mechanical work.


## New feature release

* Update `WhatsNew.txt`

* Bump version number to 2.EVEN.0:

    * `./build-scripts/update-version.sh 2 EVEN 0`

* Regenerate `configure`

* Do the release

## New bugfix release

* Check that no new API/ABI was added

    * If it was, do a new feature release (see above) instead

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is even)

    * `./build-scripts/update-version.sh 2 Y Z+1`

* Regenerate `configure`

* Do the release

## After a feature release

* Create a branch like `release-2.24.x`

* Bump version number to 2.ODD.0 for next development branch

    * `./build-scripts/update-version.sh 2 ODD 0`

## New development prerelease

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is odd)

    * `./build-scripts/update-version.sh 2 Y Z+1`

* Regenerate `configure`

* Do the release
