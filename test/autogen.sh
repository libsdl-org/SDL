#!/bin/sh

set -e

cp acinclude.m4 aclocal.m4

"${AUTOCONF:-autoconf}"
rm aclocal.m4
rm -rf autom4te.cache
