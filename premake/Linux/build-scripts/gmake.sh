#!/bin/sh
# change to directory above shell file
SCRIPTPATH=`readlink -f $0`
SCRIPTDIR=`dirname $SCRIPTPATH`
cd $SCRIPTDIR/..
$SCRIPTDIR/premake4 --file=../premake4.lua --to=./Linux gmake