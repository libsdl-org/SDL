# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in [sdk]/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLActivity {
    void manualBackButton();
    boolean setActivityTitle(java.lang.String);
    void setWindowStyle(boolean);
    void setOrientation(int, int, boolean, java.lang.String);
    void minimizeWindow();
    boolean shouldMinimizeOnFocusLoss();
    boolean isScreenKeyboardShown();
    boolean supportsRelativeMouse();
    boolean setRelativeMouseEnabled(boolean);
    boolean sendMessage(int, int);
    android.content.Context getContext();
    boolean isAndroidTV();
    boolean isTablet();
    boolean isChromebook();
    boolean isDeXMode();
    boolean getManifestEnvironmentVariables();
    boolean showTextInput(int, int, int, int);
    android.view.Surface getNativeSurface();
    void initTouch();
    int messageboxShowMessageBox(int, java.lang.String, java.lang.String, int[], int[], java.lang.String[], int[]);
    boolean clipboardHasText();
    java.lang.String clipboardGetText();
    void clipboardSetText(java.lang.String);
    int createCustomCursor(int[], int, int, int, int);
    void destroyCustomCursor(int);
    boolean setCustomCursor(int);
    boolean setSystemCursor(int);
    void requestPermission(java.lang.String, int);
    int openURL(java.lang.String);
    int showToast(java.lang.String, int, int, int, int);
    native java.lang.String nativeGetHint(java.lang.String);
    int openFileDescriptor(java.lang.String, java.lang.String);
    boolean showFileDialog(java.lang.String[], boolean, boolean, int);
    native void onNativeFileDialog(int, java.lang.String[], int);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.HIDDeviceManager {
    boolean initialize(boolean, boolean);
    boolean openDevice(int);
    int writeReport(int, byte[], boolean);
    boolean readReport(int, byte[], boolean);
    void closeDevice(int);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLAudioManager {
    void registerAudioDeviceCallback();
    void unregisterAudioDeviceCallback();
    int[] audioOpen(int, int, int, int, int);
    void audioWriteFloatBuffer(float[]);
    void audioWriteShortBuffer(short[]);
    void audioWriteByteBuffer(byte[]);
    int[] recordingOpen(int, int, int, int, int);
    int recordingReadFloatBuffer(float[], boolean);
    int recordingReadShortBuffer(short[], boolean);
    int recordingReadByteBuffer(byte[], boolean);
    void audioClose();
    void recordingClose();
    void audioSetThreadPriority(boolean, int);
    int nativeSetupJNI();
    void removeAudioDevice(boolean, int);
    void addAudioDevice(boolean, java.lang.String, int);
}

-keep,includedescriptorclasses,allowoptimization class org.libsdl.app.SDLControllerManager {
    void pollInputDevices();
    void pollHapticDevices();
    void hapticRun(int, float, int);
    void hapticStop(int);
    void hapticRumble(int, float , float, int);
}
