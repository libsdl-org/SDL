# OpenXR / VR Development with SDL

This document covers how to build OpenXR (VR/AR) applications using SDL's GPU API with OpenXR integration.

## Overview

SDL3 provides OpenXR integration through the GPU API, allowing you to render to VR/AR headsets using a unified interface across multiple graphics backends (Vulkan, D3D12, Metal).

**Key features:**
- Automatic OpenXR instance and session management
- Swapchain creation and image acquisition
- Support for multi-pass stereo rendering
- Works with desktop VR runtimes (SteamVR, Oculus, Windows Mixed Reality) and standalone headsets (Meta Quest, Pico)

## Desktop Development

### Requirements

1. **OpenXR Loader** (`openxr_loader.dll` / `libopenxr_loader.so`)
   - On Windows: Usually installed with VR runtime software (Oculus, SteamVR)
   - On Linux: Install via package manager (e.g., `libopenxr-loader1` on Ubuntu)
   - Can also use `SDL_HINT_OPENXR_LIBRARY` to specify a custom loader path

2. **OpenXR Runtime**
   - At least one OpenXR runtime must be installed and active
   - Examples: SteamVR, Oculus Desktop, Monado (Linux)

3. **VR Headset**
   - Connected and recognized by the runtime

### Basic Usage

```c
#include <openxr/openxr.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_openxr.h>

// These will be populated by SDL
XrInstance xr_instance = XR_NULL_HANDLE;
XrSystemId xr_system_id = 0;

// Create GPU device with XR enabled
SDL_PropertiesID props = SDL_CreateProperties();
SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENABLE_BOOLEAN, true);
SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_INSTANCE_POINTER, &xr_instance);
SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_SYSTEM_ID_POINTER, &xr_system_id);

// Optional: Override app name/version (defaults to SDL_SetAppMetadata values if not set)
SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME_STRING, "My VR App");
SDL_SetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_VERSION_NUMBER, 1);

SDL_GPUDevice *device = SDL_CreateGPUDeviceWithProperties(props);
SDL_DestroyProperties(props);

// xr_instance and xr_system_id are now populated by SDL
```

See `test/testgpu_spinning_cube_xr.c` for a complete example.

---

## Android Development

Building OpenXR applications for Android standalone headsets (Meta Quest, Pico, etc.) requires additional manifest configuration beyond standard Android apps.

### Android Manifest Requirements

The manifest requirements fall into three categories:

1. **OpenXR Standard (Khronos)** - Required for all OpenXR apps
2. **Platform-Specific** - Required for specific headset platforms
3. **Optional Features** - Enable additional capabilities

---

### OpenXR Standard Requirements (All Platforms)

These are required by the Khronos OpenXR specification for Android:

#### Permissions

```xml
<!-- OpenXR runtime broker communication -->
<uses-permission android:name="org.khronos.openxr.permission.OPENXR" />
<uses-permission android:name="org.khronos.openxr.permission.OPENXR_SYSTEM" />
```

#### Queries (Android 11+)

Required for the app to discover OpenXR runtimes:

```xml
<queries>
    <provider android:authorities="org.khronos.openxr.runtime_broker;org.khronos.openxr.system_runtime_broker" />
    <intent>
        <action android:name="org.khronos.openxr.OpenXRRuntimeService" />
    </intent>
    <intent>
        <action android:name="org.khronos.openxr.OpenXRApiLayerService" />
    </intent>
</queries>
```

#### Hardware Features

```xml
<!-- VR head tracking (standard OpenXR requirement) -->
<uses-feature android:name="android.hardware.vr.headtracking"
              android:required="true"
              android:version="1" />

<!-- Touchscreen not required for VR -->
<uses-feature android:name="android.hardware.touchscreen"
              android:required="false" />

<!-- Graphics requirements -->
<uses-feature android:glEsVersion="0x00030002" android:required="true" />
<uses-feature android:name="android.hardware.vulkan.level"
              android:required="true"
              android:version="1" />
<uses-feature android:name="android.hardware.vulkan.version"
              android:required="true"
              android:version="0x00401000" />
```

#### Intent Category

```xml
<activity ...>
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
        <!-- Khronos OpenXR immersive app category -->
        <category android:name="org.khronos.openxr.intent.category.IMMERSIVE_HMD" />
    </intent-filter>
</activity>
```

---

### Meta Quest Requirements

These are **required** for apps to run properly on Meta Quest devices. Without these, your app may launch in "pancake" 2D mode instead of VR.

#### VR Intent Category (Critical!)

```xml
<activity ...>
    <intent-filter>
        ...
        <!-- CRITICAL: Without this, app launches in 2D mode on Quest! -->
        <category android:name="com.oculus.intent.category.VR" />
    </intent-filter>
</activity>
```

#### Supported Devices

