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

#include "PAGExport.h"
#include <fstream>
#include <iostream>
#include "ExportComposition.h"
#include "ExportVerify.h"
#include "Marker.h"
#include "data/ImageBytes.h"
#include "sequence/AudioSequence.h"
#include "src/base/utils/Log.h"
#include "utils/AEDataTypeConverter.h"
#include "utils/AEHelper.h"
#include "utils/FileHelper.h"
#include "utils/LayerHelper.h"
#include "utils/PAGExportSessionManager.h"
#include "utils/UniqueID.h"
#include "version.h"

namespace exporter {

static void ReportExportInfo(const QVariantMap& map) {
  qDebug() << "Export Info:";
  qDebug() << map;
}

static bool ValidatePAGFile(uint8_t* data, size_t size) {
  auto pagFileDecoded = pag::File::Load(data, size);
  if (pagFileDecoded == nullptr) {
    return false;
  }
  auto bytes = pag::Codec::Encode(pagFileDecoded);
  if (bytes->length() != size) {
    LOGI("warning: bytes->length(%zu) != data.size(%zu)", bytes->length(), size);
  }

  return true;
}

template <typename T>
static void AdjustSequenceCompositionSize(pag::Composition* mainComposition) {
  mainComposition->width = 0;
  mainComposition->height = 0;
  for (auto sequence : static_cast<T>(mainComposition)->sequences) {
    if (mainComposition->width < sequence->width) {
      mainComposition->width = sequence->width;
    }
    if (mainComposition->height < sequence->height) {
      mainComposition->height = sequence->height;
    }
  }
}

static void AdjustCompositionSize(std::vector<pag::Composition*>& compositions) {
  auto mainComposition = compositions[compositions.size() - 1];
  if (mainComposition->type() == pag::CompositionType::Bitmap) {
    AdjustSequenceCompositionSize<pag::BitmapComposition*>(mainComposition);
  } else if (mainComposition->type() == pag::CompositionType::Video) {
    AdjustSequenceCompositionSize<pag::VideoComposition*>(mainComposition);
  }
}

static void ClearLayerName(pag::Composition* composition) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    layer->name = "";
    if (layer->type() == pag::LayerType::PreCompose) {
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;
      if (subCompostion->type() == pag::CompositionType::Vector) {
        ClearLayerName(subCompostion);
      }
    }
  }
}

static pag::Point GetImageMaxScale(pag::Composition* composition, pag::ID imageID) {
  if (composition->type() != pag::CompositionType::Vector) {
    return pag::Point::Zero();
  }

  pag::Point maxFactor = pag::Point::Zero();
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    pag::Point factor = pag::Point::Make(1.0f, 1.0f);

    if (layer->type() != pag::LayerType::Image) {
      continue;
    }
    if (static_cast<pag::ImageLayer*>(layer)->imageBytes->id != imageID) {
      continue;
    }
    if (layer->type() == pag::LayerType::PreCompose) {
      factor = GetImageMaxScale(static_cast<pag::PreComposeLayer*>(layer)->composition, imageID);
    }

    auto layerFactor = layer->getMaxScaleFactor();
    factor.x *= layerFactor.x;
    factor.y *= layerFactor.y;
    maxFactor.x = std::max(maxFactor.x, factor.x);
    maxFactor.y = std::max(maxFactor.y, factor.y);
  }

  return maxFactor;
}

static pag::Point GetBitmapCompositionMaxScale(pag::Composition* composition,
                                               pag::ID compositionID) {
  if (composition->type() == pag::CompositionType::Bitmap ||
      composition->type() == pag::CompositionType::Video) {
    if (composition->id == compositionID) {
      return pag::Point::Make(1.0f, 1.0f);
    } else {
      return pag::Point::Zero();
    }
  }

  pag::Point maxFactor = pag::Point::Zero();
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      auto factor = GetBitmapCompositionMaxScale(
          static_cast<pag::PreComposeLayer*>(layer)->composition, compositionID);
      auto layerFactor = layer->getMaxScaleFactor();
      factor.x *= layerFactor.x;
      factor.y *= layerFactor.y;

      maxFactor.x = std::max(maxFactor.x, factor.x);
      maxFactor.y = std::max(maxFactor.y, factor.y);
    }
  }

  return maxFactor;
}

template <typename T>
static void AdjustCompositionFrameRate(pag::Composition* composition) {
  if (composition->type() == pag::CompositionType::Vector) {
    return;
  }
  composition->frameRate = 0.0f;
  for (auto sequence : static_cast<T>(composition)->sequences) {
    if (composition->frameRate < sequence->frameRate) {
      composition->frameRate = sequence->frameRate;
      composition->duration = static_cast<pag::Frame>(sequence->frames.size());
    }
  }
}

