#!/bin/sh
#
# Print the current source revision hash, if available

SDL_ROOT=$(dirname $0)/..
cd $SDL_ROOT

if [ -x "$(command -v git)" ]; then
    rev="$(git rev-parse HEAD 2>/dev/null)"
    if [ -n "$rev" ]; then
        echo "$rev"
        exit 0
    fi
fi

if [ -x "$(command -v p4)" ]; then
    rev="$(p4 changes -m1 ./...\#have 2>/dev/null| awk '{print $2}')"
    if [ $? = 0 ]; then
        # e.g. p7511446
        echo "p${rev}"
        exit 0
    fi
fi

# output nothing if we couldn't figure it out.
exit 0

