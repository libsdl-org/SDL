# Release checklist

## New feature release

* Update `WhatsNew.txt`

* Bump version number to 2.EVEN.0:

    * `./build-scripts/update-version.sh 2 EVEN 0`
    * (spaces between each component of the version, and `EVEN` will be a real number in real life.

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release

## New bugfix release

* Check that no new API/ABI was added

    * If it was, do a new feature release (see above) instead

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is even)

    * `./build-scripts/update-version.sh 2 Y Z+1`
    * (spaces between each component of the version, and `Y` and `Z+1` will be real numbers in real life.

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release

## After a feature release

* Create a branch like `release-2.24.x`

* Bump version number to 2.ODD.0 for next development branch

    * `./build-scripts/update-version.sh 2 ODD 0`
    * (spaces between each component of the version, and `ODD` will be a real number in real life.

* Run test/versioning.sh to verify that everything is consistent

## New development prerelease

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is odd)

    * `./build-scripts/update-version.sh 2 Y Z+1`
    * (spaces between each component of the version, and `Y` and `Z+1` will be real numbers in real life.

* Regenerate `configure`

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release
