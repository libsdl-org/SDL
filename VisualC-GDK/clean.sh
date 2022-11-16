#!/bin/sh
find . -type f \( -name '*.user' -o -name '*.sdf' -o -name '*.ncb' -o -name '*.suo' \) -print -delete
find . -type f \( -name '*.bmp' -o -name '*.wav' -o -name '*.dat' \) -print -delete
find . -depth -type d \( -name Gaming.Desktop.x64 \) -exec rm -rv {} \;
find . -depth -type d \( -name Gaming.Xbox.Scarlett.x64 \) -exec rm -rv {} \;
find . -depth -type d \( -name Gaming.Xbox.XboxOne.x64 \) -exec rm -rv {} \;
