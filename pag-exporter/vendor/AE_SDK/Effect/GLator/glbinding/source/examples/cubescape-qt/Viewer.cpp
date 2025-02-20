
#include <sstream>
#include <iomanip>
#include <cassert>

#include <QShortcut>
#include <QSettings>
#include <QLabel>
#include <QVector3D>
#include <QApplication>

#include "Canvas.h"
#include "Viewer.h"

#include "ui_Viewer.h"


namespace 
{
    const QString SETTINGS_GEOMETRY ("Geometry");
    const QString SETTINGS_STATE    ("State");
    
    const QString SETTINGS_ADAPTIVE_GRID("ShowAdaptiveGrid");
}

Viewer::Viewer(
    const QSurfaceFormat & format
,   QWidget * parent
,   Qt::WindowFlags flags)

: QMainWindow(parent, flags)
, m_ui(new Ui_Viewer)
, m_canvas(nullptr)
, m_fullscreenShortcut(nullptr)
, m_swapIntervalShortcut(nullptr)
{
    m_ui->setupUi(this);
    setWindowTitle(QApplication::applicationDisplayName());

    setup();
    setupCanvas(format);

    restore();

    updateAfterFullScreenToggle();
};


Viewer::~Viewer()
{
    store();

    setCentralWidget(nullptr);

    delete m_canvas;
}

void Viewer::restore()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings s;

    restoreGeometry(s.value(SETTINGS_GEOMETRY).toByteArray());
    restoreState(s.value(SETTINGS_STATE).toByteArray());
}

void Viewer::store()
{
    QSettings s;
    s.setValue(SETTINGS_GEOMETRY, saveGeometry());
    s.setValue(SETTINGS_STATE, saveState());
}

void Viewer::setup()
{
    // ToDo: this seems to be a generic problem (should be done by qt main window itself but....)
    //       We need to parse all available shortcuts via any menubars and connect those...

    m_fullscreenShortcut.reset(new QShortcut(m_ui->toggleFullScreenAction->shortcut(), this));
    connect(m_fullscreenShortcut.data(), &QShortcut::activated, this, &Viewer::toggleFullScreen);

    m_swapIntervalShortcut.reset(new QShortcut(m_ui->toggleSwapIntervalAction->shortcut(), this));
    connect(m_swapIntervalShortcut.data(), &QShortcut::activated, this, &Viewer::toggleSwapInterval);

    m_fpsLabel = new QLabel(m_ui->statusbar);
    m_ui->statusbar->addPermanentWidget(m_fpsLabel);

    m_numLabel = new QLabel(m_ui->statusbar);
    m_ui->statusbar->addPermanentWidget(m_numLabel);

}

void Viewer::setupCanvas(const QSurfaceFormat & format)
{
    m_canvas = new Canvas(format);
    m_canvas->setContinuousRepaint(true, 0);
    m_canvas->setSwapInterval(Canvas::VerticalSyncronization);

    connect(m_canvas, &Canvas::fpsUpdate, this, &Viewer::fpsChanged);
    connect(m_canvas, &Canvas::numCubesUpdate, this, &Viewer::numCubesChanged);

    QWidget * widget = QWidget::createWindowContainer(m_canvas);
    widget->setMinimumSize(1, 1);
    widget->setAutoFillBackground(false); // Important for overdraw, not occluding the scene.
    widget->setFocusPolicy(Qt::TabFocus);

    setCentralWidget(widget);
    show();
}

void Viewer::fpsChanged(float fps)
{
    m_fpsLabel->setText(QString(" %1 fps ")
        .arg(fps, 2, 'g', 4));
}

void Viewer::numCubesChanged(int numCubes)
{
    m_numLabel->setText(QString("#cubes: %2 (%1 x %1)")
        .arg(numCubes).arg(numCubes * numCubes));
}

void Viewer::on_toggleFullScreenAction_triggered(bool)
{
    toggleFullScreen();
}

void Viewer::toggleFullScreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();

    updateAfterFullScreenToggle();
}

void Viewer::updateAfterFullScreenToggle()
{
    m_ui->menubar->setVisible(isFullScreen());
    m_ui->statusbar->setVisible(isFullScreen());

    m_ui->menubar->setVisible(!isFullScreen());
    m_ui->statusbar->setVisible(!isFullScreen());
    m_fullscreenShortcut->setEnabled(isFullScreen());
    m_swapIntervalShortcut->setEnabled(isFullScreen());
}

void Viewer::on_toggleSwapIntervalAction_triggered(bool)
{
    toggleSwapInterval();
}

void Viewer::toggleSwapInterval()
{
    assert(m_canvas);
    m_canvas->toggleSwapInterval();
}

void Viewer::on_quitAction_triggered(bool)
{
    QApplication::quit();
}
