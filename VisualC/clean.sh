find . -type d -name 'Debug' -exec rm -rv {} \;
find . -type d -name 'Release' -exec rm -rv {} \;
find . -type f -name '*.user' -exec rm -v {} \;
find . -type f -name '*.ncb' -exec rm -v {} \;
find . -type f -name '*.suo' -exec rm -v {} \;
