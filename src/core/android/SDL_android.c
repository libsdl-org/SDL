/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_PLATFORM_ANDROID

#include "SDL_android.h"

#include "../../events/SDL_events_c.h"
#include "../../video/android/SDL_androidkeyboard.h"
#include "../../video/android/SDL_androidmouse.h"
#include "../../video/android/SDL_androidtouch.h"
#include "../../video/android/SDL_androidpen.h"
#include "../../video/android/SDL_androidvideo.h"
#include "../../video/android/SDL_androidwindow.h"
#include "../../video/android/SDL_androidevents.h"
#include "../../joystick/android/SDL_sysjoystick_c.h"
#include "../../haptic/android/SDL_syshaptic_c.h"
#include "../../hidapi/android/hid.h"
#include "../../SDL_hints_c.h"

#include <android/log.h>
#include <android/configuration.h>
#include <android/asset_manager_jni.h>
#include <sys/system_properties.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#define SDL_JAVA_PREFIX                               org_libsdl_app
#define CONCAT1(prefix, class, function)              CONCAT2(prefix, class, function)
#define CONCAT2(prefix, class, function)              Java_##prefix##_##class##_##function
#define SDL_JAVA_INTERFACE(function)                  CONCAT1(SDL_JAVA_PREFIX, SDLActivity, function)
#define SDL_JAVA_AUDIO_INTERFACE(function)            CONCAT1(SDL_JAVA_PREFIX, SDLAudioManager, function)
#define SDL_JAVA_CONTROLLER_INTERFACE(function)       CONCAT1(SDL_JAVA_PREFIX, SDLControllerManager, function)
#define SDL_JAVA_INTERFACE_INPUT_CONNECTION(function) CONCAT1(SDL_JAVA_PREFIX, SDLInputConnection, function)

// Java class SDLActivity
JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetVersion)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeInitMainThread)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeCleanupMainThread)(
    JNIEnv *env, jclass cls);

JNIEXPORT int JNICALL SDL_JAVA_INTERFACE(nativeRunMain)(
    JNIEnv *env, jclass cls,
    jstring library, jstring function, jobject array);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDropFile)(
    JNIEnv *env, jclass jcls,
    jstring filename);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetScreenResolution)(
    JNIEnv *env, jclass jcls,
    jint surfaceWidth, jint surfaceHeight,
    jint deviceWidth, jint deviceHeight, jfloat density, jfloat rate);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeResize)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceCreated)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceChanged)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeScreenKeyboardShown)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeScreenKeyboardHidden)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyDown)(
    JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyUp)(
    JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(onNativeSoftReturnKey)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeTouch)(
    JNIEnv *env, jclass jcls,
    jint touch_device_id_in, jint pointer_finger_id_in,
    jint action, jfloat x, jfloat y, jfloat p);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchStart)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchUpdate)(
    JNIEnv *env, jclass jcls,
    jfloat scale);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchEnd)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeMouse)(
    JNIEnv *env, jclass jcls,
    jint button, jint action, jfloat x, jfloat y, jboolean relative);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePen)(
    JNIEnv *env, jclass jcls,
    jint pen_id_in, jint device_type, jint button, jint action, jfloat x, jfloat y, jfloat p);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeAccel)(
    JNIEnv *env, jclass jcls,
    jfloat x, jfloat y, jfloat z);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeClipboardChanged)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeLowMemory)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeLocaleChanged)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDarkModeChanged)(
    JNIEnv *env, jclass cls, jboolean enabled);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSendQuit)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeQuit)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePause)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeResume)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeFocusChanged)(
    JNIEnv *env, jclass cls, jboolean hasFocus);

JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetHint)(
    JNIEnv *env, jclass cls,
    jstring name);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeGetHintBoolean)(
    JNIEnv *env, jclass cls,
    jstring name, jboolean default_value);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetenv)(
    JNIEnv *env, jclass cls,
    jstring name, jstring value);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetNaturalOrientation)(
    JNIEnv *env, jclass cls,
    jint orientation);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeRotationChanged)(
    JNIEnv *env, jclass cls,
    jint rotation);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeInsetsChanged)(
    JNIEnv *env, jclass cls,
    jint left, jint right, jint top, jint bottom);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeAddTouch)(
    JNIEnv *env, jclass cls,
    jint touchId, jstring name);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePermissionResult)(
    JNIEnv *env, jclass cls,
    jint requestCode, jboolean result);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeAllowRecreateActivity)(
    JNIEnv *env, jclass jcls);

JNIEXPORT int JNICALL SDL_JAVA_INTERFACE(nativeCheckSDLThreadCounter)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeFileDialog)(
    JNIEnv *env, jclass jcls,
    jint requestCode, jobjectArray fileList, jint filter);

static JNINativeMethod SDLActivity_tab[] = {
    { "nativeGetVersion", "()Ljava/lang/String;", SDL_JAVA_INTERFACE(nativeGetVersion) },
    { "nativeSetupJNI", "()V", SDL_JAVA_INTERFACE(nativeSetupJNI) },
    { "nativeInitMainThread", "()V", SDL_JAVA_INTERFACE(nativeInitMainThread) },
    { "nativeCleanupMainThread", "()V", SDL_JAVA_INTERFACE(nativeCleanupMainThread) },
    { "nativeRunMain", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)I", SDL_JAVA_INTERFACE(nativeRunMain) },
    { "onNativeDropFile", "(Ljava/lang/String;)V", SDL_JAVA_INTERFACE(onNativeDropFile) },
    { "nativeSetScreenResolution", "(IIIIFF)V", SDL_JAVA_INTERFACE(nativeSetScreenResolution) },
    { "onNativeResize", "()V", SDL_JAVA_INTERFACE(onNativeResize) },
    { "onNativeSurfaceCreated", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceCreated) },
    { "onNativeSurfaceChanged", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceChanged) },
    { "onNativeSurfaceDestroyed", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed) },
    { "onNativeScreenKeyboardShown", "()V", SDL_JAVA_INTERFACE(onNativeScreenKeyboardShown) },
    { "onNativeScreenKeyboardHidden", "()V", SDL_JAVA_INTERFACE(onNativeScreenKeyboardHidden) },
    { "onNativeKeyDown", "(I)V", SDL_JAVA_INTERFACE(onNativeKeyDown) },
    { "onNativeKeyUp", "(I)V", SDL_JAVA_INTERFACE(onNativeKeyUp) },
    { "onNativeSoftReturnKey", "()Z", SDL_JAVA_INTERFACE(onNativeSoftReturnKey) },
    { "onNativeKeyboardFocusLost", "()V", SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost) },
    { "onNativeTouch", "(IIIFFF)V", SDL_JAVA_INTERFACE(onNativeTouch) },
    { "onNativePinchStart", "()V", SDL_JAVA_INTERFACE(onNativePinchStart) },
    { "onNativePinchUpdate", "(F)V", SDL_JAVA_INTERFACE(onNativePinchUpdate) },
    { "onNativePinchEnd", "()V", SDL_JAVA_INTERFACE(onNativePinchEnd) },
    { "onNativeMouse", "(IIFFZ)V", SDL_JAVA_INTERFACE(onNativeMouse) },
    { "onNativePen", "(IIIIFFF)V", SDL_JAVA_INTERFACE(onNativePen) },
    { "onNativeAccel", "(FFF)V", SDL_JAVA_INTERFACE(onNativeAccel) },
    { "onNativeClipboardChanged", "()V", SDL_JAVA_INTERFACE(onNativeClipboardChanged) },
    { "nativeLowMemory", "()V", SDL_JAVA_INTERFACE(nativeLowMemory) },
    { "onNativeLocaleChanged", "()V", SDL_JAVA_INTERFACE(onNativeLocaleChanged) },
    { "onNativeDarkModeChanged", "(Z)V", SDL_JAVA_INTERFACE(onNativeDarkModeChanged) },
    { "nativeSendQuit", "()V", SDL_JAVA_INTERFACE(nativeSendQuit) },
    { "nativeQuit", "()V", SDL_JAVA_INTERFACE(nativeQuit) },
    { "nativePause", "()V", SDL_JAVA_INTERFACE(nativePause) },
    { "nativeResume", "()V", SDL_JAVA_INTERFACE(nativeResume) },
    { "nativeFocusChanged", "(Z)V", SDL_JAVA_INTERFACE(nativeFocusChanged) },
    { "nativeGetHint", "(Ljava/lang/String;)Ljava/lang/String;", SDL_JAVA_INTERFACE(nativeGetHint) },
    { "nativeGetHintBoolean", "(Ljava/lang/String;Z)Z", SDL_JAVA_INTERFACE(nativeGetHintBoolean) },
    { "nativeSetenv", "(Ljava/lang/String;Ljava/lang/String;)V", SDL_JAVA_INTERFACE(nativeSetenv) },
    { "nativeSetNaturalOrientation", "(I)V", SDL_JAVA_INTERFACE(nativeSetNaturalOrientation) },
    { "onNativeRotationChanged", "(I)V", SDL_JAVA_INTERFACE(onNativeRotationChanged) },
    { "onNativeInsetsChanged", "(IIII)V", SDL_JAVA_INTERFACE(onNativeInsetsChanged) },
    { "nativeAddTouch", "(ILjava/lang/String;)V", SDL_JAVA_INTERFACE(nativeAddTouch) },
    { "nativePermissionResult", "(IZ)V", SDL_JAVA_INTERFACE(nativePermissionResult) },
    { "nativeAllowRecreateActivity", "()Z", SDL_JAVA_INTERFACE(nativeAllowRecreateActivity) },
    { "nativeCheckSDLThreadCounter", "()I", SDL_JAVA_INTERFACE(nativeCheckSDLThreadCounter) },
    { "onNativeFileDialog", "(I[Ljava/lang/String;I)V", SDL_JAVA_INTERFACE(onNativeFileDialog) }
};

// Java class SDLInputConnection
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText)(
    JNIEnv *env, jclass cls,
    jstring text, jint newCursorPosition);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar)(
    JNIEnv *env, jclass cls,
    jchar chUnicode);

static JNINativeMethod SDLInputConnection_tab[] = {
    { "nativeCommitText", "(Ljava/lang/String;I)V", SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText) },
    { "nativeGenerateScancodeForUnichar", "(C)V", SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar) }
};

// Java class SDLAudioManager
JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL
    SDL_JAVA_AUDIO_INTERFACE(nativeAddAudioDevice)(JNIEnv *env, jclass jcls, jboolean recording, jstring name,
                                             jint device_id);

JNIEXPORT void JNICALL
    SDL_JAVA_AUDIO_INTERFACE(nativeRemoveAudioDevice)(JNIEnv *env, jclass jcls, jboolean recording,
                                                jint device_id);

static JNINativeMethod SDLAudioManager_tab[] = {
    { "nativeSetupJNI", "()V", SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI) },
    { "nativeAddAudioDevice", "(ZLjava/lang/String;I)V", SDL_JAVA_AUDIO_INTERFACE(nativeAddAudioDevice) },
    { "nativeRemoveAudioDevice", "(ZI)V", SDL_JAVA_AUDIO_INTERFACE(nativeRemoveAudioDevice) }
};

// Java class SDLControllerManager
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass jcls);

JNIEXPORT jboolean JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT jboolean JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint axis, jfloat value);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint hat_id, jint x, jint y);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name, jstring device_desc, jint vendor_id, jint product_id,
    jint button_mask, jint naxes, jint axis_mask, jint nhats, jboolean can_rumble, jboolean has_rgb_led);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic)(
    JNIEnv *env, jclass jcls,
    jint device_id);

