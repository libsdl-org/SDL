#!/bin/sh

function pause() {
  read -p "$*"
}

function pass() {
	if [ -d "$1/Build/Debug" ]; then
	cd $1/Build/Debug
	echo "Testing:" $1
	echo "\033]0;Testing: $1\007"
	"./$@"
	cd ../../..
	pause "Press any key to continue..."
	fi
}

function randomfile() {
	fcount=($1/*.*)
	fcount=${#fcount[@]}
	fpick=$(($RANDOM % $fcount))
	for d in $1/*.*; do
		if [[ $fpick -eq 0 ]]; then
			RETURN=$d
			echo $d
			return
		fi
		fpick=$(($fpick - 1))
	done
}

function testspecial() {
	if [ -d "$1/Build/Debug" ]; then
		cd $1/Build/Debug
		randomfile $2
		cd ../../..
		pass $1 $RETURN
	fi
}

# change to directory above command file
cd `dirname $0`/..
cd tests

pass "checkkeys"
pass "loopwave"
#pass "testatomic"
pass "testaudioinfo"
pass "testautomation"
pass "testdraw2"
pass "testchessboard"
pass "testerror"
pass "testfile"
pass "testfilesystem"
pass "testgamecontroller"
pass "testgesture"
pass "testgl2"
pass "testgles"
pass "testhaptic"
pass "testiconv"
pass "testime"
pass "testintersection"
pass "testjoystick"
pass "testkeys"
#pass "testloadso"
pass "testlock"
pass "testmessage"
#pass "testmultiaudio"
pass "testnative"
pass "testoverlay2"
pass "testplatform"
pass "testpower"
pass "testrelative"
pass "testrendercopyex"
pass "testrendertarget"
pass "testresample" "sample.wav" "newsample.wav" "44100"
pass "testrumble"
pass "testscale"
pass "testsem" 1
pass "testshader"
#testspecial "testshape" ./shapes
#testspecial "testshape" ./shapes
#testspecial "testshape" ./shapes
pass "testsprite2"
pass "testspriteminimal"
pass "teststreaming"
pass "testthread"
pass "testtimer"
pass "testver"
pass "testwm2"
pass "torturethread"

cd ..