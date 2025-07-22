/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "AlertInfoModel.h"
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QQmlContext>
#include <QThread>
#include "utils/AEHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

AlertInfoModel::AlertInfoModel(QObject* parent) : QAbstractListModel(parent) {
}

AlertInfoModel::~AlertInfoModel() {
  alertInfos.clear();
}

int AlertInfoModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return static_cast<int>(alertInfos.size());
}

QVariant AlertInfoModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(alertInfos.size())) {
    return QVariant();
  }

  const AlertInfo& alertInfo = alertInfos[index.row()];

  switch (role) {
    case IsErrorRole:
      return alertInfo.isError;
    case InfoRole:
      return QString::fromStdString(alertInfo.info);
    case SuggestionRole:
      return QString::fromStdString(alertInfo.suggest);
    case IsFoldRole:
      return alertInfo.isFold;
    case CompoNameRole:
      return QString::fromStdString(alertInfo.compName);
    case LayerNameRole:
      return QString::fromStdString(alertInfo.layerName);
    case HasLayerNameRole:
      return !alertInfo.layerName.empty();
    case HasSuggestionRole:
      return !alertInfo.suggest.empty();
    default:
      return QVariant();
  }
}

QHash<int, QByteArray> AlertInfoModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IsErrorRole] = "isError";
  roles[InfoRole] = "errorInfo";
  roles[SuggestionRole] = "suggestion";
  roles[IsFoldRole] = "isFold";
  roles[CompoNameRole] = "compositionName";
  roles[LayerNameRole] = "layerName";
  roles[HasLayerNameRole] = "hasLayerName";
  roles[HasSuggestionRole] = "hasSuggestion";
  return roles;
}

bool AlertInfoModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(alertInfos.size())) {
    return false;
  }

  AlertInfo& alertInfo = alertInfos[index.row()];

  switch (role) {
    case IsFoldRole:
      alertInfo.isFold = value.toBool();
      Q_EMIT dataChanged(index, index);
      return true;
    default:
      return false;
  }
}

bool AlertInfoModel::locateAlert(int row) {
  if (row < 0 || row >= static_cast<int>(alertInfos.size())) {
    return false;
  }

  AlertInfo& alertInfo = alertInfos[row];
  alertInfo.select();
  return true;
}

QVariantList AlertInfoModel::getAlertInfos() const {
  QVariantList result;
  for (const AlertInfo& alertInfo : alertInfos) {
    result.append(alertInfoToVariantMap(alertInfo));
  }
  return result;
}

int AlertInfoModel::getAlertCount() const {
  return static_cast<int>(alertInfos.size());
}

void AlertInfoModel::jumpToUrl() {
  QDesktopServices::openUrl(QUrl(QLatin1String("https://pag.art/docs/pag-export-verify.html")));
}

void AlertInfoModel::setAlertInfos(std::vector<AlertInfo>& infos) {
  beginResetModel();
  alertInfos = std::move(infos);
  endResetModel();

  Q_EMIT alertInfoChanged();
}

bool AlertInfoModel::WarningsAlert(std::vector<AlertInfo>& infos) {
  bool hasData = !infos.empty() || !alertInfos.empty();
  if (!hasData) {
    return false;
  }

  int argc = 0;
  app = std::make_unique<QApplication>(argc, nullptr);
  app->setObjectName("PAG-Alert");
  alertEngine = std::make_unique<QQmlApplicationEngine>(app.get());

  QQmlContext* context = alertEngine->rootContext();

  if (!infos.empty()) {
    setAlertInfos(infos);
  }

  context->setContextProperty("alertModel", this);
  alertEngine->load(QUrl(QStringLiteral("qrc:/qml/AlertWarning.qml")));
  alertWindow = qobject_cast<QQuickWindow*>(alertEngine->rootObjects().first());
  if (alertWindow) {
    alertWindow->setPersistentGraphics(true);
    alertWindow->setPersistentSceneGraph(true);
    alertWindow->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  }

  alertWindow->show();
  app->exec();
  return true;
}

