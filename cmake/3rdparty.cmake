function(get_clang_tidy_ignored_files OUTVAR)
  set(3RD_PARTY_SOURCES
      # Public GL headers
      "SDL_egl.h"
      "SDL_hidapi.h"
      "SDL_opengl.h"
      "SDL_opengl_glext.h"
      "SDL_opengles2_gl2.h"
      "SDL_opengles2_gl2ext.h"
      "SDL_opengles2_gl2platform.h"
      "SDL_opengles2_khrplatform.h"
      # stdlib
      "SDL_malloc.c"
      "SDL_qsort.c"
      "SDL_strtokr.c"
      # edid
      "edid-parse.c"
      "edid.h"
      # imKStoUCS
      "imKStoUCS.c"
      "imKStoUCS.h"
      # Joystick controller type
      "controller_type.h"
      "controller_type.c"
      # HIDAPI Steam controller
      "controller_constants.h"
      "controller_structs.h"
      # YUV2RGB
      "yuv_rgb.c"
      "yuv_rgb_lsx_func.h"
      "yuv_rgb_sse_func.h"
      "yuv_rgb_std_func.h"
      # LIBM
      "e_atan2.c"
      "e_exp.c"
      "e_fmod.c"
      "e_log10.c"
      "e_log.c"
      "e_pow.c"
      "e_rem_pio2.c"
      "e_sqrt.c"
      "k_cos.c"
      "k_rem_pio2.c"
      "k_sin.c"
      "k_tan.c"
      "s_atan.c"
      "s_copysign.c"
      "s_cos.c"
      "s_fabs.c"
      "s_floor.c"
      "s_scalbn.c"
      "s_sin.c"
      "s_tan.c"
      "math_private.h"
      "math_libm.h"
      # EGL
      "egl.h"
      "eglext.h"
      "eglplatform.h"
      # GLES2
      "gl2.h"
      "gl2ext.h"
      "gl2platform.h"
      # KHR
      "khrplatform.h"
      # Vulkan
      "vk_icd.h"
      "vk_layer.h"
      "vk_platform.h"
      "vk_sdk_platform.h"
      "vulkan_android.h"
      "vulkan_beta.h"
      "vulkan_core.h"
      "vulkan_directfb.h"
      "vulkan_fuchsia.h"
      "vulkan_ggp.h"
      "vulkan_ios.h"
      "vulkan_macos.h"
      "vulkan_metal.h"
      "vulkan_screen.h"
      "vulkan_vi.h"
      "vulkan_wayland.h"
      "vulkan_win32.h"
      "vulkan_xcb.h"
      "vulkan_xlib_xrandr.h"
      "vulkan_xlib.h"
      "vulkan.h"
      "vulkan_enums.hpp"
      "vulkan_format_traits.hpp"
      "vulkan_funcs.hpp"
      "vulkan_handles.hpp"
      "vulkan_hash.hpp"
      "vulkan_raii.hpp"
      "vulkan_static_assertions.hpp"
      "vulkan_structs.hpp"
      "vulkan_to_string.hpp"
      # HIDAPI
      "hid.c"
      "hid.cpp"
      "hid.m"
      "hidraw.cpp"
      "hidusb.cpp"
      "hidapi.h"
      # XSETTINGS
      "xsettings-client.c"
      "xsettings-client.h")

  foreach(SOURCE_FILE ${3RD_PARTY_SOURCES})
    list(APPEND IGNORED_LIST "{\"name\":\"${SOURCE_FILE}\",\"lines\":[[1,1]]}")
  endforeach()

  string(REPLACE ";" "," IGNORED_FILES "${IGNORED_LIST}")
  set(${OUTVAR}
      "${IGNORED_FILES}"
      PARENT_SCOPE)
endfunction()
