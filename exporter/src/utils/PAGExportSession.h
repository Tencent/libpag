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
#include <memory>
#include <string>
#include "../Config/ConfigParam.h"
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AlertInfo.h"
#include "pag/file.h"
#include "pag/types.h"

namespace exporter {

#define RECORD_ERROR(statements)                          \
  do {                                                    \
    if ((statements) != A_Err_NONE) {                     \
      session->pushWarning(AlertInfoType::ExportAEError); \
    }                                                     \
  } while (0)

class AlertInfoManager;

class PAGExportSession {
 public:
  PAGExportSession(const std::string& path);
  ~PAGExportSession();

  void checkParamValid();

  pag::TextDocumentHandle currentTextDocument();
  pag::GradientColorHandle currentGradientColors(const std::vector<std::string>& matchNames,
                                                 int index = 0);
  pag::TextDirection currentTextDocumentDirection();

  AEGP_ItemH getCompItemHById(pag::ID id);
  AEGP_LayerH getLayerHById(pag::ID id);
  bool isVideoReplaceLayer(AEGP_LayerH layerH);
  void pushWarning(AlertInfoType type, const std::string& addInfo = "");
  void pushWarning(AlertInfoType type, pag::ID compId, pag::ID layerId,
                   const std::string& addInfo = "");

  std::vector<char> fileBytes = {};
  const std::vector<char>& getFileBytes();

  AEGP_PluginID pluginID = 0L;
  std::shared_ptr<AEGP_SuiteHandler> suites = nullptr;
  std::string outputPath = "";
  AlertInfoManager& alertInfoManager = AlertInfoManager::GetInstance();
  std::atomic_bool bEarlyExit = false;
  ConfigParam configParam = {};

  std::vector<std::pair<AEGP_LayerH, bool>> imageLayerHList = {};
  std::unordered_map<pag::ID, AEGP_ItemH> compItemHList = {};
  std::unordered_map<pag::ID, AEGP_LayerH> layerHList = {};

  float frameRate = -1;
  pag::ID curCompId = 0;
  pag::ID curLayerId = 0;
  int layerIndex = 0;
  int gradientIndex = 0;
  int keyframeNum = 0;  // for text
  int keyframeIndex = 0;
  // Whether the alpha channel has been detected in the video sequence frame
  bool alphaDetected = false;
  // Whether the video sequence frame contains an alpha channel
  bool hasAlpha = false;

  bool enableRunScript = true;

  bool enableFontFile = false /*DEFAULT_ENABLE_FONT_FILE*/;
  std::vector<std::string> fontFilePathList = {};

  pag::ID recordCompId = 0;
  pag::ID recordLayerId = 0;
  std::vector<pag::TextDirection> textDirectList = {};
  bool enableAudio = true;
  bool enableForceStaticBMP = true;

  std::vector<std::shared_ptr<pag::Composition>> compositions = {};

 private:
  std::vector<std::vector<float>> extractFloatArraysByKey(const std::string& xmlContent,
                                                          const std::string& keyName);
  std::vector<pag::AlphaStop> ParseAlphaStops(const std::string& text);
  std::vector<pag::ColorStop> ParseColorStops(const std::string& text);
  static pag::GradientColorHandle DefaultGradientColors();
  pag::GradientColorHandle ParseGradientColor(const std::string& text);
  template <typename T>
  void ResortGradientStops(std::vector<T>& list);
};

}  // namespace exporter
