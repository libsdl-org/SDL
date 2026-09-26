/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "QSDLCanvas.h"

#include <QMessageBox>

QSDLCanvas::QSDLCanvas(QWidget *parent, bool updateCanvasSize) : QWidget(parent), m_drawTimer(this)
{
    // Allow SDL to draw to the window
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    m_updateCanvasSize = updateCanvasSize;
}

QSDLCanvas::~QSDLCanvas()
{
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
}

void QSDLCanvas::onUpdate()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        onEvent(&event);
    }

    onDraw();
}

void QSDLCanvas::changeRenderer(const QString &name)
{
    m_rendererName = name;
    m_rendererNameChanged = true;

    recreateRenderer();
}

void QSDLCanvas::drawRendererName(bool draw)
{
    m_draw_renderer_name = draw;
}

void QSDLCanvas::onDraw()
{
    char text_buffer[256];
    SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(m_renderer);

    // Do your drawing here

    if (m_draw_renderer_name) {
        const char *renderer_name = SDL_GetRendererName(m_renderer);
        if (renderer_name == NULL) {
            SDL_snprintf(text_buffer, sizeof(text_buffer), "(null) [%s]", SDL_GetError());
            renderer_name = text_buffer;
        }
        SDL_Point render_size;
        SDL_GetRenderOutputSize(m_renderer, &render_size.x, &render_size.y);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugText(m_renderer, render_size.x - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(renderer_name), render_size.y - 1 * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE, renderer_name);
    }

    SDL_RenderPresent(m_renderer);
}

void QSDLCanvas::onEvent(SDL_Event *event)
{

}

void QSDLCanvas::resizeEvent(QResizeEvent *event)
{
    if (m_updateCanvasSize && m_window) {
        const QSize canvasSize = event->size();
        SDL_SetWindowSize(m_window, canvasSize.width(), canvasSize.height());
    }
}

void QSDLCanvas::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (!m_window) {
        SDL_PropertiesID props = SDL_CreateProperties();
        QString video_driver = SDL_GetCurrentVideoDriver();
        if (video_driver == "cocoa") {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, (void *)winId());
        } else if (video_driver == "x11") {
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, (int)winId());
        } else if (video_driver == "wayland") {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, (void *)winId());
        } else if (video_driver == "windows") {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, (void *)winId());
        } else {
            SDL_Log("QSDLCanvas does not support this video driver: %s", SDL_GetCurrentVideoDriver());
            QMessageBox::critical(nullptr, "Unknown SDL video driver", QString{"Don't know how to create SDL_Window from native handle for '%1' SDL video driver"}.arg(SDL_GetCurrentVideoDriver()));
            return;
        }
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, false);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
        if (m_updateCanvasSize) {
            QSize canvasSize = size();
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, canvasSize.width());
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, canvasSize.height());
        }
        m_window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
    }
    if (!m_window) {
        SDL_Log("Failed to create SDL window (%s)", SDL_GetError());
        return;
    }

    recreateRenderer();

    int ms = (int)(1000.0f / framerate());
    connect(&m_drawTimer, SIGNAL(timeout()), this, SLOT(onUpdate()));
    m_drawTimer.start(ms);
}

void QSDLCanvas::recreateRenderer()
{
    if (m_rendererNameChanged) {
        m_rendererNameChanged = false;
        QByteArray name_byte_array = m_rendererName.toUtf8();
        const char *name_bytes = name_byte_array.data();
        if (m_rendererName.length() == 0) {
            name_bytes = nullptr;
        }
        if (m_renderer) {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
        m_renderer = SDL_CreateRenderer(m_window, name_bytes);
        if (!m_renderer) {
            SDL_Log("Couldn't create SDL renderer(%s): %s", name_bytes, SDL_GetError());
            return;
        }
        SDL_Log("Render driver changed to %s", name_bytes);
    }
}