```xml
<application ...>
    <!-- Required: Specifies which Quest devices are supported -->
    <meta-data android:name="com.oculus.supportedDevices"
               android:value="quest|quest2|questpro|quest3|quest3s" />
</application>
```

#### Focus Handling (Recommended)

```xml
<application ...>
    <!-- Properly handles when user opens the Quest system menu -->
    <meta-data android:name="com.oculus.vr.focusaware"
               android:value="true" />
</application>
```

#### Hand Tracking (Optional)

```xml
<!-- Feature declaration -->
<uses-feature android:name="oculus.software.handtracking"
              android:required="false" />

<application ...>
    <!-- V2.0 allows app to launch without controllers -->
    <meta-data android:name="com.oculus.handtracking.version"
               android:value="V2.0" />
    <meta-data android:name="com.oculus.handtracking.frequency"
               android:value="HIGH" />
</application>
```

#### VR Splash Screen (Optional)

```xml
<application ...>
    <meta-data android:name="com.oculus.ossplash"
               android:value="true" />
    <meta-data android:name="com.oculus.ossplash.colorspace"
               android:value="QUEST_SRGB_NONGAMMA" />
    <meta-data android:name="com.oculus.ossplash.background"
               android:resource="@drawable/vr_splash" />
</application>
```

---

### Pico Requirements

For Pico Neo, Pico 4, and other Pico headsets:

#### VR Intent Category

```xml
<activity ...>
    <intent-filter>
        ...
        <!-- Pico VR category -->
        <category android:name="com.picovr.intent.category.VR" />
    </intent-filter>
</activity>
```

#### Supported Devices (Optional)

```xml
<application ...>
    <!-- Pico device support -->
    <meta-data android:name="pvr.app.type"
               android:value="vr" />
</application>
```

---

### HTC Vive Focus / VIVE XR Elite

```xml
<activity ...>
    <intent-filter>
        ...
        <!-- HTC Vive category -->
        <category android:name="com.htc.intent.category.VRAPP" />
    </intent-filter>
</activity>
```

---

## Quick Reference Table

| Declaration | Purpose | Scope |
|-------------|---------|-------|
| `org.khronos.openxr.permission.OPENXR` | Runtime communication | All OpenXR |
| `android.hardware.vr.headtracking` | Marks app as VR | All OpenXR |
| `org.khronos.openxr.intent.category.IMMERSIVE_HMD` | Khronos standard VR category | All OpenXR |
| `com.oculus.intent.category.VR` | Launch in VR mode | Meta Quest |
| `com.oculus.supportedDevices` | Device compatibility | Meta Quest |
| `com.oculus.vr.focusaware` | System menu handling | Meta Quest |
| `com.picovr.intent.category.VR` | Launch in VR mode | Pico |
| `com.htc.intent.category.VRAPP` | Launch in VR mode | HTC Vive |

---

## Example Manifest

SDL provides an example XR manifest template at:
`test/android/cmake/AndroidManifest.xr.xml.cmake`

This template includes:
- All Khronos OpenXR requirements
- Meta Quest support (configurable via `SDL_ANDROID_XR_META_SUPPORT` CMake option)
- Proper intent filters for VR launching

---

## Common Issues

### App launches in 2D "pancake" mode

**Cause:** Missing platform-specific VR intent category.

**Solution:** Add the appropriate category for your target platform:
- Meta Quest: `com.oculus.intent.category.VR`
- Pico: `com.picovr.intent.category.VR`
- HTC: `com.htc.intent.category.VRAPP`

### "No OpenXR runtime found" error

**Cause:** The OpenXR loader can't find a runtime.

**Solutions:**
- **Desktop:** Ensure VR software (SteamVR, Oculus) is installed and running
- **Android:** Ensure your manifest has the correct `<queries>` block for runtime discovery
- **Linux:** Install `libopenxr-loader1` and configure the active runtime

### OpenXR loader not found

**Cause:** `openxr_loader.dll` / `libopenxr_loader.so` is not in the library path.

**Solutions:**
- Install the Khronos OpenXR SDK
- On Windows, VR runtimes typically install this, but may not add it to PATH
- Use `SDL_HINT_OPENXR_LIBRARY` to specify the loader path explicitly

### Vulkan validation errors on shutdown

**Cause:** GPU resources destroyed while still in use.

**Solution:** Call `SDL_WaitForGPUIdle(device)` before releasing any GPU resources or destroying the device.

---

## Additional Resources

- [Khronos OpenXR Specification](https://www.khronos.org/openxr/)
- [Meta Quest Developer Documentation](https://developer.oculus.com/documentation/)
- [Pico Developer Documentation](https://developer.pico-interactive.com/)
- [SDL GPU API Documentation](https://wiki.libsdl.org/)
- Example code: `test/testgpu_spinning_cube_xr.c`
