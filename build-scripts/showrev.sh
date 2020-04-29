#!/bin/sh
#
# Print the current source revision, if available

SDL_ROOT=$(dirname $0)/..
cd $SDL_ROOT

if [ -x "$(command -v hg)" ]; then
    rev="$(hg parents --template 'hg-{rev}:{node|short}' 2>/dev/null)"
    if [ $? = 0 ]; then
        echo $rev
        exit 0
    fi
fi

if [ -x "$(command -v p4)" ]; then
    rev="$(p4 changes -m1 ./...\#have 2>/dev/null| awk '{print $2}')"
    if [ $? = 0 ]; then
        echo $rev
        exit 0
    fi
fi

echo "hg-0:baadf00d"
exit 1
