#include <string>
#include <QFont>
#include <QQuickStyle>
#include <QApplication>
#include <pag/pag.h>
#include "common/version.h"
#include "report/PAGReport.h"
#include "license/LicenseDialog.h"
#include "rendering/PAGViewWindow.h"
#include "rendering/PAGWindowHelper.h"
#include "profiling/PAGBenchmarkModel.h"
#include "profiling/PAGRunTimeChartModel.h"
#include "profiling/PAGRunTimeModelManager.h"
#if defined(__APPLE__)
#include "common/PAGApplication.h"
#elif defined(WIN32)
#include "windows/SingleApplication.h"
#endif

void initReportConfig(const std::string& appVersion) {
#if defined(__APPLE__)
  PAGReport::getInstance()->setAppKey("0MAC04NRHX4I60N9");
#elif defined(WIN32)
  PAGReport::getInstance()->setAppKey("0WIN0KMRHX4YCIW9");
#endif
  PAGReport::getInstance()->setAppName("PAGViewer");
  PAGReport::getInstance()->setAppVersion(appVersion);
  PAGReport::getInstance()->setAppBundleId("com.tencent.pagplayer");
}

int main(int argc, char *argv[]) {
  bool cpuMode = false;
  std::string fileToOpen;

  for (int i = 0; i < argc; i++) {
    auto arg = std::string(argv[i]);
    if (arg == "-cpu") {
      cpuMode = true;
    } else if (arg == "-top") {
      PAGWindowHelper::SetTopWindow(true);
    } else if (argc > 1) {
      fileToOpen = arg;
    }
  }

  QApplication::setApplicationName("PAGPlayer");
  QApplication::setOrganizationName("PAGTeam");
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  QQuickStyle::setStyle("Universal");

  if (cpuMode) {
    QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
  }

  std::vector<std::string> fallbackList;
  QSurfaceFormat defaultFormat = QSurfaceFormat();
  defaultFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
  defaultFormat.setVersion(3, 2);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);
#if defined(WIN32)
  QFont defaultFonts("Microsoft Yahei");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"Microsoft YaHei"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);
  SingleApplication app(argc, argv);
#elif defined(__APPLE__)
  QFont defaultFonts("Helvetica Neue,PingFang SC");
  QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"PingFang SC", "Apple Color Emoji"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);
  PAGApplication app(argc, argv);
#endif
  QApplication::setWindowIcon(QIcon(":/image/appicon.png"));

  qRegisterMetaType<std::string>("std::string");
  // TODO add other types
  qmlRegisterType<PAGChartData>("PAG", 1, 0, "PAGChartData");
  qmlRegisterType<PAGViewWindow>("PAG", 1, 0, "PAGViewWindow");
  qmlRegisterType<PAGBenchmarkModel>("PAG", 1, 0, "PAGBenchmarkModel");
  qmlRegisterType<PAGRunTimeModelManager>("PAG", 1, 0, "PAGRunTimeModelManager");

  if (!LicenseDialog::isUserAgreeWithLicense() && !LicenseDialog::requestUserAgreement()) {
    qDebug() << "User disagree with license";
    return 0;
  }

  initReportConfig(AppVersion + "(" + UpdateChannel + ")");

  auto path = QString::fromLocal8Bit(fileToOpen.data());
  app.openFile(path);

  return app.exec();
}