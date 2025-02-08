#!/bin/sh

cd "$(dirname $0)/../src"

echo "Running clang-format in $(pwd)"

find . -regex '.*\.[chm]p*' -exec clang-format -i {} \;

# Revert third-party code
git checkout \
    events/imKStoUCS.* \
    hidapi \
    joystick/controller_type.c \
    joystick/controller_type.h \
    joystick/hidapi/steam/controller_constants.h \
    joystick/hidapi/steam/controller_structs.h \
    libm \
    stdlib/SDL_malloc.c \
    stdlib/SDL_qsort.c \
    stdlib/SDL_strtokr.c \
    video/khronos \
    video/x11/edid.h \
    video/x11/edid-parse.c \
    video/x11/xsettings-client.* \
    video/yuv2rgb
clang-format -i hidapi/SDL_hidapi.c

# Revert generated code
git checkout \
    dynapi/SDL_dynapi_overrides.h \
    dynapi/SDL_dynapi_procs.h \
    render/*/*Shader*.h \
    render/metal/SDL_shaders_metal_*.h \
    render/vitagxm/SDL_render_vita_gxm_shaders.h \
    video/directx/SDL_d3d12_xbox_cmacros.h \
    video/directx/d3d12.h \
    video/directx/d3d12sdklayers.h \

echo "clang-format complete!"
