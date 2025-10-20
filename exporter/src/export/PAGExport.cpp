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
#include "Marker.h"
#include "src/base/utils/Log.h"
#include "utils/AEDataTypeConverter.h"
#include "utils/AEHelper.h"
#include "utils/FileHelper.h"
#include "utils/UniqueID.h"

namespace exporter {

static bool ValidatePAGFile(uint8_t* data, size_t size) {
  auto pagFileDecoded = pag::File::Load(data, size);
  if (pagFileDecoded == nullptr) {
    return false;
  }
  auto bytes = pag::Codec::Encode(pagFileDecoded);
  if (bytes->length() != size) {
    LOGI("warning: bytes->length(%zu) != data.size(%zu)", bytes->length(), size);
    return false;
  }

  return true;
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
  if (pagFile == nullptr) {
    return false;
  }

  const auto bytes = pag::Codec::Encode(pagFile);
  if (bytes->length() == 0) {
    return false;
  }
  if (!ValidatePAGFile(bytes->data(), bytes->length())) {
    return false;
  }
  if (!WriteToFile(session->outputPath, reinterpret_cast<char*>(bytes->data()),
                   static_cast<std::streamsize>(bytes->length()))) {
    return false;
  }

  return true;
}

std::shared_ptr<pag::File> PAGExport::exportAsFile() {
  return nullptr;
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
  for (auto image : session->imageBytesList) {
    if (refImages.count(image)) {
      images.push_back(image);
    } else {
      delete image;
    }
  }
  return images;
}

void PAGExport::exportResources(std::vector<pag::Composition*>&) {
}

}  // namespace exporter
