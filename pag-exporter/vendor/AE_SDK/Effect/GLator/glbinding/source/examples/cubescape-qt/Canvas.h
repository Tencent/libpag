
#pragma once

#include <QWindow>

#include <QList>
#include <QTime>
#include <QScopedPointer>

class QOpenGLContext;
class QSurfaceFormat;
class QBasicTimer;
class QTimerEvent;
class QKeyEvent;


class Painter;

class Canvas : public QWindow
{
    Q_OBJECT

public:
    enum SwapInterval
    {
        NoVerticalSyncronization        =  0
    ,   VerticalSyncronization          =  1 ///< WGL_EXT_swap_control, GLX_EXT_swap_control, GLX_SGI_video_sync
    ,   AdaptiveVerticalSyncronization  = -1 ///< requires EXT_swap_control_tear
    };

public:
    Canvas(
        const QSurfaceFormat & format
    ,   QScreen * screen = nullptr);

    virtual ~Canvas();

    // from QWindow
    virtual QSurfaceFormat format() const;

    void setContinuousRepaint(bool enable, int msec = 1000 / 60);
    bool continuousRepaint() const;

    void setSwapInterval(SwapInterval swapInterval);
    static const QString swapIntervalToString(SwapInterval swapInterval);

public slots:
    void toggleSwapInterval();
    

protected:
    virtual void initializeGL(const QSurfaceFormat & format);
    virtual void paintGL();

    virtual void resizeEvent(QResizeEvent * event);
    virtual void keyPressEvent(QKeyEvent * event);

    void timerEvent(QTimerEvent * event);

signals:
    void fpsUpdate(float fps);
    void numCubesUpdate(int numCubes);

protected:
    QScopedPointer<QOpenGLContext> m_context;

    SwapInterval m_swapInterval;    ///< required for toggle

    QScopedPointer<QBasicTimer> m_repaintTimer;
    QTime m_fpsTimer;

    long double m_swapts;
    unsigned int m_swaps;

    bool m_update; // checked in paintGL, if true, update of program gets triggered

    bool m_continuousRepaint;

    Painter * m_painter;
};
