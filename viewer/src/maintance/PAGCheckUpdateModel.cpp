#include "PAGCheckUpdateModel.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QRegularExpression>
#include "common/version.h"
#include "PAGUpdater.h"
#include "PAGPluginInstaller.h"

const std::string MAC_REPO_URL_TEST = "aHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvdGVzdC9wbGF5ZXJfbWFj";
const std::string MAC_REPO_URL_RELEASE = "aHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvcGxheWVyX21hYw==";
const std::string WINDOWS_REPO_URL_TEST = "HaHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvYmV0YS92aWV3ZXJfd2luZG93cw==";
const std::string WINDOWS_REPO_URL_RELEASE = "@aHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvdmlld2VyX3dpbmRvd3M=";
const std::string WINDOWS_REPO_URL_TEST_PLAYER = "HaHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvdGVzdC9wbGF5ZXJfd2luZG93cw==";
const std::string WINDOWS_REPO_URL_RELEASE_PLAYER = "@aHR0cDovL2RsZGlyMS5xcS5jb20vcXFtaS9saWJwYWcvcGxheWVyX3dpbmRvd3M=";

auto PAGCheckUpdateModel::onToolProcessEnd(int exitCode, QProcess::ExitStatus exitStatus) -> void {
  auto result = false;
  if (toolProcess->error() != QProcess::UnknownError) {
    qDebug() << toolProcess->error();
    qDebug() << "Error checking for updates";
  } else {
    QByteArray data = toolProcess->readAllStandardOutput();
    QByteArray errorData = toolProcess->readAllStandardError();

    qDebug() << "Get data: " << qPrintable(data);
    qDebug() << "Get errorData: " << qPrintable(errorData);

    if (!data.isEmpty()) {
      result = true;
    }
  }

  Q_EMIT checkUpdateResult(result);
  delete toolProcess;
}

auto PAGCheckUpdateModel::isBetaVersion() -> bool {
  return UpdateChannel == "beta";
}

auto PAGCheckUpdateModel::copyFileToPath(const QString& sourceDir, QString toDir, bool coverFileIfExist) -> bool {
  toDir.replace("\\", "/");
  if (sourceDir == toDir) {
    return true;
  }
  if (!QFile::exists(sourceDir)) {
    return false;
  }
  QDir *createfile = new QDir;
  if (createfile->exists(toDir)) {
    if (coverFileIfExist) {
      createfile->remove(toDir);
    }
  }

  if (!QFile::copy(sourceDir, toDir)) {
    return false;
  }
  return true;
}

auto PAGCheckUpdateModel::setIsBetaVersion(bool isBetaVersion) -> bool {
  return true;
}

auto PAGCheckUpdateModel::hasPluginUpdate() -> bool {
  return PAGPluginInstaller::HasUpdate();
}

auto PAGCheckUpdateModel::checkPlayerUpdate(bool showUI, QString feedUrl) -> bool {
  auto url = feedUrl.toStdString();
  PAGUpdater::checkUpdates(showUI, url);
  return false;
}

auto PAGCheckUpdateModel::startUpdatePlayer() -> bool {
  return true;
}

auto PAGCheckUpdateModel::updatePreviousVersion() -> bool {
#if defined(__APPLE__)
  return true;
#elif defined(WIN32)
  // TODO Improve the code
  QString currentPath = qApp->applicationDirPath();
  if(currentPath.indexOf("PAGPlayer")<1){
      return true;
  }
  qDebug() << "CurrentPath: " << currentPath;

  QString toolPath =  currentPath +"/maintenancetool.ini";
  QFile data(toolPath);
  data.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  data.open(QIODevice::Text | QIODevice::ReadOnly);
  QString dataText = data.readAll();

  bool replaced = false;
  QRegularExpression regular = QRegularExpression(WINDOWS_REPO_URL_RELEASE_PLAYER.c_str());
  QString replacementText = QString(WINDOWS_REPO_URL_TEST.c_str());
  QRegularExpressionMatchIterator iter = regular.globalMatch(dataText);
  while (iter.hasNext()) {
    QRegularExpressionMatch match = iter.next();
    dataText.replace(match.capturedStart(0), match.capturedLength(0), replacementText);
	  replaced = true;
  }

  regular = QRegularExpression(WINDOWS_REPO_URL_TEST_PLAYER.c_str());
  replacementText = QString(WINDOWS_REPO_URL_TEST.c_str());
  iter = regular.globalMatch(dataText);
  while(iter.hasNext()) {
    QRegularExpressionMatch match = iter.next();
    dataText.replace(match.capturedStart(0), match.capturedLength(0), replacementText);
	  replaced = true;
  }

  if (!replaced) {
	  return true;
  }

  QString toolPathBak = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  toolPathBak.append("/maintenancetool.ini.bak");
  QFile newData(toolPathBak);
  newData.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  if (newData.open(QFile::WriteOnly | QFile::Truncate)) {
      QTextStream out(&newData);
      out << dataText;
  }
  newData.close();

  QProcess process;
  process.setWorkingDirectory(currentPath);
  QString command = currentPath + "/copy.bat";
	command = QDir::toNativeSeparators(command);
	toolPathBak = QDir::toNativeSeparators(toolPathBak);
	process.start(command, QStringList() << toolPathBak);
	bool result = process.waitForFinished();

  qDebug()<< "Progress.error():" <<process.error();

	QByteArray outputData = process.readAllStandardOutput();
	QByteArray errorData = process.readAllStandardError();
	qDebug() << "OutputData: " << outputData;
	qDebug() << "ErrorData: " << errorData;

  return result;
#endif
}

auto PAGCheckUpdateModel::installAEPlugin(bool bForce) -> int{
  return PAGPluginInstaller::InstallPlugins(bForce);
}

auto PAGCheckUpdateModel::uninstallAEPlugin() -> int {
  return PAGPluginInstaller::UninstallPlugins();
}

auto PAGCheckUpdateModel::revealInFinder(const QString &path) -> void {
#if defined(__APPLE__)
  QFileInfo file(path);
  QStringList args;
  if (!file.isDir()) {
    args << "-R";
  }
  args << path;
  QProcess::startDetached("open", args);
#elif defined(__APPLE__)
  auto localBitPath = path.toLocal8Bit().toStdString();
  QString currentPath = QString::fromLocal8Bit(localBitPath.c_str());
  QStringList args;
  args << "/select," << QDir::toNativeSeparators(currentPath);
  toolProcess->startDetached("explorer.exe", args);
#endif
}