#!/bin/sh

commit=$(git rev-parse HEAD)
echo "Creating release workflow for commit $commit"
gh workflow run release.yml --ref main -f commit=$commit

