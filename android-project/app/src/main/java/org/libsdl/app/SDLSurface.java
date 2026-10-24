package org.libsdl.app;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.Insets;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.ScaleGestureDetector;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import java.util.function.Consumer;

/**
    SDLSurface. This is what we draw on, so we need to know when it's created
    in order to do anything useful.

    Because of this, that's where we set up the SDL thread
*/
public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback,
    View.OnApplyWindowInsetsListener, View.OnKeyListener, View.OnTouchListener,
    SensorEventListener, ScaleGestureDetector.OnScaleGestureListener {

    // Sensors
    protected SensorManager mSensorManager;
    protected Display mDisplay;

    // Keep track of the surface size to normalize touch events
    protected float mWidth, mHeight;

    // Is SurfaceView ready for rendering
    protected boolean mIsSurfaceReady;

    // Is on-screen keyboard visible
    protected boolean mKeyboardVisible;

    // Pinch events
    private final ScaleGestureDetector scaleGestureDetector;

    // HDR headroom changes
    protected Consumer<Display> mHdrSdrRatioListener;
    protected float mDesiredHDRHeadroom = -1.0f;
    protected boolean mHDRActive;
    protected boolean mHDRMeasured;    // true if we've measured a stable HDR headroom value
    protected boolean mHDRStable;      // true if the HDR headroom value has settled
    protected float mReportedHDRHeadroom = -1.0f;
    protected final Runnable mHDRStableRunnable = new Runnable() {
        @Override
        public void run() {
            try {
                if (mDisplay.getHdrSdrRatio() <= 1.01f) {
                    // We're not in HDR mode yet
                    return;
                }
            } catch (Exception ignored) {
                return;
            }
            mHDRStable = true;
            mHDRMeasured = true;
            updateHDRHeadroom();
        }
    };

    // How long the HDR/SDR ratio has to stay the same before we consider the display done ramping up
    protected static final long HDR_STABLE_DELAY_MS = 250;

    // While the HDR/SDR ratio is ramping, only report changes in headroom bigger than this fraction
    protected static final float HDR_RAMP_REPORT_THRESHOLD = 0.1f;

    // Startup
    protected SDLSurface(Context context) {
        super(context);
        getHolder().addCallback(this);

        scaleGestureDetector = new ScaleGestureDetector(context, this);

        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnApplyWindowInsetsListener(this);
        setOnKeyListener(this);
        setOnTouchListener(this);

        mDisplay = ((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        mSensorManager = (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);

        setOnGenericMotionListener(SDLActivity.getMotionListener());

        // Some arbitrary defaults to avoid a potential division by zero
        mWidth = 1.0f;
        mHeight = 1.0f;

        mIsSurfaceReady = false;
        if (Build.VERSION.SDK_INT >= 26) {
            // We can use CapturedPointerListener and avoid many shenanigans and problems
            setOnCapturedPointerListener(SDLActivity.getMotionListener());
        }
    }

    protected void handlePause() {
        enableSensor(Sensor.TYPE_ACCELEROMETER, false);
    }

    protected void handleResume() {
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnApplyWindowInsetsListener(this);
        setOnKeyListener(this);
        setOnTouchListener(this);
        enableSensor(Sensor.TYPE_ACCELEROMETER, true);
    }

    protected Surface getNativeSurface() {
        return getHolder().getSurface();
    }

    // Called when we have a valid drawing surface
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.v("SDL", "surfaceCreated()");
        enableHDRListener(true);
        SDLActivity.onNativeSurfaceCreated();
    }

    // Called when we lose the surface
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.v("SDL", "surfaceDestroyed()");

        // Transition to pause, if needed
        SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED;
        SDLActivity.handleNativeState();

        mIsSurfaceReady = false;
        SDLActivity.onNativeSurfaceDestroyed();
        enableHDRListener(false);
    }

    // Called when the surface is resized
    @Override
    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.v("SDL", "surfaceChanged()");

        if (SDLActivity.mSingleton == null) {
            return;
        }

        mWidth = width;
        mHeight = height;
        int nDeviceWidth = width;
        int nDeviceHeight = height;
        float density = 1.0f;
        try
        {
            DisplayMetrics realMetrics = new DisplayMetrics();
            mDisplay.getRealMetrics( realMetrics );
            nDeviceWidth = realMetrics.widthPixels;
            nDeviceHeight = realMetrics.heightPixels;
            // Use densityDpi instead of density to more closely match what the UI scale is
            density = (float)realMetrics.densityDpi / 160.0f;
        } catch(Exception ignored) {
        }

        synchronized(SDLActivity.getContext()) {
            // In case we're waiting on a size change after going fullscreen, send a notification.
            SDLActivity.getContext().notifyAll();
        }

        Log.v("SDL", "Window size: " + width + "x" + height);
        Log.v("SDL", "Device size: " + nDeviceWidth + "x" + nDeviceHeight);
        SDLActivity.nativeSetScreenResolution(width, height, nDeviceWidth, nDeviceHeight, density, mDisplay.getRefreshRate());
        updateHDRHeadroom();
        SDLActivity.onNativeResize();

        // Prevent a screen distortion glitch,
        // for instance when the device is in Landscape and a Portrait App is resumed.
        boolean skip = false;
        int requestedOrientation = SDLActivity.mSingleton.getRequestedOrientation();

        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT) {
            if (mWidth > mHeight) {
               skip = true;
            }
        } else if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE) {
            if (mWidth < mHeight) {
               skip = true;
            }
        }

        // Special Patch for Square Resolution: Black Berry Passport
        if (skip) {
           double min = Math.min(mWidth, mHeight);
           double max = Math.max(mWidth, mHeight);

           if (max / min < 1.20) {
              Log.v("SDL", "Don't skip on such aspect-ratio. Could be a square resolution.");
              skip = false;
           }
        }

        // Don't skip if we might be multi-window or have popup dialogs
        if (skip) {
            if (Build.VERSION.SDK_INT >= 24 /* Android 7.0 (N) */) {
                skip = false;
            }
        }

        if (skip) {
           Log.v("SDL", "Skip .. Surface is not ready.");
           mIsSurfaceReady = false;
           return;
        }

        /* If the surface has been previously destroyed by onNativeSurfaceDestroyed, recreate it here */
        SDLActivity.onNativeSurfaceChanged();

        /* Surface is ready */
        mIsSurfaceReady = true;

        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED;
        SDLActivity.handleNativeState();
    }

    // Window inset
    @Override
    public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= 30 /* Android 11 (R) */) {
            Insets combined = insets.getInsets(WindowInsets.Type.systemBars() |
                                               WindowInsets.Type.systemGestures() |
                                               WindowInsets.Type.mandatorySystemGestures() |
                                               WindowInsets.Type.tappableElement() |
                                               WindowInsets.Type.displayCutout());

            SDLActivity.onNativeInsetsChanged(combined.left, combined.right, combined.top, combined.bottom);

            if (insets.isVisible(WindowInsets.Type.ime())) {
                if (!mKeyboardVisible) {
                    mKeyboardVisible = true;
                    SDLActivity.onNativeScreenKeyboardShown();
                }
            } else {
                if (mKeyboardVisible) {
                    mKeyboardVisible = false;
                    SDLActivity.onNativeScreenKeyboardHidden();
                }
            }
        }

        // Pass these to any child views in case they need them
        return insets;
    }

    // Key events
    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        return SDLActivity.handleKeyEvent(v, keyCode, event, null);
    }

    private float getNormalizedX(float x)
    {
        if (mWidth <= 1) {
            return 0.5f;
        } else {
            return (x / (mWidth - 1));
        }
    }

    private float getNormalizedY(float y)
    {
        if (mHeight <= 1) {
            return 0.5f;
        } else {
            return (y / (mHeight - 1));
        }
    }

    // Touch events
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        /* Ref: http://developer.android.com/training/gestures/multi.html */
        int touchDevId = event.getDeviceId();
        final int pointerCount = event.getPointerCount();
        int action = event.getActionMasked();
        int pointerId;
        int i = 0;
        float x,y,p;

        if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_POINTER_DOWN)
            i = event.getActionIndex();

        do {
            int toolType = event.getToolType(i);

            if (toolType == MotionEvent.TOOL_TYPE_MOUSE) {
                int buttonState = event.getButtonState();
                boolean relative = false;

                // We need to check if we're in relative mouse mode and get the axis offset rather than the x/y values
                // if we are. We'll leverage our existing mouse motion listener
                SDLGenericMotionListener_API14 motionListener = SDLActivity.getMotionListener();
                x = motionListener.getEventX(event, i);
                y = motionListener.getEventY(event, i);
                relative = motionListener.inRelativeMode();

                SDLActivity.onNativeMouse(buttonState, action, x, y, relative);                 

            } else if (toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER) {
                pointerId = event.getPointerId(i);
                x = event.getX(i);
                y = event.getY(i);
                p = event.getPressure(i);
                if (p > 1.0f) {
                    // may be larger than 1.0f on some devices
                    // see the documentation of getPressure(i)
                    p = 1.0f;
                }

                // BUTTON_STYLUS_PRIMARY is 2^5, so shift by 4, and apply SDL_PEN_INPUT_DOWN/SDL_PEN_INPUT_ERASER_TIP
                int buttonState = (event.getButtonState() >> 4) | (1 << (toolType == MotionEvent.TOOL_TYPE_STYLUS ? 0 : 30));
                if ((event.getButtonState() & MotionEvent.BUTTON_TERTIARY) != 0) {
                    buttonState |= 0x08;
                }

                SDLActivity.onNativePen(pointerId, SDLActivity.getMotionListener().getPenDeviceType(event.getDevice()), buttonState, action, x, y, p);
            } else { // MotionEvent.TOOL_TYPE_FINGER or MotionEvent.TOOL_TYPE_UNKNOWN
                pointerId = event.getPointerId(i);
                x = getNormalizedX(event.getX(i));
                y = getNormalizedY(event.getY(i));
                p = event.getPressure(i);
                if (p > 1.0f) {
                    // may be larger than 1.0f on some devices
                    // see the documentation of getPressure(i)
                    p = 1.0f;
                }

                SDLActivity.onNativeTouch(touchDevId, pointerId, action, x, y, p);
            }

            // Non-primary up/down
            if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_POINTER_DOWN)
                break;
        } while (++i < pointerCount);

        scaleGestureDetector.onTouchEvent(event);

        return true;
    }

    // HDR state
    protected float getEstimatedHDRHeadroom() {
        float headroom = 1.0f;

        // Before Android 15 we can't request HDR headroom for the surface, and
        // we don't know how HDR content is shown relative to the SDR white level,
        // so applications can't render HDR content in a meaningful way, so leave
        // HDR disabled in that case.
        if (Build.VERSION.SDK_INT < 35 /* Android 15 (V) */) {
            return headroom;
        }

        try {
            Display.HdrCapabilities caps = mDisplay.getHdrCapabilities();
            if (caps != null) {
                for (int type : caps.getSupportedHdrTypes()) {
                    if (type == Display.HdrCapabilities.HDR_TYPE_HDR10) {
                        // Estimate the headroom from the display peak luminance and the
                        // BT.2408 reference white. We don't know the actual SDR white level,
                        // so this is only a rough guess until we've shown HDR content and
                        // seen how far the display goes above SDR white.
                        final float SDR_reference_white_nits = 203.0f;
                        float max_luminance = caps.getDesiredMaxLuminance();
                        if (max_luminance <= SDR_reference_white_nits) {
                            // The display didn't report a usable peak luminance
                            max_luminance = 1000.0f;
                        }
                        headroom = max_luminance / SDR_reference_white_nits;
                        break;
                    }
                }
            }

            if (Build.VERSION.SDK_INT >= 36 /* Android 16 */ && mDisplay.isHdrSdrRatioAvailable()) {
                float highest = mDisplay.getHighestHdrSdrRatio();
                if (highest > 1.0f) {
                    if (headroom > 1.0f) {
                        // Use the minimum of our estimate and the highest possible headroom
                        headroom = Math.min(headroom, highest);
                    } else {
                        headroom = highest;
                    }
                }
            }
        } catch (Exception ignored) {
        }

        return headroom;
    }

    protected void updateHDRHeadroom() {
        float headroom = getEstimatedHDRHeadroom();

        if (mHDRActive && headroom > 1.0f) {
            try {
                if (mDisplay.isHdrSdrRatioAvailable()) {
                    // We're requesting as much headroom as the display can do, so once the
                    // display has finished ramping up, the HDR/SDR ratio is the headroom that's
                    // actually available.
                    //
                    // Until then, keep reporting the estimate so applications don't see the
                    // headroom drop to nothing and then ramp back up.
                    //
                    // After that, report the ratio even if the display can't currently go above
                    // the SDR white level, e.g. at maximum brightness, but keep it slightly
                    // above 1.0 so HDR stays enabled.
                    float current = mDisplay.getHdrSdrRatio();
                    if (mHDRMeasured) {
                        headroom = Math.max(current, 1.01f);
                    } else {
                        headroom = Math.max(headroom, current);
                    }
                }
            } catch (Exception ignored) {
            }
        }

        // The ratio changes every frame while the display ramps, so only report significant changes until it settles
        if (mHDRActive && !mHDRStable && mReportedHDRHeadroom > 0.0f &&
            Math.abs(headroom - mReportedHDRHeadroom) < HDR_RAMP_REPORT_THRESHOLD * mReportedHDRHeadroom) {
            return;
        }

        mReportedHDRHeadroom = headroom;
        SDLActivity.nativeSetScreenHDRHeadroom(headroom);
    }

    protected void onHDRSDRRatioChanged() {
        if (mHDRActive) {
            // Wait for the headroom value to stabilize again
            mHDRStable = false;
            removeCallbacks(mHDRStableRunnable);
            postDelayed(mHDRStableRunnable, HDR_STABLE_DELAY_MS);
        }
        updateHDRHeadroom();
    }

    protected void setDesiredHDRHeadroom(float headroom) {
        if (Build.VERSION.SDK_INT < 35 /* Android 15 (V) */) {
            return;
        }

        // Android only shows content above the SDR white level when more than 1.01 headroom is requested
        if (headroom > 1.0f && headroom <= 1.01f) {
            headroom = 1.02f;
        }

        if (headroom == mDesiredHDRHeadroom) {
            return;
        }

        mDesiredHDRHeadroom = headroom;
        setDesiredHdrHeadroom(headroom);

        boolean active = (headroom > 1.01f);
        if (active != mHDRActive) {
            mHDRActive = active;
            mHDRMeasured = false;
            mHDRStable = false;
            removeCallbacks(mHDRStableRunnable);
            if (active) {
                postDelayed(mHDRStableRunnable, HDR_STABLE_DELAY_MS);
            }
            updateHDRHeadroom();
        }
    }

    protected void enableHDRListener(boolean enabled) {
        if (Build.VERSION.SDK_INT < 35 /* Android 15 (V) */) {
            return;
        }

        try {
            if (enabled) {
                if (mHdrSdrRatioListener == null && mDisplay.isHdrSdrRatioAvailable()) {
                    mHdrSdrRatioListener = new Consumer<Display>() {
                        @Override
                        public void accept(Display display) {
                            onHDRSDRRatioChanged();
                        }
                    };
                    mDisplay.registerHdrSdrRatioChangedListener(getContext().getMainExecutor(), mHdrSdrRatioListener);
                }
            } else {
                if (mHdrSdrRatioListener != null) {
                    mDisplay.unregisterHdrSdrRatioChangedListener(mHdrSdrRatioListener);
                    mHdrSdrRatioListener = null;
                }
                removeCallbacks(mHDRStableRunnable);
            }
        } catch (Exception ignored) {
        }
    }

    // Sensor events
    protected void enableSensor(int sensortype, boolean enabled) {
        // TODO: This uses getDefaultSensor - what if we have >1 accels?
        if (enabled) {
            SDLSensorManager.registerListener(mSensorManager, this,
                            mSensorManager.getDefaultSensor(sensortype),
                            SensorManager.SENSOR_DELAY_GAME);
        } else {
            SDLSensorManager.unregisterListener(mSensorManager, this,
                            mSensorManager.getDefaultSensor(sensortype));
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // TODO
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {

            // Since we may have an orientation set, we won't receive onConfigurationChanged events.
            // We thus should check here.
            int newRotation;

            float x, y;
            switch (mDisplay.getRotation()) {
                case Surface.ROTATION_0:
                default:
                    x = event.values[0];
                    y = event.values[1];
                    newRotation = 0;
                    break;
                case Surface.ROTATION_90:
                    x = -event.values[1];
                    y = event.values[0];
                    newRotation = 90;
                    break;
                case Surface.ROTATION_180:
                    x = -event.values[0];
                    y = -event.values[1];
                    newRotation = 180;
                    break;
                case Surface.ROTATION_270:
                    x = event.values[1];
                    y = -event.values[0];
                    newRotation = 270;
                    break;
            }

            if (newRotation != SDLActivity.mCurrentRotation) {
                SDLActivity.mCurrentRotation = newRotation;
                SDLActivity.onNativeRotationChanged(newRotation);
            }
        }
    }

    // Prevent android internal NullPointerException (https://github.com/libsdl-org/SDL/issues/13306)
    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        try {
            return super.onResolvePointerIcon(event, pointerIndex);
        } catch (NullPointerException e) {
            return null;
        }
    }

    // Captured pointer events for API 26.
    @Override
    public boolean onCapturedPointerEvent(MotionEvent event)
    {
        return SDLActivity.getMotionListener().onGenericMotion(this, event);
    }

    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        float scale = detector.getScaleFactor();
        float span_x = getNormalizedX(detector.getCurrentSpanX());
        float span_y = getNormalizedY(detector.getCurrentSpanY());
        float focus_x = getNormalizedX(detector.getFocusX());
        float focus_y = getNormalizedY(detector.getFocusY());
        SDLActivity.onNativePinchUpdate(scale, span_x, span_y, focus_x, focus_y);
        return true;
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        float span_x = getNormalizedX(detector.getCurrentSpanX());
        float span_y = getNormalizedY(detector.getCurrentSpanY());
        float focus_x = getNormalizedX(detector.getFocusX());
        float focus_y = getNormalizedY(detector.getFocusY());
        SDLActivity.onNativePinchStart(span_x, span_y, focus_x, focus_y);
        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        SDLActivity.onNativePinchEnd();
    }

}
