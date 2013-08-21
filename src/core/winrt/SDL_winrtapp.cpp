
#include <string>
#include <unordered_map>
#include <sstream>

#include "ppltasks.h"

extern "C" {
#include "SDL_assert.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_main.h"
#include "SDL_stdinc.h"
#include "SDL_render.h"
#include "../../video/SDL_sysvideo.h"
//#include "../../SDL_hints_c.h"
#include "../../events/scancodes_windows.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../render/SDL_sysrender.h"
}

#include "../../video/winrt/SDL_winrtvideo.h"
#include "SDL_winrtapp.h"

using namespace concurrency;
using namespace std;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Devices::Input;
using namespace Windows::Graphics::Display;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;

// Compile-time debugging options:
// To enable, uncomment; to disable, comment them out.
//#define LOG_POINTER_EVENTS 1
//#define LOG_WINDOW_EVENTS 1
//#define LOG_ORIENTATION_EVENTS 1


// HACK, DLudwig: The C-style main() will get loaded via the app's
// WinRT-styled main(), which is part of SDLmain_for_WinRT.cpp.
// This seems wrong on some level, but does seem to work.
typedef int (*SDL_WinRT_MainFunction)(int, char **);
static SDL_WinRT_MainFunction SDL_WinRT_main = nullptr;

// HACK, DLudwig: record a reference to the global, Windows RT 'app'/view.
// SDL/WinRT will use this throughout its code.
//
// TODO, WinRT: consider replacing SDL_WinRTGlobalApp with something
// non-global, such as something created inside
// SDL_InitSubSystem(SDL_INIT_VIDEO), or something inside
// SDL_CreateWindow().
SDL_WinRTApp ^ SDL_WinRTGlobalApp = nullptr;

ref class SDLApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};

IFrameworkView^ SDLApplicationSource::CreateView()
{
    // TODO, WinRT: see if this function (CreateView) can ever get called
    // more than once.  For now, just prevent it from ever assigning
    // SDL_WinRTGlobalApp more than once.
    SDL_assert(!SDL_WinRTGlobalApp);
    SDL_WinRTApp ^ app = ref new SDL_WinRTApp();
    if (!SDL_WinRTGlobalApp)
    {
        SDL_WinRTGlobalApp = app;
    }
    return app;
}

__declspec(dllexport) int SDL_WinRT_RunApplication(SDL_WinRT_MainFunction mainFunction)
{
    SDL_WinRT_main = mainFunction;
    auto direct3DApplicationSource = ref new SDLApplicationSource();
    CoreApplication::Run(direct3DApplicationSource);
    return 0;
}

static void WINRT_SetDisplayOrientationsPreference(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    SDL_assert(SDL_strcmp(name, SDL_HINT_ORIENTATIONS) == 0);

    // Start with no orientation flags, then add each in as they're parsed
    // from newValue.
    unsigned int orientationFlags = 0;
    if (newValue) {
        std::istringstream tokenizer(newValue);
        while (!tokenizer.eof()) {
            std::string orientationName;
            std::getline(tokenizer, orientationName, ' ');
            if (orientationName == "LandscapeLeft") {
                orientationFlags |= (unsigned int) DisplayOrientations::LandscapeFlipped;
            } else if (orientationName == "LandscapeRight") {
                orientationFlags |= (unsigned int) DisplayOrientations::Landscape;
            } else if (orientationName == "Portrait") {
                orientationFlags |= (unsigned int) DisplayOrientations::Portrait;
            } else if (orientationName == "PortraitUpsideDown") {
                orientationFlags |= (unsigned int) DisplayOrientations::PortraitFlipped;
            }
        }
    }

    // If no valid orientation flags were specified, use a reasonable set of defaults:
    if (!orientationFlags) {
        // TODO, WinRT: consider seeing if an app's default orientation flags can be found out via some API call(s).
        orientationFlags = (unsigned int) ( \
            DisplayOrientations::Landscape |
            DisplayOrientations::LandscapeFlipped |
            DisplayOrientations::Portrait |
            DisplayOrientations::PortraitFlipped);
    }

    // Set the orientation/rotation preferences.  Please note that this does
    // not constitute a 100%-certain lock of a given set of possible
    // orientations.  According to Microsoft's documentation on Windows RT [1]
    // when a device is not capable of being rotated, Windows may ignore
    // the orientation preferences, and stick to what the device is capable of
    // displaying.
    //
    // [1] Documentation on the 'InitialRotationPreference' setting for a
    // Windows app's manifest file describes how some orientation/rotation
    // preferences may be ignored.  See
    // http://msdn.microsoft.com/en-us/library/windows/apps/hh700343.aspx
    // for details.  Microsoft's "Display orientation sample" also gives an
    // outline of how Windows treats device rotation
    // (http://code.msdn.microsoft.com/Display-Orientation-Sample-19a58e93).
    DisplayProperties::AutoRotationPreferences = (DisplayOrientations) orientationFlags;
}

SDL_WinRTApp::SDL_WinRTApp() :
    m_windowClosed(false),
    m_windowVisible(true),
    m_sdlWindowData(NULL),
    m_sdlVideoDevice(NULL),
    m_useRelativeMouseMode(false)
{
}

void SDL_WinRTApp::Initialize(CoreApplicationView^ applicationView)
{
    applicationView->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &SDL_WinRTApp::OnActivated);

    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs^>(this, &SDL_WinRTApp::OnSuspending);

    CoreApplication::Resuming +=
        ref new EventHandler<Platform::Object^>(this, &SDL_WinRTApp::OnResuming);

    DisplayProperties::OrientationChanged +=
        ref new DisplayPropertiesEventHandler(this, &SDL_WinRTApp::OnOrientationChanged);

    // Register the hint, SDL_HINT_ORIENTATIONS, with SDL.  This needs to be
    // done before the hint's callback is registered (as of Feb 22, 2013),
    // otherwise the hint callback won't get registered.
    //
    // WinRT, TODO: see if an app's default orientation can be found out via WinRT API(s), then set the initial value of SDL_HINT_ORIENTATIONS accordingly.
    //SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");   // DavidL: this is no longer needed (for SDL_AddHintCallback)
    SDL_AddHintCallback(SDL_HINT_ORIENTATIONS, WINRT_SetDisplayOrientationsPreference, NULL);
}

