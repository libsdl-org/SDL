LOCAL_PATH := $(call my-dir)

###########################
#
# SDL shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := SDL3

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/include/build_config $(LOCAL_PATH)/src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/aaudio/*.c) \
	$(wildcard $(LOCAL_PATH)/src/audio/openslES/*.c) \
	$(LOCAL_PATH)/src/atomic/SDL_atomic.c.arm \
	$(LOCAL_PATH)/src/atomic/SDL_spinlock.c.arm \
	$(wildcard $(LOCAL_PATH)/src/camera/*.c) \
	$(wildcard $(LOCAL_PATH)/src/camera/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/camera/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/core/*.c) \
	$(wildcard $(LOCAL_PATH)/src/core/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/cpuinfo/*.c) \
	$(LOCAL_PATH)/src/dialog/SDL_dialog_utils.c \
	$(LOCAL_PATH)/src/dialog/android/SDL_androiddialog.c \
	$(wildcard $(LOCAL_PATH)/src/dynapi/*.c) \
	$(wildcard $(LOCAL_PATH)/src/events/*.c) \
	$(wildcard $(LOCAL_PATH)/src/file/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gpu/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gpu/vulkan/*.c) \
	$(wildcard $(LOCAL_PATH)/src/haptic/*.c) \
	$(wildcard $(LOCAL_PATH)/src/haptic/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/haptic/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/hidapi/*.c) \
	$(wildcard $(LOCAL_PATH)/src/hidapi/android/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/joystick/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/hidapi/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/steam/*.c) \
	$(wildcard $(LOCAL_PATH)/src/joystick/virtual/*.c) \
	$(wildcard $(LOCAL_PATH)/src/loadso/dlopen/*.c) \
	$(wildcard $(LOCAL_PATH)/src/locale/*.c) \
	$(wildcard $(LOCAL_PATH)/src/locale/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/main/*.c) \
	$(wildcard $(LOCAL_PATH)/src/main/generic/*.c) \
	$(wildcard $(LOCAL_PATH)/src/misc/*.c) \
	$(wildcard $(LOCAL_PATH)/src/misc/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/power/*.c) \
	$(wildcard $(LOCAL_PATH)/src/power/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/process/*.c) \
	$(wildcard $(LOCAL_PATH)/src/process/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/filesystem/*.c) \
	$(wildcard $(LOCAL_PATH)/src/filesystem/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/filesystem/posix/*.c) \
	$(wildcard $(LOCAL_PATH)/src/sensor/*.c) \
	$(wildcard $(LOCAL_PATH)/src/sensor/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/sensor/dummy/*.c) \
	$(wildcard $(LOCAL_PATH)/src/render/*.c) \
	$(wildcard $(LOCAL_PATH)/src/render/*/*.c) \
	$(wildcard $(LOCAL_PATH)/src/stdlib/*.c) \
	$(wildcard $(LOCAL_PATH)/src/storage/*.c) \
	$(wildcard $(LOCAL_PATH)/src/storage/generic/*.c) \
	$(wildcard $(LOCAL_PATH)/src/thread/*.c) \
	$(wildcard $(LOCAL_PATH)/src/thread/pthread/*.c) \
	$(wildcard $(LOCAL_PATH)/src/time/*.c) \
	$(wildcard $(LOCAL_PATH)/src/time/unix/*.c) \
	$(wildcard $(LOCAL_PATH)/src/timer/*.c) \
	$(wildcard $(LOCAL_PATH)/src/timer/unix/*.c) \
	$(wildcard $(LOCAL_PATH)/src/video/*.c) \
	$(wildcard $(LOCAL_PATH)/src/video/android/*.c) \
	$(wildcard $(LOCAL_PATH)/src/video/yuv2rgb/*.c))

LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES
LOCAL_CFLAGS += \
	-Wall -Wextra \
	-Wmissing-prototypes \
	-Wunreachable-code-break \
	-Wunneeded-internal-declaration \
	-Wmissing-variable-declarations \
	-Wfloat-conversion \
	-Wshorten-64-to-32 \
	-Wunreachable-code-return \
	-Wshift-sign-overflow \
	-Wstrict-prototypes \
	-Wkeyword-macro \

# Warnings we haven't fixed (yet)
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-sign-compare

LOCAL_CXXFLAGS += -std=gnu++11

LOCAL_LDLIBS := -ldl -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

LOCAL_LDFLAGS := -Wl,--no-undefined -Wl,--no-undefined-version -Wl,--version-script=$(LOCAL_PATH)/src/dynapi/SDL_dynapi.sym

ifeq ($(NDK_DEBUG),1)
    cmd-strip :=
endif

LOCAL_STATIC_LIBRARIES := cpufeatures

include $(BUILD_SHARED_LIBRARY)


###########################
#
# SDL_test static library
#
###########################

LOCAL_MODULE := SDL3_test

LOCAL_MODULE_FILENAME := libSDL3_test

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/test/*.c))

LOCAL_LDLIBS :=

LOCAL_LDFLAGS :=

LOCAL_EXPORT_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)


###########################
#
# SDL static library
#
###########################

LOCAL_MODULE := SDL3_static

LOCAL_MODULE_FILENAME := libSDL3

LOCAL_LDLIBS :=

LOCAL_LDFLAGS :=

LOCAL_EXPORT_LDLIBS := -ldl -lGLESv1_CM -lGLESv2 -llog -landroid

include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/cpufeatures)