static void AdjustmentPreComposeLayerForVideoComposition(std::shared_ptr<PAGExportSession> session,
                                                         pag::Layer* layer, void* ctx) {
  auto videoComposition = static_cast<pag::VideoComposition*>(ctx);
  auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
  if (preComposeLayer->composition != nullptr &&
      preComposeLayer->composition->uniqueID == videoComposition->uniqueID) {
    if (session->videoCompositionStartTime.find(videoComposition->uniqueID) !=
        session->videoCompositionStartTime.end()) {
      preComposeLayer->compositionStartTime +=
          session->videoCompositionStartTime[videoComposition->uniqueID];
    }
  }
}

static void AdjustTrackMatteLayer(std::shared_ptr<PAGExportSession> session) {
  pag::ID maxID = 0;
  std::vector<pag::Layer*> trackMatteLayers = {};
  for (auto& composition : session->compositions) {
    if (composition->type() == pag::CompositionType::Vector) {
      auto vectorComposition = static_cast<pag::VectorComposition*>(composition);
      for (size_t i = 0; i < vectorComposition->layers.size(); i++) {
        auto layer = vectorComposition->layers[i];
        maxID = std::max(maxID, layer->id);
        if (layer->trackMatteLayer != nullptr && i > 0) {
          trackMatteLayers.push_back(layer->trackMatteLayer);
        }
      }
    }
  }
  for (auto layer : trackMatteLayers) {
    maxID += 1;
    auto itemIter = session->itemHandleMap.find(layer->id);
    if (itemIter != session->itemHandleMap.end()) {
      session->itemHandleMap[maxID] = itemIter->second;
    }
    layer->id = maxID;
  }
}

PAGExport::PAGExport(const PAGExportConfigParam& configParam)
    : itemHandle(configParam.activeItemHandle),
      session(
          std::make_shared<PAGExportSession>(configParam.activeItemHandle, configParam.outputPath)),
      timeSetter(configParam.activeItemHandle, -100.0f) {
  session->exportAudio = configParam.exportAudio;
  session->hardwareEncode = configParam.hardwareEncode;
  session->exportActually = configParam.exportActually;
  session->showAlertInfo = configParam.showAlertInfo;
}

bool PAGExport::exportFile() {
  if (itemHandle == nullptr || session->outputPath.empty()) {
    return false;
  }
  auto pagFile = exportAsFile();
  QVariantMap exportInfo;
  exportInfo["AEVersion"] = QString::fromStdString(GetAEVersion());
  exportInfo["Event"] = "EXPORT_PAG";
  if (pagFile == nullptr) {
    exportInfo["FailReason"] = "ExportFail";
    exportInfo["ExportStatus"] = "fail";
    ReportExportInfo(exportInfo);
    return false;
  }
  exportInfo["PAGLayerCount"] = QString::number(pagFile->numLayers());
  exportInfo["VideoCompositionCount"] = QString::number(pagFile->numVideos());
  exportInfo["PAGTextLayerCount"] = QString::number(pagFile->numTexts());
  exportInfo["PAGImageLayerCount"] = QString::number(pagFile->numImages());

  const auto bytes = pag::Codec::Encode(pagFile);
  if (bytes->length() == 0) {
    exportInfo["FailReason"] = "EncodeFail";
    exportInfo["ExportStatus"] = "fail";
    ReportExportInfo(exportInfo);
    return false;
  }
  if (!ValidatePAGFile(bytes->data(), bytes->length())) {
    exportInfo["FailReason"] = "ValidateFail";
    exportInfo["ExportStatus"] = "fail";
    ReportExportInfo(exportInfo);
    return false;
  }
  if (!WriteToFile(session->outputPath, reinterpret_cast<char*>(bytes->data()),
                   static_cast<std::streamsize>(bytes->length()))) {
    exportInfo["FailReason"] = "WriteFileFail";
    exportInfo["ExportStatus"] = "fail";
    ReportExportInfo(exportInfo);
    return false;
  }
  exportInfo["ExportStatus"] = "success";
  ReportExportInfo(exportInfo);
  return true;
}