void SDL_WinRTApp::OnOrientationChanged(Object^ sender)
{
#if LOG_ORIENTATION_EVENTS==1
    CoreWindow^ window = CoreWindow::GetForCurrentThread();
    if (window) {
        SDL_Log("%s, current orientation=%d, native orientation=%d, auto rot. pref=%d, CoreWindow Size={%f,%f}\n",
            __FUNCTION__,
            (int)DisplayProperties::CurrentOrientation,
            (int)DisplayProperties::NativeOrientation,
            (int)DisplayProperties::AutoRotationPreferences,
            window->Bounds.Width,
            window->Bounds.Height);
    } else {
        SDL_Log("%s, current orientation=%d, native orientation=%d, auto rot. pref=%d\n",
            __FUNCTION__,
            (int)DisplayProperties::CurrentOrientation,
            (int)DisplayProperties::NativeOrientation,
            (int)DisplayProperties::AutoRotationPreferences);
    }
#endif
}

void SDL_WinRTApp::SetWindow(CoreWindow^ window)
{
#if LOG_WINDOW_EVENTS==1
    SDL_Log("%s, current orientation=%d, native orientation=%d, auto rot. pref=%d, window Size={%f,%f}\n",
        __FUNCTION__,
        (int)DisplayProperties::CurrentOrientation,
        (int)DisplayProperties::NativeOrientation,
        (int)DisplayProperties::AutoRotationPreferences,
        window->Bounds.Width,
        window->Bounds.Height);
#endif

    window->SizeChanged += 
        ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &SDL_WinRTApp::OnWindowSizeChanged);

    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &SDL_WinRTApp::OnVisibilityChanged);

    window->Closed += 
        ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &SDL_WinRTApp::OnWindowClosed);

#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
    window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
#endif

    window->PointerPressed +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &SDL_WinRTApp::OnPointerPressed);

    window->PointerReleased +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &SDL_WinRTApp::OnPointerReleased);

    window->PointerWheelChanged +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &SDL_WinRTApp::OnPointerWheelChanged);

    window->PointerMoved +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &SDL_WinRTApp::OnPointerMoved);

#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
    // Retrieves relative-only mouse movements:
    Windows::Devices::Input::MouseDevice::GetForCurrentView()->MouseMoved +=
        ref new TypedEventHandler<MouseDevice^, MouseEventArgs^>(this, &SDL_WinRTApp::OnMouseMoved);
#endif

    window->KeyDown +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &SDL_WinRTApp::OnKeyDown);

    window->KeyUp +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &SDL_WinRTApp::OnKeyUp);
}

void SDL_WinRTApp::Load(Platform::String^ entryPoint)
{
}

void SDL_WinRTApp::Run()
{
    SDL_SetMainReady();
    if (SDL_WinRT_main)
    {
        // TODO, WinRT: pass the C-style main() a reasonably realistic
        // representation of command line arguments.
        int argc = 0;
        char **argv = NULL;
        SDL_WinRT_main(argc, argv);
    }
}

void SDL_WinRTApp::PumpEvents()
{
    if (!m_windowClosed)
    {
        if (m_windowVisible)
        {
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        }
        else
        {
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }
}

void SDL_WinRTApp::Uninitialize()
{
}

void SDL_WinRTApp::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
#if LOG_WINDOW_EVENTS==1
    SDL_Log("%s, size={%f,%f}, current orientation=%d, native orientation=%d, auto rot. pref=%d, m_sdlWindowData?=%s\n",
        __FUNCTION__,
        args->Size.Width, args->Size.Height,
        (int)DisplayProperties::CurrentOrientation,
        (int)DisplayProperties::NativeOrientation,
        (int)DisplayProperties::AutoRotationPreferences,
        (m_sdlWindowData ? "yes" : "no"));
#endif

    if (m_sdlWindowData) {
        // Make the new window size be the one true fullscreen mode.
        // This change was done, in part, to allow the Direct3D 11.1 renderer
        // to receive window-resize events as a device rotates.
        // Before, rotating a device from landscape, to portrait, and then
        // back to landscape would cause the Direct3D 11.1 swap buffer to
        // not get resized appropriately.  SDL would, on the rotation from
        // landscape to portrait, re-resize the SDL window to it's initial
        // size (landscape).  On the subsequent rotation, SDL would drop the
        // window-resize event as it appeared the SDL window didn't change
        // size, and the Direct3D 11.1 renderer wouldn't resize its swap
        // chain.
        //
        // TODO, WinRT: consider dropping old display modes after the fullscreen window changes size (from rotations, etc.)
        m_sdlWindowData->sdlWindow->fullscreen_mode = SDL_WinRTGlobalApp->GetMainDisplayMode();
        SDL_AddDisplayMode(&m_sdlVideoDevice->displays[0], &m_sdlWindowData->sdlWindow->fullscreen_mode);

        // HACK, Feb 19, 2013: SDL_WINDOWEVENT_RESIZED events, when sent,
        // will attempt to fix the values of the main window's renderer's
        // viewport.  While this can be good, it does appear to be buggy,
        // and can cause a fullscreen viewport to become corrupted.  This
        // behavior was noticed on a Surface RT while rotating the device
        // from landscape to portrait.  Oddly enough, this did not occur
        // in the Windows Simulator.
        //
        // Backing up, then restoring, the main renderer's 'resized' flag
        // seems to fix fullscreen viewport problems when rotating a
        // Windows device.
        //
        // Commencing hack in 3... 2... 1...
        //
        // UPDATE, SDL 2.0.0 update: the 'resized' flag is now gone.  This
        // hack might not be necessary any more.
        //
        //SDL_Renderer * rendererForMainWindow = SDL_GetRenderer(m_sdlWindowData->sdlWindow);
        // For now, limit the hack to when the Direct3D 11.1 is getting used:
        //const bool usingD3D11Renderer = \
        //    (rendererForMainWindow != NULL) &&
        //    (SDL_strcmp(rendererForMainWindow->info.name, "direct3d 11.1") == 0);
        //SDL_bool wasD3D11RendererResized = SDL_FALSE;
        //if (usingD3D11Renderer) {
        //    wasD3D11RendererResized = rendererForMainWindow->resized;
        //}

        // Send the window-resize event to the rest of SDL, and to apps:
        const int windowWidth = (int) ceil(args->Size.Width);
        const int windowHeight = (int) ceil(args->Size.Height);
        SDL_SendWindowEvent(
            m_sdlWindowData->sdlWindow,
            SDL_WINDOWEVENT_RESIZED,
            windowWidth,
            windowHeight);

        //// Viewport hack, part two:
        //if (usingD3D11Renderer) {
        //    rendererForMainWindow->resized = wasD3D11RendererResized;
        //}
    }
}

