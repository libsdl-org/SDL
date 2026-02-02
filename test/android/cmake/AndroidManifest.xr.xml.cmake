<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="@ANDROID_MANIFEST_PACKAGE@"
    android:versionCode="1"
    android:versionName="1.0"
    android:installLocation="auto">

    <!-- OpenGL ES 3.2 for Vulkan fallback on XR devices -->
    <uses-feature android:glEsVersion="0x00030002" android:required="true" />
    
    <!-- Vulkan requirements for Quest -->
    <uses-feature android:name="android.hardware.vulkan.level" android:required="true" android:version="1" />
    <uses-feature android:name="android.hardware.vulkan.version" android:required="true" android:version="0x00401000" />

    <!-- VR Head Tracking (required for OpenXR on Meta Quest) -->
    <uses-feature android:name="android.hardware.vr.headtracking" android:required="true" android:version="1" />

    <!-- Touchscreen not required for VR -->
    <uses-feature
        android:name="android.hardware.touchscreen"
        android:required="false" />
    
    <!-- Hand tracking support -->
    <uses-feature android:name="oculus.software.handtracking" android:required="false" />

    <!-- Game controller support -->
    <uses-feature
        android:name="android.hardware.bluetooth"
        android:required="false" />
    <uses-feature
        android:name="android.hardware.gamepad"
        android:required="false" />
    <uses-feature
        android:name="android.hardware.usb.host"
        android:required="false" />

    <!-- Allow access to the vibrator (for controller haptics) -->
    <uses-permission android:name="android.permission.VIBRATE" />
    
    <!-- OpenXR permissions (for runtime broker communication) -->
    <uses-permission android:name="org.khronos.openxr.permission.OPENXR" />
    <uses-permission android:name="org.khronos.openxr.permission.OPENXR_SYSTEM" />

    <!-- OpenXR runtime/layer queries -->
    <queries>
        <provider android:authorities="org.khronos.openxr.runtime_broker;org.khronos.openxr.system_runtime_broker" />
        <intent>
            <action android:name="org.khronos.openxr.OpenXRRuntimeService" />
        </intent>
        <intent>
            <action android:name="org.khronos.openxr.OpenXRApiLayerService" />
        </intent>
    </queries>

    <application
        android:allowBackup="true"
        android:icon="@mipmap/sdl-test"
        android:roundIcon="@mipmap/sdl-test_round"
        android:label="@string/label"
        android:supportsRtl="true"
        android:theme="@android:style/Theme.Black.NoTitleBar.Fullscreen"
        android:enableOnBackInvokedCallback="false"
        android:hardwareAccelerated="true">

        <!-- Meta Quest supported devices -->
        <meta-data android:name="com.oculus.supportedDevices" android:value="quest|quest2|questpro|quest3|quest3s" />
        <meta-data android:name="com.oculus.vr.focusaware" android:value="true" />
        
        <!-- Hand tracking support level (V2 allows launching without controllers) -->
        <meta-data android:name="com.oculus.handtracking.version" android:value="V2.0" />
        <meta-data android:name="com.oculus.handtracking.frequency" android:value="HIGH" />

        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLTestActivity"
            android:exported="true"
            android:label="@string/label"
            android:alwaysRetainTaskState="true"
            android:launchMode="singleTask"
            android:configChanges="density|keyboard|keyboardHidden|navigation|orientation|screenLayout|screenSize|uiMode"
            android:screenOrientation="landscape"
            android:theme="@android:style/Theme.Black.NoTitleBar.Fullscreen"
            android:excludeFromRecents="false"
            android:resizeableActivity="false"
            tools:ignore="NonResizeableActivity">

            <!-- Standard launcher intent -->
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
                <!-- VR intent category for Meta Quest -->
                <category android:name="com.oculus.intent.category.VR" />
                <!-- Khronos OpenXR category (for broader compatibility) -->
                <category android:name="org.khronos.openxr.intent.category.IMMERSIVE_HMD" />
            </intent-filter>
        </activity>

        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLEntryTestActivity"
            android:exported="false"
            android:label="@string/label">
        </activity>
    </application>
</manifest>