static JNINativeMethod SDLControllerManager_tab[] = {
    { "nativeSetupJNI", "()V", SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI) },
    { "onNativePadDown", "(II)Z", SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown) },
    { "onNativePadUp", "(II)Z", SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp) },
    { "onNativeJoy", "(IIF)V", SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy) },
    { "onNativeHat", "(IIII)V", SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat) },
    { "nativeAddJoystick", "(ILjava/lang/String;Ljava/lang/String;IIIIIIZZ)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick) },
    { "nativeRemoveJoystick", "(I)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick) },
    { "nativeAddHaptic", "(ILjava/lang/String;)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic) },
    { "nativeRemoveHaptic", "(I)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic) }
};

// Uncomment this to log messages entering and exiting methods in this file
// #define DEBUG_JNI
// #define DEBUG_RPC

static void checkJNIReady(void);

/*******************************************************************************
 This file links the Java side of Android with libsdl
*******************************************************************************/
#include <jni.h>

/*******************************************************************************
                               Globals
*******************************************************************************/
static pthread_key_t mThreadKey;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static JavaVM *mJavaVM = NULL;

// Main activity
static jclass mActivityClass;

// method signatures
static jmethodID midClipboardGetText;
static jmethodID midClipboardHasText;
static jmethodID midClipboardSetText;
static jmethodID midCreateCustomCursor;
static jmethodID midDestroyCustomCursor;
static jmethodID midGetContext;
static jmethodID midGetManifestEnvironmentVariables;
static jmethodID midGetNativeSurface;
static jmethodID midInitTouch;
static jmethodID midIsAndroidTV;
static jmethodID midIsChromebook;
static jmethodID midIsDeXMode;
static jmethodID midIsTablet;
static jmethodID midManualBackButton;
static jmethodID midMinimizeWindow;
static jmethodID midOpenURL;
static jmethodID midRequestPermission;
static jmethodID midShowToast;
static jmethodID midSendMessage;
static jmethodID midSetActivityTitle;
static jmethodID midSetCustomCursor;
static jmethodID midSetOrientation;
static jmethodID midSetRelativeMouseEnabled;
static jmethodID midSetSystemCursor;
static jmethodID midSetWindowStyle;
static jmethodID midShouldMinimizeOnFocusLoss;
static jmethodID midShowTextInput;
static jmethodID midSupportsRelativeMouse;
static jmethodID midOpenFileDescriptor;
static jmethodID midShowFileDialog;
static jmethodID midGetPreferredLocales;

// audio manager
static jclass mAudioManagerClass;

// method signatures
static jmethodID midRegisterAudioDeviceCallback;
static jmethodID midUnregisterAudioDeviceCallback;
static jmethodID midAudioSetThreadPriority;

// controller manager
static jclass mControllerManagerClass;

// method signatures
static jmethodID midPollInputDevices;
static jmethodID midJoystickSetLED;
static jmethodID midPollHapticDevices;
static jmethodID midHapticRun;
static jmethodID midHapticRumble;
static jmethodID midHapticStop;

// Accelerometer data storage
static SDL_DisplayOrientation displayNaturalOrientation;
static SDL_DisplayOrientation displayCurrentOrientation;
static float fLastAccelerometer[3];
static bool bHasNewData;

static bool bHasEnvironmentVariables;

// Android AssetManager
static void Internal_Android_Create_AssetManager(void);
static void Internal_Android_Destroy_AssetManager(void);
static AAssetManager *asset_manager = NULL;
static jobject javaAssetManagerRef = 0;

static SDL_Mutex *Android_LifecycleMutex = NULL;
static SDL_Semaphore *Android_LifecycleEventSem = NULL;
static SDL_AndroidLifecycleEvent Android_LifecycleEvents[SDL_NUM_ANDROID_LIFECYCLE_EVENTS];
static int Android_NumLifecycleEvents;

// RPC commands. from SDLActivity thread to C Thread
typedef enum {

    // SDLActivity_tab

    // RPC_cmd_nativeGetVersion,
    // RPC_cmd_nativeInitMainThread,
    // RPC_cmd_nativeCleanupMainThread,
    // RPC_cmd_nativeRunMain,
    RPC_cmd_onNativeDropFile,
    RPC_cmd_nativeSetScreenResolution,
    RPC_cmd_onNativeResize,
    RPC_cmd_onNativeSurfaceCreated,
    RPC_cmd_onNativeSurfaceChanged,
    RPC_cmd_onNativeSurfaceDestroyed,
    RPC_cmd_onNativeScreenKeyboardShown,
    RPC_cmd_onNativeScreenKeyboardHidden,
    RPC_cmd_onNativeKeyDown,
    RPC_cmd_onNativeKeyUp,
    RPC_cmd_onNativeSoftReturnKey,
    RPC_cmd_onNativeKeyboardFocusLost,
    RPC_cmd_onNativeTouch,
    RPC_cmd_onNativePinchStart,
    RPC_cmd_onNativePinchUpdate,
    RPC_cmd_onNativePinchEnd,
    RPC_cmd_onNativeMouse,
    RPC_cmd_onNativePen,
    RPC_cmd_onNativeAccel,
    RPC_cmd_onNativeClipboardChanged,
// lifecycle event
//    RPC_cmd_nativeLowMemory,
    RPC_cmd_onNativeLocaleChanged,
    RPC_cmd_onNativeDarkModeChanged,
// lifecycle event
//    RPC_cmd_nativeSendQuit,
// onDestroy / inverse of nativeSetupJNI
//    RPC_cmd_nativeQuit,
// lifecycle event
//    RPC_cmd_nativePause,
//    RPC_cmd_nativeResume,
    RPC_cmd_nativeFocusChanged,
// there are not returned void and so, they cannot really be defered to C Thread:
//    RPC_cmd_nativeGetHint,
//    RPC_cmd_nativeGetHintBoolean,

// special case with recreate activity:
//    RPC_cmd_nativeSetenv,
//
    RPC_cmd_nativeSetNaturalOrientation,
    RPC_cmd_onNativeRotationChanged,
    RPC_cmd_onNativeInsetsChanged,
    RPC_cmd_nativeAddTouch,
    RPC_cmd_nativePermissionResult,

// only used within the SDLActivity thread:
//    RPC_cmd_nativeAllowRecreateActivity,
//    RPC_cmd_nativeCheckSDLThreadCounter,
    RPC_cmd_onNativeFileDialog,

    // SDLInputConnection_tab
    RPC_cmd_nativeCommitText,
    RPC_cmd_nativeGenerateScancodeForUnichar,

    // SDLControllerManager_tab
#ifdef GAMEPAD_AS_RPC
    RPC_cmd_onNativePadDown,
    RPC_cmd_onNativePadUp,
    RPC_cmd_onNativeJoy,
    RPC_cmd_onNativeHat,
    RPC_cmd_nativeAddJoystick,
    RPC_cmd_nativeRemoveJoystick,
    RPC_cmd_nativeAddHaptic,
    RPC_cmd_nativeRemoveHaptic,
#endif

    // SDLAudioManager_tab
    RPC_cmd_nativeAddAudioDevice,
    RPC_cmd_nativeRemoveAudioDevice

    // RPC TODO HID ? see HIDDeviceManager_tab

} RPC_cmd_t;


#ifdef DEBUG_RPC

static const char *cmd2Str(RPC_cmd_t cmd) {
    switch (cmd) {
#define CASE(x)     case RPC_cmd_ ## x: return #x;
    // SDLActivity_tab
    CASE(onNativeDropFile);
    CASE(nativeSetScreenResolution);
    CASE(onNativeResize);
    CASE(onNativeSurfaceCreated);
    CASE(onNativeSurfaceChanged);
    CASE(onNativeSurfaceDestroyed);
    CASE(onNativeScreenKeyboardShown);
    CASE(onNativeScreenKeyboardHidden);
    CASE(onNativeKeyDown);
    CASE(onNativeKeyUp);
    CASE(onNativeSoftReturnKey);
    CASE(onNativeKeyboardFocusLost);
    CASE(onNativeTouch);
    CASE(onNativePinchStart);
    CASE(onNativePinchUpdate);
    CASE(onNativePinchEnd);
    CASE(onNativeMouse);
    CASE(onNativePen);
    CASE(onNativeAccel);
    CASE(onNativeClipboardChanged);
    CASE(onNativeLocaleChanged);
    CASE(onNativeDarkModeChanged);
    CASE(nativeFocusChanged);
    CASE(nativeSetNaturalOrientation);
    CASE(onNativeRotationChanged);
    CASE(onNativeInsetsChanged);
    CASE(nativeAddTouch);
    CASE(nativePermissionResult);
    CASE(onNativeFileDialog);

    // SDLInputConnection_tab
    CASE(nativeCommitText);
    CASE(nativeGenerateScancodeForUnichar);

    // SDLAudioManager_tab
    CASE(nativeAddAudioDevice);
    CASE(nativeRemoveAudioDevice);

#ifdef GAMEPAD_AS_RPC
    // SDLControllerManager_tab
    CASE(onNativePadDown);
    CASE(onNativePadUp);
    CASE(onNativeJoy);
    CASE(onNativeHat);
    CASE(nativeAddJoystick);
    CASE(nativeRemoveJoystick);
    CASE(nativeAddHaptic);
    CASE(nativeRemoveHaptic);
#endif

#undef CASE
        default:
            return "unknown";
    }
}
#endif


// RPC TODO: enable timestamp ?
#define RPC_Send                                \
    /* data.timestamp = SDL_GetTicks(); */      \
    RPC_Send__(&data, sizeof(data));            \

#define RPC_Add(foo)    data.foo = foo;

#define RPC_AddString(foo)                                              \
    const char *utf##foo = (*env)->GetStringUTFChars(env, foo, NULL);   \
    data.foo = SDL_strdup(utf##foo);                                    \
    (*env)->ReleaseStringUTFChars(env, foo, utf##foo);                  \

#define RPC_Prepare(foo)                \
    RPC_data_##foo##_t data;            \
    data.cmd = RPC_cmd_##foo;           \

#define RPC_SendWithoutData(foo)        \
    RPC_data_t data;                    \
    data.cmd = RPC_cmd_##foo;           \
    data.timestamp = SDL_GetTicks();    \
    RPC_Send__(&data, sizeof(data));    \

static void RPC_Send__(void *data, int len);
static void RPC_Init();

// header for command without data needed
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
} RPC_data_t;


/*******************************************************************************
                 Functions called by JNI
*******************************************************************************/

/* From http://developer.android.com/guide/practices/jni.html
 * All threads are Linux threads, scheduled by the kernel.
 * They're usually started from managed code (using Thread.start), but they can also be created elsewhere and then
 * attached to the JavaVM. For example, a thread started with pthread_create can be attached with the
 * JNI AttachCurrentThread or AttachCurrentThreadAsDaemon functions. Until a thread is attached, it has no JNIEnv,
 * and cannot make JNI calls.
 * Attaching a natively-created thread causes a java.lang.Thread object to be constructed and added to the "main"
 * ThreadGroup, making it visible to the debugger. Calling AttachCurrentThread on an already-attached thread
 * is a no-op.
 * Note: You can call this function any number of times for the same thread, there's no harm in it
 */

/* From http://developer.android.com/guide/practices/jni.html
 * Threads attached through JNI must call DetachCurrentThread before they exit. If coding this directly is awkward,
 * in Android 2.0 (Eclair) and higher you can use pthread_key_create to define a destructor function that will be
 * called before the thread exits, and call DetachCurrentThread from there. (Use that key with pthread_setspecific
 * to store the JNIEnv in thread-local-storage; that way it'll be passed into your destructor as the argument.)
 * Note: The destructor is not called unless the stored value is != NULL
 * Note: You can call this function any number of times for the same thread, there's no harm in it
 *       (except for some lost CPU cycles)
 */

// Set local storage value
static bool Android_JNI_SetEnv(JNIEnv *env)
{
    int status = pthread_setspecific(mThreadKey, env);
    if (status < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed pthread_setspecific() in Android_JNI_SetEnv() (err=%d)", status);
        return false;
    }
    return true;
}

// Get local storage value
JNIEnv *Android_JNI_GetEnv(void)
{
    // Get JNIEnv from the Thread local storage
    JNIEnv *env = pthread_getspecific(mThreadKey);
    if (!env) {
        // If it fails, try to attach ! (e.g the thread isn't created with SDL_CreateThread()
        int status;

        // There should be a JVM
        if (!mJavaVM) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed, there is no JavaVM");
            return NULL;
        }

        /* Attach the current thread to the JVM and get a JNIEnv.
         * It will be detached by pthread_create destructor 'Android_JNI_ThreadDestroyed' */
        status = (*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL);
        if (status < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed to attach current thread (err=%d)", status);
            return NULL;
        }

        // Save JNIEnv into the Thread local storage
        if (!Android_JNI_SetEnv(env)) {
            return NULL;
        }
    }

    return env;
}

// Set up an external thread for using JNI with Android_JNI_GetEnv()
bool Android_JNI_SetupThread(void)
{
    JNIEnv *env;
    int status;

    // There should be a JVM
    if (!mJavaVM) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed, there is no JavaVM");
        return false;
    }

    /* Attach the current thread to the JVM and get a JNIEnv.
     * It will be detached by pthread_create destructor 'Android_JNI_ThreadDestroyed' */
    status = (*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL);
    if (status < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed to attach current thread (err=%d)", status);
        return false;
    }

    // Save JNIEnv into the Thread local storage
    if (!Android_JNI_SetEnv(env)) {
        return false;
    }

    return true;
}

// Destructor called for each thread where mThreadKey is not NULL
static void Android_JNI_ThreadDestroyed(void *value)
{
    // The thread is being destroyed, detach it from the Java VM and set the mThreadKey value to NULL as required
    JNIEnv *env = (JNIEnv *)value;
    if (env) {
        (*mJavaVM)->DetachCurrentThread(mJavaVM);
        Android_JNI_SetEnv(NULL);
    }
}

// Creation of local storage mThreadKey
static void Android_JNI_CreateKey(void)
{
    int status = pthread_key_create(&mThreadKey, Android_JNI_ThreadDestroyed);
    if (status < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Error initializing mThreadKey with pthread_key_create() (err=%d)", status);
    }
}

static void Android_JNI_CreateKey_once(void)
{
    int status = pthread_once(&key_once, Android_JNI_CreateKey);
    if (status < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Error initializing mThreadKey with pthread_once() (err=%d)", status);
    }
}

static void register_methods(JNIEnv *env, const char *classname, JNINativeMethod *methods, int nb)
{
    jclass clazz = (*env)->FindClass(env, classname);
    if (!clazz || (*env)->RegisterNatives(env, clazz, methods, nb) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed to register methods of %s", classname);
        return;
    }
}

// Library init
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;

    mJavaVM = vm;

    if ((*mJavaVM)->GetEnv(mJavaVM, (void **)&env, JNI_VERSION_1_4) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "Failed to get JNI Env");
        return JNI_VERSION_1_4;
    }

    register_methods(env, "org/libsdl/app/SDLActivity", SDLActivity_tab, SDL_arraysize(SDLActivity_tab));
    register_methods(env, "org/libsdl/app/SDLInputConnection", SDLInputConnection_tab, SDL_arraysize(SDLInputConnection_tab));
    register_methods(env, "org/libsdl/app/SDLAudioManager", SDLAudioManager_tab, SDL_arraysize(SDLAudioManager_tab));
    register_methods(env, "org/libsdl/app/SDLControllerManager", SDLControllerManager_tab, SDL_arraysize(SDLControllerManager_tab));
    register_methods(env, "org/libsdl/app/HIDDeviceManager", HIDDeviceManager_tab, SDL_arraysize(HIDDeviceManager_tab));

    return JNI_VERSION_1_4;
}

void checkJNIReady(void)
{
    if (!mActivityClass || !mAudioManagerClass || !mControllerManagerClass) {
        // We aren't fully initialized, let's just return.
        return;
    }

    SDL_SetMainReady();
}

