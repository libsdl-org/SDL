#!/bin/sh

cd "$(dirname $0)/.."

for subdir in build-scripts src test; do
    echo "Running clang-format in $(pwd)/$subdir"
    find $subdir -regex '.*\.[chm]p*' -exec clang-format -i {} \;
done

# Revert third-party code
git checkout \
    src/events/imKStoUCS.* \
    src/hidapi \
    src/joystick/controller_type.c \
    src/joystick/controller_type.h \
    src/joystick/hidapi/steam/controller_constants.h \
    src/joystick/hidapi/steam/controller_structs.h \
    src/libm \
    src/stdlib/SDL_malloc.c \
    src/stdlib/SDL_qsort.c \
    src/stdlib/SDL_strtokr.c \
    src/video/arm \
    src/video/khronos \
    src/video/x11/edid-parse.c \
    src/video/yuv2rgb
clang-format -i src/hidapi/SDL_hidapi.c

# Revert generated code
git checkout \
    src/audio/SDL_audio_channel_converters.h \
    src/audio/SDL_audio_resampler_filter.h \
    src/dynapi/SDL_dynapi_overrides.h \
    src/dynapi/SDL_dynapi_procs.h \
    src/render/metal/SDL_shaders_metal_*.h

echo "clang-format complete!"
