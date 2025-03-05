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
#include "PAGExporter.h"
#include <QtCore/QObject>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
// #include "AudioSequence.h"
#include "src/exports/ExportVerify/ExportVerify.h"
#include "src/exports/Composition/BitmapComposition.h"
// #include "ImageBytes.h"
// #include "UI/CompressionOperate.h"
// #include "UI/ProgressWindow.h"
#include "src/ui/qt/ErrorList/AlertInfoUI.h"
// #include "VideoComposition.h"
// #include "exports/Marker.h"
#include "AEMarker/AEMarker.h"
#include "CommonMethod.h"
#include "pag/file.h"
// #include "utils/AEUtils.h"
// #include "utils/BaseCommon.h"
// #include "utils/FileAttributes.h"
// #include "utils/UniqueID.h"
// #include "report/PAGReport.h"

PAGExporter::PAGExporter(const AEGP_ItemH& activeItemH, std::string outputPath,
                         ProgressBase* progressBase)
    : context(outputPath), projectPlayTime(activeItemH, -100.0f) {
  // context.progressWindow = ProgressWindow::MakeProgressWindow(&context, progressBase).release();
}

static void reportExportFail() {
  // PAGReport::getInstance()->addParam("ExportStatus", "fail");
  // PAGReport::getInstance()->report();
}

static void reportExportSuccess() {
  // PAGReport::getInstance()->addParam("ExportStatus", "success");
  // PAGReport::getInstance()->report();
}

bool PAGExporter::ExportFile(const AEGP_ItemH& activeItemH, std::string outputPath,
                             bool enableAudio, bool enableBeforeWarning, QString titleName,
                             QString progressTips, ProgressBase* progressBase,
                             std::vector<pagexporter::AlertInfo>* alertInfos, bool bHardware) {
  if (activeItemH == nullptr || outputPath.empty()) {
    return false;
  }
  PAGExporter exporter(activeItemH, outputPath, progressBase);
  exporter.context.enableAudio = enableAudio;
  exporter.context.enableBeforeWarning = enableBeforeWarning;
  exporter.context.bHardware = bHardware;
  exporter.context.disableShowAlert = (alertInfos != nullptr);
  if (exporter.context.progressWindow != nullptr) {
    auto nameStr = titleName.toStdString();
    auto tipStr = progressTips.toStdString();
    // exporter.context.progressWindow->setTitleName(nameStr);
    // exporter.context.progressWindow->setProgressTips(tipStr);
  }

  auto pagFile = exporter.ExportPAG(activeItemH);
  if (alertInfos != nullptr) {
    for (auto info : exporter.context.alertInfos.saveWarnings) {
      alertInfos->push_back(info);
    }
  }
  int numLayers = pagFile ? pagFile->numLayers() : 0;
  int numVideos = pagFile ? pagFile->numVideos() : 0;
  int numTexts = pagFile ? pagFile->numTexts() : 0;
  int numImages = pagFile ? pagFile->numImages() : 0;
  // PAGReport::getInstance()->setEvent("EXPORT_PAG");
  // PAGReport::getInstance()->addParam("PAGLayerCount", std::to_string(numLayers));
  // PAGReport::getInstance()->addParam("VideoCompositionCount", std::to_string(numVideos));
  // PAGReport::getInstance()->addParam("PAGTextLayerCount", std::to_string(numTexts));
  // PAGReport::getInstance()->addParam("PAGImageLayerCount", std::to_string(numImages));
  // PAGReport::getInstance()->addParam("AEVersion", AEUtils::GetAeVersion());
  if (pagFile == nullptr) {
    reportExportFail();
    return false;
  }

  auto bytes = pag::Codec::Encode(pagFile);
  if (bytes->length() > 0) {
    std::ofstream out(outputPath, std::ios::binary);
    out.write(reinterpret_cast<char*>(bytes->data()), bytes->length());
    out.close();
  } else {
    // ErrorAlert(QObject::tr("PAG编码错误.").toStdString());
    reportExportFail();
    return false;
  }

  auto pagFileDecoded = pag::File::Load(bytes->data(), bytes->length());
  if (pagFileDecoded == nullptr) {
    // ErrorAlert(QObject::tr("PAG解码错误.").toStdString());
    reportExportFail();
    return false;
  }

  auto fileAttributes2 = &(pagFileDecoded->fileAttributes);
  if (fileAttributes2 != nullptr) {
    printf("timestamp=%llu\n", fileAttributes2->timestamp);
    printf("pluginVersion=%s\n", fileAttributes2->pluginVersion.c_str());
    printf("aeVersion=%s\n", fileAttributes2->aeVersion.c_str());
    printf("systemVersion=%s\n", fileAttributes2->systemVersion.c_str());
    printf("author=%s\n", fileAttributes2->author.c_str());
    printf("scene=%s\n", fileAttributes2->scene.c_str());
    for (auto warning : fileAttributes2->warnings) {
      printf("warning=%s\n", warning.c_str());
    }
  }

  auto bytes2 = pag::Codec::Encode(pagFileDecoded);  // 再编码检查一下
  if (bytes2->length() != bytes->length()) {
    printf("warning: bytes2->length(%zu) != bytes->length(%zu)\n", bytes2->length(),
           bytes->length());
  }
  // PAGReport::getInstance()->addParam("FileSize", std::to_string(bytes->length()));
  reportExportSuccess();
  return true;
}

