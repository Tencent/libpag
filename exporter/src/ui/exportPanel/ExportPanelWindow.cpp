/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "ExportPanelWindow.h"
#include <QApplication>
#include <QQmlContext>
#include <QThread>
#include <QTimer>
#include <map>
#include "CompositionsModel.h"
#include "export/ExportComposition.h"
#include "platform/PlatformHelper.h"
#include "src/export/ExportLayer.h"
#include "utils/AEHelper.h"
#include "utils/AEResource.h"
#include "utils/FileHelper.h"
#include "utils/PAGExportSessionManager.h"
#include "utils/StringHelper.h"

namespace exporter {

ExportPanelWindow::ExportPanelWindow(QApplication* app, QObject* parent) : BaseWindow(app, parent) {
  init();
}

void ExportPanelWindow::init() {
  if (QThread::currentThread() != app->thread()) {
    qCritical() << "Must call init() in main thread";
    return;
  }

  compositionsModel = std::make_unique<CompositionsModel>(engine.get());

  QQmlContext* context = engine->rootContext();
  QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
  context->setContextProperty("exportingPanelWindow", this);
  QQmlEngine::setObjectOwnership(compositionsModel.get(), QQmlEngine::CppOwnership);
  context->setContextProperty("compositionsModel", compositionsModel.get());

  engine->load(QUrl(QStringLiteral("qrc:/qml/ExportPanel.qml")));

  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  QQuickWindow::setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);

  resources = AEResource::GetAEResourceList();
  compositionsModel->setQmlEngine(engine.get());
  compositionsModel->setAEResources(resources);
}

QString ExportPanelWindow::getBackgroundColor(int row) const {
  const auto& resource = resources[row];
  AEGP_CompH compH = GetItemCompH(resource->itemHandle);
  auto color = GetCompBackgroundColor(compH);
  return ColorToQString(color);
}

Q_INVOKABLE bool ExportPanelWindow::isAEWindowActive() {
  return IsAEWindowActive();
}

void ExportPanelWindow::viewLayers(const std::shared_ptr<AEResource>& resource) {
  if (!resource->composition.children.empty() || !resource->composition.textLayers.empty() ||
      !resource->composition.imageLayers.empty()) {
    return;
  }
  if (resource->type != AEResourceType::Composition) {
    return;
  }
  AEGP_CompH compH = GetItemCompH(resource->itemHandle);
  A_long layerCount = 0;
  GetSuites()->LayerSuite6()->AEGP_GetCompNumLayers(compH, &layerCount);
  for (A_long index = 0; index < layerCount; index++) {
    AEGP_LayerH layerHandle = nullptr;
    GetSuites()->LayerSuite6()->AEGP_GetCompLayerByIndex(compH, index, &layerHandle);
    if (layerHandle == nullptr) {
      continue;
    }

    AEResource::Layer layer;
    layer.layerID = GetLayerID(layerHandle);
    layer.name = GetLayerName(layerHandle);
    layer.layerHandle = layerHandle;
    AEGP_ItemH layerItemHandle = nullptr;
    ExportLayerType layerType = GetLayerType(layerHandle);
    if (layerType == ExportLayerType::Text) {
      resource->composition.textLayers.push_back(layer);
    } else if (layerType == ExportLayerType::Image || layerType == ExportLayerType::Video) {
      layerItemHandle = GetLayerItemH(layerHandle);
      auto iter = std::find_if(
          resource->composition.imageLayers.begin(), resource->composition.imageLayers.end(),
          [&](const auto& item) { return GetLayerItemH(item.layerHandle) == layerItemHandle; });
      if (iter == resource->composition.imageLayers.end()) {
        resource->composition.imageLayers.push_back(layer);
      }
    } else if (layerType == ExportLayerType::PreCompose) {
      layerItemHandle = GetLayerItemH(layerHandle);
      auto iter = std::find_if(resources.begin(), resources.end(), [&](const auto& item) {
        return item->itemHandle == layerItemHandle;
      });
      if (iter != resources.end()) {
        resource->composition.children.push_back(*iter);
      }
    }
  }

  for (const auto& child : resource->composition.children) {
    viewLayers(child);
  }
}

std::shared_ptr<PAGExportSession> ExportPanelWindow::getSession(A_long ID) {
  auto iter = sessionMap.find(ID);
  if (iter == sessionMap.end()) {
    return nullptr;
  }
  return iter->second;
}

void ExportPanelWindow::updateSession(A_long ID) {
  auto iter = std::find_if(resources.begin(), resources.end(),
                           [&](const auto& resource) { return resource->ID == ID; });
  if (iter == resources.end()) {
    return;
  }
  const auto& resource = *iter;
  std::string tempPagPath = JoinPaths(GetTempFolderPath(), "tmp.pag");
  auto session = std::make_shared<PAGExportSession>(resource->itemHandle, tempPagPath);
  PAGExportSessionManager::GetInstance()->setCurrentSession(session);
  session->exportAudio = false;
  session->enableRunScript = false;
  ExportComposition(session, resource->itemHandle);
  sessionMap[resource->ID] = session;
  PAGExportSessionManager::GetInstance()->unsetCurrentSession(session);
}

}  // namespace exporter
