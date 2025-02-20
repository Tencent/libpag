
#pragma once

#include <QMainWindow>
#include <QScopedPointer>


class Ui_Viewer;

class QLabel;
class QSurfaceFormat;
class QShortcut;

class Canvas;


class Viewer : public QMainWindow
{
    Q_OBJECT

public:
    Viewer(
        const QSurfaceFormat & format
    ,   QWidget * parent = nullptr
    ,   Qt::WindowFlags flags = NULL);

    virtual ~Viewer();

public slots:
    void fpsChanged(float fps);
    void numCubesChanged(int numCubes);

protected slots:
    void on_toggleFullScreenAction_triggered(bool checked);
    void toggleFullScreen();
    void on_toggleSwapIntervalAction_triggered(bool checked);
    void toggleSwapInterval();

    void on_quitAction_triggered(bool checked);

protected:
    void setup();
    void setupCanvas(const QSurfaceFormat & format);

    void store();
    void restore();

    void updateAfterFullScreenToggle();

protected:
    const QScopedPointer<Ui_Viewer> m_ui;

    Canvas * m_canvas;

    QLabel * m_fpsLabel;
    QLabel * m_numLabel;

    QScopedPointer<QShortcut> m_fullscreenShortcut;
    QScopedPointer<QShortcut> m_swapIntervalShortcut;
};
