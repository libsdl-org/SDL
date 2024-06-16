<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="@ANDROID_MANIFEST_PACKAGE@">

    <!-- OpenGL ES 2.0 -->
    <uses-feature android:glEsVersion="0x00020000" />

    <!-- Touchscreen support -->
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

    <!-- External mouse input events -->
    <uses-feature
        android:name="android.hardware.type.pc"
        android:required="false" />

    <!-- Allow access to the vibrator -->
    <uses-permission android:name="android.permission.VIBRATE" />

    <!-- Allow access to the microphone -->
    <uses-permission android:name="android.permission.RECORD_AUDIO" />

    <!-- Allow access to the camera -->
    <uses-permission android:name="android.permission.CAMERA" />
    <uses-feature android:name="android.hardware.camera" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/sdl-test"
        android:roundIcon="@mipmap/sdl-test_round"
        android:label="@string/label"
        android:supportsRtl="true"
        android:hardwareAccelerated="true"
        tools:targetApi="31">
        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLTestActivity"
            android:exported="true"
            android:label="@string/label"
            android:alwaysRetainTaskState="true"
            android:launchMode="singleInstance"
            android:configChanges="layoutDirection|locale|orientation|uiMode|screenLayout|screenSize|smallestScreenSize|keyboard|keyboardHidden|navigation"
            android:preferMinimalPostProcessing="true"
            android:screenOrientation="fullSensor">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
            <intent-filter>
                <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
            </intent-filter>
            <meta-data
                android:name="android.app.shortcuts"
                android:resource="@xml/shortcuts" />
        </activity>
        <activity
            android:name="@ANDROID_MANIFEST_PACKAGE@.SDLEntryTestActivity"
            android:exported="false"
            android:label="@string/label">
        </activity>
    </application>
</manifest>
