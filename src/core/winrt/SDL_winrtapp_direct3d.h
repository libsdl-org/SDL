#pragma once

#include <Windows.h>

extern int SDL_WinRTInitNonXAMLApp(int (*mainFunction)(int, char **));

ref class SDL_WinRTApp sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
    SDL_WinRTApp();
    
    // IFrameworkView Methods.
    virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
    virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
    virtual void Load(Platform::String^ entryPoint);
    virtual void Run();
    virtual void Uninitialize();

internal:
    // SDL-specific methods
    void PumpEvents();

protected:
    // Event Handlers.

#if WINAPI_FAMILY == WINAPI_FAMILY_APP  // for Windows 8/8.1/RT apps... (and not Phone apps)
    void OnSettingsPaneCommandsRequested(
        Windows::UI::ApplicationSettings::SettingsPane ^p,
        Windows::UI::ApplicationSettings::SettingsPaneCommandsRequestedEventArgs ^args);
#endif // if WINAPI_FAMILY == WINAPI_FAMILY_APP

    void OnOrientationChanged(Platform::Object^ sender);
    void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
    void OnLogicalDpiChanged(Platform::Object^ sender);
    void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
    void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
    void OnResuming(Platform::Object^ sender, Platform::Object^ args);
    void OnExiting(Platform::Object^ sender, Platform::Object^ args);
    void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);
    void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
    void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnMouseMoved(Windows::Devices::Input::MouseDevice^ mouseDevice, Windows::Devices::Input::MouseEventArgs^ args);
    void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
    void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    void OnBackButtonPressed(Platform::Object^ sender, Windows::Phone::UI::Input::BackPressedEventArgs^ args);
#endif

private:
    bool m_windowClosed;
    bool m_windowVisible;
};

extern SDL_WinRTApp ^ SDL_WinRTGlobalApp;
