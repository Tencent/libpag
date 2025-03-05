#include <string>
#include <QFont>
#include <QQuickStyle>
#include <QApplication>
#include <pag/pag.h>
#include "common/version.h"
#include "report/PAGReport.h"
#include "license/LicenseDialog.h"
#include "rendering/PAGWindowHelper.h"
#include "rendering/PAGViewWindow.h"
#if defined(PAG_MACOS)
#include "macos/PAGApplication.h"
#elif defined(PAG_WINDOWS)

#endif

void initReportConfig(const std::string& appVersion) {
#if defined(PAG_MACOS)
  PAGReport::getInstance()->setAppKey("0MAC04NRHX4I60N9");
#elif defined(PAG_WINDOWS)
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
      // TODO(markffan)
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
#if defined(PAG_WINDOWS)
  QFont defaultFonts("Microsoft Yahei");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"Microsoft YaHei"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);
  // TODO
#elif defined(PAG_MACOS)
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
  // TODO
  // qmlRegisterType<PAGUITask>("PAG", 1, 0, "PAGUITask");
  // qmlRegisterType<ChartModel>("PAG", 1, 0, "ChartModel");
  qmlRegisterType<PAGViewWindow>("PAG", 1, 0, "PAGViewWindow");
  // qmlRegisterType<RunTimeModel>("PAG", 1, 0, "RunTimeModel");
  // qmlRegisterType<ChartColumModel>("PAG", 1, 0, "ChartColumModel");
  // qmlRegisterType<FileTaskFactory>("PAG", 1, 0, "FileTaskFactory");
  // qmlRegisterType<PAGBenchmarkModel>("PAG", 1, 0, "PAGBenchmarkModel");
  // qmlRegisterUncreatableType<PAGFileTask>("PAG", 1, 0, "PAGFileTask", "");
  // qmlRegisterUncreatableType<TextDocument>("PAG", 1, 0, "TextDocument", "");

  if (!LicenseDialog::isUserAgreeWithLicense() && !LicenseDialog::requestUserAgreement()) {
    qDebug() << "User disagree with license";
    return 0;
  }

  initReportConfig(AppVersion + "(" + UpdateChannel + ")");

  auto path = QString::fromLocal8Bit(fileToOpen.data());
  app.openFile(path);

  return app.exec();
}