static pag::Point GetImageMaxScale(pag::Composition* composition, pag::ID imageID) {
  if (composition->type() != pag::CompositionType::Vector) {
    return pag::Point::Zero();
  }

  pag::Point maxFactor = pag::Point::Zero();
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    pag::Point factor = pag::Point::Make(1.0f, 1.0f);

    if (layer->type() == pag::LayerType::PreCompose) {
      factor = GetImageMaxScale(static_cast<pag::PreComposeLayer*>(layer)->composition, imageID);
    } else if (layer->type() != pag::LayerType::Image ||
               static_cast<pag::ImageLayer*>(layer)->imageBytes->id != imageID) {
      continue;
    }

    auto layerFactor = layer->getMaxScaleFactor();
    factor.x *= layerFactor.x;
    factor.y *= layerFactor.y;

    maxFactor.x = std::max(maxFactor.x, factor.x);  // x/y"分别"取最大值
    maxFactor.y = std::max(maxFactor.y, factor.y);
  }

  return maxFactor;
}

void PAGExporter::exportRescaleImages() {
  if (context.imageBytesList.size() <= 0) {
    return;
  }

  auto mainComposition =
      context.compositions[context.compositions.size() - 1];  // get main composition
  context.factorList.clear();
  for (int index = 0;
       index < static_cast<int>(context.imageBytesList.size()) && !context.bEarlyExit; index++) {
    auto image = context.imageBytesList[index];
    auto refactor = context.reFactorList[index];

    float factor = 1.0f;
    if (context.exportTagLevel >= static_cast<uint16_t>(pag::TagCode::ImageBytesV2)) {
      auto point = GetImageMaxScale(mainComposition, image->id);
      factor = std::max(point.x, point.y);
      factor *= context.imagePixelRatio;
      if (factor > 1.0) {
        factor = 1.0;
      }
    }
    factor = factor * refactor;
    context.factorList.push_back(factor);
    if (factor > 0.0) {
      // ReExportImageBytes(&context, index, factor);
    }

    if (context.progressWindow != nullptr) {
      // context.progressWindow->increaseEncodedFrames();  // 更新进度
    }
  }
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

      maxFactor.x = std::max(maxFactor.x, factor.x);  // x/y"分别"取最大值
      maxFactor.y = std::max(maxFactor.y, factor.y);
    }
  }

  return maxFactor;
}

// 计算id被引用的次数（排除起始点相同的）
static void GetRefCountFromCompostion(std::vector<double>& startTimes,
                                      pag::Composition* composition, double startTime, pag::ID id) {
  if (composition->id == id) {
    for (auto time : startTimes) {
      if (fabs(time - startTime) < 1.0 / composition->frameRate / 2) {  // (diff time < half frame)
        return;
      }
    }
    startTimes.push_back(startTime);
  } else if (composition->type() == pag::CompositionType::Vector) {
    for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
      if (layer->type() == pag::LayerType::PreCompose) {
        auto layerStartTime = layer->startTime / composition->frameRate;
        GetRefCountFromCompostion(startTimes,
                                  static_cast<pag::PreComposeLayer*>(layer)->composition,
                                  startTime + layerStartTime, id);
      }
    }
  }
}