// Get SDL version -- called before SDL_main() to verify JNI bindings
JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetVersion)(JNIEnv *env, jclass cls)
{
    char version[128];

    SDL_snprintf(version, sizeof(version), "%d.%d.%d", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

    return (*env)->NewStringUTF(env, version);
}

// Activity initialization -- called before SDL_main() to initialize JNI bindings
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "nativeSetupJNI()");

    // Start with a clean slate
    SDL_ClearError();


    RPC_Init();


    /*
     * Create mThreadKey so we can keep track of the JNIEnv assigned to each thread
     * Refer to http://developer.android.com/guide/practices/design/jni.html for the rationale behind this
     */
    Android_JNI_CreateKey_once();

    // Save JNIEnv of SDLActivity
    Android_JNI_SetEnv(env);

    if (!mJavaVM) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to found a JavaVM");
    }

    Android_LifecycleMutex = SDL_CreateMutex();
    if (!Android_LifecycleMutex) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create Android_LifecycleMutex mutex");
    }

    Android_LifecycleEventSem = SDL_CreateSemaphore(0);
    if (!Android_LifecycleEventSem) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create Android_LifecycleEventSem semaphore");
    }

    mActivityClass = (jclass)((*env)->NewGlobalRef(env, cls));

    midClipboardGetText = (*env)->GetStaticMethodID(env, mActivityClass, "clipboardGetText", "()Ljava/lang/String;");
    midClipboardHasText = (*env)->GetStaticMethodID(env, mActivityClass, "clipboardHasText", "()Z");
    midClipboardSetText = (*env)->GetStaticMethodID(env, mActivityClass, "clipboardSetText", "(Ljava/lang/String;)V");
    midCreateCustomCursor = (*env)->GetStaticMethodID(env, mActivityClass, "createCustomCursor", "([IIIII)I");
    midDestroyCustomCursor = (*env)->GetStaticMethodID(env, mActivityClass, "destroyCustomCursor", "(I)V");
    midGetContext = (*env)->GetStaticMethodID(env, mActivityClass, "getContext", "()Landroid/app/Activity;");
    midGetManifestEnvironmentVariables = (*env)->GetStaticMethodID(env, mActivityClass, "getManifestEnvironmentVariables", "()Z");
    midGetNativeSurface = (*env)->GetStaticMethodID(env, mActivityClass, "getNativeSurface", "()Landroid/view/Surface;");
    midInitTouch = (*env)->GetStaticMethodID(env, mActivityClass, "initTouch", "()V");
    midIsAndroidTV = (*env)->GetStaticMethodID(env, mActivityClass, "isAndroidTV", "()Z");
    midIsChromebook = (*env)->GetStaticMethodID(env, mActivityClass, "isChromebook", "()Z");
    midIsDeXMode = (*env)->GetStaticMethodID(env, mActivityClass, "isDeXMode", "()Z");
    midIsTablet = (*env)->GetStaticMethodID(env, mActivityClass, "isTablet", "()Z");
    midManualBackButton = (*env)->GetStaticMethodID(env, mActivityClass, "manualBackButton", "()V");
    midMinimizeWindow = (*env)->GetStaticMethodID(env, mActivityClass, "minimizeWindow", "()V");
    midOpenURL = (*env)->GetStaticMethodID(env, mActivityClass, "openURL", "(Ljava/lang/String;)Z");
    midRequestPermission = (*env)->GetStaticMethodID(env, mActivityClass, "requestPermission", "(Ljava/lang/String;I)V");
    midShowToast = (*env)->GetStaticMethodID(env, mActivityClass, "showToast", "(Ljava/lang/String;IIII)Z");
    midSendMessage = (*env)->GetStaticMethodID(env, mActivityClass, "sendMessage", "(II)Z");
    midSetActivityTitle = (*env)->GetStaticMethodID(env, mActivityClass, "setActivityTitle", "(Ljava/lang/String;)Z");
    midSetCustomCursor = (*env)->GetStaticMethodID(env, mActivityClass, "setCustomCursor", "(I)Z");
    midSetOrientation = (*env)->GetStaticMethodID(env, mActivityClass, "setOrientation", "(IIZLjava/lang/String;)V");
    midSetRelativeMouseEnabled = (*env)->GetStaticMethodID(env, mActivityClass, "setRelativeMouseEnabled", "(Z)Z");
    midSetSystemCursor = (*env)->GetStaticMethodID(env, mActivityClass, "setSystemCursor", "(I)Z");
    midSetWindowStyle = (*env)->GetStaticMethodID(env, mActivityClass, "setWindowStyle", "(Z)V");
    midShouldMinimizeOnFocusLoss = (*env)->GetStaticMethodID(env, mActivityClass, "shouldMinimizeOnFocusLoss", "()Z");
    midShowTextInput = (*env)->GetStaticMethodID(env, mActivityClass, "showTextInput", "(IIIII)Z");
    midSupportsRelativeMouse = (*env)->GetStaticMethodID(env, mActivityClass, "supportsRelativeMouse", "()Z");
    midOpenFileDescriptor = (*env)->GetStaticMethodID(env, mActivityClass, "openFileDescriptor", "(Ljava/lang/String;Ljava/lang/String;)I");
    midShowFileDialog = (*env)->GetStaticMethodID(env, mActivityClass, "showFileDialog", "([Ljava/lang/String;ZZI)Z");
    midGetPreferredLocales = (*env)->GetStaticMethodID(env, mActivityClass, "getPreferredLocales", "()Ljava/lang/String;");

    if (!midClipboardGetText ||
        !midClipboardHasText ||
        !midClipboardSetText ||
        !midCreateCustomCursor ||
        !midDestroyCustomCursor ||
        !midGetContext ||
        !midGetManifestEnvironmentVariables ||
        !midGetNativeSurface ||
        !midInitTouch ||
        !midIsAndroidTV ||
        !midIsChromebook ||
        !midIsDeXMode ||
        !midIsTablet ||
        !midManualBackButton ||
        !midMinimizeWindow ||
        !midOpenURL ||
        !midRequestPermission ||
        !midShowToast ||
        !midSendMessage ||
        !midSetActivityTitle ||
        !midSetCustomCursor ||
        !midSetOrientation ||
        !midSetRelativeMouseEnabled ||
        !midSetSystemCursor ||
        !midSetWindowStyle ||
        !midShouldMinimizeOnFocusLoss ||
        !midShowTextInput ||
        !midSupportsRelativeMouse ||
        !midOpenFileDescriptor ||
        !midShowFileDialog ||
        !midGetPreferredLocales) {
        __android_log_print(ANDROID_LOG_WARN, "SDL", "Missing some Java callbacks, do you have the latest version of SDLActivity.java?");
    }

    checkJNIReady();
}

// Audio initialization -- called before SDL_main() to initialize JNI bindings
JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "AUDIO nativeSetupJNI()");

    mAudioManagerClass = (jclass)((*env)->NewGlobalRef(env, cls));

    midRegisterAudioDeviceCallback = (*env)->GetStaticMethodID(env, mAudioManagerClass,
                                                         "registerAudioDeviceCallback",
                                                         "()V");
    midUnregisterAudioDeviceCallback = (*env)->GetStaticMethodID(env, mAudioManagerClass,
                                                         "unregisterAudioDeviceCallback",
                                                         "()V");
    midAudioSetThreadPriority = (*env)->GetStaticMethodID(env, mAudioManagerClass,
                                                          "audioSetThreadPriority", "(ZI)V");

    if (!midRegisterAudioDeviceCallback || !midUnregisterAudioDeviceCallback || !midAudioSetThreadPriority) {
        __android_log_print(ANDROID_LOG_WARN, "SDL",
                            "Missing some Java callbacks, do you have the latest version of SDLAudioManager.java?");
    }

    checkJNIReady();
}

// Controller initialization -- called before SDL_main() to initialize JNI bindings
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "CONTROLLER nativeSetupJNI()");

    mControllerManagerClass = (jclass)((*env)->NewGlobalRef(env, cls));

    midPollInputDevices = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                                    "pollInputDevices", "()V");
    midJoystickSetLED = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                              "joystickSetLED", "(IIII)V");
    midPollHapticDevices = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                                     "pollHapticDevices", "()V");
    midHapticRun = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                             "hapticRun", "(IFI)V");
    midHapticRumble = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                             "hapticRumble", "(IFFI)V");
    midHapticStop = (*env)->GetStaticMethodID(env, mControllerManagerClass,
                                              "hapticStop", "(I)V");

    if (!midPollInputDevices || !midJoystickSetLED || !midPollHapticDevices || !midHapticRun || !midHapticRumble || !midHapticStop) {
        __android_log_print(ANDROID_LOG_WARN, "SDL", "Missing some Java callbacks, do you have the latest version of SDLControllerManager.java?");
    }

    checkJNIReady();
}

static int run_count = 0;
static bool allow_recreate_activity;
static bool allow_recreate_activity_set;

JNIEXPORT int JNICALL SDL_JAVA_INTERFACE(nativeCheckSDLThreadCounter)(
    JNIEnv *env, jclass jcls)
{
    int tmp = run_count;
    run_count += 1;
    return tmp;
}

void Android_SetAllowRecreateActivity(bool enabled)
{
    allow_recreate_activity = enabled;
    allow_recreate_activity_set = true;
}

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeAllowRecreateActivity)(
    JNIEnv *env, jclass jcls)
{
    return allow_recreate_activity;
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeInitMainThread)(
    JNIEnv *env, jclass jcls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "nativeInitSDLThread() %d time", run_count);
    run_count += 1;

    // Save JNIEnv of SDLThread
    Android_JNI_SetEnv(env);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeCleanupMainThread)(
    JNIEnv *env, jclass jcls)
{
    /* This is a Java thread, it doesn't need to be Detached from the JVM.
     * Set to mThreadKey value to NULL not to call pthread_create destructor 'Android_JNI_ThreadDestroyed' */
    Android_JNI_SetEnv(NULL);
}

// Start up the SDL app
JNIEXPORT int JNICALL SDL_JAVA_INTERFACE(nativeRunMain)(JNIEnv *env, jclass cls, jstring library, jstring function, jobject array)
{
    int status = -1;
    const char *library_file;
    void *library_handle;

    library_file = (*env)->GetStringUTFChars(env, library, NULL);
    library_handle = dlopen(library_file, RTLD_GLOBAL);

    if (library_handle == NULL) {
        /* When deploying android app bundle format uncompressed native libs may not extract from apk to filesystem.
           In this case we should use lib name without path. https://bugzilla.libsdl.org/show_bug.cgi?id=4739 */
        const char *library_name = SDL_strrchr(library_file, '/');
        if (library_name && *library_name) {
            library_name += 1;
            library_handle = dlopen(library_name, RTLD_GLOBAL);
        }
    }

    if (library_handle) {
        const char *function_name;
        SDL_main_func SDL_main;

        function_name = (*env)->GetStringUTFChars(env, function, NULL);
        SDL_main = (SDL_main_func)dlsym(library_handle, function_name);
        if (SDL_main) {
            // Use the name "app_process" for argv[0] so PHYSFS_platformCalcBaseDir() works.
            //   https://github.com/love2d/love-android/issues/24
            // (note that PhysicsFS hasn't used argv on Android in a long time, but we'll keep this for compat at least for SDL3's lifetime.  --ryan.)
            const char *argv0 = "app_process";
            const int len = (*env)->GetArrayLength(env, array);  // argv elements, not counting argv[0].

            size_t total_alloc_len = (SDL_strlen(argv0) + 1) + ((len + 2) * sizeof (char *));  // len+2 to allocate an array that also holds argv0 and a NULL terminator.
            for (int i = 0; i < len; ++i) {
                total_alloc_len++;  // null terminator.
                jstring string = (*env)->GetObjectArrayElement(env, array, i);
                if (string) {
                    const char *utf = (*env)->GetStringUTFChars(env, string, 0);
                    if (utf) {
                        total_alloc_len += SDL_strlen(utf) + 1;
                        (*env)->ReleaseStringUTFChars(env, string, utf);
                    }
                    (*env)->DeleteLocalRef(env, string);
                }
            }

            void *args = malloc(total_alloc_len);  // This should NOT be SDL_malloc()
            if (!args) { // uhoh.
                __android_log_print(ANDROID_LOG_ERROR, "SDL", "nativeRunMain(): Out of memory parsing command line!");
            } else {
                size_t remain = total_alloc_len - (sizeof (char *) * (len + 2));
                int argc = 0;
                char **argv = (char **) args;
                char *ptr = (char *) &argv[len + 2];
                size_t cpy = SDL_strlcpy(ptr, argv0, remain) + 1;
                argv[argc++] = ptr;
                SDL_assert(cpy <= remain); remain -= cpy; ptr += cpy;
                for (int i = 0; i < len; ++i) {
                    jstring string = (*env)->GetObjectArrayElement(env, array, i);
                    const char *utf = string ? (*env)->GetStringUTFChars(env, string, 0) : NULL;
                    cpy = SDL_strlcpy(ptr, utf ? utf : "", remain) + 1;
                    if (cpy < remain) {
                        argv[argc++] = ptr;
                        remain -= cpy;
                        ptr += cpy;
                    }
                    if (utf) {
                        (*env)->ReleaseStringUTFChars(env, string, utf);
                    }
                    if (string) {
                        (*env)->DeleteLocalRef(env, string);
                    }
                }
                argv[argc] = NULL;

                // Run the application.
                status = SDL_RunApp(argc, argv, SDL_main, NULL);

                // Release the arguments.
                free(args);  // This should NOT be SDL_free()
            }
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "SDL", "nativeRunMain(): Couldn't find function %s in library %s", function_name, library_file);
        }
        (*env)->ReleaseStringUTFChars(env, function, function_name);

        dlclose(library_handle);

    } else {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "nativeRunMain(): Couldn't load library %s", library_file);
    }
    (*env)->ReleaseStringUTFChars(env, library, library_file);

    // Do not issue an exit or the whole application will terminate instead of just the SDL thread
    // exit(status);

    return status;
}

static int FindLifecycleEvent(SDL_AndroidLifecycleEvent event)
{
    for (int index = 0; index < Android_NumLifecycleEvents; ++index) {
        if (Android_LifecycleEvents[index] == event) {
            return index;
        }
    }
    return -1;
}

static void RemoveLifecycleEvent(int index)
{
    if (index < Android_NumLifecycleEvents - 1) {
        SDL_memmove(&Android_LifecycleEvents[index], &Android_LifecycleEvents[index+1], (Android_NumLifecycleEvents - index - 1) * sizeof(Android_LifecycleEvents[index]));
    }
    --Android_NumLifecycleEvents;
}

void Android_SendLifecycleEvent(SDL_AndroidLifecycleEvent event)
{
    SDL_LockMutex(Android_LifecycleMutex);
    {
        int index;
        bool add_event = true;

        switch (event) {
        case SDL_ANDROID_LIFECYCLE_WAKE:
            // We don't need more than one wake queued
            index = FindLifecycleEvent(SDL_ANDROID_LIFECYCLE_WAKE);
            if (index >= 0) {
                add_event = false;
            }
            break;
        case SDL_ANDROID_LIFECYCLE_PAUSE:
            // If we have a resume queued, just stay in the paused state
            index = FindLifecycleEvent(SDL_ANDROID_LIFECYCLE_RESUME);
            if (index >= 0) {
                RemoveLifecycleEvent(index);
                add_event = false;
            }
            break;
        case SDL_ANDROID_LIFECYCLE_RESUME:
            // If we have a pause queued, just stay in the resumed state
            index = FindLifecycleEvent(SDL_ANDROID_LIFECYCLE_PAUSE);
            if (index >= 0) {
                RemoveLifecycleEvent(index);
                add_event = false;
            }
            break;
        case SDL_ANDROID_LIFECYCLE_LOWMEMORY:
            // We don't need more than one low memory event queued
            index = FindLifecycleEvent(SDL_ANDROID_LIFECYCLE_LOWMEMORY);
            if (index >= 0) {
                add_event = false;
            }
            break;
        case SDL_ANDROID_LIFECYCLE_DESTROY:
            // Remove all other events, we're done!
            while (Android_NumLifecycleEvents > 0) {
                RemoveLifecycleEvent(0);
            }
            break;
        default:
            SDL_assert(!"Sending unexpected lifecycle event");
            add_event = false;
            break;
        }

        if (add_event) {
            SDL_assert(Android_NumLifecycleEvents < SDL_arraysize(Android_LifecycleEvents));
            Android_LifecycleEvents[Android_NumLifecycleEvents++] = event;
            SDL_SignalSemaphore(Android_LifecycleEventSem);
        }
    }
    SDL_UnlockMutex(Android_LifecycleMutex);
}