void SDL_WinRTApp::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
#if LOG_WINDOW_EVENTS==1
    SDL_Log("%s, visible?=%s, m_sdlWindowData?=%s\n",
        __FUNCTION__,
        (args->Visible ? "yes" : "no"),
        (m_sdlWindowData ? "yes" : "no"));
#endif

    m_windowVisible = args->Visible;
    if (m_sdlWindowData) {
        SDL_bool wasSDLWindowSurfaceValid = m_sdlWindowData->sdlWindow->surface_valid;

        if (args->Visible) {
            SDL_SendWindowEvent(m_sdlWindowData->sdlWindow, SDL_WINDOWEVENT_SHOWN, 0, 0);
        } else {
            SDL_SendWindowEvent(m_sdlWindowData->sdlWindow, SDL_WINDOWEVENT_HIDDEN, 0, 0);
        }

        // HACK: Prevent SDL's window-hide handling code, which currently
        // triggers a fake window resize (possibly erronously), from
        // marking the SDL window's surface as invalid.
        //
        // A better solution to this probably involves figuring out if the
        // fake window resize can be prevented.
        m_sdlWindowData->sdlWindow->surface_valid = wasSDLWindowSurfaceValid;
    }
}

void SDL_WinRTApp::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
#if LOG_WINDOW_EVENTS==1
    SDL_Log("%s\n", __FUNCTION__);
#endif
    m_windowClosed = true;
}

static Uint8
WINRT_GetSDLButtonForPointerPoint(PointerPoint ^ pt)
{
    switch (pt->Properties->PointerUpdateKind)
    {
        case PointerUpdateKind::LeftButtonPressed:
        case PointerUpdateKind::LeftButtonReleased:
            return SDL_BUTTON_LEFT;

        case PointerUpdateKind::RightButtonPressed:
        case PointerUpdateKind::RightButtonReleased:
            return SDL_BUTTON_RIGHT;

        case PointerUpdateKind::MiddleButtonPressed:
        case PointerUpdateKind::MiddleButtonReleased:
            return SDL_BUTTON_MIDDLE;

        case PointerUpdateKind::XButton1Pressed:
        case PointerUpdateKind::XButton1Released:
            return SDL_BUTTON_X1;

        case PointerUpdateKind::XButton2Pressed:
        case PointerUpdateKind::XButton2Released:
            return SDL_BUTTON_X2;

        default:
            break;
    }

    return 0;
}

static const char *
WINRT_ConvertPointerUpdateKindToString(PointerUpdateKind kind)
{
    switch (kind)
    {
        case PointerUpdateKind::Other:
            return "Other";
        case PointerUpdateKind::LeftButtonPressed:
            return "LeftButtonPressed";
        case PointerUpdateKind::LeftButtonReleased:
            return "LeftButtonReleased";
        case PointerUpdateKind::RightButtonPressed:
            return "RightButtonPressed";
        case PointerUpdateKind::RightButtonReleased:
            return "RightButtonReleased";
        case PointerUpdateKind::MiddleButtonPressed:
            return "MiddleButtonPressed";
        case PointerUpdateKind::MiddleButtonReleased:
            return "MiddleButtonReleased";
        case PointerUpdateKind::XButton1Pressed:
            return "XButton1Pressed";
        case PointerUpdateKind::XButton1Released:
            return "XButton1Released";
        case PointerUpdateKind::XButton2Pressed:
            return "XButton2Pressed";
        case PointerUpdateKind::XButton2Released:
            return "XButton2Released";
    }

    return "";
}

static void
WINRT_LogPointerEvent(const string & header, PointerEventArgs ^ args, Point transformedPoint)
{
    PointerPoint ^ pt = args->CurrentPoint;
    SDL_Log("%s: Position={%f,%f}, Transformed Pos={%f, %f}, MouseWheelDelta=%d, FrameId=%d, PointerId=%d, PointerUpdateKind=%s\n",
        header.c_str(),
        pt->Position.X, pt->Position.Y,
        transformedPoint.X, transformedPoint.Y,
        pt->Properties->MouseWheelDelta,
        pt->FrameId,
        pt->PointerId,
        WINRT_ConvertPointerUpdateKindToString(args->CurrentPoint->Properties->PointerUpdateKind));
}

