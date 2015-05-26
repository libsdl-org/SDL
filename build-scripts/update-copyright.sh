#!/bin/sh

find . -type f -exec grep -Il "Copyright" {} \;     \
| grep -v \.hg                             \
| while read i;                            \
do \
  sed -ie "s/\(.*Copyright.*\)[0-9]\{4\}\( *Sam Lantinga\)/\1`date +%Y`\2/" "$i"; \
  rm "${i}e"; \
done
