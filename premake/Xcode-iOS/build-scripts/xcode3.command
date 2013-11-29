#!/bin/sh
# change to directory above command file
cd `dirname $0`/..
`dirname $0`/premake4 --file=../premake4.lua --to=./Xcode-iOS --ios xcode3