void SDL_WinRTApp::OnPointerPressed(CoreWindow^ sender, PointerEventArgs^ args)
{
#if LOG_POINTER_EVENTS
    WINRT_LogPointerEvent("mouse down", args, TransformCursor(args->CurrentPoint->Position));
#endif

    if (m_sdlWindowData) {
        Uint8 button = WINRT_GetSDLButtonForPointerPoint(args->CurrentPoint);
        if (button) {
            SDL_SendMouseButton(m_sdlWindowData->sdlWindow, 0, SDL_PRESSED, button);
        }
    }
}

void SDL_WinRTApp::OnPointerReleased(CoreWindow^ sender, PointerEventArgs^ args)
{
#if LOG_POINTER_EVENTS
    WINRT_LogPointerEvent("mouse up", args, TransformCursor(args->CurrentPoint->Position));
#endif

    if (m_sdlWindowData) {
        Uint8 button = WINRT_GetSDLButtonForPointerPoint(args->CurrentPoint);
        if (button) {
            SDL_SendMouseButton(m_sdlWindowData->sdlWindow, 0, SDL_RELEASED, button);
        }
    }
}

void SDL_WinRTApp::OnPointerWheelChanged(CoreWindow^ sender, PointerEventArgs^ args)
{
#if LOG_POINTER_EVENTS
    WINRT_LogPointerEvent("wheel changed", args, TransformCursor(args->CurrentPoint->Position));
#endif

    if (m_sdlWindowData) {
        // FIXME: This may need to accumulate deltas up to WHEEL_DELTA
        short motion = args->CurrentPoint->Properties->MouseWheelDelta / WHEEL_DELTA;
        SDL_SendMouseWheel(m_sdlWindowData->sdlWindow, 0, 0, motion);
    }
}

static inline int _lround(float arg) {
    if (arg >= 0.0f) {
        return (int)floor(arg + 0.5f);
    } else {
        return (int)ceil(arg - 0.5f);
    }
}

void SDL_WinRTApp::OnMouseMoved(MouseDevice^ mouseDevice, MouseEventArgs^ args)
{
    if (m_sdlWindowData && m_useRelativeMouseMode) {
        // DLudwig, 2012-12-28: On some systems, namely Visual Studio's Windows
        // Simulator, as well as Windows 8 in a Parallels 8 VM, MouseEventArgs'
        // MouseDelta field often reports very large values.  More information
        // on this can be found at the following pages on MSDN:
        //  - http://social.msdn.microsoft.com/Forums/en-US/winappswithnativecode/thread/a3c789fa-f1c5-49c4-9c0a-7db88d0f90f8
        //  - https://connect.microsoft.com/VisualStudio/Feedback/details/756515
        //
        // The values do not appear to be as large when running on some systems,
        // most notably a Surface RT.  Furthermore, the values returned by
        // CoreWindow's PointerMoved event, and sent to this class' OnPointerMoved
        // method, do not ever appear to be large, even when MouseEventArgs'
        // MouseDelta is reporting to the contrary.
        //
        // On systems with the large-values behavior, it appears that the values
        // get reported as if the screen's size is 65536 units in both the X and Y
        // dimensions.  This can be viewed by using Windows' now-private, "Raw Input"
        // APIs.  (GetRawInputData, RegisterRawInputDevices, WM_INPUT, etc.)
        //
        // MSDN's documentation on MouseEventArgs' MouseDelta field (at
        // http://msdn.microsoft.com/en-us/library/windows/apps/windows.devices.input.mouseeventargs.mousedelta ),
        // does not seem to indicate (to me) that its values should be so large.  It
        // says that its values should be a "change in screen location".  I could
        // be misinterpreting this, however a post on MSDN from a Microsoft engineer (see: 
        // http://social.msdn.microsoft.com/Forums/en-US/winappswithnativecode/thread/09a9868e-95bb-4858-ba1a-cb4d2c298d62 ),
        // indicates that these values are in DIPs, which is the same unit used
        // by CoreWindow's PointerMoved events (via the Position field in its CurrentPoint
        // property.  See http://msdn.microsoft.com/en-us/library/windows/apps/windows.ui.input.pointerpoint.position.aspx
        // for details.)
        //
        // To note, PointerMoved events are sent a 'RawPosition' value (via the
        // CurrentPoint property in MouseEventArgs), however these do not seem
        // to exhibit the same large-value behavior.
        //
        // The values passed via PointerMoved events can't always be used for relative
        // mouse motion, unfortunately.  Its values are bound to the cursor's position,
        // which stops when it hits one of the screen's edges.  This can be a problem in
        // first person shooters, whereby it is normal for mouse motion to travel far
        // along any one axis for a period of time.  MouseMoved events do not have the
        // screen-bounding limitation, and can be used regardless of where the system's
        // cursor is.
        //
        // One possible workaround would be to programmatically set the cursor's
        // position to the screen's center (when SDL's relative mouse mode is enabled),
        // however Windows RT does not yet seem to have the ability to set the cursor's
        // position via a public API.  Win32 did this via an API call, SetCursorPos,
        // however WinRT makes this function be private.  Apps that use it won't get
        // approved for distribution in the Windows Store.  I've yet to be able to find
        // a suitable, store-friendly counterpart for WinRT.
        //
        // There may be some room for a workaround whereby OnPointerMoved's values
        // are compared to the values from OnMouseMoved in order to detect
        // when this bug is active.  A suitable transformation could then be made to
        // OnMouseMoved's values.  For now, however, the system-reported values are sent
        // to SDL with minimal transformation: from native screen coordinates (in DIPs)
        // to SDL window coordinates.
        //
        const Point mouseDeltaInDIPs((float)args->MouseDelta.X, (float)args->MouseDelta.Y);
        const Point mouseDeltaInSDLWindowCoords = TransformCursor(mouseDeltaInDIPs);
        SDL_SendMouseMotion(
            m_sdlWindowData->sdlWindow,
            0,
            1,
            _lround(mouseDeltaInSDLWindowCoords.X),
            _lround(mouseDeltaInSDLWindowCoords.Y));
    }
}