bool Android_WaitLifecycleEvent(SDL_AndroidLifecycleEvent *event, Sint64 timeoutNS)
{
    bool got_event = false;

    while (!got_event && SDL_WaitSemaphoreTimeoutNS(Android_LifecycleEventSem, timeoutNS)) {
        SDL_LockMutex(Android_LifecycleMutex);
        {
            if (Android_NumLifecycleEvents > 0) {
                *event = Android_LifecycleEvents[0];
                RemoveLifecycleEvent(0);
                got_event = true;
            }
        }
        SDL_UnlockMutex(Android_LifecycleMutex);
    }
    return got_event;
}

void Android_LockActivityState(void)
{
    SDL_LockMutex(Android_LifecycleMutex);
}

void Android_UnlockActivityState(void)
{
    SDL_UnlockMutex(Android_LifecycleMutex);
}

// Drop file

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    char *filename;
} RPC_data_onNativeDropFile_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDropFile)(
    JNIEnv *env, jclass jcls,
    jstring filename)
{
    RPC_Prepare(onNativeDropFile);
    RPC_AddString(filename);
    RPC_Send;
}

// Set screen resolution
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int surfaceWidth;
    int surfaceHeight;
    int deviceWidth;
    int deviceHeight;
    float density;
    float rate;
} RPC_data_nativeSetScreenResolution_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetScreenResolution)(
    JNIEnv *env, jclass jcls,
    jint surfaceWidth, jint surfaceHeight,
    jint deviceWidth, jint deviceHeight, jfloat density, jfloat rate)
{
    RPC_Prepare(nativeSetScreenResolution);
    RPC_Add(surfaceWidth);
    RPC_Add(surfaceHeight);
    RPC_Add(deviceWidth);
    RPC_Add(deviceHeight);
    RPC_Add(density);
    RPC_Add(rate);
    RPC_Send;
}

// Resize
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeResize)(
    JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeResize);
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int orientation;
} RPC_data_nativeSetNaturalOrientation_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetNaturalOrientation)(
    JNIEnv *env, jclass jcls,
    jint orientation)
{
    RPC_Prepare(nativeSetNaturalOrientation);
    RPC_Add(orientation);
    RPC_Send;
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int rotation;
} RPC_data_onNativeRotationChanged_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeRotationChanged)(
    JNIEnv *env, jclass jcls,
    jint rotation)
{
    RPC_Prepare(onNativeRotationChanged);
    RPC_Add(rotation);
    RPC_Send;
}


typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int left;
    int right;
    int top;
    int bottom;
} RPC_data_onNativeInsetsChanged_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeInsetsChanged)(
    JNIEnv *env, jclass jcls,
    jint left, jint right, jint top, jint bottom)
{
    RPC_Prepare(onNativeInsetsChanged);
    RPC_Add(left);
    RPC_Add(right);
    RPC_Add(top);
    RPC_Add(bottom);
    RPC_Send;
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int touchId;
    char *name;
} RPC_data_nativeAddTouch_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeAddTouch)(
    JNIEnv *env, jclass cls,
    jint touchId, jstring name)
{
    RPC_Prepare(nativeAddTouch);
    RPC_Add(touchId);
    RPC_AddString(name);
    RPC_Send;
}


typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    bool recording;
    char *name;
    int device_id;
} RPC_data_nativeAddAudioDevice_t;


JNIEXPORT void JNICALL
SDL_JAVA_AUDIO_INTERFACE(nativeAddAudioDevice)(JNIEnv *env, jclass jcls, jboolean recording,
                                         jstring name, jint device_id)
{
#if ALLOW_MULTIPLE_ANDROID_AUDIO_DEVICES
    RPC_Prepare(nativeAddAudioDevice);
    RPC_Add(recording);
    RPC_AddString(name);
    RPC_Add(device_id);
    RPC_Send;
#endif
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    bool recording;
    int device_id;
} RPC_data_nativeRemoveAudioDevice_t;

JNIEXPORT void JNICALL
SDL_JAVA_AUDIO_INTERFACE(nativeRemoveAudioDevice)(JNIEnv *env, jclass jcls, jboolean recording,
                                            jint device_id)
{
#if ALLOW_MULTIPLE_ANDROID_AUDIO_DEVICES
    RPC_Prepare(nativeRemoveAudioDevice);
    RPC_Add(recording);
    RPC_Add(device_id);
    RPC_Send;
#endif
}

// Paddown
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
    int keycode;
} RPC_data_onNativePadDown_t;

JNIEXPORT jboolean JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode)
{
#ifdef SDL_JOYSTICK_ANDROID
    int button = Android_keycode_to_SDL(keycode);
    if (button >= 0) {
#ifdef GAMEPAD_AS_RPC
        RPC_Prepare(onNativePadDown);
        RPC_Add(device_id);
        RPC_Add(keycode);
        RPC_Send;
#else
        Android_OnPadDown(device_id, keycode);
#endif
        return true;
    }
    return false;
#else
    return false;
#endif // SDL_JOYSTICK_ANDROID
}

// Padup
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
int device_id;
    int keycode;
} RPC_data_onNativePadUp_t;

JNIEXPORT jboolean JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode)
{
#ifdef SDL_JOYSTICK_ANDROID
    int button = Android_keycode_to_SDL(keycode);
    if (button >= 0) {
#ifdef GAMEPAD_AS_RPC
        RPC_Prepare(onNativePadUp);
        RPC_Add(device_id);
        RPC_Add(keycode);
        RPC_Send;
#else
        Android_OnPadUp(device_id, keycode);
#endif
        return true;
    }
    return false;
#else
    return false;
#endif // SDL_JOYSTICK_ANDROID
}


// Joy
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
    int axis;
    float value;
} RPC_data_onNativeJoy_t;

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint axis, jfloat value)
{
#ifdef SDL_JOYSTICK_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(onNativeJoy);
    RPC_Add(device_id);
    RPC_Add(axis);
    RPC_Add(value);
    RPC_Send;
#else
    Android_OnJoy(device_id, axis, value);
#endif
#endif
}

// POV Hat
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
    int hat_id;
    int x;
    int y;
} RPC_data_onNativeHat_t;

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint hat_id, jint x, jint y)
{
#ifdef SDL_JOYSTICK_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(onNativeHat);
    RPC_Add(device_id);
    RPC_Add(hat_id);
    RPC_Add(x);
    RPC_Add(y);
    RPC_Send;
#else
    Android_OnHat(device_id, hat_id, x, y);
#endif
#endif
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
    char *device_name;
    char *device_desc;
    int vendor_id;
    int product_id;
    int button_mask;
    int naxes;
    int axis_mask;
    int nhats;
    bool can_rumble;
    bool has_rgb_led;
} RPC_data_nativeAddJoystick_t;


JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name, jstring device_desc,
    jint vendor_id, jint product_id,
    jint button_mask, jint naxes, jint axis_mask, jint nhats, jboolean can_rumble, jboolean has_rgb_led)
{
#ifdef SDL_JOYSTICK_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(nativeAddJoystick);
    RPC_Add(device_id);
    RPC_AddString(device_name);
    RPC_AddString(device_desc);
    RPC_Add(vendor_id);
    RPC_Add(product_id);
    RPC_Add(button_mask);
    RPC_Add(naxes);
    RPC_Add(axis_mask);
    RPC_Add(nhats);
    RPC_Add(can_rumble);
    RPC_Add(has_rgb_led);
    RPC_Send;
#else
    Android_AddJoystick(device_id, device_name, device_desc,
            vendor_id, product_id, button_mask, naxes,
            axis_mask, nhats, can_rumble, has_rgb_led);
#endif
#endif
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
} RPC_data_nativeRemoveJoystick_t;

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id)
{
#ifdef SDL_JOYSTICK_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(nativeRemoveJoystick);
    RPC_Add(device_id);
    RPC_Send;
#else
    Android_RemoveJoystick(device_id);
#endif
#endif
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
    char *device_name;
} RPC_data_nativeAddHaptic_t;


JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic)(
    JNIEnv *env, jclass jcls, jint device_id, jstring device_name)
{
#ifdef SDL_HAPTIC_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(nativeAddHaptic);
    RPC_Add(device_name);
    RPC_AddString(device_name);
    RPC_Send;
#else
    Android_AddHaptic(device_id, device_name);
#endif
#endif
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int device_id;
} RPC_data_nativeRemoveHaptic_t;

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic)(
    JNIEnv *env, jclass jcls, jint device_id)
{
#ifdef SDL_HAPTIC_ANDROID
#ifdef GAMEPAD_AS_RPC
    RPC_Prepare(nativeRemoveHaptic);
    RPC_Add(device_id);
    RPC_Send;
#else
    Android_RemoveHaptic(device_id);
#endif
#endif
}

// Called from surfaceCreated()
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceCreated)(JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeSurfaceCreated);
}

// Called from surfaceChanged()
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceChanged)(JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeSurfaceChanged);
}

// Called from surfaceDestroyed()
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed)(JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeSurfaceDestroyed);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeScreenKeyboardShown)(JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeScreenKeyboardShown);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeScreenKeyboardHidden)(JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeScreenKeyboardHidden);
}

// Keydown
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int keycode;
} RPC_data_onNativeKeyDown_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyDown)(
    JNIEnv *env, jclass jcls,
    jint keycode)
{
    RPC_Prepare(onNativeKeyDown);
    RPC_Add(keycode);
    RPC_Send;
}

// Keyup
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int keycode;
} RPC_data_onNativeKeyUp_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyUp)(
    JNIEnv *env, jclass jcls,
    jint keycode)
{
    RPC_Prepare(onNativeKeyUp);
    RPC_Add(keycode);
    RPC_Send;
}

// Virtual keyboard return key might stop text input
JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(onNativeSoftReturnKey)(
    JNIEnv *env, jclass jcls)
{
    if (SDL_GetHintBoolean(SDL_HINT_RETURN_KEY_HIDES_IME, false)) {

        RPC_SendWithoutData(onNativeSoftReturnKey);

        return JNI_TRUE;
    }
    return JNI_FALSE;
}

// Keyboard Focus Lost
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost)(
    JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeKeyboardFocusLost);
}

// Touch
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int touch_device_id_in;
    int pointer_finger_id_in;
    int action;
    float x;
    float y;
    float p;
} RPC_data_onNativeTouch_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeTouch)(
    JNIEnv *env, jclass jcls,
    jint touch_device_id_in, jint pointer_finger_id_in,
    jint action, jfloat x, jfloat y, jfloat p)
{
    RPC_Prepare(onNativeTouch);
    RPC_Add(touch_device_id_in);
    RPC_Add(pointer_finger_id_in);
    RPC_Add(action);
    RPC_Add(x);
    RPC_Add(y);
    RPC_Add(p);
    RPC_Send;
}

// Pinch
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchStart)(
    JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativePinchStart);
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    float scale;
} RPC_data_onNativePinchUpdate_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchUpdate)(
    JNIEnv *env, jclass jcls, jfloat scale)
{
    RPC_Prepare(onNativePinchUpdate);
    RPC_Add(scale);
    RPC_Send;
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePinchEnd)(
    JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativePinchEnd);
}

// Mouse
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int button;
    int action;
    float x;
    float y;
    bool relative;
} RPC_data_onNativeMouse_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeMouse)(
    JNIEnv *env, jclass jcls,
    jint button, jint action, jfloat x, jfloat y, jboolean relative)
{
    RPC_Prepare(onNativeMouse);
    RPC_Add(button);
    RPC_Add(action);
    RPC_Add(x);
    RPC_Add(y);
    RPC_Add(relative);
    RPC_Send;
}

// Pen
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int pen_id_in;
    int device_type;
    int button;
    int action;
    float x;
    float y;
    float p;
} RPC_data_onNativePen_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativePen)(
    JNIEnv *env, jclass jcls,
    jint pen_id_in, jint device_type, jint button, jint action, jfloat x, jfloat y, jfloat p)
{
    RPC_Prepare(onNativePen);
    RPC_Add(pen_id_in);
    RPC_Add(device_type);
    RPC_Add(button);
    RPC_Add(action);
    RPC_Add(x);
    RPC_Add(y);
    RPC_Add(p);
    RPC_Send;
}

// Accelerometer
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    float x;
    float y;
    float z;
} RPC_data_onNativeAccel_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeAccel)(
    JNIEnv *env, jclass jcls,
    jfloat x, jfloat y, jfloat z)
{
    RPC_Prepare(onNativeAccel);
    RPC_Add(x);
    RPC_Add(y);
    RPC_Add(z);
    RPC_Send;
}

// Clipboard
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeClipboardChanged)(
    JNIEnv *env, jclass jcls)
{
    RPC_SendWithoutData(onNativeClipboardChanged);
}

// Low memory
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeLowMemory)(
    JNIEnv *env, jclass cls)
{
    Android_SendLifecycleEvent(SDL_ANDROID_LIFECYCLE_LOWMEMORY);
}

/* Locale
 * requires android:configChanges="layoutDirection|locale" in AndroidManifest.xml */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeLocaleChanged)(
    JNIEnv *env, jclass cls)
{
    RPC_SendWithoutData(onNativeLocaleChanged);
}

// Dark mode
typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    bool enabled;
} RPC_data_onNativeDarkModeChanged_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDarkModeChanged)(
    JNIEnv *env, jclass cls, jboolean enabled)
{
    RPC_Prepare(onNativeDarkModeChanged);
    RPC_Add(enabled);
    RPC_Send;
}

// Send Quit event to "SDLThread" thread
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSendQuit)(
    JNIEnv *env, jclass cls)
{
    Android_SendLifecycleEvent(SDL_ANDROID_LIFECYCLE_DESTROY);
}

