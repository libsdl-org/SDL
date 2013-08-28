#pragma once

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
    void OnOrientationChanged(Platform::Object^ sender);
    void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
    void OnLogicalDpiChanged(Platform::Object^ sender);
    void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
    void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
    void OnResuming(Platform::Object^ sender, Platform::Object^ args);
    void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);
    void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
    void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnMouseMoved(Windows::Devices::Input::MouseDevice^ mouseDevice, Windows::Devices::Input::MouseEventArgs^ args);
    void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
    void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);

private:
    bool m_windowClosed;
    bool m_windowVisible;
};
