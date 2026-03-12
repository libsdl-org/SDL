package org.libsdl.app;

import android.app.Activity;
import android.content.Context;

import java.lang.reflect.Method;

/**
    SDL library initialization
*/
public class SDL {

    // Subsystem flags matching SDL_INIT_* values for selective initialization.
    // When using SDL as an embedded library (e.g. for joystick-only support),
    // pass only the flags you need to setupJNI()/initialize() to avoid
    // requiring stub Java classes for unused subsystems.
    public static final int SDL_INIT_AUDIO      = 0x00000010;
    public static final int SDL_INIT_VIDEO      = 0x00000020;
    public static final int SDL_INIT_JOYSTICK   = 0x00000200;
    public static final int SDL_INIT_HAPTIC     = 0x00001000;
    public static final int SDL_INIT_GAMEPAD    = 0x00002000;
    public static final int SDL_INIT_SENSOR     = 0x00008000;
    public static final int SDL_INIT_CAMERA     = 0x00010000;

    // All subsystems (default behavior, backwards compatible)
    public static final int SDL_INIT_EVERYTHING = SDL_INIT_AUDIO | SDL_INIT_VIDEO |
        SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_SENSOR | SDL_INIT_CAMERA;

    // Track which subsystems were initialized
    private static int mInitializedSubsystems = 0;

    // Full initialization (backwards compatible)
    static public void setupJNI() {
        setupJNI(SDL_INIT_EVERYTHING);
    }

    // Selective initialization: only set up JNI for requested subsystems
    static public void setupJNI(int subsystems) {
        mInitializedSubsystems = subsystems;

        SDLActivity.nativeSetupJNI();

        if ((subsystems & SDL_INIT_AUDIO) != 0) {
            SDLAudioManager.nativeSetupJNI();
        }

        SDLControllerManager.nativeSetupJNI();
    }

    // Full initialization (backwards compatible)
    static public void initialize() {
        initialize(mInitializedSubsystems != 0 ? mInitializedSubsystems : SDL_INIT_EVERYTHING);
    }

    // Selective initialization: only initialize requested subsystems
    static public void initialize(int subsystems) {
        setContext(null);

        if ((subsystems & SDL_INIT_VIDEO) != 0) {
            SDLActivity.initialize();
        }

        if ((subsystems & SDL_INIT_AUDIO) != 0) {
            SDLAudioManager.initialize();
        }

        SDLControllerManager.initialize();
    }

    // This function stores the current activity (SDL or not)
    static public void setContext(Activity context) {
        if ((mInitializedSubsystems & SDL_INIT_AUDIO) != 0) {
            SDLAudioManager.setContext(context);
        }
        mContext = context;
    }

    static public Activity getContext() {
        return mContext;
    }

    static void loadLibrary(String libraryName) throws UnsatisfiedLinkError, SecurityException, NullPointerException {
        loadLibrary(libraryName, mContext);
    }

    static void loadLibrary(String libraryName, Context context) throws UnsatisfiedLinkError, SecurityException, NullPointerException {

        if (libraryName == null) {
            throw new NullPointerException("No library name provided.");
        }

        try {
            // Let's see if we have ReLinker available in the project.  This is necessary for
            // some projects that have huge numbers of local libraries bundled, and thus may
            // trip a bug in Android's native library loader which ReLinker works around.  (If
            // loadLibrary works properly, ReLinker will simply use the normal Android method
            // internally.)
            //
            // To use ReLinker, just add it as a dependency.  For more information, see
            // https://github.com/KeepSafe/ReLinker for ReLinker's repository.
            //
            Class<?> relinkClass = context.getClassLoader().loadClass("com.getkeepsafe.relinker.ReLinker");
            Class<?> relinkListenerClass = context.getClassLoader().loadClass("com.getkeepsafe.relinker.ReLinker$LoadListener");
            Class<?> contextClass = context.getClassLoader().loadClass("android.content.Context");
            Class<?> stringClass = context.getClassLoader().loadClass("java.lang.String");

            // Get a 'force' instance of the ReLinker, so we can ensure libraries are reinstalled if
            // they've changed during updates.
            Method forceMethod = relinkClass.getDeclaredMethod("force");
            Object relinkInstance = forceMethod.invoke(null);
            Class<?> relinkInstanceClass = relinkInstance.getClass();

            // Actually load the library!
            Method loadMethod = relinkInstanceClass.getDeclaredMethod("loadLibrary", contextClass, stringClass, stringClass, relinkListenerClass);
            loadMethod.invoke(relinkInstance, context, libraryName, null, null);
        }
        catch (final Throwable e) {
            // Fall back
            try {
                System.loadLibrary(libraryName);
            }
            catch (final UnsatisfiedLinkError ule) {
                throw ule;
            }
            catch (final SecurityException se) {
                throw se;
            }
        }
    }

    protected static Activity mContext;
}