bool AlertInfoModel::ErrorsAlert(std::vector<AlertInfo>& infos) {
  bool hasData = !infos.empty() || !alertInfos.empty() || !errorMessage.isEmpty();
  if (!hasData) {
    return false;
  }

  int argc = 0;
  app = std::make_unique<QApplication>(argc, nullptr);
  app->setObjectName("PAG-Error");
  alertEngine = std::make_unique<QQmlApplicationEngine>(app.get());

  QQmlContext* context = alertEngine->rootContext();

  if (!infos.empty()) {
    setAlertInfos(infos);
  }

  context->setContextProperty("alertInfoModel", this);
  alertEngine->load(QUrl(QStringLiteral("qrc:/qml/AlertError.qml")));

  if (alertEngine->rootObjects().isEmpty()) {
    return false;
  }

  alertWindow = qobject_cast<QQuickWindow*>(alertEngine->rootObjects().first());
  if (!alertWindow) {
    return false;
  }

  alertWindow->setPersistentGraphics(true);
  alertWindow->setPersistentSceneGraph(true);
  alertWindow->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  alertWindow->show();
  app->exec();
  return true;
}

std::string AlertInfoModel::browseForSave(bool useScript) {
  auto suites = AEHelper::GetSuites();
  auto pluginID = AEHelper::GetPluginID();
  AEGP_ItemH activeItemH = AEHelper::GetActiveCompositionItem();
  if (activeItemH == nullptr) {
    return "";
  }
  std::string itemName = AEHelper::GetItemName(activeItemH);
  QString projectPath = AEHelper::GetProjectPath();

  QString fullPath = QDir(projectPath).filePath(QString::fromStdString(itemName));
  std::string filePath = fullPath.toStdString();

  static std::string LastOutputPath;
  static std::string LastFilePath;

  QString defaultPath = QString::fromStdString(filePath);
  if (!LastOutputPath.empty() && LastFilePath == filePath) {
    defaultPath = QString::fromStdString(LastOutputPath);
  }

  std::string outputPath = "";
  if (useScript) {
    QString scriptPath = defaultPath;
    scriptPath.replace('\\', '/');
    auto scriptText = "var file = new File(\"" + scriptPath.toStdString() + "\");\n" +
                      "var result = file.saveDlg();\n" + "result ? result.fsName : '';";
    outputPath = AEHelper::RunScript(suites, pluginID, scriptText);
  } else {
    QString saveFilePath = QFileDialog::getSaveFileName(QApplication::topLevelWidgets().value(0),
                                                        QObject::tr("选择存储路径"), defaultPath);
    outputPath = saveFilePath.toStdString();
  }

  if (!outputPath.empty()) {
    LastFilePath = filePath;
    LastOutputPath = outputPath;
    outputPath = StringHelper::ConvertStringEncoding(outputPath);
    StringHelper::InsureStringSuffix(outputPath, ".pag");
  }
  return outputPath;
}

QVariantMap AlertInfoModel::alertInfoToVariantMap(const AlertInfo& alertInfo) const {
  QVariantMap map;
  map["isError"] = alertInfo.isError;
  map["errorInfo"] = QString::fromStdString(alertInfo.info);
  map["suggestion"] = QString::fromStdString(alertInfo.suggest);
  map["isFold"] = alertInfo.isFold;
  map["compositionName"] = QString::fromStdString(alertInfo.compName);
  map["layerName"] = QString::fromStdString(alertInfo.layerName);
  map["hasLayerName"] = !alertInfo.layerName.empty();
  map["hasSuggestion"] = !alertInfo.suggest.empty();
  return map;
}

const AlertInfo* AlertInfoModel::getAlertInfo(const QModelIndex& index) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(alertInfos.size())) {
    return nullptr;
  }
  return &alertInfos[index.row()];
}

QString AlertInfoModel::getErrorMessage() const {
  return errorMessage;
}

void AlertInfoModel::setErrorMessage(const QString& message) {
  if (errorMessage != message) {
    errorMessage = message;
    Q_EMIT errorMessageChanged();
  }
}

}  // namespace exporter