// Activity ends
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeQuit)(
    JNIEnv *env, jclass cls)
{
    const char *str;

    if (Android_LifecycleMutex) {
        SDL_DestroyMutex(Android_LifecycleMutex);
        Android_LifecycleMutex = NULL;
    }

    if (Android_LifecycleEventSem) {
        SDL_DestroySemaphore(Android_LifecycleEventSem);
        Android_LifecycleEventSem = NULL;
    }

    Android_NumLifecycleEvents = 0;

    Internal_Android_Destroy_AssetManager();

    str = SDL_GetError();
    if (str && str[0]) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "SDLActivity thread ends (error=%s)", str);
    } else {
        __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "SDLActivity thread ends");
    }
}

// Pause
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePause)(
    JNIEnv *env, jclass cls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "nativePause()");

    Android_SendLifecycleEvent(SDL_ANDROID_LIFECYCLE_PAUSE);
}

// Resume
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeResume)(
    JNIEnv *env, jclass cls)
{
    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "nativeResume()");

    Android_SendLifecycleEvent(SDL_ANDROID_LIFECYCLE_RESUME);
}

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    bool hasFocus;
} RPC_data_nativeFocusChanged_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeFocusChanged)(
    JNIEnv *env, jclass cls, jboolean hasFocus)
{
    RPC_Prepare(nativeFocusChanged);
    RPC_Add(hasFocus);
    RPC_Send;
}


typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    char *text;
    int newCursorPosition;
} RPC_data_nativeCommitText_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText)(
    JNIEnv *env, jclass cls,
    jstring text, jint newCursorPosition)
{
    RPC_Prepare(nativeCommitText);
    RPC_AddString(text);
    RPC_Add(newCursorPosition);
    RPC_Send;
}


typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    char chUnicode;
} RPC_data_nativeGenerateScancodeForUnichar_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar)(
    JNIEnv *env, jclass cls,
    jchar chUnicode)
{
    RPC_Prepare(nativeGenerateScancodeForUnichar);
    RPC_Add(chUnicode);
    RPC_Send;
}

JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetHint)(
    JNIEnv *env, jclass cls,
    jstring name)
{
    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    const char *hint = SDL_GetHint(utfname);

    jstring result = (*env)->NewStringUTF(env, hint);
    (*env)->ReleaseStringUTFChars(env, name, utfname);

    return result;
}

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeGetHintBoolean)(
    JNIEnv *env, jclass cls,
    jstring name, jboolean default_value)
{
    jboolean result;

    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    result = SDL_GetHintBoolean(utfname, default_value);
    (*env)->ReleaseStringUTFChars(env, name, utfname);

    return result;
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetenv)(
    JNIEnv *env, jclass cls,
    jstring name, jstring value)
{
    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    const char *utfvalue = (*env)->GetStringUTFChars(env, value, NULL);

    // This is only called at startup, to initialize the environment
    // Note that we call setenv() directly to avoid affecting SDL environments
    setenv(utfname, utfvalue, 1); // This should NOT be SDL_setenv()

    if (SDL_strcmp(utfname, SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY) == 0) {
        // Special handling for this hint, which needs to persist outside the normal application flow
        // Only set this the first time we run, in case it's been set by the application via SDL_SetHint()
        if (!allow_recreate_activity_set) {
            Android_SetAllowRecreateActivity(SDL_GetStringBoolean(utfvalue, false));
        }
    }

    (*env)->ReleaseStringUTFChars(env, name, utfname);
    (*env)->ReleaseStringUTFChars(env, value, utfvalue);
}

/*******************************************************************************
             Functions called by SDL into Java
*******************************************************************************/

static SDL_AtomicInt s_active;
struct LocalReferenceHolder
{
    JNIEnv *m_env;
    const char *m_func;
};

static struct LocalReferenceHolder LocalReferenceHolder_Setup(const char *func)
{
    struct LocalReferenceHolder refholder;
    refholder.m_env = NULL;
    refholder.m_func = func;
#ifdef DEBUG_JNI
    SDL_Log("Entering function %s", func);
#endif
    return refholder;
}

static bool LocalReferenceHolder_Init(struct LocalReferenceHolder *refholder, JNIEnv *env)
{
    const int capacity = 16;
    if ((*env)->PushLocalFrame(env, capacity) < 0) {
        SDL_SetError("Failed to allocate enough JVM local references");
        return false;
    }
    SDL_AtomicIncRef(&s_active);
    refholder->m_env = env;
    return true;
}

static void LocalReferenceHolder_Cleanup(struct LocalReferenceHolder *refholder)
{
#ifdef DEBUG_JNI
    SDL_Log("Leaving function %s", refholder->m_func);
#endif
    if (refholder->m_env) {
        JNIEnv *env = refholder->m_env;
        (*env)->PopLocalFrame(env, NULL);
        SDL_AtomicDecRef(&s_active);
    }
}

ANativeWindow *Android_JNI_GetNativeWindow(void)
{
    ANativeWindow *anw = NULL;
    jobject s;
    JNIEnv *env = Android_JNI_GetEnv();

    s = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetNativeSurface);
    if (s) {
        anw = ANativeWindow_fromSurface(env, s);
        (*env)->DeleteLocalRef(env, s);
    }

    return anw;
}

void Android_JNI_SetActivityTitle(const char *title)
{
    JNIEnv *env = Android_JNI_GetEnv();

    jstring jtitle = (*env)->NewStringUTF(env, title);
    (*env)->CallStaticBooleanMethod(env, mActivityClass, midSetActivityTitle, jtitle);
    (*env)->DeleteLocalRef(env, jtitle);
}

void Android_JNI_SetWindowStyle(bool fullscreen)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, midSetWindowStyle, fullscreen ? 1 : 0);
}

void Android_JNI_SetOrientation(int w, int h, int resizable, const char *hint)
{
    JNIEnv *env = Android_JNI_GetEnv();

    jstring jhint = (*env)->NewStringUTF(env, (hint ? hint : ""));
    (*env)->CallStaticVoidMethod(env, mActivityClass, midSetOrientation, w, h, (resizable ? 1 : 0), jhint);
    (*env)->DeleteLocalRef(env, jhint);
}

SDL_DisplayOrientation Android_JNI_GetDisplayNaturalOrientation(void)
{
    return displayNaturalOrientation;
}

SDL_DisplayOrientation Android_JNI_GetDisplayCurrentOrientation(void)
{
    return displayCurrentOrientation;
}

void Android_JNI_MinimizeWindow(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, midMinimizeWindow);
}

bool Android_JNI_ShouldMinimizeOnFocusLoss(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midShouldMinimizeOnFocusLoss);
}

bool Android_JNI_GetAccelerometerValues(float values[3])
{
    bool result = false;

    if (bHasNewData) {
        int i;
        for (i = 0; i < 3; ++i) {
            values[i] = fLastAccelerometer[i];
        }
        bHasNewData = false;
        result = true;
    }

    return result;
}

/*
 * Audio support
 */
void Android_StartAudioHotplug(SDL_AudioDevice **default_playback, SDL_AudioDevice **default_recording)
{
    JNIEnv *env = Android_JNI_GetEnv();
    // this will fire the callback for each existing device right away (which will eventually SDL_AddAudioDevice), and again later when things change.
    (*env)->CallStaticVoidMethod(env, mAudioManagerClass, midRegisterAudioDeviceCallback);
    *default_playback = *default_recording = NULL;  // !!! FIXME: how do you decide the default device id?
}

void Android_StopAudioHotplug(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mAudioManagerClass, midUnregisterAudioDeviceCallback);
}

static void Android_JNI_AudioSetThreadPriority(int recording, int device_id)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mAudioManagerClass, midAudioSetThreadPriority, recording, device_id);
}

void Android_AudioThreadInit(SDL_AudioDevice *device)
{
    Android_JNI_AudioSetThreadPriority((int) device->recording, (int)device->instance_id);
}

// Test for an exception and call SDL_SetError with its detail if one occurs
// If the parameter silent is truthy then SDL_SetError() will not be called.
static bool Android_JNI_ExceptionOccurred(bool silent)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jthrowable exception;

    // Detect mismatch LocalReferenceHolder_Init/Cleanup
    SDL_assert(SDL_GetAtomicInt(&s_active) > 0);

    exception = (*env)->ExceptionOccurred(env);
    if (exception != NULL) {
        jmethodID mid;

        // Until this happens most JNI operations have undefined behaviour
        (*env)->ExceptionClear(env);

        if (!silent) {
            jclass exceptionClass = (*env)->GetObjectClass(env, exception);
            jclass classClass = (*env)->FindClass(env, "java/lang/Class");
            jstring exceptionName;
            const char *exceptionNameUTF8;
            jstring exceptionMessage;

            mid = (*env)->GetMethodID(env, classClass, "getName", "()Ljava/lang/String;");
            exceptionName = (jstring)(*env)->CallObjectMethod(env, exceptionClass, mid);
            exceptionNameUTF8 = (*env)->GetStringUTFChars(env, exceptionName, 0);

            (*env)->DeleteLocalRef(env, classClass);

            mid = (*env)->GetMethodID(env, exceptionClass, "getMessage", "()Ljava/lang/String;");
            exceptionMessage = (jstring)(*env)->CallObjectMethod(env, exception, mid);

            (*env)->DeleteLocalRef(env, exceptionClass);

            if (exceptionMessage != NULL) {
                const char *exceptionMessageUTF8 = (*env)->GetStringUTFChars(env, exceptionMessage, 0);
                SDL_SetError("%s: %s", exceptionNameUTF8, exceptionMessageUTF8);
                (*env)->ReleaseStringUTFChars(env, exceptionMessage, exceptionMessageUTF8);
                (*env)->DeleteLocalRef(env, exceptionMessage);
            } else {
                SDL_SetError("%s", exceptionNameUTF8);
            }

            (*env)->ReleaseStringUTFChars(env, exceptionName, exceptionNameUTF8);
            (*env)->DeleteLocalRef(env, exceptionName);
        }

        return true;
    }

    return false;
}

static void Internal_Android_Create_AssetManager(void)
{

    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    JNIEnv *env = Android_JNI_GetEnv();
    jmethodID mid;
    jobject context;
    jobject javaAssetManager;

    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return;
    }

    // context = SDLActivity.getContext();
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);

    // javaAssetManager = context.getAssets();
    mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                              "getAssets", "()Landroid/content/res/AssetManager;");
    javaAssetManager = (*env)->CallObjectMethod(env, context, mid);

    /**
     * Given a Dalvik AssetManager object, obtain the corresponding native AAssetManager
     * object.  Note that the caller is responsible for obtaining and holding a VM reference
     * to the jobject to prevent its being garbage collected while the native object is
     * in use.
     */
    javaAssetManagerRef = (*env)->NewGlobalRef(env, javaAssetManager);
    asset_manager = AAssetManager_fromJava(env, javaAssetManagerRef);

    if (!asset_manager) {
        (*env)->DeleteGlobalRef(env, javaAssetManagerRef);
        Android_JNI_ExceptionOccurred(true);
    }

    LocalReferenceHolder_Cleanup(&refs);
}

static void Internal_Android_Destroy_AssetManager(void)
{
    JNIEnv *env = Android_JNI_GetEnv();

    if (asset_manager) {
        (*env)->DeleteGlobalRef(env, javaAssetManagerRef);
        asset_manager = NULL;
    }
}

static const char *GetAssetPath(const char *path)
{
    if (path && path[0] == '.' && path[1] == '/') {
        path += 2;
        while (*path == '/') {
            ++path;
        }
    }
    return path;
}

bool Android_JNI_FileOpen(void **puserdata, const char *fileName, const char *mode)
{
    SDL_assert(puserdata != NULL);

    AAsset *asset = NULL;
    *puserdata = NULL;

    if (!asset_manager) {
        Internal_Android_Create_AssetManager();
        if (!asset_manager) {
            return SDL_SetError("Couldn't create asset manager");
        }
    }

    fileName = GetAssetPath(fileName);

    asset = AAssetManager_open(asset_manager, fileName, AASSET_MODE_UNKNOWN);
    if (!asset) {
        return SDL_SetError("Couldn't open asset '%s'", fileName);
    }

    *puserdata = (void *)asset;
    return true;
}

size_t Android_JNI_FileRead(void *userdata, void *buffer, size_t size, SDL_IOStatus *status)
{
    const int bytes = AAsset_read((AAsset *)userdata, buffer, size);
    if (bytes < 0) {
        SDL_SetError("AAsset_read() failed");
        *status = SDL_IO_STATUS_ERROR;
        return 0;
    } else if (bytes < size) {
        *status = SDL_IO_STATUS_EOF;
    }
    return (size_t)bytes;
}

size_t Android_JNI_FileWrite(void *userdata, const void *buffer, size_t size, SDL_IOStatus *status)
{
    SDL_SetError("Cannot write to Android package filesystem");
    *status = SDL_IO_STATUS_ERROR;
    return 0;
}

Sint64 Android_JNI_FileSize(void *userdata)
{
    return (Sint64) AAsset_getLength64((AAsset *)userdata);
}

Sint64 Android_JNI_FileSeek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    return (Sint64) AAsset_seek64((AAsset *)userdata, offset, (int)whence);
}

bool Android_JNI_FileClose(void *userdata)
{
    AAsset_close((AAsset *)userdata);
    return true;
}

bool Android_JNI_EnumerateAssetDirectory(const char *path, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    SDL_assert(path != NULL);

    if (!asset_manager) {
        Internal_Android_Create_AssetManager();
        if (!asset_manager) {
            return SDL_SetError("Couldn't create asset manager");
        }
    }

    path = GetAssetPath(path);

    AAssetDir *adir = AAssetManager_openDir(asset_manager, path);
    if (!adir) {
        return SDL_SetError("AAssetManager_openDir failed");
    }

    SDL_EnumerationResult result = SDL_ENUM_CONTINUE;
    const char *ent;
    while ((result == SDL_ENUM_CONTINUE) && ((ent = AAssetDir_getNextFileName(adir)) != NULL)) {
        result = cb(userdata, path, ent);
    }

    AAssetDir_close(adir);

    return (result != SDL_ENUM_FAILURE);
}

