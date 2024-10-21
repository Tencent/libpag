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

#include "RenderCache.h"
#include <functional>
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "rendering/caches/ImageContentCache.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/editing/ImageReplacement.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/renderers/FilterRenderer.h"
#include "rendering/sequences/SequenceImageProxy.h"
#include "rendering/sequences/SequenceInfo.h"
#include "tgfx/core/Clock.h"

namespace pag {
// 300M设置的大一些用于兜底，通常在大于20M时就开始随时清理。
static constexpr int MAX_GRAPHICS_MEMORY = 314572800;
static constexpr int PURGEABLE_GRAPHICS_MEMORY = 20971520;  // 20M
static constexpr int PURGEABLE_EXPIRED_FRAME = 10;
static constexpr float SCALE_FACTOR_PRECISION = 0.001f;
static constexpr float MIPMAP_ENABLED_THRESHOLD = -1.0f;      // 临时关闭 mipmap
static constexpr int64_t DECODING_VISIBLE_DISTANCE = 500000;  // 提前 500ms 开始解码。

RenderCache::RenderCache(PAGStage* stage) : _uniqueID(UniqueID::Next()), stage(stage) {
}

RenderCache::~RenderCache() {
  releaseAll();
}

uint32_t RenderCache::getContentVersion() const {
  return stage->getContentVersion();
}

bool RenderCache::videoEnabled() const {
  return _videoEnabled;
}

void RenderCache::setVideoEnabled(bool value) {
  if (_videoEnabled == value) {
    return;
  }
  _videoEnabled = value;
  clearAllSequenceCaches();
}

bool RenderCache::initFilter(Filter* filter) {
  tgfx::Clock clock = {};
  auto result = filter->initialize(getContext());
  programCompilingTime += clock.measure();
  return result;
}

void RenderCache::prepareLayers() {
  int64_t timeDistance = DECODING_VISIBLE_DISTANCE;
#ifdef PAG_BUILD_FOR_WEB
  // always prepare the whole timeline on the web platoform.
  timeDistance = INT64_MAX;
#endif
  auto layerDistances = stage->findNearlyVisibleLayersIn(timeDistance);
  for (auto& item : layerDistances) {
    for (auto pagLayer : item.second) {
      if (pagLayer->layerType() == LayerType::PreCompose) {
        preparePreComposeLayer(static_cast<PreComposeLayer*>(pagLayer->layer));
      } else if (pagLayer->layerType() == LayerType::Image) {
        prepareImageLayer(static_cast<PAGImageLayer*>(pagLayer));
      }
    }
  }
}

void RenderCache::preparePreComposeLayer(PreComposeLayer* layer) {
  auto composition = layer->composition;
  if (composition->type() != CompositionType::Video &&
      composition->type() != CompositionType::Bitmap) {
    return;
  }
  usedAssets.insert(composition->uniqueID);
  auto sequence = Sequence::Get(composition);
  auto info = SequenceInfo::Make(sequence);
  if (composition->staticContent()) {
    SequenceImageProxy proxy(info, 0);
    prepareAssetImage(composition->uniqueID, &proxy);
    return;
  }
  auto result = sequenceCaches.find(composition->uniqueID);
  if (result != sequenceCaches.end()) {
    return;
  }
  auto queue = makeSequenceImageQueue(info);
  queue->prepareNextImage();
}

void RenderCache::prepareImageLayer(PAGImageLayer* pagLayer) {
  std::shared_ptr<Graphic> graphic = nullptr;
  auto replacement = static_cast<PAGImageLayer*>(pagLayer)->replacement;
  if (replacement != nullptr) {
    graphic = replacement->getGraphic();
  } else {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    graphic = ImageContentCache::GetGraphic(imageBytes);
  }
  if (graphic) {
    graphic->prepare(this);
  }
}

void RenderCache::prepareNextFrame() {
#ifndef PAG_BUILD_FOR_WEB
  for (auto& item : usedSequences) {
    for (auto& map : item.second) {
      map.second->prepareNextImage();
    }
  }
#endif
}

void RenderCache::clearExpiredSequences() {
  std::vector<ID> expiredSequences = {};
  for (auto& item : sequenceCaches) {
    if (usedAssets.count(item.first) == 0) {
      expiredSequences.push_back(item.first);
    }
  }
  for (auto& id : expiredSequences) {
    clearSequenceCache(id);
  }
}

bool RenderCache::snapshotEnabled() const {
  return _snapshotEnabled;
}

void RenderCache::setSnapshotEnabled(bool value) {
  if (_snapshotEnabled == value) {
    return;
  }
  _snapshotEnabled = value;
  clearAllSnapshots();
}

void RenderCache::beginFrame() {
  usedAssets = {};
  usedSequences = {};
  resetPerformance();
}

void RenderCache::attachToContext(tgfx::Context* current, bool forDrawing) {
  if (contextID > 0 && contextID != current->uniqueID()) {
    // Context 改变需要清理内部所有缓存，这里用 uniqueID
    // 而不用指针比较，是因为指针析构后再创建可能会地址重合。
    releaseAll();
  }
  context = current;
  context->setCacheLimit(MAX_GRAPHICS_MEMORY);
  contextID = context->uniqueID();
  isDrawingFrame = forDrawing;
  if (!isDrawingFrame) {
    return;
  }
  auto removedAssets = stage->getRemovedAssets();
  for (auto assetID : removedAssets) {
    removeSnapshot(assetID);
    assetImages.erase(assetID);
    decodedAssetImages.erase(assetID);
    clearSequenceCache(assetID);
    clearFilterCache(assetID);
    removeTextAtlas(assetID);
  }
}

void RenderCache::releaseAll() {
  clearAllSnapshots();
  clearAllTextAtlas();
  graphicsMemory = 0;
  clearAllSequenceCaches();
  for (auto& item : filterCaches) {
    delete item.second;
  }
  filterCaches.clear();
  delete motionBlurFilter;
  motionBlurFilter = nullptr;
  delete transform3DFilter;
  transform3DFilter = nullptr;
  contextID = 0;
}

void RenderCache::detachFromContext() {
  if (!isDrawingFrame) {
    context = nullptr;
    return;
  }
  prepareNextFrame();
  recordPerformance();
  clearExpiredSequences();
  clearExpiredDecodedImages();
  clearExpiredSnapshots();
  if (!timestamps.empty()) {
    // Always purge recycled resources that haven't been used in 1 frame.
    context->purgeResourcesNotUsedSince(timestamps.back(), true);
  }
  if (context->memoryUsage() + graphicsMemory > PURGEABLE_GRAPHICS_MEMORY &&
      timestamps.size() == PURGEABLE_EXPIRED_FRAME) {
    // Purge all types of resources that haven't been used in 10 frames when the total memory usage
    // is over 20M.
    context->purgeResourcesNotUsedSince(timestamps.front(), false);
  }
  timestamps.push(std::chrono::steady_clock::now());
  while (timestamps.size() > PURGEABLE_EXPIRED_FRAME) {
    timestamps.pop();
  }
  context = nullptr;
}

Snapshot* RenderCache::getSnapshot(ID assetID) const {
  if (!_snapshotEnabled) {
    return nullptr;
  }
  auto result = snapshotCaches.find(assetID);
  if (result != snapshotCaches.end()) {
    return result->second;
  }
  return nullptr;
}

Snapshot* RenderCache::getSnapshot(const Picture* picture) {
  if (!_snapshotEnabled) {
    return nullptr;
  }
  usedAssets.insert(picture->assetID);
  auto maxScaleFactor = stage->getAssetMaxScale(picture->assetID);
  auto scaleFactor = picture->getScaleFactor(maxScaleFactor);
  auto snapshot = getSnapshot(picture->assetID);
  if (snapshot && (snapshot->makerKey != picture->uniqueKey ||
                   fabsf(snapshot->scaleFactor() - scaleFactor) > SCALE_FACTOR_PRECISION)) {
    removeSnapshot(picture->assetID);
    snapshot = nullptr;
  }
  if (snapshot) {
    moveSnapshotToHead(snapshot);
    return snapshot;
  }

  if (scaleFactor < SCALE_FACTOR_PRECISION || graphicsMemory >= MAX_GRAPHICS_MEMORY) {
    return nullptr;
  }
  auto minScaleFactor = stage->getAssetMinScale(picture->assetID);
  bool enableMipmap = minScaleFactor / scaleFactor < MIPMAP_ENABLED_THRESHOLD;
  auto newSnapshot = picture->makeSnapshot(this, scaleFactor, enableMipmap);
  if (newSnapshot == nullptr) {
    return nullptr;
  }
  snapshot = newSnapshot.release();
  snapshot->assetID = picture->assetID;
  snapshot->makerKey = picture->uniqueKey;
  graphicsMemory += snapshot->memoryUsage();
  snapshotLRU.push_front(snapshot);
  snapshotPositions[snapshot] = snapshotLRU.begin();
  snapshotCaches[picture->assetID] = snapshot;
  return snapshot;
}

void RenderCache::removeSnapshot(ID assetID) {
  auto snapshot = snapshotCaches.find(assetID);
  if (snapshot == snapshotCaches.end()) {
    return;
  }
  removeSnapshotFromLRU(snapshot->second);
  graphicsMemory -= snapshot->second->memoryUsage();
  delete snapshot->second;
  snapshotCaches.erase(assetID);
}

void RenderCache::moveSnapshotToHead(Snapshot* snapshot) {
  removeSnapshotFromLRU(snapshot);
  snapshot->idleFrames = 0;
  snapshotLRU.push_front(snapshot);
  snapshotPositions[snapshot] = snapshotLRU.begin();
}

void RenderCache::removeSnapshotFromLRU(Snapshot* snapshot) {
  auto position = snapshotPositions.find(snapshot);
  if (position != snapshotPositions.end()) {
    snapshotLRU.erase(position->second);
  }
}

TextAtlas* RenderCache::getTextAtlas(ID assetID) const {
  auto textAtlas = textAtlases.find(assetID);
  if (textAtlas == textAtlases.end()) {
    return nullptr;
  }
  return textAtlas->second;
}

TextAtlas* RenderCache::getTextAtlas(const TextBlock* textBlock) {
  auto maxScaleFactor = stage->getAssetMaxScale(textBlock->assetID());
  auto textAtlas = getTextAtlas(textBlock->assetID());
  if (textAtlas && (textAtlas->textGlyphsID() != textBlock->id() ||
                    fabsf(textAtlas->scaleFactor() - maxScaleFactor) > SCALE_FACTOR_PRECISION)) {
    removeTextAtlas(textBlock->assetID());
    textAtlas = nullptr;
  }
  if (textAtlas) {
    return textAtlas;
  }
  if (maxScaleFactor < SCALE_FACTOR_PRECISION) {
    return nullptr;
  }
  textAtlas = TextAtlas::Make(textBlock, this, maxScaleFactor).release();
  if (textAtlas) {
    graphicsMemory += textAtlas->memoryUsage();
    textAtlases[textBlock->assetID()] = textAtlas;
  }
  return textAtlas;
}

void RenderCache::removeTextAtlas(ID assetID) {
  auto textAtlas = textAtlases.find(assetID);
  if (textAtlas == textAtlases.end()) {
    return;
  }
  graphicsMemory -= textAtlas->second->memoryUsage();
  delete textAtlas->second;
  textAtlases.erase(textAtlas);
}

void RenderCache::clearAllTextAtlas() {
  for (auto atlas : textAtlases) {
    graphicsMemory -= atlas.second->memoryUsage();
    delete atlas.second;
  }
  textAtlases.clear();
}

void RenderCache::clearAllSnapshots() {
  for (auto& item : snapshotCaches) {
    graphicsMemory -= item.second->memoryUsage();
    delete item.second;
  }
  snapshotCaches.clear();
  snapshotLRU.clear();
  snapshotPositions.clear();
}

void RenderCache::clearExpiredSnapshots() {
  std::vector<Snapshot*> expiredSnapshots;
  size_t releaseMemory = 0;
  for (auto snapshotIter = snapshotLRU.rbegin(); snapshotIter != snapshotLRU.rend();
       snapshotIter++) {
    auto* snapshot = *snapshotIter;
    // 只有 Snapshot 数量可能会比较多，使用 LRU
    // 来避免遍历完整的列表，遇到第一个用过的就可以取消遍历。
    if (usedAssets.count(snapshot->assetID) > 0) {
      break;
    }
    snapshot->idleFrames++;
    if (snapshot->idleFrames < PURGEABLE_EXPIRED_FRAME &&
        graphicsMemory - releaseMemory < PURGEABLE_GRAPHICS_MEMORY) {
      // 总显存占用未超过20M且所有缓存均未超过10帧未使用，跳过清理。
      continue;
    }
    releaseMemory += snapshot->memoryUsage();
    expiredSnapshots.push_back(snapshot);
  }
  for (const auto& snapshot : expiredSnapshots) {
    removeSnapshot(snapshot->assetID);
  }
}

void RenderCache::prepareAssetImage(ID assetID, const ImageProxy* proxy) {
  usedAssets.insert(assetID);
  if (decodedAssetImages.count(assetID) != 0 || hasSnapshot(assetID)) {
    return;
  }
  auto image = getAssetImageInternal(assetID, proxy);
  if (image == nullptr) {
    return;
  }
  auto decodedImage = image->makeDecoded(context);
  if (decodedImage != image) {
    decodedAssetImages[assetID] = decodedImage;
  }
}

std::shared_ptr<tgfx::Image> RenderCache::getAssetImage(ID assetID, const ImageProxy* proxy) {
  usedAssets.insert(assetID);
  auto result = decodedAssetImages.find(assetID);
  if (result != decodedAssetImages.end()) {
    auto decodedImage = result->second;
    decodedAssetImages.erase(result);
    return decodedImage;
  }
  return getAssetImageInternal(assetID, proxy);
}

std::shared_ptr<tgfx::Image> RenderCache::getAssetImageInternal(ID assetID,
                                                                const ImageProxy* proxy) {
  auto image = assetImages[assetID];
  if (image != nullptr) {
    return image;
  }
  image = proxy->makeImage(this);
  if (image == nullptr) {
    return nullptr;
  }
  auto scaleFactor = stage->getAssetMinScale(assetID);
  if (scaleFactor < MIPMAP_ENABLED_THRESHOLD) {
    image = image->makeMipmapped(true);
  }
  assetImages[assetID] = image;
  return image;
}

void RenderCache::clearExpiredDecodedImages() {
  std::vector<ID> expiredList = {};
  for (auto& item : decodedAssetImages) {
    if (usedAssets.count(item.first) == 0) {
      expiredList.push_back(item.first);
    }
  }
  for (auto& assetID : expiredList) {
    decodedAssetImages.erase(assetID);
  }
  expiredList = {};
}

//===================================== sequence caches =====================================

void RenderCache::prepareSequenceImage(std::shared_ptr<SequenceInfo> sequence, Frame targetFrame) {
  auto queue = getSequenceImageQueue(sequence, targetFrame);
  if (queue != nullptr) {
    queue->prepare(targetFrame);
  }
}

std::shared_ptr<tgfx::Image> RenderCache::getSequenceImage(std::shared_ptr<SequenceInfo> sequence,
                                                           Frame targetFrame) {
  auto queue = getSequenceImageQueue(sequence, targetFrame);
  if (queue == nullptr) {
    return nullptr;
  }
  // We don't check mipmaps for sequences since there is currently no backend support yet.
  return queue->getImage(targetFrame);
}

SequenceImageQueue* RenderCache::getSequenceImageQueue(std::shared_ptr<SequenceInfo> sequence,
                                                       Frame targetFrame) {
  if (sequence == nullptr) {
    return nullptr;
  }
  if (sequence->staticContent()) {
    // Should not get here, we treat sequences with static content as normal asset images.
    return nullptr;
  }
  auto assetID = sequence->uniqueID();
  usedAssets.insert(assetID);
  auto& sequenceMap = usedSequences[assetID];
  auto result = sequenceMap.find(targetFrame);
  if (result != sequenceMap.end()) {
    return result->second;
  }
  auto queue = findNearestSequenceImageQueue(sequence, targetFrame);
  if (queue == nullptr) {
    queue = makeSequenceImageQueue(sequence);
  }
  if (queue != nullptr) {
    sequenceMap[targetFrame] = queue;
  }
  return queue;
}

SequenceImageQueue* RenderCache::findNearestSequenceImageQueue(
    std::shared_ptr<SequenceInfo> sequence, Frame targetFrame) {
  auto assetID = sequence->uniqueID();
  auto result = sequenceCaches.find(assetID);
  if (result == sequenceCaches.end()) {
    return nullptr;
  }
  auto& sequenceMap = usedSequences[assetID];
  std::unordered_set<SequenceImageQueue*> usedQueues = {};
  for (auto& item : sequenceMap) {
    usedQueues.insert(item.second);
  }
  std::vector<SequenceImageQueue*> freeQueues = {};
  for (auto& item : result->second) {
    if (usedQueues.count(item) == 0) {
      freeQueues.push_back(item);
    }
  }
  if (freeQueues.empty()) {
    return nullptr;
  }
  SequenceImageQueue* queue = nullptr;
  Frame minDistance = INT64_MAX;
  for (auto& item : freeQueues) {
    if (item->currentFrame == targetFrame) {
      // use the current cached image directly.
      queue = item;
      break;
    }
    auto distance = targetFrame - item->preparedFrame;
    if (distance < 0) {
      distance += sequence->duration();
    }
    if (distance >= 0 && distance < minDistance) {
      queue = item;
      minDistance = distance;
    }
  }
  if (queue == nullptr) {
    queue = freeQueues.front();
  }
  return queue;
}

SequenceImageQueue* RenderCache::makeSequenceImageQueue(std::shared_ptr<SequenceInfo> sequence) {
  if (!_videoEnabled && sequence->isVideo()) {
    return nullptr;
  }
  auto layer = stage->getLayerFromReferenceMap(sequence->uniqueID());
  auto queue = SequenceImageQueue::MakeFrom(sequence, layer, _useDiskCache).release();
  if (queue == nullptr) {
    return nullptr;
  }
  auto assetID = sequence->uniqueID();
  sequenceCaches[assetID].push_back(queue);
  return queue;
}

void RenderCache::clearAllSequenceCaches() {
  for (auto& item : sequenceCaches) {
    removeSnapshot(item.first);
    for (auto queue : item.second) {
      delete queue;
    }
  }
  sequenceCaches.clear();
}

void RenderCache::clearSequenceCache(ID uniqueID) {
  auto result = sequenceCaches.find(uniqueID);
  if (result != sequenceCaches.end()) {
    removeSnapshot(result->first);
    for (auto queue : result->second) {
      delete queue;
    }
    sequenceCaches.erase(result);
  }
}

//===================================== filter caches =====================================

LayerFilter* RenderCache::getFilterCache(LayerStyle* layerStyle) {
  return getLayerFilterCache(layerStyle->uniqueID, [=]() -> LayerFilter* {
    return LayerFilter::Make(layerStyle).release();
  });
}

LayerFilter* RenderCache::getFilterCache(Effect* effect) {
  return getLayerFilterCache(effect->uniqueID,
                             [=]() -> LayerFilter* { return LayerFilter::Make(effect).release(); });
}

LayerFilter* RenderCache::getLayerFilterCache(ID uniqueID,
                                              const std::function<LayerFilter*()>& makeFilter) {
  LayerFilter* filter = nullptr;
  auto result = filterCaches.find(uniqueID);
  if (result == filterCaches.end()) {
    filter = makeFilter();
    if (filter && !initFilter(filter)) {
      delete filter;
      filter = nullptr;
    }
    if (filter != nullptr) {
      filterCaches.insert(std::make_pair(uniqueID, filter));
    }
  } else {
    filter = static_cast<LayerFilter*>(result->second);
  }
  return filter;
}

Filter* RenderCache::getTransform3DFilter() {
  if (transform3DFilter == nullptr) {
    transform3DFilter = Make3DFilter(getContext());
  }
  return transform3DFilter;
}

MotionBlurFilter* RenderCache::getMotionBlurFilter() {
  if (motionBlurFilter == nullptr) {
    motionBlurFilter = new MotionBlurFilter();
    if (!initFilter(motionBlurFilter)) {
      delete motionBlurFilter;
      motionBlurFilter = nullptr;
    }
  }
  return motionBlurFilter;
}

LayerStylesFilter* RenderCache::getLayerStylesFilter(Layer* layer) {
  LayerStylesFilter* filter = nullptr;
  auto result = filterCaches.find(layer->uniqueID);
  if (result == filterCaches.end()) {
    filter = new LayerStylesFilter(this);
    if (initFilter(filter)) {
      filterCaches.insert(std::make_pair(layer->uniqueID, filter));
    } else {
      delete filter;
      filter = nullptr;
    }
  } else {
    filter = static_cast<LayerStylesFilter*>(result->second);
  }
  return filter;
}

void RenderCache::clearFilterCache(ID uniqueID) {
  auto result = filterCaches.find(uniqueID);
  if (result != filterCaches.end()) {
    delete result->second;
    filterCaches.erase(result);
  }
}

std::shared_ptr<File> RenderCache::getFileByAssetID(ID assetID) {
  auto layer = stage->getLayerFromReferenceMap(assetID);
  if (layer == nullptr) {
    return nullptr;
  }
  return layer->file;
}

void RenderCache::recordPerformance() {
  for (auto& item : sequenceCaches) {
    for (auto& queue : item.second) {
      queue->reportPerformance(this);
    }
  }
}

void RenderCache::recordImageDecodingTime(int64_t decodingTime) {
  imageDecodingTime += decodingTime;
}

void RenderCache::recordTextureUploadingTime(int64_t time) {
  textureUploadingTime += time;
}

void RenderCache::recordProgramCompilingTime(int64_t time) {
  programCompilingTime += time;
}
}  // namespace pag
