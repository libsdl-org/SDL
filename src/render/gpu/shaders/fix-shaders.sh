#!/bin/sh
#
# Update generated shader code to fix compiler warnings

sed -i '' 's,^const,static const,' *.h
sed -i '' 's,const unsigned,const signed,' *.dxbc.h
