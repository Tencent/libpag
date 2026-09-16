/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
static constexpr float MIPMAP_ENABLED_THRESHOLD = 0.4f;
static constexpr int64_t DECODING_VISIBLE_DISTANCE = 500000;  // 提前 500ms 开始解码。
static constexpr int64_t SEQUENCE_RETRY_INITIAL_DELAY = 100000;
static constexpr int64_t SEQUENCE_RETRY_MAX_DELAY = 2000000;

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
  if (!value) {
    clearVideoSequenceCaches();
  }
  _sequenceCacheInvalidated = true;
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
    prepareStaticSequenceImage(info);
    return;
  }
  auto result = sequenceCaches.find(composition->uniqueID);
  if (result != sequenceCaches.end()) {
    return;
  }
  auto queue = makeSequenceImageQueue(info);
  if (queue) {
    queue->prepareNextImage();
  }
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
  checkSequenceDecodeFailure();
#ifndef PAG_BUILD_FOR_WEB
  for (auto& item : usedSequences) {
    if (!canRetrySequence(item.first)) {
      continue;
    }
    for (auto& map : item.second) {
      map.second.queue->prepareNextImage();
    }
  }
#endif
}

void RenderCache::checkSequenceDecodeFailure() {
  std::unordered_set<ID> failedAssets = {};
  std::unordered_set<ID> succeededAssets = {};
  for (auto& item : usedStaticSequences) {
    auto& result = item.second;
    if (result == nullptr) {
      continue;
    }
    auto status = result->status.load(std::memory_order_acquire);
    if (status == SequenceReadStatus::Succeeded) {
      succeededAssets.insert(item.first);
      continue;
    }
    if (status != SequenceReadStatus::Failed) {
      continue;
    }
    failedAssets.insert(item.first);
    lastFrameHasSequenceDecodeFailure = true;
    _sequenceCacheInvalidated = true;
    removeSnapshot(item.first);
    assetImages.erase(item.first);
    decodedAssetImages.erase(item.first);
    staticSequenceResults.erase(item.first);
  }
  for (auto& item : usedSequences) {
    for (auto& map : item.second) {
      auto& usage = map.second;
      if (usage.result == nullptr) {
        continue;
      }
      auto status = usage.result->status.load(std::memory_order_acquire);
      if (status == SequenceReadStatus::Succeeded) {
        succeededAssets.insert(item.first);
        continue;
      }
      if (status != SequenceReadStatus::Failed) {
        continue;
      }
      failedAssets.insert(item.first);
      lastFrameHasSequenceDecodeFailure = true;
      _sequenceCacheInvalidated = true;
      usage.queue->invalidateFailedRequest(usage.result->requestID, usage.result->targetFrame);
    }
  }
  for (auto assetID : failedAssets) {
    recordSequenceFailure(assetID);
  }
  for (auto assetID : succeededAssets) {
    if (failedAssets.count(assetID) == 0) {
      recordSequenceSuccess(assetID);
    }
  }
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
  usedStaticSequences = {};
  usedSequences = {};
  lastFrameHasSequenceDecodeFailure = false;
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
    staticSequenceResults.erase(assetID);
    usedStaticSequences.erase(assetID);
    clearSequenceCache(assetID);
  }
}

void RenderCache::releaseAll() {
  clearAllSnapshots();
  graphicsMemory = 0;
  clearAllSequenceCaches();
  filterResourcesMap.clear();
  contextID = 0;
}

