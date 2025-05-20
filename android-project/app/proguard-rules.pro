# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in [sdk]/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   https://developer.android.com/build/shrink-code

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLActivity {
    java.lang.String nativeGetHint(java.lang.String); # Java-side doesn't use this, so it gets minified, but C-side still tries to register it
    java.lang.String clipboardGetText();
    boolean clipboardHasText();
    void clipboardSetText(java.lang.String);
    int createCustomCursor(int[], int, int, int, int);
    void destroyCustomCursor(int);
    android.content.Context getContext();
    boolean getManifestEnvironmentVariables();
    android.view.Surface getNativeSurface();
    void initTouch();
    boolean isAndroidTV();
    boolean isChromebook();
    boolean isDeXMode();
    boolean isScreenKeyboardShown();
    boolean isTablet();
    void manualBackButton();
    int messageboxShowMessageBox(int, java.lang.String, java.lang.String, int[], int[], java.lang.String[], int[]);
    void minimizeWindow();
    boolean openURL(java.lang.String);
    void requestPermission(java.lang.String, int);
    boolean showToast(java.lang.String, int, int, int, int);
    boolean sendMessage(int, int);
    boolean setActivityTitle(java.lang.String);
    boolean setCustomCursor(int);
    void setOrientation(int, int, boolean, java.lang.String);
    boolean setRelativeMouseEnabled(boolean);
    boolean setSystemCursor(int);
    void setWindowStyle(boolean);
    boolean shouldMinimizeOnFocusLoss();
    boolean showTextInput(int, int, int, int, int);
    boolean supportsRelativeMouse();
    int openFileDescriptor(java.lang.String, java.lang.String);
    boolean showFileDialog(java.lang.String[], boolean, boolean, int);
    java.lang.String getPreferredLocales();
    java.lang.String formatLocale(java.util.Locale);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.HIDDeviceManager {
    void closeDevice(int);
    boolean initialize(boolean, boolean);
    boolean openDevice(int);
    boolean readReport(int, byte[], boolean);
    int writeReport(int, byte[], boolean);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLAudioManager {
    void registerAudioDeviceCallback();
    void unregisterAudioDeviceCallback();
    void audioSetThreadPriority(boolean, int);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLControllerManager {
    void pollInputDevices();
    void pollHapticDevices();
    void hapticRun(int, float, int);
    void hapticRumble(int, float, float, int);
    void hapticStop(int);
}
