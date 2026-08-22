/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <QTimer>
#include <QWidget>
#include <QResizeEvent>

#include <SDL3/SDL.h>

class QSDLCanvas : public QWidget
{
    Q_OBJECT

public:
    QSDLCanvas(QWidget *parent = nullptr, bool updateCanvasSize = false);
    virtual ~QSDLCanvas();

public Q_SLOTS:
    void changeRenderer(const QString &name);
    void drawRendererName(bool draw);

protected Q_SLOTS:
    virtual void onUpdate();

protected:

    void recreateRenderer();

    // Override this to set the update framerate, this is called when the widget is shown
    virtual float framerate() { return 60.0f; }

    // Override this to draw to the canvas
    virtual void onDraw();

    // Override this to handle SDL events
    // This example assumes there is only one canvas and it should process all SDL events
    virtual void onEvent(SDL_Event *event);

    virtual void resizeEvent(QResizeEvent *event) override;

    // Handle the Qt show event
    virtual void showEvent(QShowEvent *event) override;

    // Let Qt know that we're going to do our own drawing
    virtual QPaintEngine *paintEngine() const override { return nullptr; }

protected:
    SDL_Window *m_window = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    QTimer m_drawTimer;

    bool m_updateCanvasSize = false;
    bool m_draw_renderer_name = true;
    QString m_rendererName;
    bool m_rendererNameChanged = true;
};