void RenderCache::detachFromContext() {
  if (!isDrawingFrame) {
    context = nullptr;
    return;
  }
  recordPerformance();
  clearExpiredSequences();
  clearExpiredDecodedImages();
  clearExpiredSnapshots();
  if (!timestamps.empty()) {
    // Always purge recycled resources that haven't been used in 1 frame.
    context->purgeResourcesNotUsedSince(timestamps.back());
  }
  if (context->memoryUsage() + graphicsMemory > PURGEABLE_GRAPHICS_MEMORY &&
      timestamps.size() == PURGEABLE_EXPIRED_FRAME) {
    // Purge all types of resources that haven't been used in 10 frames when the total memory usage
    // is over 20M.
    context->purgeResourcesNotUsedSince(timestamps.front());
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

std::shared_ptr<tgfx::Image> RenderCache::applyAssetMipmaps(
    ID assetID, std::shared_ptr<tgfx::Image> image) {
  if (image != nullptr && stage->getAssetMinScale(assetID) < MIPMAP_ENABLED_THRESHOLD) {
    image = image->makeMipmapped(true);
  }
  return image;
}

std::shared_ptr<tgfx::Image> RenderCache::getAssetImageInternal(ID assetID,
                                                                const ImageProxy* proxy) {
  auto image = assetImages[assetID];
  if (image != nullptr) {
    return image;
  }
  image = applyAssetMipmaps(assetID, proxy->makeImage(this));
  if (image == nullptr) {
    return nullptr;
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

void RenderCache::prepareStaticSequenceImage(std::shared_ptr<SequenceInfo> sequence) {
  if (sequence == nullptr || (!_videoEnabled && sequence->isVideo())) {
    return;
  }
  auto assetID = sequence->uniqueID();
  if (!canRetrySequence(assetID)) {
    return;
  }
  usedAssets.insert(assetID);
  if (sequence->isVideo()) {
    staticVideoSequenceIDs.insert(assetID);
  }
  if (decodedAssetImages.count(assetID) != 0 || hasSnapshot(assetID)) {
    return;
  }
  auto image = assetImages[assetID];
  if (image == nullptr) {
    auto result = std::make_shared<SequenceReadResult>();
    result->requestID = 1;
    result->targetFrame = 0;
    image = sequence->makeStaticImage(getFileByAssetID(assetID), _useDiskCache, result);
    image = applyAssetMipmaps(assetID, std::move(image));
    if (image == nullptr) {
      result->status.store(SequenceReadStatus::Failed, std::memory_order_release);
      usedStaticSequences[assetID] = result;
      return;
    }
    assetImages[assetID] = image;
    staticSequenceResults[assetID] = result;
  }
  auto decodedImage = image->makeDecoded(context);
  if (decodedImage != image) {
    decodedAssetImages[assetID] = decodedImage;
  }
}

std::shared_ptr<tgfx::Image> RenderCache::getStaticSequenceImage(
    std::shared_ptr<SequenceInfo> sequence) {
  if (sequence == nullptr || (!_videoEnabled && sequence->isVideo())) {
    return nullptr;
  }
  auto assetID = sequence->uniqueID();
  if (!canRetrySequence(assetID)) {
    return nullptr;
  }
  usedAssets.insert(assetID);
  if (sequence->isVideo()) {
    staticVideoSequenceIDs.insert(assetID);
  }
  usedStaticSequences[assetID] = staticSequenceResults[assetID];
  auto result = decodedAssetImages.find(assetID);
  if (result != decodedAssetImages.end()) {
    auto decodedImage = result->second;
    decodedAssetImages.erase(result);
    return decodedImage;
  }
  auto image = assetImages[assetID];
  if (image != nullptr) {
    return image;
  }
  auto readResult = std::make_shared<SequenceReadResult>();
  readResult->requestID = 1;
  readResult->targetFrame = 0;
  image = sequence->makeStaticImage(getFileByAssetID(assetID), _useDiskCache, readResult);
  image = applyAssetMipmaps(assetID, std::move(image));
  if (image == nullptr) {
    readResult->status.store(SequenceReadStatus::Failed, std::memory_order_release);
    usedStaticSequences[assetID] = readResult;
    return nullptr;
  }
  assetImages[assetID] = image;
  staticSequenceResults[assetID] = readResult;
  usedStaticSequences[assetID] = readResult;
  return image;
}

void RenderCache::prepareSequenceImage(std::shared_ptr<SequenceInfo> sequence, Frame targetFrame) {
  if (sequence == nullptr || !canRetrySequence(sequence->uniqueID())) {
    return;
  }
  auto queue = getSequenceImageQueue(sequence, targetFrame);
  if (queue == nullptr) {
    return;
  }
  std::shared_ptr<SequenceReadResult> result = nullptr;
  queue->prepare(targetFrame, &result);
  usedSequences[sequence->uniqueID()][targetFrame].result = std::move(result);
}

std::shared_ptr<tgfx::Image> RenderCache::getSequenceImage(std::shared_ptr<SequenceInfo> sequence,
                                                           Frame targetFrame) {
  if (sequence == nullptr || !canRetrySequence(sequence->uniqueID())) {
    return nullptr;
  }
  auto queue = getSequenceImageQueue(sequence, targetFrame);
  if (queue == nullptr) {
    return nullptr;
  }
  std::shared_ptr<SequenceReadResult> result = nullptr;
  auto image = queue->getImage(targetFrame, &result);
  usedSequences[sequence->uniqueID()][targetFrame].result = std::move(result);
  // We don't check mipmaps for sequences since there is currently no backend support yet.
  return image;
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
    return result->second.queue;
  }
  auto queue = findNearestSequenceImageQueue(sequence, targetFrame);
  if (queue == nullptr) {
    queue = makeSequenceImageQueue(sequence);
  }
  if (queue != nullptr) {
    sequenceMap[targetFrame].queue = queue;
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
    usedQueues.insert(item.second.queue);
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

bool RenderCache::canRetrySequence(ID uniqueID) {
  auto result = sequenceFailureStates.find(uniqueID);
  if (result == sequenceFailureStates.end()) {
    return true;
  }
  if (tgfx::Clock::Now() >= result->second.retryAfterTime) {
    return true;
  }
  // The sequence is intentionally absent while cooling down. Keep the frame marked as failed so
  // PAGDecoder never persists an incomplete render into its disk sequence cache.
  lastFrameHasSequenceDecodeFailure = true;
  _sequenceCacheInvalidated = true;
  return false;
}

void RenderCache::recordSequenceFailure(ID uniqueID) {
  auto& state = sequenceFailureStates[uniqueID];
  auto shift = std::min<uint32_t>(state.consecutiveFailures, 5);
  state.consecutiveFailures++;
  auto delay = std::min<int64_t>(SEQUENCE_RETRY_INITIAL_DELAY << shift,
                                 SEQUENCE_RETRY_MAX_DELAY);
  state.retryAfterTime = tgfx::Clock::Now() + delay;
}

void RenderCache::recordSequenceSuccess(ID uniqueID) {
  sequenceFailureStates.erase(uniqueID);
}

void RenderCache::clearStaticSequenceCache(ID uniqueID) {
  removeSnapshot(uniqueID);
  assetImages.erase(uniqueID);
  decodedAssetImages.erase(uniqueID);
  staticSequenceResults.erase(uniqueID);
  usedStaticSequences.erase(uniqueID);
  staticVideoSequenceIDs.erase(uniqueID);
  sequenceFailureStates.erase(uniqueID);
}

void RenderCache::clearVideoSequenceCaches() {
  auto staticVideoIDs = staticVideoSequenceIDs;
  for (auto assetID : staticVideoIDs) {
    clearStaticSequenceCache(assetID);
  }
  for (auto result = sequenceCaches.begin(); result != sequenceCaches.end();) {
    auto& queues = result->second;
    if (queues.empty() || !queues.front()->sequence->isVideo()) {
      ++result;
      continue;
    }
    auto assetID = result->first;
    removeSnapshot(assetID);
    usedSequences.erase(assetID);
    sequenceFailureStates.erase(assetID);
    for (auto queue : queues) {
      delete queue;
    }
    result = sequenceCaches.erase(result);
  }
}

void RenderCache::clearAllSequenceCaches() {
  for (auto& item : staticSequenceResults) {
    removeSnapshot(item.first);
    assetImages.erase(item.first);
    decodedAssetImages.erase(item.first);
  }
  staticSequenceResults.clear();
  usedStaticSequences.clear();
  staticVideoSequenceIDs.clear();
  usedSequences.clear();
  sequenceFailureStates.clear();
  for (auto& item : sequenceCaches) {
    removeSnapshot(item.first);
    for (auto queue : item.second) {
      delete queue;
    }
  }
  sequenceCaches.clear();
}

void RenderCache::clearSequenceCache(ID uniqueID) {
  clearStaticSequenceCache(uniqueID);
  usedSequences.erase(uniqueID);
  sequenceFailureStates.erase(uniqueID);
  auto result = sequenceCaches.find(uniqueID);
  if (result != sequenceCaches.end()) {
    removeSnapshot(result->first);
    for (auto queue : result->second) {
      delete queue;
    }
    sequenceCaches.erase(result);
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

FilterResources* RenderCache::findFilterResources(ID type) {
  auto result = filterResourcesMap.find(type);
  if (result != filterResourcesMap.end()) {
    return result->second.get();
  }
  return nullptr;
}

void RenderCache::addFilterResources(ID type, std::unique_ptr<FilterResources> resources) {
  if (resources == nullptr) {
    return;
  }
  filterResourcesMap[type] = std::move(resources);
}
}  // namespace pag
