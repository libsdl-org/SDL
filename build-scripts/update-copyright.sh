#!/bin/sh

find . -type f -exec grep -Il "Copyright" {} \;     \
| grep -v \.hg                             \
| while read $file;                            \
do \
  LC_ALL=C sed -ie "s/\(.*Copyright.*\)[0-9]\{4\}\( *Sam Lantinga\)/\1`date +%Y`\2/" "$file"; \
  rm "${file}e"; \
done

for file in COPYING.txt VisualC-WinRT/SDL2-WinRT.nuspec VisualC-WinRT/SDL2main-WinRT-NonXAML.nuspec src/main/windows/version.rc visualtest/COPYING.txt
do if [ -f "$file" ]; then
     sed 's/$'"/`echo \\\r`/" $file > $file.new
     mv $file.new $file
   fi
done