// Applies necessary geometric transformations to raw cursor positions:
Point SDL_WinRTApp::TransformCursor(Point rawPosition)
{
    if ( ! m_sdlWindowData || ! m_sdlWindowData->sdlWindow ) {
        return rawPosition;
    }
    CoreWindow ^ nativeWindow = CoreWindow::GetForCurrentThread();
    Point outputPosition;
    outputPosition.X = rawPosition.X * (((float32)m_sdlWindowData->sdlWindow->w) / nativeWindow->Bounds.Width);
    outputPosition.Y = rawPosition.Y * (((float32)m_sdlWindowData->sdlWindow->h) / nativeWindow->Bounds.Height);
    return outputPosition;
}

void SDL_WinRTApp::OnPointerMoved(CoreWindow^ sender, PointerEventArgs^ args)
{
#if LOG_POINTER_EVENTS
    WINRT_LogPointerEvent("pointer moved", args, TransformCursor(args->CurrentPoint->Position));
#endif

    if (m_sdlWindowData && ! m_useRelativeMouseMode)
    {
        Point transformedPoint = TransformCursor(args->CurrentPoint->Position);
        SDL_SendMouseMotion(m_sdlWindowData->sdlWindow, 0, 0, (int)transformedPoint.X, (int)transformedPoint.Y);
    }
}

