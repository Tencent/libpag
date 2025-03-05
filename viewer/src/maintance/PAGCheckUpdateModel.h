#ifndef MAINTANCE_PAG_CHECK_UPDATE_MODEL_H_
#define MAINTANCE_PAG_CHECK_UPDATE_MODEL_H_

#include <QObject>
#include <QProcess>

class PAGCheckUpdateModel : public QObject {
  Q_OBJECT
 public:
  Q_SIGNAL void checkUpdateResult(bool hasUpdate);

  Q_SLOT void onToolProcessEnd(int exitCode, QProcess::ExitStatus exitStatus);

  Q_INVOKABLE bool isBetaVersion();
  Q_INVOKABLE bool copyFileToPath(const QString& sourceDir ,QString toDir, bool coverFileIfExist);
  Q_INVOKABLE bool hasPluginUpdate();
  Q_INVOKABLE bool setIsBetaVersion(bool isBetaVersion);
  Q_INVOKABLE bool checkPlayerUpdate(bool showUI, QString feedUrl);
  Q_INVOKABLE bool startUpdatePlayer();
  Q_INVOKABLE bool updatePreviousVersion();
  Q_INVOKABLE int installAEPlugin(bool bForce = false);
  Q_INVOKABLE int uninstallAEPlugin();
  Q_INVOKABLE void revealInFinder(const QString &path);

 private:
  QProcess *toolProcess = nullptr;
};

#endif // MAINTANCE_PAG_CHECK_UPDATE_MODEL_H_