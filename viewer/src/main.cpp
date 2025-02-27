#include <string>
#include <QFont>
#include <QQuickStyle>
#include <QApplication>
#include <QQuickWindow>
#include "license/LicenseDialog.h"
#include "rendering/WindowHelper.h"
#include "rendering/PAGQuickItem.h"
#if defined(PAG_MACOS)
#include "macos/PAGApplication.h"
#elif defined(PAG_WINDOWS)

#endif

int main(int argc, char *argv[]) {
  bool cpuMode = false;
  std::string fileToOpen;

  for (int i = 0; i < argc; i++) {
    auto arg = std::string(argv[i]);
    if (arg == "-cpu") {
      cpuMode = true;
    } else if (arg == "-top") {
      // TODO(markffan)
      WindowHelper::SetTopWindow(true);
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
#if defined(PAG_MACOS)
  QFont defaultFonts("Microsoft Yahei");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"Microsoft YaHei"};
  // pag::PAGFont::SetFallbackFontNames(fallbackList);
  // pag::SDKLicenseManager::GetInstance()->loadLicense("com.tencent.pagplayer");
  PAGApplication app(argc, argv);

#elif defined(PAG_WINDOWS)
  QFont defaultFonts("Helvetica Neue,PingFang SC");
  QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"PingFang SC", "Apple Color Emoji"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);
  pag::SDKLicenseManager::GetInstance()->loadLicense("com.tencent.pagplayer");
  // TODO
#endif
  QApplication::setWindowIcon(QIcon(":/image/appicon.png"));

  qRegisterMetaType<std::string>("std::string");
  // TODO
  // qmlRegisterType<PAGUITask>("PAG", 1, 0, "PAGUITask");
  // qmlRegisterType<ChartModel>("PAG", 1, 0, "ChartModel");
  qmlRegisterType<PAGQuickItem>("PAG", 1, 0, "PAGQuickItem");
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

  auto path = QString::fromLocal8Bit(fileToOpen.data());
  app.openFile(path);

  return app.exec();
}