bool Android_JNI_GetAssetPathInfo(const char *path, SDL_PathInfo *info)
{
    if (!asset_manager) {
        Internal_Android_Create_AssetManager();
        if (!asset_manager) {
            return SDL_SetError("Couldn't create asset manager");
        }
    }

    path = GetAssetPath(path);

    // this is sort of messy, but there isn't a stat()-like interface to the Assets.
    AAsset *aasset = AAssetManager_open(asset_manager, path, AASSET_MODE_UNKNOWN);
    if (aasset) {  // it's a file!
        info->type = SDL_PATHTYPE_FILE;
        info->size = (Uint64) AAsset_getLength64(aasset);
        AAsset_close(aasset);
        return true;
    }

    AAssetDir *adir = AAssetManager_openDir(asset_manager, path);
    if (adir) {  // This does _not_ return NULL for a missing directory! Treat empty directories as missing. Better than nothing.  :/
        const bool contains_something = (AAssetDir_getNextFileName(adir) != NULL);  // if not NULL, there are files in this directory, so it's _definitely_ a directory.
        AAssetDir_close(adir);
        if (contains_something) {
            info->type = SDL_PATHTYPE_DIRECTORY;
            info->size = 0;
            return true;
        }
    }

    return SDL_SetError("Couldn't open asset '%s'", path);
}

bool Android_JNI_SetClipboardText(const char *text)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jstring string = (*env)->NewStringUTF(env, text);
    (*env)->CallStaticVoidMethod(env, mActivityClass, midClipboardSetText, string);
    (*env)->DeleteLocalRef(env, string);
    return true;
}

char *Android_JNI_GetClipboardText(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    char *text = NULL;
    jstring string;

    string = (*env)->CallStaticObjectMethod(env, mActivityClass, midClipboardGetText);
    if (string) {
        const char *utf = (*env)->GetStringUTFChars(env, string, 0);
        if (utf) {
            text = SDL_strdup(utf);
            (*env)->ReleaseStringUTFChars(env, string, utf);
        }
        (*env)->DeleteLocalRef(env, string);
    }

    return (!text) ? SDL_strdup("") : text;
}

bool Android_JNI_HasClipboardText(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midClipboardHasText);
}

/* returns 0 on success or -1 on error (others undefined then)
 * returns truthy or falsy value in plugged, charged and battery
 * returns the value in seconds and percent or -1 if not available
 */
int Android_JNI_GetPowerInfo(int *plugged, int *charged, int *battery, int *seconds, int *percent)
{
    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    JNIEnv *env = Android_JNI_GetEnv();
    jmethodID mid;
    jobject context;
    jstring action;
    jclass cls;
    jobject filter;
    jobject intent;
    jstring iname;
    jmethodID imid;
    jstring bname;
    jmethodID bmid;
    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return -1;
    }

    // context = SDLActivity.getContext();
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);

    action = (*env)->NewStringUTF(env, "android.intent.action.BATTERY_CHANGED");

    cls = (*env)->FindClass(env, "android/content/IntentFilter");

    mid = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;)V");
    filter = (*env)->NewObject(env, cls, mid, action);

    (*env)->DeleteLocalRef(env, action);

    mid = (*env)->GetMethodID(env, mActivityClass, "registerReceiver", "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");
    intent = (*env)->CallObjectMethod(env, context, mid, NULL, filter);

    (*env)->DeleteLocalRef(env, filter);

    cls = (*env)->GetObjectClass(env, intent);

    imid = (*env)->GetMethodID(env, cls, "getIntExtra", "(Ljava/lang/String;I)I");

    // Watch out for C89 scoping rules because of the macro
#define GET_INT_EXTRA(var, key)                                  \
    int var;                                                     \
    iname = (*env)->NewStringUTF(env, key);                      \
    (var) = (*env)->CallIntMethod(env, intent, imid, iname, -1); \
    (*env)->DeleteLocalRef(env, iname);

    bmid = (*env)->GetMethodID(env, cls, "getBooleanExtra", "(Ljava/lang/String;Z)Z");

    // Watch out for C89 scoping rules because of the macro
#define GET_BOOL_EXTRA(var, key)                                            \
    int var;                                                                \
    bname = (*env)->NewStringUTF(env, key);                                 \
    (var) = (*env)->CallBooleanMethod(env, intent, bmid, bname, JNI_FALSE); \
    (*env)->DeleteLocalRef(env, bname);

    if (plugged) {
        // Watch out for C89 scoping rules because of the macro
        GET_INT_EXTRA(plug, "plugged") // == BatteryManager.EXTRA_PLUGGED (API 5)
        if (plug == -1) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        // 1 == BatteryManager.BATTERY_PLUGGED_AC
        // 2 == BatteryManager.BATTERY_PLUGGED_USB
        *plugged = (0 < plug) ? 1 : 0;
    }

    if (charged) {
        // Watch out for C89 scoping rules because of the macro
        GET_INT_EXTRA(status, "status") // == BatteryManager.EXTRA_STATUS (API 5)
        if (status == -1) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        // 5 == BatteryManager.BATTERY_STATUS_FULL
        *charged = (status == 5) ? 1 : 0;
    }

    if (battery) {
        GET_BOOL_EXTRA(present, "present") // == BatteryManager.EXTRA_PRESENT (API 5)
        *battery = present ? 1 : 0;
    }

    if (seconds) {
        *seconds = -1; // not possible
    }

    if (percent) {
        int level;
        int scale;

        // Watch out for C89 scoping rules because of the macro
        {
            GET_INT_EXTRA(level_temp, "level") // == BatteryManager.EXTRA_LEVEL (API 5)
            level = level_temp;
        }
        // Watch out for C89 scoping rules because of the macro
        {
            GET_INT_EXTRA(scale_temp, "scale") // == BatteryManager.EXTRA_SCALE (API 5)
            scale = scale_temp;
        }

        if ((level == -1) || (scale == -1)) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        *percent = level * 100 / scale;
    }

    (*env)->DeleteLocalRef(env, intent);

    LocalReferenceHolder_Cleanup(&refs);
    return 0;
}

// Add all touch devices
void Android_JNI_InitTouch(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, midInitTouch);
}

void Android_JNI_PollInputDevices(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midPollInputDevices);
}

void Android_JNI_JoystickSetLED(int device_id, int red, int green, int blue)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midJoystickSetLED, device_id, red, green, blue);
}

void Android_JNI_PollHapticDevices(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midPollHapticDevices);
}

void Android_JNI_HapticRun(int device_id, float intensity, int length)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midHapticRun, device_id, intensity, length);
}

void Android_JNI_HapticRumble(int device_id, float low_frequency_intensity, float high_frequency_intensity, int length)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midHapticRumble, device_id, low_frequency_intensity, high_frequency_intensity, length);
}

void Android_JNI_HapticStop(int device_id)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, midHapticStop, device_id);
}

// See SDLActivity.java for constants.
#define COMMAND_SET_KEEP_SCREEN_ON 5

bool SDL_SendAndroidMessage(Uint32 command, int param)
{
    CHECK_PARAM(command < 0x8000) {
        return SDL_InvalidParamError("command");
    }
    return Android_JNI_SendMessage(command, param);
}

// sends message to be handled on the UI event dispatch thread
bool Android_JNI_SendMessage(int command, int param)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midSendMessage, command, param);
}

bool Android_JNI_SuspendScreenSaver(bool suspend)
{
    return Android_JNI_SendMessage(COMMAND_SET_KEEP_SCREEN_ON, (suspend == false) ? 0 : 1);
}

void Android_JNI_ShowScreenKeyboard(int input_type, SDL_Rect *inputRect)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticBooleanMethod(env, mActivityClass, midShowTextInput,
                                    input_type,
                                    inputRect->x,
                                    inputRect->y,
                                    inputRect->w,
                                    inputRect->h);
}

void Android_JNI_HideScreenKeyboard(void)
{
    // has to match Activity constant
    const int COMMAND_TEXTEDIT_HIDE = 3;
    Android_JNI_SendMessage(COMMAND_TEXTEDIT_HIDE, 0);
}

bool Android_JNI_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    JNIEnv *env;
    jclass clazz;
    jmethodID mid;
    jobject context;
    jstring title;
    jstring message;
    jintArray button_flags;
    jintArray button_ids;
    jobjectArray button_texts;
    jintArray colors;
    jobject text;
    jint temp;
    int i;

    env = Android_JNI_GetEnv();

    // convert parameters

    clazz = (*env)->FindClass(env, "java/lang/String");

    title = (*env)->NewStringUTF(env, messageboxdata->title);
    message = (*env)->NewStringUTF(env, messageboxdata->message);

    button_flags = (*env)->NewIntArray(env, messageboxdata->numbuttons);
    button_ids = (*env)->NewIntArray(env, messageboxdata->numbuttons);
    button_texts = (*env)->NewObjectArray(env, messageboxdata->numbuttons,
                                          clazz, NULL);
    for (i = 0; i < messageboxdata->numbuttons; ++i) {
        const SDL_MessageBoxButtonData *sdlButton;

        if (messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
            sdlButton = &messageboxdata->buttons[messageboxdata->numbuttons - 1 - i];
        } else {
            sdlButton = &messageboxdata->buttons[i];
        }

        temp = sdlButton->flags;
        (*env)->SetIntArrayRegion(env, button_flags, i, 1, &temp);
        temp = sdlButton->buttonID;
        (*env)->SetIntArrayRegion(env, button_ids, i, 1, &temp);
        text = (*env)->NewStringUTF(env, sdlButton->text);
        (*env)->SetObjectArrayElement(env, button_texts, i, text);
        (*env)->DeleteLocalRef(env, text);
    }

    if (messageboxdata->colorScheme) {
        colors = (*env)->NewIntArray(env, SDL_MESSAGEBOX_COLOR_COUNT);
        for (i = 0; i < SDL_MESSAGEBOX_COLOR_COUNT; ++i) {
            temp = (0xFFU << 24) |
                   (messageboxdata->colorScheme->colors[i].r << 16) |
                   (messageboxdata->colorScheme->colors[i].g << 8) |
                   (messageboxdata->colorScheme->colors[i].b << 0);
            (*env)->SetIntArrayRegion(env, colors, i, 1, &temp);
        }
    } else {
        colors = NULL;
    }

    (*env)->DeleteLocalRef(env, clazz);

    // context = SDLActivity.getContext();
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);

    clazz = (*env)->GetObjectClass(env, context);

    mid = (*env)->GetMethodID(env, clazz,
                              "messageboxShowMessageBox", "(ILjava/lang/String;Ljava/lang/String;[I[I[Ljava/lang/String;[I)I");
    *buttonID = (*env)->CallIntMethod(env, context, mid,
                                      (jint)messageboxdata->flags,
                                      title,
                                      message,
                                      button_flags,
                                      button_ids,
                                      button_texts,
                                      colors);

    (*env)->DeleteLocalRef(env, context);
    (*env)->DeleteLocalRef(env, clazz);

    // delete parameters

    (*env)->DeleteLocalRef(env, title);
    (*env)->DeleteLocalRef(env, message);
    (*env)->DeleteLocalRef(env, button_flags);
    (*env)->DeleteLocalRef(env, button_ids);
    (*env)->DeleteLocalRef(env, button_texts);
    (*env)->DeleteLocalRef(env, colors);

    return true;
}

/*
//////////////////////////////////////////////////////////////////////////////
//
// Functions exposed to SDL applications in SDL_system.h
//////////////////////////////////////////////////////////////////////////////
*/

void *SDL_GetAndroidJNIEnv(void)
{
    return Android_JNI_GetEnv();
}

void *SDL_GetAndroidActivity(void)
{
    // See SDL_system.h for caveats on using this function.

    JNIEnv *env = Android_JNI_GetEnv();
    if (!env) {
        return NULL;
    }

    // return SDLActivity.getContext();
    return (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);
}

int SDL_GetAndroidSDKVersion(void)
{
    static int sdk_version;
    if (!sdk_version) {
        char sdk[PROP_VALUE_MAX] = { 0 };
        if (__system_property_get("ro.build.version.sdk", sdk) != 0) {
            sdk_version = SDL_atoi(sdk);
        }
    }
    return sdk_version;
}

bool SDL_IsAndroidTablet(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midIsTablet);
}

bool SDL_IsAndroidTV(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midIsAndroidTV);
}

bool SDL_IsChromebook(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midIsChromebook);
}

bool SDL_IsDeXMode(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midIsDeXMode);
}

void SDL_SendAndroidBackButton(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, midManualBackButton);
}

const char *SDL_GetAndroidInternalStoragePath(void)
{
    static char *s_AndroidInternalFilesPath = NULL;

    if (!s_AndroidInternalFilesPath) {
        struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
        jmethodID mid;
        jobject context;
        jobject fileObject;
        jstring pathString;
        const char *path;

        JNIEnv *env = Android_JNI_GetEnv();
        if (!LocalReferenceHolder_Init(&refs, env)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // context = SDLActivity.getContext();
        context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);
        if (!context) {
            SDL_SetError("Couldn't get Android context!");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // fileObj = context.getFilesDir();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                                  "getFilesDir", "()Ljava/io/File;");
        fileObject = (*env)->CallObjectMethod(env, context, mid);
        if (!fileObject) {
            SDL_SetError("Couldn't get internal directory");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // path = fileObject.getCanonicalPath();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
                                  "getCanonicalPath", "()Ljava/lang/String;");
        pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);
        if (Android_JNI_ExceptionOccurred(false)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        path = (*env)->GetStringUTFChars(env, pathString, NULL);
        s_AndroidInternalFilesPath = SDL_strdup(path);
        (*env)->ReleaseStringUTFChars(env, pathString, path);

        LocalReferenceHolder_Cleanup(&refs);
    }
    return s_AndroidInternalFilesPath;
}

Uint32 SDL_GetAndroidExternalStorageState(void)
{
    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    jmethodID mid;
    jclass cls;
    jstring stateString;
    const char *state_string;
    Uint32 stateFlags;

    JNIEnv *env = Android_JNI_GetEnv();
    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return 0;
    }

    cls = (*env)->FindClass(env, "android/os/Environment");
    mid = (*env)->GetStaticMethodID(env, cls,
                                    "getExternalStorageState", "()Ljava/lang/String;");
    stateString = (jstring)(*env)->CallStaticObjectMethod(env, cls, mid);

    state_string = (*env)->GetStringUTFChars(env, stateString, NULL);

    // Print an info message so people debugging know the storage state
    __android_log_print(ANDROID_LOG_INFO, "SDL", "external storage state: %s", state_string);

    if (SDL_strcmp(state_string, "mounted") == 0) {
        stateFlags = SDL_ANDROID_EXTERNAL_STORAGE_READ |
                     SDL_ANDROID_EXTERNAL_STORAGE_WRITE;
    } else if (SDL_strcmp(state_string, "mounted_ro") == 0) {
        stateFlags = SDL_ANDROID_EXTERNAL_STORAGE_READ;
    } else {
        stateFlags = 0;
    }
    (*env)->ReleaseStringUTFChars(env, stateString, state_string);

    LocalReferenceHolder_Cleanup(&refs);

    return stateFlags;
}

