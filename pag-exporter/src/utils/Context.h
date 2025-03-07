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
#ifndef CONTEXT_H
#define CONTEXT_H
#include <assert.h>
#include <pag/file.h>
#include <atomic>
#include <filesystem>
#include <unordered_map>
#include "AEGP_SuiteHandler.h"
#include "src/utils/AlertInfo.h"
#include "ConfigParam.h"
class ProgressWindow;
namespace pagexporter {
typedef std::pair<float, float> ScaleFactorAndFpsFactor;
struct ScaleAndFps {
  int32_t width;
  int32_t height;
  float frameRate;
};

enum class ErrorCode {
  ErrorNone = 0,
  ErrorUnknown = -1,
  ErrorPointNull = -2,
  ErrorNoMemory = -3,

  ErrorExporter = -100,
  ErrorEncoder = -102,
  ErrorDecoder = -103,

  ErrorTagLevelOutOfRange = -200,
  ErrorNoComposition = -201,
  ErrorNumBitmapsOutOfRange = -202,
  ErrorWebpEncoder = -203,
  ErrorExportRender = -204,
  ErrorExportBitmapSequence = -205,
  ErrorVideoEncoder = -206,
  ErrorNumSequenceOutOfRange = -207,
  ErrorExportVideoSequence = -208,
  ErrorExportAudioSequence = -209,
};

#define RECORD_ERROR(statements)                                       \
  do {                                                                 \
    if ((statements) != A_Err_NONE) {                                  \
      context->pushWarning(pagexporter::AlertInfoType::ExportAEError); \
    }                                                                  \
  } while (0)

enum class PresetTagLevel {
  TagLevelStable = static_cast<uint16_t>(
      pag::TagCode::
          TextAnimatorPropertiesStrokeColor),  // 稳定版本：发布一个比较大的稳定版本时应更新此值
  TagLevelMin = static_cast<uint16_t>(pag::TagCode::DropShadowStyle),  // 最低版本
  TagLevelMax = (static_cast<uint16_t>(pag::TagCode::Count) - 1),      // 最新版本
};


#define DEFAULT_BITMAP_KEYFRAME_INTERVAL 60
#define DEFAULT_BITMAP_MAX_RESOLUTION 720
#define DEFAULT_TAG_MODE ExportVersionType::Stable
#define DEFAULT_TAG_LEVEL 1023
#define DEFAULT_IMAGE_QUALITY 80
#define DEFAULT_IMAGE_PIXEL_RATIO 2.0
#define DEFAULT_SEQUENCE_SUFFIX "_bmp"
#define DEFAULT_SEQUENCE_TYPE pag::CompositionType::Video
#define DEFAULT_ENABLE_LAYERNAME true
#define DEFAULT_ENABLE_FONTFILE false
#define DEFAULT_ENABLE_COMPRESS_PANEL false
#define DEFAULT_SEQUENCE_QUALITY 80
#define DEFAULT_SCENES ExportScenes::GeneralScene
#define DEFAULT_ENABLE_AUDIO true
#define DEFAULT_LANGUAGE LanguageType::Chinese

class Context {
 public:
  Context(std::string outputPath);
  ~Context();
  pag::TextDocumentHandle currentTextDocument();
  pag::GradientColorHandle currentGradientColors(const std::vector<std::string>& matchNames,
                                                 int index = 0);
  pag::Enum currentTextDocumentDirection();
  void setParam(ConfigParam* configParam);
  void checkParamValid();

  AEGP_PluginID pluginID;
  const AEGP_SuiteHandler& suites;
  std::vector<pag::Composition*> compositions;
  std::vector<pag::Composition*> tmpCompositions;  // 用于视频序列帧裁剪
  std::vector<pag::ImageBytes*> imageBytesList;
  std::vector<float> factorList;    //生效的导出factor
  std::vector<float> reFactorList;  //用户在导出面板中设置的factor
  // 图片图层缓存列表，key-value 中的 value 为 true 的话，代表是视频占位图,
  // 否则是普通图片
  std::vector<std::pair<AEGP_LayerH, bool>> imageLayerHList;
  std::unordered_map<pag::ID, AEGP_ItemH> compItemHList;
  std::unordered_map<pag::ID, AEGP_LayerH> layerHList;
  AEGP_ItemH getCompItemHById(pag::ID id);
  AEGP_LayerH getLayerHById(pag::ID id);
  bool isVideoReplaceLayer(AEGP_LayerH layerH) const;

