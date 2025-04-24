
#include "Canvas.h"

#include <cassert>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif

#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>

#include <QDebug>
#include <QString>
#include <QApplication>
#include <QBasicTimer>
#include <QResizeEvent>

#include <QOpenGLContext>

#include "Painter.h"


Canvas::Canvas(
  const QSurfaceFormat & format
, QScreen * screen)
: QWindow(screen)
, m_context(new QOpenGLContext)
, m_swapInterval(VerticalSyncronization)
, m_repaintTimer(new QBasicTimer())
, m_swapts(0.0)
, m_swaps(0)
, m_update(false)
, m_continuousRepaint(false)
, m_painter(nullptr)
{
    setSurfaceType(OpenGLSurface);

    create();

    initializeGL(format);

    qDebug() << "Press i or d to either increase or decrease number of cubes.";
    qDebug();
}

Canvas::~Canvas()
{
    delete m_painter;
}

QSurfaceFormat Canvas::format() const
{
    if (!m_context)
        return QSurfaceFormat();

    return m_context->format();
}

void Canvas::setContinuousRepaint(
    bool enable
,   int msec)
{
    if (m_continuousRepaint)
        m_repaintTimer->stop();

    m_continuousRepaint = enable;

    if (m_continuousRepaint)
        m_repaintTimer->start(msec, this);
}

bool Canvas::continuousRepaint() const
{
    return m_continuousRepaint;
}

void Canvas::initializeGL(const QSurfaceFormat & format)
{
    m_context->setFormat(format);
    m_context->create();

    m_context->makeCurrent(this);

    if (!m_painter)
    {
        m_painter = new Painter();
        m_painter->initialize();

        emit numCubesUpdate(m_painter->numCubes());
    }

    // print some gl infos (query)

    qDebug();
#if (QT_VERSION >= 0x050300)
    qDebug() << "OpenGL API:     " << (m_context->isOpenGLES() ? "GLES" : "GL");
#endif
    qDebug() << "OpenGL Version: " << qPrintable(QString::fromStdString(
        glbinding::ContextInfo::version().toString()));
    qDebug() << "OpenGL Vendor:  " << qPrintable(QString::fromStdString(
        glbinding::ContextInfo::vendor()));
    qDebug() << "OpenGL Renderer:" << qPrintable(QString::fromStdString(
        glbinding::ContextInfo::renderer()));
    qDebug();

    m_context->doneCurrent();

    m_fpsTimer.start();
}

void Canvas::resizeEvent(QResizeEvent * event)
{
    if (!m_painter)
        return;

    m_context->makeCurrent(this);

    m_painter->resize(event->size().width(), event->size().height());

    m_context->doneCurrent();

    if (isExposed() && Hidden != visibility())
        paintGL();
}

void Canvas::paintGL()
{
    if (!m_painter || !isExposed() || Hidden == visibility())
        return;

    m_context->makeCurrent(this);

    m_painter->draw();

    m_context->swapBuffers(this);
    m_context->doneCurrent();

    ++m_swaps;

    if (m_fpsTimer.elapsed() - m_swapts >= 1e+3)
    {
        const float fps = 1e+3f * static_cast<float>(static_cast<long double>
            (m_swaps) / (m_fpsTimer.elapsed() - m_swapts));

        emit fpsUpdate(fps);

        m_swapts = m_fpsTimer.elapsed();
        m_swaps = 0;
    }
}

void Canvas::timerEvent(QTimerEvent * event)
{
    assert(m_repaintTimer);

    if(event->timerId() != m_repaintTimer->timerId())
        return;

    paintGL();
}

void Canvas::setSwapInterval(SwapInterval swapInterval)
{
    m_context->makeCurrent(this);

    bool result(false);
    m_swapInterval = swapInterval;

#ifdef WIN32

    // ToDo: C++11 - type aliases
    typedef bool(WINAPI * SWAPINTERVALEXTPROC) (int);
    static SWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

    if (!wglSwapIntervalEXT)
        wglSwapIntervalEXT = reinterpret_cast<SWAPINTERVALEXTPROC>(m_context->getProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalEXT)
        result = wglSwapIntervalEXT(swapInterval);

#elif __APPLE__

    CGLContextObj contextObj;
    GLint swapIntervalParam = swapInterval;
    
    contextObj = CGLGetCurrentContext();

    CGLError error = CGLSetParameter(contextObj, kCGLCPSwapInterval, &swapIntervalParam);
    result = (error == kCGLNoError);

#else
    // ToDo: C++11 - type aliases
    typedef int(APIENTRY * SWAPINTERVALEXTPROC) (int);
    static SWAPINTERVALEXTPROC glXSwapIntervalSGI = nullptr;

    if (!glXSwapIntervalSGI)
        glXSwapIntervalSGI = reinterpret_cast<SWAPINTERVALEXTPROC>(m_context->getProcAddress("glXSwapIntervalSGI"));
    if (glXSwapIntervalSGI)
        result = glXSwapIntervalSGI(swapInterval);

#endif

    if (!result)
        qWarning("Setting swap interval to %s failed."
            , qPrintable(swapIntervalToString(swapInterval)));
    else
        qDebug("Setting swap interval to %s."
            , qPrintable(swapIntervalToString(swapInterval)));

    m_context->doneCurrent();
}

void Canvas::toggleSwapInterval()
{
    switch (m_swapInterval)
    {
    case NoVerticalSyncronization:
        setSwapInterval(VerticalSyncronization);
        break;
    case VerticalSyncronization:
        setSwapInterval(AdaptiveVerticalSyncronization);
        break;
    case AdaptiveVerticalSyncronization:
        setSwapInterval(NoVerticalSyncronization);
        break;
    }
}

const QString Canvas::swapIntervalToString(SwapInterval swapInterval)
{
    switch (swapInterval)
    {
    case NoVerticalSyncronization:
        return QString("NoVerticalSyncronization");
    case VerticalSyncronization:
        return QString("VerticalSyncronization");
    case AdaptiveVerticalSyncronization:
        return QString("AdaptiveVerticalSyncronization");
    default:
        return QString();
    }
}

void Canvas::keyPressEvent(QKeyEvent * event)
{
    bool numCubesChanged = false;

    if (event->key() == Qt::Key_I)
    {
        m_painter->setNumCubes(m_painter->numCubes() + 1);
        numCubesChanged = true;
    }

    if (event->key() == Qt::Key_D)
    {
        m_painter->setNumCubes(m_painter->numCubes() - 1);
        numCubesChanged = true;
    }

    if (numCubesChanged)
        emit numCubesUpdate(m_painter->numCubes());

    if (event->isAccepted())
        paintGL();
}