std::shared_ptr<pag::File> PAGExport::exportAsFile() {
  auto id = GetItemID(itemHandle);

  PAGExportSessionManager::GetInstance()->setCurrentSession(session);
  ScopedAssign<pag::ID> arCI(session->compID, id);
  session->progressModel.addTotalSteps(1);

  ExportComposition(session, itemHandle);
  AdjustTrackMatteLayer(session);
  if (session->stopExport) {
    return nullptr;
  }

  pag::Codec::InstallReferences(session->compositions);
  addRootComposition();
  if (session->stopExport) {
    return nullptr;
  }

  auto compositions = session->compositions;
  if (session->exportAudio && session->configParam.isTagCodeSupport(pag::TagCode::AudioBytes)) {
    GetAudioSequence(itemHandle, session->outputPath, compositions[compositions.size() - 1]);
    CombineAudioMarkers(compositions);
  }
  if (session->stopExport) {
    return nullptr;
  }

  std::vector<pag::ImageBytes*> images = getRefImages(compositions);
  CheckBeforeExport(session, compositions, images);

  if (session->stopExport ||
      (session->showAlertInfo && !AlertInfoManager::GetInstance().showAlertInfo())) {
    canceled = true;
    return nullptr;
  }

  exportResources(compositions);
  if (session->stopExport) {
    return nullptr;
  }

  AdjustCompositionSize(compositions);
  CheckAfterExport(session, compositions);
  if (session->stopExport) {
    return nullptr;
  }

  if (!session->configParam.exportLayerName ||
      !session->configParam.isTagCodeSupport(pag::TagCode::LayerAttributesV2)) {
    ClearLayerName(compositions.back());
  }

  if (!session->exportActually) {
    return nullptr;
  }

  auto pagFile = pag::Codec::VerifyAndMake(compositions, images);
  if (pagFile == nullptr) {
    session->pushWarning(AlertInfoType::PAGVerifyError);
    return nullptr;
  }

  CheckGraphicsMemory(session, pagFile);

  if (session->showAlertInfo && !AlertInfoManager::GetInstance().showAlertInfo()) {
    canceled = true;
    return nullptr;
  }

  ExportTimeStretch(pagFile, session, itemHandle);
  ExportLayerEditable(pagFile, session, itemHandle);
  ExportImageFillMode(pagFile, itemHandle);

  session->progressModel.addFinishedSteps();
  PAGExportSessionManager::GetInstance()->setCurrentSession(session);

  return pagFile;
}

void PAGExport::addRootComposition() const {
  const auto& Suites = GetSuites();
  auto mainComposition = session->compositions[session->compositions.size() - 1];
  AEGP_CompH compositionHandle = GetItemCompH(itemHandle);

  A_Time workAreaStart = {};
  A_Time workAreaDuration = {};
  Suites->CompSuite6()->AEGP_GetCompWorkAreaStart(compositionHandle, &workAreaStart);
  Suites->CompSuite6()->AEGP_GetCompWorkAreaDuration(compositionHandle, &workAreaDuration);
  pag::Frame start = AEDurationToFrame(workAreaStart, session->frameRate);
  pag::Frame end = start + AEDurationToFrame(workAreaDuration, session->frameRate);
  auto duration = mainComposition->duration;
  if (start == 0) {
    if (duration > end) {
      mainComposition->duration = end;
    }
    return;
  }

  duration = end - start;

  auto rootComposition = new pag::VectorComposition();
  auto rootLayer = new pag::PreComposeLayer();
  rootLayer->id = GetLayerUniqueID(session->compositions);
  rootLayer->transform = new pag::Transform2D();
  rootLayer->transform->position = new pag::Property<pag::Point>();
  rootLayer->transform->anchorPoint = new pag::Property<pag::Point>();
  rootLayer->transform->scale = new pag::Property<pag::Point>();
  rootLayer->transform->rotation = new pag::Property<float>();
  rootLayer->transform->opacity = new pag::Property<pag::Opacity>();
  rootLayer->transform->anchorPoint->value.x = 0;
  rootLayer->transform->anchorPoint->value.y = 0;
  rootLayer->transform->position->value.x = 0;
  rootLayer->transform->position->value.y = 0;
  rootLayer->transform->scale->value.x = 1.0f;
  rootLayer->transform->scale->value.y = 1.0f;
  rootLayer->transform->rotation->value = 0.0f;
  rootLayer->transform->opacity->value = pag::Opaque;
  rootLayer->composition = mainComposition;
  rootLayer->containingComposition = rootComposition;
  rootLayer->startTime = 0;
  rootLayer->duration = duration;
  rootLayer->compositionStartTime = -start;

  rootComposition->layers.push_back(rootLayer);
  rootComposition->width = mainComposition->width;
  rootComposition->height = mainComposition->height;
  rootComposition->duration = duration;
  rootComposition->frameRate = mainComposition->frameRate;
  rootComposition->id = GetCompositionUniqueID(session->compositions);
  rootComposition->backgroundColor = mainComposition->backgroundColor;

  rootComposition->audioBytes = mainComposition->audioBytes;
  rootComposition->audioMarkers = mainComposition->audioMarkers;
  rootComposition->audioStartTime = mainComposition->audioStartTime;
  mainComposition->audioBytes = nullptr;
  mainComposition->audioMarkers.clear();
  mainComposition->audioStartTime = pag::ZeroFrame;

  session->compositions.push_back(rootComposition);
}

