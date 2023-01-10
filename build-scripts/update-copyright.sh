#!/bin/sh

find . -type f \
| grep -v \.git                                 \
| while read file; do                           \
    LC_ALL=C sed -b -i "s/\(.*Copyright.*\)[0-9]\{4\}\( *Sam Lantinga\)/\1`date +%Y`\2/" "$file"; \
done
