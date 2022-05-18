/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "PAGStage.h"
#include <algorithm>
#include "base/utils/MatrixUtil.h"
#include "base/utils/TimeUtil.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/editing/ImageReplacement.h"
#include "rendering/renderers/CompositionRenderer.h"
#include "rendering/utils/LockGuard.h"

namespace pag {
std::shared_ptr<PAGStage> PAGStage::Make(int width, int height) {
  auto stage = std::shared_ptr<PAGStage>(new PAGStage(width, height));
  stage->weakThis = stage;
  return stage;
}

PAGStage::PAGStage(int width, int height) : PAGComposition(width, height) {
  rootLocker = std::make_shared<std::mutex>();
  stage = this;
}

PAGStage::~PAGStage() {
  // 需要提前调用 removeAllLayers(),
  // 否则父类析构时再调用会依赖 stage 的相关属性,
  // 此时 stage 已经销毁.
  removeAllLayers();
}

void PAGStage::setCacheScale(float value) {
  if (value <= 0 || value > 1.0f) {
    value = 1.0f;
  }
  _cacheScale = value;
}

std::shared_ptr<PAGComposition> PAGStage::getRootComposition() {
  if (layers.empty()) {
    return nullptr;
  }
  auto layer = layers.front();
  if (layer->layerType() != LayerType::PreCompose) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGComposition>(layer);
}

uint32_t PAGStage::getContentVersion() const {
  return contentVersion;
}

std::shared_ptr<File> PAGStage::getFileFromReferenceMap(ID uniqueID) {
  auto result = layerReferenceMap.find(uniqueID);
  if (result == layerReferenceMap.end()) {
    return nullptr;
  }
  return result->second.front()->getFile();
}

void PAGStage::addReference(PAGLayer* pagLayer) {
  addToReferenceMap(pagLayer->uniqueID(), pagLayer);
  addToReferenceMap(pagLayer->layer->uniqueID, pagLayer);
  if (pagLayer->layerType() == LayerType::PreCompose) {
    auto composition = static_cast<PreComposeLayer*>(pagLayer->layer)->composition;
    addToReferenceMap(composition->uniqueID, pagLayer);
  } else if (pagLayer->layerType() == LayerType::Image) {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    addToReferenceMap(imageBytes->uniqueID, pagLayer);
    auto pagImage = static_cast<PAGImageLayer*>(pagLayer)->getPAGImage();
    if (pagImage != nullptr) {
      addReference(pagImage.get(), pagLayer);
    }
  }
  auto targetLayer = pagLayer->layer;
  for (auto& style : targetLayer->layerStyles) {
    addToReferenceMap(style->uniqueID, pagLayer);
  }
  for (auto& effect : targetLayer->effects) {
    addToReferenceMap(effect->uniqueID, pagLayer);
  }
  invalidateCacheScale(pagLayer);
}

void PAGStage::addToReferenceMap(ID uniqueID, PAGLayer* pagLayer) {
  auto& layers = layerReferenceMap[uniqueID];
  auto position = std::find(layers.begin(), layers.end(), pagLayer);
  if (position == layers.end()) {
    layers.push_back(pagLayer);
  }
}

void PAGStage::removeReference(PAGLayer* pagLayer) {
  // 重置 rootVersion，防止 rootComposition 被移除又加入一个 version 相同的其他 Composition。
  rootVersion = -1;
  removeFromReferenceMap(pagLayer->uniqueID(), pagLayer);
  removeFromReferenceMap(pagLayer->layer->uniqueID, pagLayer);
  if (pagLayer->layerType() == LayerType::PreCompose) {
    auto composition = static_cast<PreComposeLayer*>(pagLayer->layer)->composition;
    auto empty = removeFromReferenceMap(composition->uniqueID, pagLayer);
    if (empty) {
      sequenceCache.erase(composition->uniqueID);
    }
  } else if (pagLayer->layerType() == LayerType::Image) {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    removeFromReferenceMap(imageBytes->uniqueID, pagLayer);
    auto pagImage = static_cast<PAGImageLayer*>(pagLayer)->getPAGImage();
    if (pagImage != nullptr) {
      removeReference(pagImage.get(), pagLayer);
    }
  }
  auto targetLayer = pagLayer->layer;
  for (auto& style : targetLayer->layerStyles) {
    removeFromReferenceMap(style->uniqueID, pagLayer);
  }
  for (auto& effect : targetLayer->effects) {
    removeFromReferenceMap(effect->uniqueID, pagLayer);
  }
  invalidateCacheScale(pagLayer);
}

bool PAGStage::removeFromReferenceMap(ID uniqueID, PAGLayer* pagLayer) {
  auto result = layerReferenceMap.find(uniqueID);
  if (result == layerReferenceMap.end()) {
    return true;
  }
  auto& layers = result->second;
  auto position = std::find(layers.begin(), layers.end(), pagLayer);
  if (position == layers.end()) {
    return false;
  }
  auto empty = layers.size() == 1;
  if (empty) {
    layerReferenceMap.erase(result);
    invalidAssets.insert(uniqueID);
  } else {
    layers.erase(position);
  }
  return empty;
}

void PAGStage::addReference(PAGImage* pagImage, pag::PAGLayer* pagLayer) {
  addToReferenceMap(pagImage->uniqueID(), pagLayer);
  pagImageMap[pagImage->uniqueID()] = pagImage;
}

void PAGStage::removeReference(PAGImage* pagImage, pag::PAGLayer* pagLayer) {
  auto empty = removeFromReferenceMap(pagImage->uniqueID(), pagLayer);
  if (empty) {
    pagImageMap.erase(pagImage->uniqueID());
  }
}

void PAGStage::invalidateCacheScale(PAGLayer* pagLayer) {
  std::vector<ID> invalidIDs = {};
  invalidIDs.push_back(pagLayer->uniqueID());
  invalidIDs.push_back(pagLayer->layer->uniqueID);
  if (pagLayer->layerType() == LayerType::PreCompose) {
    auto composition = static_cast<PreComposeLayer*>(pagLayer->layer)->composition;
    invalidIDs.push_back(composition->uniqueID);
  } else if (pagLayer->layerType() == LayerType::Image) {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    invalidIDs.push_back(imageBytes->uniqueID);
    auto pagImage = static_cast<PAGImageLayer*>(pagLayer)->getPAGImage();
    if (pagImage != nullptr) {
      invalidIDs.push_back(pagImage->uniqueID());
    }
  }
  for (auto id : invalidIDs) {
    scaleFactorCache.erase(id);
  }
}

std::shared_ptr<Graphic> PAGStage::getSequenceGraphic(Composition* composition,
                                                      Frame compositionFrame) {
  auto result = sequenceCache.find(composition->id);
  if (result != sequenceCache.end()) {
    if (result->second.compositionFrame == compositionFrame) {
      return result->second.graphic;
    }
    sequenceCache.erase(result);
  }
  SequenceCache cache = {};
  cache.graphic = RenderSequenceComposition(composition, compositionFrame);
  cache.compositionFrame = compositionFrame;
  sequenceCache[composition->uniqueID] = cache;
  return cache.graphic;
}

std::map<int64_t, std::vector<PAGLayer*>> PAGStage::findNearlyVisibleLayersIn(
    int64_t timeDistance) {
  std::map<int64_t, std::vector<PAGLayer*>> distanceMap = {};
  auto root = getRootComposition();
  if (root == nullptr) {
    return distanceMap;
  }
  auto rootDuration = root->durationInternal();
  auto globalFrameRate = frameRateInternal();
  auto globalFrame = root->localFrameToGlobal(root->currentFrameInternal());
  auto globalCurrent = FrameToTime(globalFrame, globalFrameRate);
  if (rootVersion != root->contentVersion) {
    layerStartTimeMap = {};
    updateLayerStartTime(root.get());
    rootVersion = root->contentVersion;
  }
  for (auto& item : layerStartTimeMap) {
    auto visibleStart = item.second;
    if (globalCurrent > visibleStart) {
      // 循环预测
      visibleStart += rootDuration;
    }
    auto distance = visibleStart - globalCurrent;
    if (distance > 0 && distance <= timeDistance) {
      distanceMap[distance].push_back(item.first);
    }
  }
  return distanceMap;
}

void PAGStage::updateLayerStartTime(PAGLayer* pagLayer) {
  if (pagLayer->layerType() == LayerType::PreCompose) {
    updateChildLayerStartTime(static_cast<PAGComposition*>(pagLayer));
    auto composition = static_cast<PreComposeLayer*>(pagLayer->layer)->composition;
    if (composition->type() != CompositionType::Video &&
        composition->type() != CompositionType::Bitmap) {
      return;
    }
  } else if (pagLayer->layerType() != LayerType::Image) {
    return;
  }
  auto frame = pagLayer->localFrameToGlobal(pagLayer->startFrame);
  layerStartTimeMap[pagLayer] = FrameToTime(frame, frameRateInternal());
}

void PAGStage::updateChildLayerStartTime(PAGComposition* pagComposition) {
  for (auto& childLayer : pagComposition->layers) {
    if (!childLayer->layerVisible || childLayer->_excludedFromTimeline) {
      // 不可见和脱离时间轴的图层不需要预测。
      continue;
    }
    updateLayerStartTime(childLayer.get());
  }
}

std::unordered_set<ID> PAGStage::getRemovedAssets() {
  if (invalidAssets.empty()) {
    return {};
  }
  std::unordered_set<ID> removedAssets = {};
  for (auto id : invalidAssets) {
    if (layerReferenceMap.count(id) == 0) {
      removedAssets.insert(id);
    }
  }
  invalidAssets = {};
  return removedAssets;
}

float PAGStage::getAssetMaxScale(ID referenceID) {
  return getMaxScaleFactor(referenceID) * _cacheScale;
}

float PAGStage::getMaxScaleFactor(ID referenceID) {
  auto result = scaleFactorCache.find(referenceID);
  if (result != scaleFactorCache.end()) {
    return result->second;
  }
  auto maxScaleFactor = calcMaxScaleFactor(referenceID);
  if (maxScaleFactor == FLT_MIN) {
    return 0;
  }
  scaleFactorCache[referenceID] = maxScaleFactor;
  return maxScaleFactor;
}

float PAGStage::calcMaxScaleFactor(ID referenceID) {
  auto reference = layerReferenceMap.find(referenceID);
  if (reference == layerReferenceMap.end()) {
    return FLT_MIN;
  }
  std::vector<PAGLayer*> pagLayers = {};
  auto isPAGImage = pagImageMap.count(referenceID) > 0;
  auto isPAGLayer =
      reference->second.size() == 1 && reference->second.front()->uniqueID() == referenceID;

  auto func = [isPAGImage, isPAGLayer, &pagLayers](PAGLayer* pagLayer) {
    // 内容修改过的图层有独立的缓存，若当前不是在计算 PAGImage 或 PAGLayer 的最大缩放值
    // （这两种情况下图层内容发生过修改），计算列表应该只考虑那些内容没被修改过的图层引用。
    if (isPAGImage || isPAGLayer || !pagLayer->contentModified()) {
      pagLayers.push_back(pagLayer);
    }
  };
  std::for_each(reference->second.begin(), reference->second.end(), func);
  if (pagLayers.empty()) {
    return 0;
  }
  float maxScaleFactor = 0;
  auto forEachFunc = [&maxScaleFactor, isPAGImage, this](PAGLayer* pagLayer) {
    auto scale = GetLayerContentScaleFactor(pagLayer, isPAGImage);
    auto scaleFactor = getLayerScaleFactor(pagLayer, scale);
    if (scaleFactor > maxScaleFactor) {
      maxScaleFactor = scaleFactor;
    }
  };
  std::for_each(pagLayers.begin(), pagLayers.end(), forEachFunc);
  return maxScaleFactor;
}

tgfx::Point PAGStage::GetLayerContentScaleFactor(PAGLayer* pagLayer, bool isPAGImage) {
  tgfx::Point scale = {1, 1};
  if (pagLayer->layerType() == LayerType::Image) {
    if (isPAGImage) {
      scale = static_cast<PAGImageLayer*>(pagLayer)->replacement->getScaleFactor();
    } else {
      scale.x = 1.0f / static_cast<ImageLayer*>(pagLayer->layer)->imageBytes->scaleFactor;
      scale.y = scale.x;
    }
  } else if (pagLayer->layerType() == LayerType::PreCompose) {
    auto composition =
        static_cast<PreComposeLayer*>(static_cast<PAGComposition*>(pagLayer)->layer)->composition;
    if (composition->type() == CompositionType::Video ||
        composition->type() == CompositionType::Bitmap) {
      auto sequence = Sequence::Get(composition);
      scale.x = static_cast<float>(composition->width) / static_cast<float>(sequence->width);
      scale.y = static_cast<float>(composition->height) / static_cast<float>(sequence->height);
    }
  }
  return scale;
}

float PAGStage::getLayerScaleFactor(PAGLayer* pagLayer, tgfx::Point scale) {
  auto parent = pagLayer;
  while (parent) {
    auto layerScaleFactor = parent->layerCache->getMaxScaleFactor();
    scale.x *= fabs(layerScaleFactor.x);
    scale.y *= fabs(layerScaleFactor.y);
    auto matrixScaleFactor = GetScaleFactor(ToTGFX(parent->layerMatrix));
    scale.x *= fabs(matrixScaleFactor.x);
    scale.y *= fabs(matrixScaleFactor.y);
    if (parent->_parent) {
      parent = parent->_parent;
    } else if (parent->trackMatteOwner) {
      parent = parent->trackMatteOwner->_parent;
    } else {
      break;
    }
  }
  return std::max(scale.x, scale.y);
}
}  // namespace pag
