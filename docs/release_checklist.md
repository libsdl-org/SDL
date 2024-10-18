# Release checklist

* Run `build-scripts/create-release.py -R libsdl-org/SDL --ref <git-ref>` to command
  GitHub Actions to start creating release assets.
  It's advisable to run this script regularly, and also prior to any release step.
  When creating the release assets, `<git-ref>` must be the release tag
  This makes sure the revision string baked into the archives is correct.

* When changing the version, run `build-scripts/update-version.sh X Y Z`,
  where `X Y Z` are the major version, minor version, and patch level. So
  `3 8 1` means "change the version to 3.8.1". This script does much of the
  mechanical work.


## New feature release

* Update `WhatsNew.txt`

* Bump version number to 3.EVEN.0:

    * `./build-scripts/update-version.sh 3 EVEN 0`

* Do the release

* Update the website file include/header.inc.php to reflect the new version

## New bugfix release

* Check that no new API/ABI was added

    * If it was, do a new feature release (see above) instead

* Bump version number from 3.Y.Z to 3.Y.(Z+1) (Y is even)

    * `./build-scripts/update-version.sh 3 Y Z+1`

* Do the release

* Update the website file include/header.inc.php to reflect the new version

## After a feature release

* Create a branch like `release-3.4.x`

* Bump version number to 3.ODD.0 for next development branch

    * `./build-scripts/update-version.sh 3 ODD 0`

## New development prerelease

* Bump version number from 3.Y.Z to 3.Y.(Z+1) (Y is odd)

    * `./build-scripts/update-version.sh 3 Y Z+1`

* Do the release