std::vector<pag::ImageBytes*> PAGExport::getRefImages(
    const std::vector<pag::Composition*>& compositions) {
  std::unordered_set<pag::ImageBytes*> refImages = {};
  for (const auto& composition : compositions) {
    if (composition->type() != pag::CompositionType::Vector) {
      continue;
    }
    for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
      auto imageLayer = static_cast<pag::ImageLayer*>(layer);
      if (layer->type() == pag::LayerType::Image && imageLayer->imageBytes) {
        refImages.insert(imageLayer->imageBytes);
      }
    }
  }

  std::vector<pag::ImageBytes*> images = {};
  std::vector<pag::ImageBytes*> newImages = {};
  std::vector<std::pair<bool, AEGP_LayerH>> newImageLayerHandleList = {};
  for (size_t i = 0; i < session->imageBytesList.size(); ++i) {
    auto image = session->imageBytesList[i];
    if (refImages.count(image)) {
      images.push_back(image);
      newImages.push_back(image);
      newImageLayerHandleList.push_back(session->imageLayerHandleList[i]);
    } else {
      delete image;
    }
  }
  session->imageBytesList = std::move(newImages);
  session->imageLayerHandleList = std::move(newImageLayerHandleList);
  return images;
}

void PAGExport::exportResources(std::vector<pag::Composition*>& compositions) {
  if (!session->exportActually) {
    return;
  }
  exportRescaleImages();
  exportRescaleBitmapCompositions(compositions);
  exportRescaleVideoCompositions(compositions);
}

void PAGExport::exportRescaleImages() const {
  if (session->imageBytesList.empty()) {
    return;
  }

  auto mainComposition = session->compositions[session->compositions.size() - 1];
  for (size_t index = 0; index < session->imageBytesList.size() && !session->stopExport; index++) {
    pag::ImageBytes* image = session->imageBytesList[index];
    bool isVideo = session->imageLayerHandleList[index].first;
    AEGP_LayerH layerHandle = session->imageLayerHandleList[index].second;

    float factor = 1.0f;
    if (session->configParam.isTagCodeSupport(pag::TagCode::ImageBytesV2)) {
      auto point = GetImageMaxScale(mainComposition, image->id);
      factor = std::max(point.x, point.y);
      if (factor <= 0.0f) {
        factor = 1.0f;
      }
      factor *= session->configParam.imagePixelRatio;
      if (factor > 1.0) {
        factor = 1.0;
      }
    }
    if (factor > 0.0f) {
      GetImageBytes(session, image, layerHandle, isVideo, factor);
    }

    session->progressModel.addFinishedSteps();
  }
}

void PAGExport::exportRescaleBitmapCompositions(
    std::vector<pag::Composition*>& compositions) const {
  auto mainComposition = compositions[compositions.size() - 1];
  for (size_t i = 0; i < compositions.size(); i++) {
    auto composition = compositions[i];
    if (session->stopExport) {
      break;
    }
    ScopedAssign<pag::ID> compID(session->compID, composition->id);
    if (composition->type() == pag::CompositionType::Bitmap) {
      auto point = GetBitmapCompositionMaxScale(mainComposition, composition->id);
      float factor = std::max(point.x, point.y);
      if (factor > 1.0) {
        factor = 1.0;
      }

      ExportBitmapComposition(session, static_cast<pag::BitmapComposition*>(composition), factor);
      AdjustCompositionFrameRate<pag::BitmapComposition*>(composition);
    }
  }
}

void PAGExport::exportRescaleVideoCompositions(std::vector<pag::Composition*>& compositions) const {
  auto mainComposition = compositions[compositions.size() - 1];
  for (size_t i = 0; i < compositions.size(); i++) {
    auto composition = compositions[i];
    if (session->stopExport) {
      break;
    }
    ScopedAssign<pag::ID> compID(session->compID, composition->id);
    if (composition->type() == pag::CompositionType::Video) {
      auto point = GetBitmapCompositionMaxScale(mainComposition, composition->id);
      float factor = std::max(point.x, point.y);
      if (factor > 1.0) {
        factor = 1.0;
      }

      ExportVideoComposition(session, compositions,
                             static_cast<pag::VideoComposition*>(composition), factor);
      TraversalLayers(session, mainComposition, pag::LayerType::PreCompose,
                      AdjustmentPreComposeLayerForVideoComposition, composition);
      AdjustCompositionFrameRate<pag::VideoComposition*>(composition);
    }
  }
}

}  // namespace exporter
