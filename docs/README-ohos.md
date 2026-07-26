OpenHarmony / HarmonyOS
================================================================================

Requirements
================================================================================

DevEco Studio 5.0.0 or later
https://developer.huawei.com/consumer/en/download

Harmony OS Development Toolchain 5.0.0 (API 12) or later (Bundled in the DevEco Studio)


How the port works
================================================================================

- OpenHarmony / Harmony OS apps are based on ArkTS/JS running on the Ark JS runtime, optionally with parts written in C
- We can use the napi and ndk provided by the Harmony SDK to fetch the app window, context and so on. We wrote a simple Harmony shell for SDL, simpily initialize the basic application and the context for SDL library, then runs your app in another thread. (Harmony JS apps only contains one thread, if we runs your app main loop directly in JS, the whole app just freezes)
- This produces a .hap or .app package which can be installed in OpenHarmony and HarmonyOS Emulators (you will need to apply for a certificate to sign the app before testing it on a real HarmonyOS machine)


Building the example app and test it (HarmonyOS Emulator)
================================================================================

Download the DevEco Studio, and open the ohos-project folder in this repo

Then, download a emulator in the Device Manager tab, and runs it.

Click the run button, the IDE will automatically build the app and run it for you, you can act with your app in the emulator window.