  float frameRate = -1;
  pag::ID curCompId = 0;
  pag::ID curLayerId = 0;
  int layerIndex = 0;
  int gradientIndex = 0;
  int keyframeNum = 0;  // for text
  int keyframeIndex = 0;
  bool alphaDetected = false;  // （视频序列帧）是否已经检测过是否含有alpha通道
  bool hasAlpha = false;       // （视频序列帧）是否含有alpha通道

  int bitmapKeyFrameInterval = DEFAULT_BITMAP_KEYFRAME_INTERVAL;  // 序列帧关键帧间距
  int bitmapMaxResolution = DEFAULT_BITMAP_MAX_RESOLUTION;        // 序列帧导出的短边最大分辨率。
  std::vector<std::pair<float, float>> scaleAndFpsList;           // scaleFactor and frameRate
  std::vector<std::pair<pag::VideoComposition*, ScaleFactorAndFpsFactor>>
      videoCompositionFactorList;
  std::vector<ScaleAndFps> originScaleAndFpsList;  // 序列帧原始 scale 和 frameRate

  ExportVersionType tagMode = DEFAULT_TAG_MODE;
  ExportScenes scenes = DEFAULT_SCENES;
  uint16_t exportTagLevel = static_cast<uint16_t>(DEFAULT_TAG_LEVEL);
  int imageQuality = DEFAULT_IMAGE_QUALITY;
  float imagePixelRatio = DEFAULT_IMAGE_PIXEL_RATIO;
  std::string sequenceSuffix = DEFAULT_SEQUENCE_SUFFIX;
  bool enableLayerName = DEFAULT_ENABLE_LAYERNAME;
  bool enableFontFile = DEFAULT_ENABLE_FONTFILE;
  std::vector<std::string> fontFilePathList;
  bool enableRunScript = true;
  int sequenceQuality = DEFAULT_SEQUENCE_QUALITY;
  bool enableCompressionPanel = DEFAULT_ENABLE_COMPRESS_PANEL;  //用户设置图片序列帧面板开关
  pag::CompositionType sequenceType = DEFAULT_SEQUENCE_TYPE;
  bool enableAudio = DEFAULT_ENABLE_AUDIO;
  bool enableBeforeWarning = false;
  bool enableForceStaticBMP = true;
  bool disableShowAlert = false;
  bool bHardware = false;

  ProgressWindow* progressWindow = nullptr;  // UI窗口
  std::string outputPath;
  pagexporter::AlertInfos alertInfos;
  void pushWarning(pagexporter::AlertInfoType type, std::string addInfo = "");
  void pushWarning(pagexporter::AlertInfoType type, pag::ID compId, pag::ID layerId,
                   const std::string& addInfo = "");

  std::vector<pag::Marker*>* pAudioMarkers = nullptr;
  std::atomic_bool bEarlyExit;

 private:
  char* fileBytes = nullptr;
  size_t fileLength = 0;
  char* getFileBytes();

  // for TextDocumentDirection
  pag::ID recordCompId = 0;
  pag::ID recordLayerId = 0;
  std::vector<pag::Enum> textDirectList;
  void exportFontFile(const pag::TextDocument* textDocument,
                      const std::unordered_map<std::string, std::string>& valueMap);
};

class TemporaryFileManager {
 public:
  TemporaryFileManager() {
  }
  ~TemporaryFileManager() {
    if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
      std::remove(tempFilePath.c_str());
    }
  }

 public:
  std::string tempFilePath = "";  // 存放临时文件路径
};

}  // namespace pagexporter
#endif  //CONTEXT_H
