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

#pragma once

#include <map>
#include <memory>
#include <string>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AlertInfo.h"
#include "config/ConfigParam.h"
#include "pag/file.h"
#include "pag/types.h"
#include "ui/ProgressModel.h"

namespace exporter {

class PAGExportSession {
 public:
  PAGExportSession(const AEGP_ItemH& activeItemHandle, const std::string& outputPath);
  void checkParamValid();
  void pushWarning(AlertInfoType type, const std::string& addInfo = "");
  pag::GradientColorHandle GetGradientColorsFromFileBytes(
      const std::vector<std::string>& matchNames, int index, int keyFrameIndex = 0);
  bool isVideoLayer(pag::ID id);
  AEGP_LayerH getLayerHByID(pag::ID id);

  bool exportActually = true;
  bool videoHasAlpha = false;
  bool videoAlphaDetected = false;
  bool showAlertInfo = false;
  bool stopExport = false;
  bool exportAudio = false;
  bool hardwareEncode = false;
  bool enableRunScript = true;
  bool exportStaticCompAsBmp = true;

  float frameRate = -1;

  pag::ID compID = 0;
  pag::ID layerID = 0;
  int layerIndex = 0;

  AEGP_ItemH itemHandle = nullptr;

  ConfigParam configParam = {};
  ProgressModel progressModel;
  AlertInfoManager& alertInfoManager = AlertInfoManager::GetInstance();

  std::vector<pag::Marker*>* audioMarkers = nullptr;
  std::vector<char> fileBytes = {};
  std::vector<pag::Composition*> compositions = {};
  std::vector<pag::ImageBytes*> imageBytesList = {};
  /* first: is video layer, second: layer handle */
  std::vector<std::pair<bool, AEGP_LayerH>> imageLayerHandleList = {};
  /* key: comp item ID, value: comp item handle */
  std::unordered_map<pag::ID, AEGP_ItemH> itemHandleMap = {};
  /* key: layer ID, value: layer handle */
  std::unordered_map<pag::ID, AEGP_LayerH> layerHandleMap = {};

  std::unordered_map<pag::ID, pag::Frame> videoCompositionStartTime = {};

  std::string outputPath = "";
};

}  // namespace exporter