const char *SDL_GetAndroidExternalStoragePath(void)
{
    static char *s_AndroidExternalFilesPath = NULL;

    if (!s_AndroidExternalFilesPath) {
        struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
        jmethodID mid;
        jobject context;
        jobject fileObject;
        jstring pathString;
        const char *path;

        JNIEnv *env = Android_JNI_GetEnv();
        if (!LocalReferenceHolder_Init(&refs, env)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // context = SDLActivity.getContext();
        context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);

        // fileObj = context.getExternalFilesDir();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                                  "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        fileObject = (*env)->CallObjectMethod(env, context, mid, NULL);
        if (!fileObject) {
            SDL_SetError("Couldn't get external directory");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // path = fileObject.getAbsolutePath();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
                                  "getAbsolutePath", "()Ljava/lang/String;");
        pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);

        path = (*env)->GetStringUTFChars(env, pathString, NULL);
        s_AndroidExternalFilesPath = SDL_strdup(path);
        (*env)->ReleaseStringUTFChars(env, pathString, path);

        LocalReferenceHolder_Cleanup(&refs);
    }
    return s_AndroidExternalFilesPath;
}

const char *SDL_GetAndroidCachePath(void)
{
    // !!! FIXME: lots of duplication with SDL_GetAndroidExternalStoragePath and SDL_GetAndroidInternalStoragePath; consolidate these functions!
    static char *s_AndroidCachePath = NULL;

    if (!s_AndroidCachePath) {
        struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
        jmethodID mid;
        jobject context;
        jobject fileObject;
        jstring pathString;
        const char *path;

        JNIEnv *env = Android_JNI_GetEnv();
        if (!LocalReferenceHolder_Init(&refs, env)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // context = SDLActivity.getContext();
        context = (*env)->CallStaticObjectMethod(env, mActivityClass, midGetContext);

        // fileObj = context.getExternalFilesDir();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                                  "getCacheDir", "()Ljava/io/File;");
        fileObject = (*env)->CallObjectMethod(env, context, mid);
        if (!fileObject) {
            SDL_SetError("Couldn't get cache directory");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        // path = fileObject.getAbsolutePath();
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
                                  "getAbsolutePath", "()Ljava/lang/String;");
        pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);

        path = (*env)->GetStringUTFChars(env, pathString, NULL);
        s_AndroidCachePath = SDL_strdup(path);
        (*env)->ReleaseStringUTFChars(env, pathString, path);

        LocalReferenceHolder_Cleanup(&refs);
    }
    return s_AndroidCachePath;
}

bool SDL_ShowAndroidToast(const char *message, int duration, int gravity, int xOffset, int yOffset)
{
    return Android_JNI_ShowToast(message, duration, gravity, xOffset, yOffset);
}

void Android_JNI_GetManifestEnvironmentVariables(void)
{
    if (!mActivityClass || !midGetManifestEnvironmentVariables) {
        __android_log_print(ANDROID_LOG_WARN, "SDL", "Request to get environment variables before JNI is ready");
        return;
    }

    if (!bHasEnvironmentVariables) {
        JNIEnv *env = Android_JNI_GetEnv();
        bool ret = (*env)->CallStaticBooleanMethod(env, mActivityClass, midGetManifestEnvironmentVariables);
        if (ret) {
            bHasEnvironmentVariables = true;
        }
    }
}

int Android_JNI_CreateCustomCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    JNIEnv *env = Android_JNI_GetEnv();
    int custom_cursor = 0;
    jintArray pixels;
    pixels = (*env)->NewIntArray(env, surface->w * surface->h);
    if (pixels) {
        (*env)->SetIntArrayRegion(env, pixels, 0, surface->w * surface->h, (int *)surface->pixels);
        custom_cursor = (*env)->CallStaticIntMethod(env, mActivityClass, midCreateCustomCursor, pixels, surface->w, surface->h, hot_x, hot_y);
        (*env)->DeleteLocalRef(env, pixels);
    } else {
        SDL_OutOfMemory();
    }
    return custom_cursor;
}

void Android_JNI_DestroyCustomCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, midDestroyCustomCursor, cursorID);
}

bool Android_JNI_SetCustomCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midSetCustomCursor, cursorID);
}

bool Android_JNI_SetSystemCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midSetSystemCursor, cursorID);
}

bool Android_JNI_SupportsRelativeMouse(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midSupportsRelativeMouse);
}

bool Android_JNI_SetRelativeMouseEnabled(bool enabled)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, midSetRelativeMouseEnabled, (enabled == 1));
}

typedef struct NativePermissionRequestInfo
{
    int request_code;
    char *permission;
    SDL_RequestAndroidPermissionCallback callback;
    void *userdata;
    struct NativePermissionRequestInfo *next;
} NativePermissionRequestInfo;

static NativePermissionRequestInfo pending_permissions;

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;
    int requestCode;
    bool result;
} RPC_data_nativePermissionResult_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePermissionResult)(
    JNIEnv *env, jclass cls,
    jint requestCode, jboolean result)
{
    RPC_Prepare(nativePermissionResult);
    RPC_Add(requestCode);
    RPC_Add(result);
    RPC_Send;
}

bool SDL_RequestAndroidPermission(const char *permission, SDL_RequestAndroidPermissionCallback cb, void *userdata)
{
    if (!permission) {
        return SDL_InvalidParamError("permission");
    } else if (!cb) {
        return SDL_InvalidParamError("cb");
    }

    NativePermissionRequestInfo *info = (NativePermissionRequestInfo *) SDL_calloc(1, sizeof (NativePermissionRequestInfo));
    if (!info) {
        return false;
    }

    info->permission = SDL_strdup(permission);
    if (!info->permission) {
        SDL_free(info);
        return false;
    }

    static SDL_AtomicInt next_request_code;
    info->request_code = SDL_AddAtomicInt(&next_request_code, 1);

    info->callback = cb;
    info->userdata = userdata;

    info->next = pending_permissions.next;
    pending_permissions.next = info;

    JNIEnv *env = Android_JNI_GetEnv();
    jstring jpermission = (*env)->NewStringUTF(env, permission);
    (*env)->CallStaticVoidMethod(env, mActivityClass, midRequestPermission, jpermission, info->request_code);
    (*env)->DeleteLocalRef(env, jpermission);

    return true;
}

// Show toast notification
bool Android_JNI_ShowToast(const char *message, int duration, int gravity, int xOffset, int yOffset)
{
    bool result;
    JNIEnv *env = Android_JNI_GetEnv();
    jstring jmessage = (*env)->NewStringUTF(env, message);
    result = (*env)->CallStaticBooleanMethod(env, mActivityClass, midShowToast, jmessage, duration, gravity, xOffset, yOffset);
    (*env)->DeleteLocalRef(env, jmessage);
    return result;
}

bool Android_JNI_GetLocale(char *buf, size_t buflen)
{
    bool result = false;
    if (buf && buflen > 0) {
        *buf = '\0';
        JNIEnv *env = Android_JNI_GetEnv();
        jstring string = (jstring)(*env)->CallStaticObjectMethod(env, mActivityClass, midGetPreferredLocales);
        if (string) {
            const char *utf8string = (*env)->GetStringUTFChars(env, string, NULL);
            if (utf8string) {
                result = true;
                SDL_strlcpy(buf, utf8string, buflen);
                (*env)->ReleaseStringUTFChars(env, string, utf8string);
            }
            (*env)->DeleteLocalRef(env, string);
        }
    }
    return result;
}

bool Android_JNI_OpenURL(const char *url)
{
    bool result;
    JNIEnv *env = Android_JNI_GetEnv();
    jstring jurl = (*env)->NewStringUTF(env, url);
    result = (*env)->CallStaticBooleanMethod(env, mActivityClass, midOpenURL, jurl);
    (*env)->DeleteLocalRef(env, jurl);
    return result;
}

int Android_JNI_OpenFileDescriptor(const char *uri, const char *mode)
{
    // Get fopen-style modes
    int moderead = 0, modewrite = 0, modeappend = 0, modeupdate = 0;

    for (const char *cmode = mode; *cmode; cmode++) {
        switch (*cmode) {
            case 'a':
                modeappend = 1;
                break;
            case 'r':
                moderead = 1;
                break;
            case 'w':
                modewrite = 1;
                break;
            case '+':
                modeupdate = 1;
                break;
            default:
                break;
        }
    }

    // Translate fopen-style modes to ContentResolver modes.
    // Android only allows "r", "w", "wt", "wa", "rw" or "rwt".
    const char *contentResolverMode = "r";

    if (moderead) {
        if (modewrite) {
            contentResolverMode = "rwt";
        } else {
            contentResolverMode = modeupdate ? "rw" : "r";
        }
    } else if (modewrite) {
        contentResolverMode = modeupdate ? "rwt" : "wt";
    } else if (modeappend) {
        contentResolverMode = modeupdate ? "rw" : "wa";
    }

    JNIEnv *env = Android_JNI_GetEnv();
    jstring jstringUri = (*env)->NewStringUTF(env, uri);
    jstring jstringMode = (*env)->NewStringUTF(env, contentResolverMode);
    jint fd = (*env)->CallStaticIntMethod(env, mActivityClass, midOpenFileDescriptor, jstringUri, jstringMode);
    (*env)->DeleteLocalRef(env, jstringUri);
    (*env)->DeleteLocalRef(env, jstringMode);

    if (fd == -1) {
        SDL_SetError("Unspecified error in JNI");
    }

    return fd;
}

static struct AndroidFileDialog
{
    int request_code;
    SDL_DialogFileCallback callback;
    void *userdata;
} mAndroidFileDialogData;

typedef struct {
    RPC_cmd_t cmd;
    Uint64 timestamp;

    int requestCode;
    int filter;

    char **charFileList;
    size_t count;

} RPC_data_onNativeFileDialog_t;

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeFileDialog)(
    JNIEnv *env, jclass jcls,
    jint requestCode, jobjectArray fileList, jint filter)
{
    size_t count = 0;
    char **charFileList = NULL;

    if (fileList) {
        count = (*env)->GetArrayLength(env, fileList);
        charFileList = SDL_calloc(count + 1, sizeof(char *));

        if (charFileList) {
            // Convert to UTF-8
            // TODO: Fix modified UTF-8 to classic UTF-8
            for (int i = 0; i < count; i++) {
                jstring string = (*env)->GetObjectArrayElement(env, fileList, i);
                if (!string) {
                    continue;
                }

                const char *utf8string = (*env)->GetStringUTFChars(env, string, NULL);
                if (!utf8string) {
                    (*env)->DeleteLocalRef(env, string);
                    continue;
                }

                char *newFile = SDL_strdup(utf8string);
                charFileList[i] = newFile;
                (*env)->ReleaseStringUTFChars(env, string, utf8string);
                (*env)->DeleteLocalRef(env, string);
            }
        }
    } else {
        SDL_SetError("Unspecified error in JNI");
    }

    RPC_Prepare(onNativeFileDialog);
    RPC_Add(requestCode);
    RPC_Add(filter);
    RPC_Add(count);
    RPC_Add(charFileList);
    RPC_Send;
}

bool Android_JNI_OpenFileDialog(
    SDL_DialogFileCallback callback, void *userdata,
    const SDL_DialogFileFilter *filters, int nfilters, bool forwrite,
    bool multiple)
{
    if (mAndroidFileDialogData.callback != NULL) {
        SDL_SetError("Only one file dialog can be run at a time.");
        return false;
    }

    if (forwrite) {
        multiple = false;
    }

    JNIEnv *env = Android_JNI_GetEnv();

    // Setup filters
    jobjectArray filtersArray = NULL;
    if (filters) {
        jclass stringClass = (*env)->FindClass(env, "java/lang/String");
        filtersArray = (*env)->NewObjectArray(env, nfilters, stringClass, NULL);
        (*env)->DeleteLocalRef(env, stringClass);

        // Convert to string
        for (int i = 0; i < nfilters; i++) {
            jstring str = (*env)->NewStringUTF(env, filters[i].pattern);
            (*env)->SetObjectArrayElement(env, filtersArray, i, str);
            (*env)->DeleteLocalRef(env, str);
        }
    }

    // Setup data
    static SDL_AtomicInt next_request_code;
    mAndroidFileDialogData.request_code = SDL_AddAtomicInt(&next_request_code, 1);
    mAndroidFileDialogData.userdata = userdata;
    mAndroidFileDialogData.callback = callback;

    // Invoke JNI
    jboolean success = (*env)->CallStaticBooleanMethod(env, mActivityClass,
        midShowFileDialog, filtersArray, (jboolean) multiple, (jboolean) forwrite, mAndroidFileDialogData.request_code);
    (*env)->DeleteLocalRef(env, filtersArray);
    if (!success) {
        mAndroidFileDialogData.callback = NULL;
        SDL_AddAtomicInt(&next_request_code, -1);
        SDL_SetError("Unspecified error in JNI");

        return false;
    }

    return true;
}


#define RPC_PAGE_SIZE (1024 * 10)
static char RPC_cmd_buffer_0[RPC_PAGE_SIZE];
static char RPC_cmd_buffer_1[RPC_PAGE_SIZE];

static char *RPC_cmd_buffer = NULL;  // RPC_cmd_buffer_0 or RPC_cmd_buffer_1
static int  RPC_cur_cmd_buffer = 0;  // 0 or 1
static int  RPC_cur_size = 0;        // used size
static int  RPC_cur_nb_cmd = 0;      // nb of cmds

static SDL_Mutex *RPC_Mutex = NULL;


static void RPC_Init()
{
    RPC_cmd_buffer = RPC_cmd_buffer_0;
    RPC_cur_cmd_buffer = 0;
    RPC_cur_size =  0;
    RPC_cur_nb_cmd = 0;

    if (!RPC_Mutex) {
        RPC_Mutex = SDL_CreateMutex();
    }

    if (!RPC_Mutex) {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "failed to create RPC_Mutex mutex");
    }

}

static void RPC_Send__(void *data, int len)
{
    SDL_LockMutex(RPC_Mutex);

    if (RPC_cur_size + len < RPC_PAGE_SIZE) {
        SDL_memcpy(RPC_cmd_buffer + RPC_cur_size, data, len);
        RPC_cur_size += len;
        RPC_cur_nb_cmd += 1;
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "SDL", "cannot add RPC of len %d", len);
    }

    SDL_UnlockMutex(RPC_Mutex);
}

