<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="@ANDROID_MANIFEST_PACKAGE@">

    <!-- OpenGL ES 3.2 for Vulkan fallback on XR devices -->
    <uses-feature android:glEsVersion="0x00030002" android:required="false" />

    <!-- VR Head Tracking (required for OpenXR on Meta Quest) -->
    <uses-feature android:name="android.hardware.vr.headtracking" android:required="true" />

    <!-- Touchscreen not required for VR -->
    <uses-feature
        android:name="android.hardware.touchscreen"
        android:required="false" />

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

    <application
        android:allowBackup="true"
        android:icon="@mipmap/sdl-test"
        android:roundIcon="@mipmap/sdl-test_round"
        android:label="@string/label"
        android:supportsRtl="true"
        android:theme="@style/AppTheme"
        android:enableOnBackInvokedCallback="false"
        android:hardwareAccelerated="true">

        <!-- Meta Quest supported devices -->
        <meta-data android:name="com.oculus.supportedDevices" android:value="quest|quest2|questpro|quest3" />

        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLTestActivity"
            android:exported="true"
            android:label="@string/label"
            android:alwaysRetainTaskState="true"
            android:launchMode="singleInstance"
            android:configChanges="density|keyboard|keyboardHidden|navigation|orientation|screenLayout|screenSize|uiMode"
            android:screenOrientation="landscape"
            android:excludeFromRecents="false"
            android:resizeableActivity="false">

            <!-- Standard launcher intent -->
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
                <!-- VR intent category for Meta Quest -->
                <category android:name="com.oculus.intent.category.VR" />
            </intent-filter>
        </activity>

        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLEntryTestActivity"
            android:exported="false"
            android:label="@string/label">
        </activity>
    </application>
</manifest>