static SDL_Scancode WinRT_Official_Keycodes[] = {
    SDL_SCANCODE_UNKNOWN, // VirtualKey.None -- 0
    SDL_SCANCODE_UNKNOWN, // VirtualKey.LeftButton -- 1
    SDL_SCANCODE_UNKNOWN, // VirtualKey.RightButton -- 2
    SDL_SCANCODE_CANCEL, // VirtualKey.Cancel -- 3
    SDL_SCANCODE_UNKNOWN, // VirtualKey.MiddleButton -- 4
    SDL_SCANCODE_UNKNOWN, // VirtualKey.XButton1 -- 5
    SDL_SCANCODE_UNKNOWN, // VirtualKey.XButton2 -- 6
    SDL_SCANCODE_UNKNOWN, // -- 7
    SDL_SCANCODE_BACKSPACE, // VirtualKey.Back -- 8
    SDL_SCANCODE_TAB, // VirtualKey.Tab -- 9
    SDL_SCANCODE_UNKNOWN, // -- 10
    SDL_SCANCODE_UNKNOWN, // -- 11
    SDL_SCANCODE_CLEAR, // VirtualKey.Clear -- 12
    SDL_SCANCODE_RETURN, // VirtualKey.Enter -- 13
    SDL_SCANCODE_UNKNOWN, // -- 14
    SDL_SCANCODE_UNKNOWN, // -- 15
    SDL_SCANCODE_LSHIFT, // VirtualKey.Shift -- 16
    SDL_SCANCODE_LCTRL, // VirtualKey.Control -- 17
    SDL_SCANCODE_MENU, // VirtualKey.Menu -- 18
    SDL_SCANCODE_PAUSE, // VirtualKey.Pause -- 19
    SDL_SCANCODE_CAPSLOCK, // VirtualKey.CapitalLock -- 20
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Kana or VirtualKey.Hangul -- 21
    SDL_SCANCODE_UNKNOWN, // -- 22
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Junja -- 23
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Final -- 24
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Hanja or VirtualKey.Kanji -- 25
    SDL_SCANCODE_UNKNOWN, // -- 26
    SDL_SCANCODE_ESCAPE, // VirtualKey.Escape -- 27
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Convert -- 28
    SDL_SCANCODE_UNKNOWN, // VirtualKey.NonConvert -- 29
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Accept -- 30
    SDL_SCANCODE_UNKNOWN, // VirtualKey.ModeChange -- 31  (maybe SDL_SCANCODE_MODE ?)
    SDL_SCANCODE_SPACE, // VirtualKey.Space -- 32
    SDL_SCANCODE_PAGEUP, // VirtualKey.PageUp -- 33
    SDL_SCANCODE_PAGEDOWN, // VirtualKey.PageDown -- 34
    SDL_SCANCODE_END, // VirtualKey.End -- 35
    SDL_SCANCODE_HOME, // VirtualKey.Home -- 36
    SDL_SCANCODE_LEFT, // VirtualKey.Left -- 37
    SDL_SCANCODE_UP, // VirtualKey.Up -- 38
    SDL_SCANCODE_RIGHT, // VirtualKey.Right -- 39
    SDL_SCANCODE_DOWN, // VirtualKey.Down -- 40
    SDL_SCANCODE_SELECT, // VirtualKey.Select -- 41
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Print -- 42  (maybe SDL_SCANCODE_PRINTSCREEN ?)
    SDL_SCANCODE_EXECUTE, // VirtualKey.Execute -- 43
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Snapshot -- 44
    SDL_SCANCODE_INSERT, // VirtualKey.Insert -- 45
    SDL_SCANCODE_DELETE, // VirtualKey.Delete -- 46
    SDL_SCANCODE_HELP, // VirtualKey.Help -- 47
    SDL_SCANCODE_0, // VirtualKey.Number0 -- 48
    SDL_SCANCODE_1, // VirtualKey.Number1 -- 49
    SDL_SCANCODE_2, // VirtualKey.Number2 -- 50
    SDL_SCANCODE_3, // VirtualKey.Number3 -- 51
    SDL_SCANCODE_4, // VirtualKey.Number4 -- 52
    SDL_SCANCODE_5, // VirtualKey.Number5 -- 53
    SDL_SCANCODE_6, // VirtualKey.Number6 -- 54
    SDL_SCANCODE_7, // VirtualKey.Number7 -- 55
    SDL_SCANCODE_8, // VirtualKey.Number8 -- 56
    SDL_SCANCODE_9, // VirtualKey.Number9 -- 57
    SDL_SCANCODE_UNKNOWN, // -- 58
    SDL_SCANCODE_UNKNOWN, // -- 59
    SDL_SCANCODE_UNKNOWN, // -- 60
    SDL_SCANCODE_UNKNOWN, // -- 61
    SDL_SCANCODE_UNKNOWN, // -- 62
    SDL_SCANCODE_UNKNOWN, // -- 63
    SDL_SCANCODE_UNKNOWN, // -- 64
    SDL_SCANCODE_A, // VirtualKey.A -- 65
    SDL_SCANCODE_B, // VirtualKey.B -- 66
    SDL_SCANCODE_C, // VirtualKey.C -- 67
    SDL_SCANCODE_D, // VirtualKey.D -- 68
    SDL_SCANCODE_E, // VirtualKey.E -- 69
    SDL_SCANCODE_F, // VirtualKey.F -- 70
    SDL_SCANCODE_G, // VirtualKey.G -- 71
    SDL_SCANCODE_H, // VirtualKey.H -- 72
    SDL_SCANCODE_I, // VirtualKey.I -- 73
    SDL_SCANCODE_J, // VirtualKey.J -- 74
    SDL_SCANCODE_K, // VirtualKey.K -- 75
    SDL_SCANCODE_L, // VirtualKey.L -- 76
    SDL_SCANCODE_M, // VirtualKey.M -- 77
    SDL_SCANCODE_N, // VirtualKey.N -- 78
    SDL_SCANCODE_O, // VirtualKey.O -- 79
    SDL_SCANCODE_P, // VirtualKey.P -- 80
    SDL_SCANCODE_Q, // VirtualKey.Q -- 81
    SDL_SCANCODE_R, // VirtualKey.R -- 82
    SDL_SCANCODE_S, // VirtualKey.S -- 83
    SDL_SCANCODE_T, // VirtualKey.T -- 84
    SDL_SCANCODE_U, // VirtualKey.U -- 85
    SDL_SCANCODE_V, // VirtualKey.V -- 86
    SDL_SCANCODE_W, // VirtualKey.W -- 87
    SDL_SCANCODE_X, // VirtualKey.X -- 88
    SDL_SCANCODE_Y, // VirtualKey.Y -- 89
    SDL_SCANCODE_Z, // VirtualKey.Z -- 90
    SDL_SCANCODE_UNKNOWN, // VirtualKey.LeftWindows -- 91  (maybe SDL_SCANCODE_APPLICATION or SDL_SCANCODE_LGUI ?)
    SDL_SCANCODE_UNKNOWN, // VirtualKey.RightWindows -- 92  (maybe SDL_SCANCODE_APPLICATION or SDL_SCANCODE_RGUI ?)
    SDL_SCANCODE_APPLICATION, // VirtualKey.Application -- 93
    SDL_SCANCODE_UNKNOWN, // -- 94
    SDL_SCANCODE_SLEEP, // VirtualKey.Sleep -- 95
    SDL_SCANCODE_KP_0, // VirtualKey.NumberPad0 -- 96
    SDL_SCANCODE_KP_1, // VirtualKey.NumberPad1 -- 97
    SDL_SCANCODE_KP_2, // VirtualKey.NumberPad2 -- 98
    SDL_SCANCODE_KP_3, // VirtualKey.NumberPad3 -- 99
    SDL_SCANCODE_KP_4, // VirtualKey.NumberPad4 -- 100
    SDL_SCANCODE_KP_5, // VirtualKey.NumberPad5 -- 101
    SDL_SCANCODE_KP_6, // VirtualKey.NumberPad6 -- 102
    SDL_SCANCODE_KP_7, // VirtualKey.NumberPad7 -- 103
    SDL_SCANCODE_KP_8, // VirtualKey.NumberPad8 -- 104
    SDL_SCANCODE_KP_9, // VirtualKey.NumberPad9 -- 105
    SDL_SCANCODE_KP_MULTIPLY, // VirtualKey.Multiply -- 106
    SDL_SCANCODE_KP_PLUS, // VirtualKey.Add -- 107
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Separator -- 108
    SDL_SCANCODE_KP_MINUS, // VirtualKey.Subtract -- 109
    SDL_SCANCODE_UNKNOWN, // VirtualKey.Decimal -- 110  (maybe SDL_SCANCODE_DECIMALSEPARATOR, SDL_SCANCODE_KP_DECIMAL, or SDL_SCANCODE_KP_PERIOD ?)
    SDL_SCANCODE_KP_DIVIDE, // VirtualKey.Divide -- 111
    SDL_SCANCODE_F1, // VirtualKey.F1 -- 112
    SDL_SCANCODE_F2, // VirtualKey.F2 -- 113
    SDL_SCANCODE_F3, // VirtualKey.F3 -- 114
    SDL_SCANCODE_F4, // VirtualKey.F4 -- 115
    SDL_SCANCODE_F5, // VirtualKey.F5 -- 116
    SDL_SCANCODE_F6, // VirtualKey.F6 -- 117
    SDL_SCANCODE_F7, // VirtualKey.F7 -- 118
    SDL_SCANCODE_F8, // VirtualKey.F8 -- 119
    SDL_SCANCODE_F9, // VirtualKey.F9 -- 120
    SDL_SCANCODE_F10, // VirtualKey.F10 -- 121
    SDL_SCANCODE_F11, // VirtualKey.F11 -- 122
    SDL_SCANCODE_F12, // VirtualKey.F12 -- 123
    SDL_SCANCODE_F13, // VirtualKey.F13 -- 124
    SDL_SCANCODE_F14, // VirtualKey.F14 -- 125
    SDL_SCANCODE_F15, // VirtualKey.F15 -- 126
    SDL_SCANCODE_F16, // VirtualKey.F16 -- 127
    SDL_SCANCODE_F17, // VirtualKey.F17 -- 128
    SDL_SCANCODE_F18, // VirtualKey.F18 -- 129
    SDL_SCANCODE_F19, // VirtualKey.F19 -- 130
    SDL_SCANCODE_F20, // VirtualKey.F20 -- 131
    SDL_SCANCODE_F21, // VirtualKey.F21 -- 132
    SDL_SCANCODE_F22, // VirtualKey.F22 -- 133
    SDL_SCANCODE_F23, // VirtualKey.F23 -- 134
    SDL_SCANCODE_F24, // VirtualKey.F24 -- 135
    SDL_SCANCODE_UNKNOWN, // -- 136
    SDL_SCANCODE_UNKNOWN, // -- 137
    SDL_SCANCODE_UNKNOWN, // -- 138
    SDL_SCANCODE_UNKNOWN, // -- 139
    SDL_SCANCODE_UNKNOWN, // -- 140
    SDL_SCANCODE_UNKNOWN, // -- 141
    SDL_SCANCODE_UNKNOWN, // -- 142
    SDL_SCANCODE_UNKNOWN, // -- 143
    SDL_SCANCODE_NUMLOCKCLEAR, // VirtualKey.NumberKeyLock -- 144
    SDL_SCANCODE_SCROLLLOCK, // VirtualKey.Scroll -- 145
    SDL_SCANCODE_UNKNOWN, // -- 146
    SDL_SCANCODE_UNKNOWN, // -- 147
    SDL_SCANCODE_UNKNOWN, // -- 148
    SDL_SCANCODE_UNKNOWN, // -- 149
    SDL_SCANCODE_UNKNOWN, // -- 150
    SDL_SCANCODE_UNKNOWN, // -- 151
    SDL_SCANCODE_UNKNOWN, // -- 152
    SDL_SCANCODE_UNKNOWN, // -- 153
    SDL_SCANCODE_UNKNOWN, // -- 154
    SDL_SCANCODE_UNKNOWN, // -- 155
    SDL_SCANCODE_UNKNOWN, // -- 156
    SDL_SCANCODE_UNKNOWN, // -- 157
    SDL_SCANCODE_UNKNOWN, // -- 158
    SDL_SCANCODE_UNKNOWN, // -- 159
    SDL_SCANCODE_LSHIFT, // VirtualKey.LeftShift -- 160
    SDL_SCANCODE_RSHIFT, // VirtualKey.RightShift -- 161
    SDL_SCANCODE_LCTRL, // VirtualKey.LeftControl -- 162
    SDL_SCANCODE_RCTRL, // VirtualKey.RightControl -- 163
    SDL_SCANCODE_MENU, // VirtualKey.LeftMenu -- 164
    SDL_SCANCODE_MENU, // VirtualKey.RightMenu -- 165
};