// 修改序列帧合成的分辨率为sequences的最大分辨率
template <typename T>
static void AdjustSequenceCompositionSize(pag::Composition* mainComposition) {
  mainComposition->width = 0;
  mainComposition->height = 0;
  for (auto sequence : static_cast<T>(mainComposition)->sequences) {
    if (mainComposition->width < sequence->width) {
      mainComposition->width = sequence->width;
      mainComposition->height = sequence->height;
    }
  }
}

// 如果mainComposition是序列帧，则修改该compostion分辨率为sequences的最大分辨率
static void AdjustCompositionSize(std::vector<pag::Composition*>& compositions) {
  auto mainComposition = compositions[compositions.size() - 1];  // get main composition
  if (mainComposition->type() == pag::CompositionType::Bitmap) {
    AdjustSequenceCompositionSize<pag::BitmapComposition*>(mainComposition);
  } else if (mainComposition->type() == pag::CompositionType::Video) {
    AdjustSequenceCompositionSize<pag::VideoComposition*>(mainComposition);
  }
}

// 修改BMP合成composition的帧率为sequence里的最大帧率，并相应修改duration
template <typename T>
static void AdjustCompositionFramerate(pag::Composition* composition) {
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

void PAGExporter::exportRescaleBitmapCompositions(std::vector<pag::Composition*>& compositions) {
  auto mainComposition = compositions[compositions.size() - 1];  // get main composition
  for (auto composition : compositions) {
    if (context.bEarlyExit) {
      break;
    }
    // AssignRecover<pag::ID> arCI(context.curCompId, composition->id);
    if (composition->type() == pag::CompositionType::Bitmap) {
      auto point = GetBitmapCompositionMaxScale(mainComposition, composition->id);
      float factor = std::max(point.x, point.y);
      if (factor > 1.0) {
        factor = 1.0;
      }

      // ReExportBitmapComposition(&context, static_cast<pag::BitmapComposition*>(composition), factor);

      // 修改composition的帧率为sequence里的最大帧率，并相应修改duration
      AdjustCompositionFramerate<pag::BitmapComposition*>(composition);
    }
  }
}

void PAGExporter::exportRescaleVideoCompositions(std::vector<pag::Composition*>& compositions) {
  auto mainComposition = compositions[compositions.size() - 1];  // get main composition
  for (auto composition : compositions) {
    if (context.bEarlyExit) {
      break;
    }
    AssignRecover<pag::ID> arCI(context.curCompId, composition->id);
    if (composition->type() == pag::CompositionType::Video) {
      auto point = GetBitmapCompositionMaxScale(mainComposition, composition->id);
      float factor = std::max(point.x, point.y);
      if (factor > 1.0) {
        factor = 1.0;
      }

      // ReExportVideoComposition(&context, compositions, static_cast<pag::VideoComposition*>(composition), factor);

      // 修改composition的帧率为sequence里的最大帧率，并相应修改duration
      AdjustCompositionFramerate<pag::VideoComposition*>(composition);
    }
  }

  for (auto composition : context.tmpCompositions) {
    auto layer = static_cast<pag::VectorComposition*>(composition)->layers[0];
    auto id = static_cast<pag::PreComposeLayer*>(layer)->composition->id;
    for (int index = 0; index < static_cast<int>(compositions.size()); index++) {
      if (compositions[index]->id == id) {
        compositions.insert(compositions.begin() + index + 1, composition);
      }
    }
  }
  context.tmpCompositions.clear();
}

static int CalBitmapFrames(pagexporter::Context& context,
                           std::vector<pag::Composition*>& compositions) {
  int totalFrames = 0;

  // 统计序列帧数目
  for (auto composition : compositions) {
    if (composition->type() == pag::CompositionType::Bitmap ||
        composition->type() == pag::CompositionType::Video) {
      for (auto scaleAndFps : context.scaleAndFpsList) {
        totalFrames +=
            static_cast<int>(composition->duration * scaleAndFps.second / composition->frameRate);
      }
    }
  }

  // 统计位图数目
  totalFrames += context.imageBytesList.size();

  return totalFrames;
}

static void CalRefCompositions(std::unordered_set<pag::Composition*>& refCompositions,
                               pag::Composition* composition) {
  if (composition->type() == pag::CompositionType::Vector) {
    for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
      auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
      if (layer->type() == pag::LayerType::PreCompose && preComposeLayer->composition) {
        CalRefCompositions(refCompositions, preComposeLayer->composition);
      }
    }
  }
  refCompositions.insert(composition);
}

