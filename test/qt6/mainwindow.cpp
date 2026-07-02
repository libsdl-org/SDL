/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <QtWidgets>

#include "mainwindow.h"
#include "QSDLCanvas.h"

MainWindow::MainWindow(bool updateCanvasSize)
{
    QWidget *widget = new QWidget;
    setCentralWidget(widget);

    m_canvas = new QSDLCanvas(this, updateCanvasSize);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 4, 0, 0);
    layout->addWidget(m_canvas);
    widget->setLayout(layout);

    createMenus();

    setWindowTitle(tr("SDL Qt Demo"));
    setMinimumSize(160, 160);
    resize(640, 480);
}

void MainWindow::createMenus()
{
    // Create "File" menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QAction *exitAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit), tr("E&xit"), this);
#else
    QAction *exitAct = new QAction(tr("E&xit"), this);
#endif
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAct);

    // Create Renderer menu
    QMenu *rendererMenu = menuBar()->addMenu(tr("&Renderer"));
    QActionGroup* renderGroup = new QActionGroup(this);
    renderGroup->setExclusive(true);
    {
        QAction *defaultDriverAction = new QAction(tr("(default)"), this);
        defaultDriverAction->setCheckable(true);
        defaultDriverAction->setChecked(true);
        renderGroup->addAction(defaultDriverAction);
        rendererMenu->addAction(defaultDriverAction);
        connect(defaultDriverAction, &QAction::triggered, [this] { m_canvas->changeRenderer(""); });
    }
    int count_render_drivers = SDL_GetNumRenderDrivers();
    for (int i = 0; i < count_render_drivers; i++) {
        QString driver_name = SDL_GetRenderDriver(i);
        QAction *driverAction = new QAction(driver_name, this);
        driverAction->setCheckable(true);
        driverAction->setChecked(false);
        renderGroup->addAction(driverAction);
        rendererMenu->addAction(driverAction);
        connect(driverAction, &QAction::triggered, [this, driver_name] { m_canvas->changeRenderer(driver_name); });
    }
    rendererMenu->addSeparator();
    QAction *drawNameAction = rendererMenu->addAction(tr("Draw name"));
    drawNameAction->setCheckable(true);
    connect(drawNameAction, &QAction::toggled, m_canvas, &QSDLCanvas::drawRendererName);
    drawNameAction->setChecked(true);

    // Create Help menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}