static std::unordered_map<int, SDL_Scancode> WinRT_Unofficial_Keycodes;

static SDL_Scancode
TranslateKeycode(int keycode)
{
    if (WinRT_Unofficial_Keycodes.empty()) {
        /* Set up a table of undocumented (by Microsoft), WinRT-specific,
           key codes: */
        // TODO, WinRT: move content declarations of WinRT_Unofficial_Keycodes into a C++11 initializer list, when possible
        WinRT_Unofficial_Keycodes[220] = SDL_SCANCODE_GRAVE;
        WinRT_Unofficial_Keycodes[222] = SDL_SCANCODE_BACKSLASH;
    }

    /* Try to get a documented, WinRT, 'VirtualKey' first (as documented at
       http://msdn.microsoft.com/en-us/library/windows/apps/windows.system.virtualkey.aspx ).
       If that fails, fall back to a Win32 virtual key.
    */
    // TODO, WinRT: try filling out the WinRT keycode table as much as possible, using the Win32 table for interpretation hints
    //SDL_Log("WinRT TranslateKeycode, keycode=%d\n", (int)keycode);
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    if (keycode < SDL_arraysize(WinRT_Official_Keycodes)) {
        scancode = WinRT_Official_Keycodes[keycode];
    }
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        if (WinRT_Unofficial_Keycodes.find(keycode) != WinRT_Unofficial_Keycodes.end()) {
            scancode = WinRT_Unofficial_Keycodes[keycode];
        }
    }
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        if (keycode < SDL_arraysize(windows_scancode_table)) {
            scancode = windows_scancode_table[keycode];
        }
    }
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        SDL_Log("WinRT TranslateKeycode, unknown keycode=%d\n", (int)keycode);
    }
    return scancode;
}

void SDL_WinRTApp::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{
    SDL_Scancode sdlScancode = TranslateKeycode((int)args->VirtualKey);
#if 0
    SDL_Keycode keycode = SDL_GetKeyFromScancode(sdlScancode);
    SDL_Log("key down, handled=%s, ext?=%s, released?=%s, menu key down?=%s, repeat count=%d, native scan code=%d, was down?=%s, vkey=%d, sdl scan code=%d (%s), sdl key code=%d (%s)\n",
        (args->Handled ? "1" : "0"),
        (args->KeyStatus.IsExtendedKey ? "1" : "0"),
        (args->KeyStatus.IsKeyReleased ? "1" : "0"),
        (args->KeyStatus.IsMenuKeyDown ? "1" : "0"),
        args->KeyStatus.RepeatCount,
        args->KeyStatus.ScanCode,
        (args->KeyStatus.WasKeyDown ? "1" : "0"),
        args->VirtualKey,
        sdlScancode,
        SDL_GetScancodeName(sdlScancode),
        keycode,
        SDL_GetKeyName(keycode));
    //args->Handled = true;
    //VirtualKey vkey = args->VirtualKey;
#endif
    SDL_SendKeyboardKey(SDL_PRESSED, sdlScancode);
}

