<?xml version="1.0" encoding="utf-8"?>
<shortcuts xmlns:android="http://schemas.android.com/apk/res/android">
    <shortcut
        android:shortcutId="modifyArguments"
        android:enabled="true"
        android:icon="@drawable/sdl-test_foreground"
        android:shortcutShortLabel="@string/shortcutModifyArgumentsShortLabel">
        <intent
            android:action="@ANDROID_MANIFEST_PACKAGE@.MODIFY_ARGUMENTS"
            android:targetPackage="@ANDROID_MANIFEST_PACKAGE@"
            android:targetClass="@ANDROID_MANIFEST_PACKAGE@.SDLEntryTestActivity" />
    </shortcut>
    <shortcut
        android:shortcutId="intermediateActivity"
        android:enabled="true"
        android:icon="@drawable/sdl-test_foreground"
        android:shortcutShortLabel="@string/shortcutIntermediateActivityShortLabel">
        <intent
            android:action="android.intent.action.MAIN"
            android:targetPackage="@ANDROID_MANIFEST_PACKAGE@"
            android:targetClass="@ANDROID_MANIFEST_PACKAGE@.SDLEntryTestActivity" />
    </shortcut>
    <!-- Specify more shortcuts here. -->
</shortcuts>