std::vector<pag::Composition*> PAGExporter::ResortCompositions(pagexporter::Context& context) {
  std::vector<pag::Composition*> compositions;
  std::unordered_set<pag::Composition*> refCompositions;
  CalRefCompositions(refCompositions, context.compositions[context.compositions.size() - 1]);
  for (auto composition : context.compositions) {
    if (refCompositions.count(composition)) {
      compositions.push_back(composition);
    } else {
      delete composition;
    }
  }
  return compositions;
}

// 根据设置在写入文件前重写 layer name
static void RewriteLayerName(pagexporter::Context* context, pag::Composition* composition) {
  if (context->enableLayerName &&
      context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::LayerAttributesV2)) {
    return;
  }

  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    layer->name = "";
    if (layer->type() == pag::LayerType::PreCompose) {
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;
      if (subCompostion->type() == pag::CompositionType::Vector) {
        RewriteLayerName(context, subCompostion);
      }
    }
  }
}

static void CombineAllAudioMarkers(std::vector<pag::Composition*>& compositions) {
  auto mainComposition = compositions[compositions.size() - 1];
  for (int i = 0; i < static_cast<int>(compositions.size()) - 1; i++) {
    for (auto marker : compositions[i]->audioMarkers) {
      mainComposition->audioMarkers.push_back(marker);
    }
    compositions[i]->audioMarkers.clear();
  }
}

void PAGExporter::exporterRescale(std::vector<pag::Composition*>& compositions) {
  // auto compressionOperate = CompressionOperate::MakeCompressionOperate(&context).release();
  // do {
  //   exportRescaleImages();
  //   exportRescaleBitmapCompositions(compositions);
  //   exportRescaleVideoCompositions(compositions);
  //   if (context.enableCompressionPanel && !context.bEarlyExit) {
  //     if (compressionOperate->showPanel()) {
  //       break;
  //     } else {  //重新导出
  //       if (context.progressWindow != nullptr) {
  //         context.progressWindow->resetEncodedFrames();  // 进度归零
  //       }
  //     }
  //   }
  // } while (context.enableCompressionPanel && !context.bEarlyExit);
  //
  // delete compressionOperate;
}