void Android_PumpRPC(SDL_Window *window)
{
    int nb_cmd = 0;
    char *cmd_buffer = NULL;

    SDL_LockMutex(RPC_Mutex);

    if (RPC_cur_nb_cmd) {
        cmd_buffer = RPC_cmd_buffer;
        nb_cmd = RPC_cur_nb_cmd;

        // Toggle cmd buffer
        RPC_cur_size = 0;
        RPC_cur_nb_cmd = 0;

        if (RPC_cur_cmd_buffer == 0) {
            RPC_cur_cmd_buffer = 1;
            RPC_cmd_buffer = RPC_cmd_buffer_1;
        } else {
            RPC_cur_cmd_buffer = 0;
            RPC_cmd_buffer = RPC_cmd_buffer_0;
        }
    }

    SDL_UnlockMutex(RPC_Mutex);


#define RPC_Get(foo)                \
    RPC_data_##foo##_t data;        \
    len = sizeof (data);            \
    SDL_memcpy(&data, cmd_buffer, len);   \


#define RPC_GetNoData               \
    len = sizeof (RPC_data_t);      \


    while (nb_cmd--) {
        RPC_cmd_t cmd;
        int len; // don't initialize so that it can trigger some warning with `-Wsometimes-uninitialized` if RPC_Get*() has be forgotten
        SDL_memcpy(&cmd, cmd_buffer, sizeof (cmd));

#ifdef DEBUG_RPC
        SDL_Log("RPC cmd: %s", cmd2Str(cmd));
#endif

        switch (cmd) {

                // ------------------------------
                // SDLActivity_tab
                // ------------------------------
            case RPC_cmd_onNativeDropFile:
                {
                    RPC_Get(onNativeDropFile);
                    SDL_SendDropFile(NULL, NULL, data.filename);
                    SDL_SendDropComplete(NULL);
                    SDL_free(data.filename);
                }
                break;

            case RPC_cmd_nativeSetScreenResolution:
                {
                    RPC_Get(nativeSetScreenResolution);
                    Android_SetScreenResolution(data.surfaceWidth, data.surfaceHeight, data.deviceWidth, data.deviceHeight, data.density, data.rate);
                }
                break;

            case RPC_cmd_onNativeResize:
                {
                    RPC_GetNoData;
                    Android_SendResize(window);
                }
                break;

            case RPC_cmd_onNativeSurfaceCreated:
                {
                    RPC_GetNoData;
                    Android_nativeSurfaceCreated(window);
                }
                break;

            case RPC_cmd_onNativeSurfaceChanged:
                {
                    RPC_GetNoData;
                    Android_nativeSurfaceChanged(window);

                    Android_RestoreScreenKeyboard(SDL_GetVideoDevice(), window);
                }
                break;

            case RPC_cmd_onNativeSurfaceDestroyed:
                {
                    RPC_GetNoData;

                    // Pumped all events, so that we enter the Pause state.
                    // and the GL context is backed up, before we destroy EGL Surface and native_window
                    //
                    // That's how it was before. Doesn't seem to make a big difference...

                    Android_PumpLifecycleEvents(window);

                    Android_nativeSurfaceDestroyed(window);
                }
                break;

            case RPC_cmd_onNativeScreenKeyboardShown:
                {
                    RPC_GetNoData;
                    SDL_SendScreenKeyboardShown();
                }
                break;

            case RPC_cmd_onNativeScreenKeyboardHidden:
                {
                    RPC_GetNoData;
                    SDL_SendScreenKeyboardHidden();
                }
                break;

            case RPC_cmd_onNativeKeyDown:
                {
                    RPC_Get(onNativeKeyDown);
                    Android_OnKeyDown(data.keycode);
                }
                break;

            case RPC_cmd_onNativeKeyUp:
                {
                    RPC_Get(onNativeKeyUp);
                    Android_OnKeyUp(data.keycode);
                }
                break;

            case RPC_cmd_onNativeSoftReturnKey:
                {
                    RPC_GetNoData;
                    SDL_StopTextInput(window);
                }
                break;

            case RPC_cmd_onNativeKeyboardFocusLost:
                {
                    RPC_GetNoData;

                    // Calling SDL_StopTextInput will take care of hiding the keyboard and cleaning up the DummyText widget
                    SDL_StopTextInput(window);
                }
                break;

            case RPC_cmd_onNativeTouch:
                {
                    RPC_Get(onNativeTouch);
                    Android_OnTouch(window, data.touch_device_id_in, data.pointer_finger_id_in, data.action, data.x, data.y, data.p);
                }
                break;

            case RPC_cmd_onNativePinchStart:
                {
                    RPC_GetNoData;
                    SDL_SendPinch(SDL_EVENT_PINCH_BEGIN, 0, window, 0);
                }
                break;

            case RPC_cmd_onNativePinchUpdate:
                {
                    RPC_Get(onNativePinchUpdate);
                    SDL_SendPinch(SDL_EVENT_PINCH_UPDATE, 0, window, data.scale);
                }
                break;

            case RPC_cmd_onNativePinchEnd:
                {
                    RPC_GetNoData;
                    SDL_SendPinch(SDL_EVENT_PINCH_END, 0, window, 0);
                }
                break;

            case RPC_cmd_onNativeMouse:
                {
                    RPC_Get(onNativeMouse);
                    Android_OnMouse(window, data.button, data.action, data.x, data.y, data.relative);
                }
                break;

            case RPC_cmd_onNativePen:
                {
                    RPC_Get(onNativePen);
                    Android_OnPen(window, data.pen_id_in, data.device_type, data.button, data.action, data.x, data.y, data.p);
                }
                break;

            case RPC_cmd_onNativeAccel:
                {
                    RPC_Get(onNativeAccel);
                    fLastAccelerometer[0] = data.x;
                    fLastAccelerometer[1] = data.y;
                    fLastAccelerometer[2] = data.z;
                    bHasNewData = true;
                }
                break;

            case RPC_cmd_onNativeClipboardChanged:
                {
                    RPC_GetNoData;

                    // TODO: compute new mime types
                    SDL_SendClipboardUpdate(false, NULL, 0);
                }
                break;

            case RPC_cmd_onNativeLocaleChanged:
                {
                    RPC_GetNoData;
                    SDL_SendAppEvent(SDL_EVENT_LOCALE_CHANGED);
                }
                break;

            case RPC_cmd_onNativeDarkModeChanged:
                {
                    RPC_Get(onNativeDarkModeChanged);
                    Android_SetDarkMode(data.enabled);
                }
                break;

            case RPC_cmd_nativeFocusChanged:
                {
                    RPC_Get(nativeFocusChanged);
                    __android_log_print(ANDROID_LOG_VERBOSE, "SDL", "nativeFocusChanged()");
                    SDL_SendWindowEvent(window, (data.hasFocus ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST), 0, 0);
                }
                break;

            case RPC_cmd_nativeSetNaturalOrientation:
                {
                    RPC_Get(nativeSetNaturalOrientation);
                    displayNaturalOrientation = (SDL_DisplayOrientation)data.orientation;
                }
                break;

            case RPC_cmd_onNativeRotationChanged:
                {
                    RPC_Get(onNativeRotationChanged);

                    if (displayNaturalOrientation == SDL_ORIENTATION_LANDSCAPE) {
                        data.rotation += 90;
                    }

                    switch (data.rotation % 360) {
                        case 0:
                            displayCurrentOrientation = SDL_ORIENTATION_PORTRAIT;
                            break;
                        case 90:
                            displayCurrentOrientation = SDL_ORIENTATION_LANDSCAPE;
                            break;
                        case 180:
                            displayCurrentOrientation = SDL_ORIENTATION_PORTRAIT_FLIPPED;
                            break;
                        case 270:
                            displayCurrentOrientation = SDL_ORIENTATION_LANDSCAPE_FLIPPED;
                            break;
                        default:
                            displayCurrentOrientation = SDL_ORIENTATION_UNKNOWN;
                            break;
                    }

                    Android_SetOrientation(displayCurrentOrientation);

                }
                break;

            case RPC_cmd_onNativeInsetsChanged:
                {
                    RPC_Get(onNativeInsetsChanged);
                    Android_SetWindowSafeAreaInsets(window, data.left, data.right, data.top, data.bottom);
                }
                break;

            case RPC_cmd_nativeAddTouch:
                {
                    RPC_Get(nativeAddTouch);
                    SDL_AddTouch(Android_ConvertJavaTouchID(data.touchId), SDL_TOUCH_DEVICE_DIRECT, data.name);
                    SDL_free(data.name);
                }
                break;

            case RPC_cmd_nativePermissionResult:
                {
                    RPC_Get(nativePermissionResult);

                    NativePermissionRequestInfo *prev = &pending_permissions;
                    for (NativePermissionRequestInfo *info = prev->next; info != NULL; info = info->next) {
                        if (info->request_code == (int) data.requestCode) {
                            prev->next = info->next;
                            info->callback(info->userdata, info->permission, data.result ? true : false);
                            SDL_free(info->permission);
                            SDL_free(info);

                            /* done */
                            break;
                        }
                        prev = info;
                    }

                }
                break;

            case RPC_cmd_onNativeFileDialog:
                {
                    RPC_Get(onNativeFileDialog);

                    if (mAndroidFileDialogData.callback != NULL && mAndroidFileDialogData.request_code == data.requestCode) {

                        // Handle some error ...
                        if (!data.charFileList) {
                            mAndroidFileDialogData.callback(mAndroidFileDialogData.userdata, NULL, -1);
                            mAndroidFileDialogData.callback = NULL;
                            goto onNativeFileDialog_end;
                        } else {
                            for (int i = 0; i < data.count; i++) {
                                if (!data.charFileList[i]) {
                                    mAndroidFileDialogData.callback(mAndroidFileDialogData.userdata, NULL, -1);
                                    mAndroidFileDialogData.callback = NULL;
                                    goto onNativeFileDialog_end;
                                }
                            }
                        }

                        // Call user-provided callback
                        SDL_ClearError();
                        mAndroidFileDialogData.callback(mAndroidFileDialogData.userdata, (const char *const *) data.charFileList, data.filter);
                        mAndroidFileDialogData.callback = NULL;
                    }

onNativeFileDialog_end:

                    // Cleanup memory
                    for (int i = 0; i < data.count; i++) {
                        SDL_free(data.charFileList[i]);
                    }
                    SDL_free(data.charFileList);
                }
                break;

                // ------------------------------
                // SDLInputConnection_tab
                // ------------------------------
            case RPC_cmd_nativeCommitText:
                {
                    RPC_Get(nativeCommitText);
                    SDL_SendKeyboardText(data.text);
                    SDL_free(data.text);
                }
                break;

            case RPC_cmd_nativeGenerateScancodeForUnichar:
                {
                    RPC_Get(nativeGenerateScancodeForUnichar);
                    SDL_SendKeyboardUnicodeKey(0, data.chUnicode);
                }
                break;


                // ------------------------------
                // SDLAudioManager_tab
                // ------------------------------
            case RPC_cmd_nativeAddAudioDevice:
                {
                    RPC_Get(nativeAddAudioDevice);
#if ALLOW_MULTIPLE_ANDROID_AUDIO_DEVICES
                    if (SDL_GetCurrentAudioDriver() != NULL) {
                        void *handle = (void *)((size_t)data.device_id);
                        if (!SDL_FindPhysicalAudioDeviceByHandle(handle)) {
                            SDL_AddAudioDevice(data.recording, SDL_strdup(data.name), NULL, handle);
                        }
                    }
#endif
                    SDL_free(data.name);
                }

                break;

            case RPC_cmd_nativeRemoveAudioDevice:
                {
                    RPC_Get(nativeRemoveAudioDevice);
#if ALLOW_MULTIPLE_ANDROID_AUDIO_DEVICES
                    if (SDL_GetCurrentAudioDriver() != NULL) {
                        SDL_Log("Removing device with handle %d, recording %d", data.device_id, data.recording);
                        SDL_AudioDeviceDisconnected(SDL_FindPhysicalAudioDeviceByHandle((void *)((size_t)data.device_id)));
                    }
#endif
                }
                break;

#ifdef GAMEPAD_AS_RPC
                // ------------------------------
                // SDLControllerManager_tab
                // ------------------------------
            case RPC_cmd_onNativePadDown:
                {
                    RPC_Get(onNativePadDown);
#ifdef SDL_JOYSTICK_ANDROID
                    Android_OnPadDown(data.device_id, data.keycode);
#endif
                }
                break;
            case RPC_cmd_onNativePadUp:
                {
                    RPC_Get(onNativePadUp);
#ifdef SDL_JOYSTICK_ANDROID
                    Android_OnPadUp(data.device_id, data.keycode);
#endif
                }
                break;

            case RPC_cmd_onNativeJoy:
                {
                    RPC_Get(onNativeJoy);
#ifdef SDL_JOYSTICK_ANDROID
                    Android_OnJoy(data.device_id, data.axis, data.value);
#endif
                }
                break;

            case RPC_cmd_onNativeHat:
                {
                    RPC_Get(onNativeHat);
#ifdef SDL_JOYSTICK_ANDROID
                    Android_OnHat(data.device_id, data.hat_id, data.x, data.y);
#endif
                }
                break;

            case RPC_cmd_nativeAddJoystick:
                {
                    RPC_Get(nativeAddJoystick);
#ifdef SDL_JOYSTICK_ANDROID
                    Android_AddJoystick(data.device_id, data.device_name, data.device_desc,
                            data.vendor_id, data.product_id, data.button_mask, data.naxes,
                            data.axis_mask, data.nhats, data.can_rumble, data.has_rgb_led);
#endif
                    SDL_free(data.device_name);
                    SDL_free(data.device_desc);

                }
                break;

            case RPC_cmd_nativeRemoveJoystick:
                {
                    RPC_Get(nativeRemoveJoystick);

#ifdef SDL_JOYSTICK_ANDROID
                    Android_RemoveJoystick(data.device_id);
#endif

                }
                break;

            case RPC_cmd_nativeAddHaptic:
                {
                    RPC_Get(nativeAddHaptic);

#ifdef SDL_HAPTIC_ANDROID
                    Android_AddHaptic(data.device_id, data.device_name);
#endif
                    SDL_free(data.device_name);

                }
                break;

            case RPC_cmd_nativeRemoveHaptic:
                {
                    RPC_Get(nativeRemoveHaptic);
#ifdef SDL_HAPTIC_ANDROID
                    Android_RemoveHaptic(data.device_id);
#endif

                }
                break;
#endif


                // ------------------------------
                // HIDDeviceManager_tab
                // ------------------------------

            default:
                len = 0;
                SDL_Log("Error unknown RPC cmd %u", cmd);

        }

        cmd_buffer += len;
    }

}

#endif // SDL_PLATFORM_ANDROID
