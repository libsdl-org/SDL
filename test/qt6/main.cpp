/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QErrorMessage>

#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("sdl3-qt-demo");
    QCoreApplication::setApplicationVersion(SDL_GetRevision());

    QCommandLineParser parser;
    parser.setApplicationDescription("SDL3 example demonstrating Qt6 integration");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(app);

    QString app_platform_name = app.platformName();
    bool updateCanvasSize = false;

    if (app_platform_name == "xcb") {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "x11", SDL_HINT_OVERRIDE);
#if defined(QT_FEATURE_wayland) && QT_CONFIG(wayland) == 1
    } else if (app_platform_name == "wayland" || app_platform_name == "wayland-egl") {
        /* Get the wl_display object from Qt */
        QNativeInterface::QWaylandApplication *qtWlApp = app.nativeInterface<QNativeInterface::QWaylandApplication>();
        SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, qtWlApp->display());
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "wayland", SDL_HINT_OVERRIDE);

        // Wayland requires explicitly setting and updating the canvas size.
        updateCanvasSize = true;
#endif
    } else if (app_platform_name == "windows") {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "windows", SDL_HINT_OVERRIDE);
    } else if (app_platform_name == "qnx") {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "qnx", SDL_HINT_OVERRIDE);
    } else if (app_platform_name == "cocoa") {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "cocoa", SDL_HINT_OVERRIDE);
    } else if (app_platform_name == "ios") {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "ios", SDL_HINT_OVERRIDE);
    } else {
        QErrorMessage error_message;
        error_message.showMessage(QString("Don't know SDL platform equivalent for qt platform: ") + app.platformName());
        app.exec();
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    MainWindow window(updateCanvasSize);
    window.show();

    const int ret = app.exec();
    SDL_Quit();
    return ret;
}