std::vector<pag::ImageBytes*> PAGExporter::ResortImages(
    pagexporter::Context& context, std::vector<pag::Composition*>& compositions) {
  std::unordered_set<pag::ImageBytes*> refImages;
  for (auto composition : compositions) {
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
  std::vector<pag::ImageBytes*> images;
  for (auto image : context.imageBytesList) {
    if (refImages.count(image)) {
      images.push_back(image);
    } else {
      delete image;
    }
  }
  return images;
}

void PAGExporter::processWorkArea(const AEGP_ItemH& activeItemH) {
  auto& suites = SUITES();
  auto mainComposition = context.compositions[context.compositions.size() - 1];
  auto compHandle = AEUtils::GetCompFromItem(activeItemH);

  // 获取工作区域
  A_Time workAreaStart = {};
  A_Time workAreaDuration = {};
  suites.CompSuite6()->AEGP_GetCompWorkAreaStart(compHandle, &workAreaStart);
  suites.CompSuite6()->AEGP_GetCompWorkAreaDuration(compHandle, &workAreaDuration);
  pag::Frame start = ExportTime(workAreaStart, &context);
  pag::Frame end = start + ExportTime(workAreaDuration, &context);
  auto duration = mainComposition->duration;
  if (start == 0) {
    if (duration > end) {
      mainComposition->duration = end;
    }
    return;
  }

  duration = end - start;

  auto newComposition = new pag::VectorComposition();
  auto newLayer = new pag::PreComposeLayer();

  // newLayer->id = GetLayerUniqueId(&context, context.compositions);
  newLayer->transform = new pag::Transform2D();
  newLayer->transform->position = new pag::Property<pag::Point>();
  newLayer->transform->anchorPoint = new pag::Property<pag::Point>();
  newLayer->transform->scale = new pag::Property<pag::Point>();
  newLayer->transform->rotation = new pag::Property<float>();
  newLayer->transform->opacity = new pag::Property<pag::Opacity>();

  newLayer->transform->anchorPoint->value.x = 0;
  newLayer->transform->anchorPoint->value.y = 0;
  newLayer->transform->position->value.x = 0;
  newLayer->transform->position->value.y = 0;
  newLayer->transform->scale->value.x = 1.0f;
  newLayer->transform->scale->value.y = 1.0f;
  newLayer->transform->rotation->value = 0.0f;
  newLayer->transform->opacity->value = pag::Opaque;
  newLayer->composition = mainComposition;
  newLayer->containingComposition = newComposition;
  newLayer->startTime = 0;
  newLayer->duration = duration;
  newLayer->compositionStartTime = -start;

  newComposition->layers.push_back(newLayer);
  newComposition->width = mainComposition->width;
  newComposition->height = mainComposition->height;
  newComposition->duration = duration;
  newComposition->frameRate = mainComposition->frameRate;
  // newComposition->id = GetCompositionUniqueId(&context, context.compositions);
  newComposition->backgroundColor = mainComposition->backgroundColor;

  newComposition->audioBytes = mainComposition->audioBytes;
  newComposition->audioMarkers = mainComposition->audioMarkers;
  newComposition->audioStartTime = mainComposition->audioStartTime;
  mainComposition->audioBytes = nullptr;
  mainComposition->audioMarkers.clear();
  mainComposition->audioStartTime = pag::ZeroFrame;

  context.compositions.push_back(newComposition);
}

std::shared_ptr<pag::File> PAGExporter::ExportPAG(const AEGP_ItemH& activeItemH) {
  auto id = AEUtils::GetItemId(activeItemH);
  AssignRecover<pag::ID> arCI(context.curCompId, id);  // 暂存当前compItemId

  ExportComposition(&context, activeItemH);
  if (context.bEarlyExit) {
    return nullptr;
  }
  pag::Codec::InstallReferences(context.compositions);

  processWorkArea(activeItemH);

  auto compositions = ResortCompositions(context);

  if (context.bEarlyExit) {
    return nullptr;
  }

  if (context.enableAudio &&
      context.exportTagLevel >= static_cast<uint16_t>(pag::TagCode::AudioBytes)) {
    // ExportAudioSequence(&context, activeItemH, compositions[compositions.size() - 1]);
    if (compositions[compositions.size() - 1]->audioBytes != nullptr) {
      CombineAllAudioMarkers(compositions);
    }
  }

  if (context.bEarlyExit) {
    return nullptr;
  }

  // 设置总帧数以便进度控制
  if (context.progressWindow != nullptr) {
    int totalFrames = CalBitmapFrames(context, compositions);
    // context.progressWindow->setTotalFrames(totalFrames);
  }

  auto images = ResortImages(context, compositions);

  // ExportVerifyBefore(context, compositions, images);
  if (context.alertInfos.show(context.enableBeforeWarning && !context.disableShowAlert,
                              !context.disableShowAlert) ||
      context.bEarlyExit) {
    return nullptr;
  }

  exporterRescale(compositions);

  if (context.bEarlyExit) {
    return nullptr;
  }

  // 如果mainComposition是VideoComposition，则修改该compostion分辨率为sequences的最大分辨率
  AdjustCompositionSize(compositions);

  // AEMarker::PrintMarkers(compositions[compositions.size()-1]);

  // ExportVerifyAfter(context, compositions, images);
  if (context.bEarlyExit) {
    return nullptr;
  }

  RewriteLayerName(&context, compositions.back());

  auto pagFile = pag::Codec::VerifyAndMake(compositions, images);
  if (pagFile == nullptr) {
    context.pushWarning(pagexporter::AlertInfoType::PAGVerifyError);
    return nullptr;
  }

  // CheckGraphicsMemery(context, pagFile);

  if (context.alertInfos.show(!context.disableShowAlert, !context.disableShowAlert)) {
    return nullptr;
  }

  AEMarker::ExportTimeStretch(pagFile, context, activeItemH);
  AEMarker::ExportLayerEditable(pagFile, context, activeItemH);
  AEMarker::ExportImageFillMode(pagFile, context, activeItemH);
  // GetFileAttributes(&(pagFile->fileAttributes), context);

  return pagFile;
}
