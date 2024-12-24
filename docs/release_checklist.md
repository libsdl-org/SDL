# Release checklist

* Run `build-scripts/create-release.py -R libsdl-org/SDL --ref <branch>` to do
  a dry run creating the release assets. Verify that the archives are correct.

* Tag the release, e.g. `git tag release-3.8.0; git push --tags`

* Run `build-scripts/create-release.py -R libsdl-org/SDL --ref <release-tag>`
  to have GitHub Actions create release assets. This makes sure the revision
  string baked into the archives is correct.

* Verify that the source archive REVISION.txt has the correct release tag.

* Sign the source archives and upload everything to libsdl.org

* Create a GitHub release and attach the archives you just generated.

## New feature release

* Update `WhatsNew.txt`

* Bump version number to 3.EVEN.0:

    * `./build-scripts/update-version.sh 3 EVEN 0`

* Do the release

* Immediately create a branch for patch releases, e.g. `git branch release-3.EVEN.x`

* Bump version number from 3.EVEN.0 to 3.(EVEN+1).0

    * `./build-scripts/update-version.sh 3 EVEN+1 0`

* Update the website file include/header.inc.php to reflect the new version

## New bugfix release

* Bump version number from 3.Y.Z to 3.Y.(Z+1) (Y is even)

    * `./build-scripts/update-version.sh 3 Y Z+1`

* Do the release

* Update the website file include/header.inc.php to reflect the new version

## New development prerelease

* Bump version number from 3.Y.Z to 3.Y.(Z+1) (Y is odd)

    * `./build-scripts/update-version.sh 3 Y Z+1`

* Do the release
