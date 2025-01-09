#!/bin/sh

testsTotal=0
testsPassed=0
testsFailed=0
testsSkipped=0

function build() {
	testsTotal=$(($testsTotal + 1))
	if [ -d "tests/$1" ]; then
		cd tests/$1
		"xcodebuild" ARCHS=x86_64 ONLY_ACTIVE_ARCH=NO -project "$1.xcodeproj/"
		if [ $? -ne 0 ]; then
			testsFailed=$(($testsFailed + 1))
		else
			testsPassed=$(($testsPassed + 1))
		fi
		cd ../..
		echo "\033]0;Building: $1\007"
	else
		testsSkipped=$(($testsSkipped + 1))
	fi
}

# change to directory above command file
cd `dirname $0`/..

# build all of the tests
for d in ./tests/*; do
	build `basename $d`
done

echo "Build Summary: Total=$testsTotal Passed=$testsPassed Failed=$testsFailed Skipped=$testsSkipped"

cd ..