void SDL_WinRTApp::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{
    SDL_Scancode sdlScancode = TranslateKeycode((int)args->VirtualKey);
#if 0
    SDL_Keycode keycode = SDL_GetKeyFromScancode(sdlScancode);
    SDL_Log("key up, handled=%s, ext?=%s, released?=%s, menu key down?=%s, repeat count=%d, native scan code=%d, was down?=%s, vkey=%d, sdl scan code=%d (%s), sdl key code=%d (%s)\n",
        (args->Handled ? "1" : "0"),
        (args->KeyStatus.IsExtendedKey ? "1" : "0"),
        (args->KeyStatus.IsKeyReleased ? "1" : "0"),
        (args->KeyStatus.IsMenuKeyDown ? "1" : "0"),
        args->KeyStatus.RepeatCount,
        args->KeyStatus.ScanCode,
        (args->KeyStatus.WasKeyDown ? "1" : "0"),
        args->VirtualKey,
        sdlScancode,
        SDL_GetScancodeName(sdlScancode),
        keycode,
        SDL_GetKeyName(keycode));
    //args->Handled = true;
#endif
    SDL_SendKeyboardKey(SDL_RELEASED, sdlScancode);
}

void SDL_WinRTApp::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
    CoreWindow::GetForCurrentThread()->Activate();
}

static int SDLCALL RemoveAppSuspendAndResumeEvents(void * userdata, SDL_Event * event)
{
    if (event->type == SDL_WINDOWEVENT)
    {
        switch (event->window.event)
        {
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
                // Return 0 to indicate that the event should be removed from the
                // event queue:
                return 0;
            default:
                break;
        }
    }

    // Return 1 to indicate that the event should stay in the event queue:
    return 1;
}

void SDL_WinRTApp::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
    // Save app state asynchronously after requesting a deferral. Holding a deferral
    // indicates that the application is busy performing suspending operations. Be
    // aware that a deferral may not be held indefinitely. After about five seconds,
    // the app will be forced to exit.
    SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();
    create_task([this, deferral]()
    {
        // Send a window-minimized event immediately to observers.
        // CoreDispatcher::ProcessEvents, which is the backbone on which
        // SDL_WinRTApp::PumpEvents is built, will not return to its caller
        // once it sends out a suspend event.  Any events posted to SDL's
        // event queue won't get received until the WinRT app is resumed.
        // SDL_AddEventWatch() may be used to receive app-suspend events on
        // WinRT.
        //
        // In order to prevent app-suspend events from being received twice:
        // first via a callback passed to SDL_AddEventWatch, and second via
        // SDL's event queue, the event will be sent to SDL, then immediately
        // removed from the queue.
        if (m_sdlWindowData)
        {
            SDL_SendWindowEvent(m_sdlWindowData->sdlWindow, SDL_WINDOWEVENT_MINIMIZED, 0, 0);   // TODO: see if SDL_WINDOWEVENT_SIZE_CHANGED should be getting triggered here (it is, currently)
            SDL_FilterEvents(RemoveAppSuspendAndResumeEvents, 0);
        }
        deferral->Complete();
    });
}

void SDL_WinRTApp::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
    // Restore any data or state that was unloaded on suspend. By default, data
    // and state are persisted when resuming from suspend. Note that this event
    // does not occur if the app was previously terminated.
    if (m_sdlWindowData)
    {
        SDL_SendWindowEvent(m_sdlWindowData->sdlWindow, SDL_WINDOWEVENT_RESTORED, 0, 0);    // TODO: see if SDL_WINDOWEVENT_SIZE_CHANGED should be getting triggered here (it is, currently)

        // Remove the app-resume event from the queue, as is done with the
        // app-suspend event.
        //
        // TODO, WinRT: consider posting this event to the queue even though
        // its counterpart, the app-suspend event, effectively has to be
        // processed immediately.
        SDL_FilterEvents(RemoveAppSuspendAndResumeEvents, 0);
    }
}

SDL_DisplayMode SDL_WinRTApp::GetMainDisplayMode()
{
    // Create an empty, zeroed-out display mode:
    SDL_DisplayMode mode;
    SDL_zero(mode);

    // Fill in most fields:
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.refresh_rate = 0;  // TODO, WinRT: see if refresh rate data is available, or relevant (for WinRT apps)
    mode.driverdata = NULL;

    // Calculate the display size given the window size, taking into account
    // the current display's DPI:
    const float currentDPI = Windows::Graphics::Display::DisplayProperties::LogicalDpi; 
    const float dipsPerInch = 96.0f;
    mode.w = (int) ((CoreWindow::GetForCurrentThread()->Bounds.Width * currentDPI) / dipsPerInch);
    mode.h = (int) ((CoreWindow::GetForCurrentThread()->Bounds.Height * currentDPI) / dipsPerInch);

    return mode;
}

const SDL_WindowData * SDL_WinRTApp::GetSDLWindowData() const
{
    return m_sdlWindowData;
}

bool SDL_WinRTApp::HasSDLWindowData() const
{
    return (m_sdlWindowData != NULL);
}

void SDL_WinRTApp::SetRelativeMouseMode(bool enable)
{
    m_useRelativeMouseMode = enable;
}

void SDL_WinRTApp::SetSDLWindowData(const SDL_WindowData * windowData)
{
    m_sdlWindowData = windowData;
}

void SDL_WinRTApp::SetSDLVideoDevice(const SDL_VideoDevice * videoDevice)
{
    m_sdlVideoDevice = videoDevice;
}
