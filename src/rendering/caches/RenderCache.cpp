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
#include "rendering/renderers/FilterRenderer.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "rendering/video/VideoDecoder.h"
#include "tgfx/core/Clock.h"

#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/VideoSequenceReader.h"
#endif

namespace pag {
// 300M设置的大一些用于兜底，通常在大于20M时就开始随时清理。
#define MAX_GRAPHICS_MEMORY 314572800
#define PURGEABLE_GRAPHICS_MEMORY 20971520  // 20M
#define PURGEABLE_EXPIRED_FRAME 10
#define SCALE_FACTOR_PRECISION 0.001f

class ImageTask : public Executor {
 public:
  static std::shared_ptr<Task> MakeAndRun(std::shared_ptr<tgfx::ImageCodec> codec,
                                          std::shared_ptr<File> file) {
    if (codec == nullptr) {
      return nullptr;
    }
    auto bitmap = new ImageTask(std::move(codec), file);
    auto task = Task::Make(std::unique_ptr<ImageTask>(bitmap));
    task->run();
    return task;
  }

  std::shared_ptr<tgfx::ImageBuffer> getBuffer() const {
    return buffer;
  }

 private:
  std::shared_ptr<tgfx::ImageBuffer> buffer = {};
  std::shared_ptr<tgfx::ImageCodec> codec = nullptr;
  // Make a reference to file when image made from imageByte of file.
  std::shared_ptr<File> file = nullptr;

  explicit ImageTask(std::shared_ptr<tgfx::ImageCodec> codec, std::shared_ptr<File> file)
      : codec(std::move(codec)), file(std::move(file)) {
  }

  void execute() override {
    buffer = codec->makeBuffer();
  }
};

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

void RenderCache::prepareLayers(int64_t timeDistance) {
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
  auto timeScale = layer->containingComposition
                       ? (composition->frameRate / layer->containingComposition->frameRate)
                       : 1.0f;
  auto compositionFrame = static_cast<Frame>(
      roundf(static_cast<float>(layer->startTime - layer->compositionStartTime) * timeScale));
  if (compositionFrame < 0) {
    compositionFrame = 0;
  }
  auto assetID = composition->uniqueID;
  usedAssets.insert(assetID);
  if (composition->staticContent() && hasSnapshot(assetID)) {
    // 静态的序列帧采用位图的缓存逻辑，如果上层缓存过 Snapshot 就不需要预测。
    return;
  }
  auto sequence = Sequence::Get(composition);
  auto targetFrame = sequence->toSequenceFrame(compositionFrame);
  auto result = sequenceCaches.find(assetID);
  if (result != sequenceCaches.end()) {
    // 更新循环预测起点，激活预测。
    auto reader = result->second.front();
    reader->pendingFirstFrame = targetFrame;
    return;
  }
  auto reader = makeSequenceReader(sequence);
  if (reader) {
    reader->prepare(targetFrame);
  }
}

void RenderCache::prepareImageLayer(PAGImageLayer* pagLayer) {
  std::shared_ptr<Graphic> graphic = nullptr;
  auto pagImage = static_cast<PAGImageLayer*>(pagLayer)->getPAGImage();
  if (pagImage != nullptr) {
    graphic = pagImage->getGraphic();
  } else {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    graphic = ImageContentCache::GetGraphic(imageBytes);
  }
  if (graphic) {
    graphic->prepare(this);
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
  usedSequences = {};
  resetPerformance();
}

void RenderCache::attachToContext(tgfx::Context* current, bool forHitTest) {
  if (deviceID > 0 && deviceID != current->device()->uniqueID()) {
    // Context 改变需要清理内部所有缓存，这里用 uniqueID
    // 而不用指针比较，是因为指针析构后再创建可能会地址重合。
    releaseAll();
  }
  context = current;
  deviceID = context->device()->uniqueID();
  hitTestOnly = forHitTest;
  if (hitTestOnly) {
    return;
  }
  auto removedAssets = stage->getRemovedAssets();
  for (auto assetID : removedAssets) {
    removeSnapshot(assetID);
    removePathSnapshots(assetID);
    imageTasks.erase(assetID);
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
  deviceID = 0;
}

void RenderCache::detachFromContext() {
  if (hitTestOnly) {
    context = nullptr;
    return;
  }
  clearExpiredSequences();
  clearExpiredBitmaps();
  clearExpiredSnapshots();
  auto currentTimestamp = tgfx::Clock::Now();
  context->purgeResourcesNotUsedIn(currentTimestamp - lastTimestamp);
  lastTimestamp = currentTimestamp;
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

Snapshot* RenderCache::makeSnapshot(float scaleFactor, const std::function<Snapshot*()>& maker) {
  if (scaleFactor < SCALE_FACTOR_PRECISION || graphicsMemory >= MAX_GRAPHICS_MEMORY) {
    return nullptr;
  }
  auto snapshot = maker();
  if (snapshot == nullptr) {
    return nullptr;
  }
  graphicsMemory += snapshot->memoryUsage();
  snapshotLRU.push_front(snapshot);
  return snapshot;
}

Snapshot* RenderCache::getSnapshot(const Picture* image) {
  if (!_snapshotEnabled) {
    return nullptr;
  }
  usedAssets.insert(image->assetID);
  auto maxScaleFactor = stage->getAssetMaxScale(image->assetID);
  auto scaleFactor = image->getScaleFactor(maxScaleFactor);
  auto snapshot = getSnapshot(image->assetID);
  if (snapshot && (snapshot->makerKey != image->uniqueKey ||
                   fabsf(snapshot->scaleFactor() - scaleFactor) > SCALE_FACTOR_PRECISION)) {
    removeSnapshot(image->assetID);
    snapshot = nullptr;
  }
  if (snapshot) {
    moveSnapshotToHead(snapshot);
    return snapshot;
  }
  snapshot =
      makeSnapshot(scaleFactor, [&]() { return image->makeSnapshot(this, scaleFactor).release(); });
  if (snapshot == nullptr) {
    return nullptr;
  }
  snapshot->assetID = image->assetID;
  snapshot->makerKey = image->uniqueKey;
  snapshotCaches[image->assetID] = snapshot;
  return snapshot;
}

Snapshot* RenderCache::getSnapshot(ID assetID, const tgfx::Path& path) const {
  if (!_snapshotEnabled) {
    return nullptr;
  }
  auto result = pathCaches.find(assetID);
  if (result != pathCaches.end()) {
    auto iter = result->second.find(path);
    if (iter != result->second.end()) {
      return iter->second;
    }
  }
  return nullptr;
}

Snapshot* RenderCache::getSnapshot(const Shape* shape) {
  if (!_snapshotEnabled) {
    return nullptr;
  }
  usedAssets.insert(shape->assetID);
  auto scaleFactor = stage->getAssetMaxScale(shape->assetID);
  auto snapshot = getSnapshot(shape->assetID, shape->path);
  if (snapshot && fabsf(snapshot->scaleFactor() - scaleFactor) > SCALE_FACTOR_PRECISION) {
    removeSnapshot(shape->assetID, shape->path);
    snapshot = nullptr;
  }
  if (snapshot) {
    moveSnapshotToHead(snapshot);
    return snapshot;
  }
  snapshot =
      makeSnapshot(scaleFactor, [&]() { return shape->makeSnapshot(this, scaleFactor).release(); });
  if (snapshot == nullptr) {
    return nullptr;
  }
  snapshot->assetID = shape->assetID;
  snapshot->path = shape->path;
  pathCaches[shape->assetID][shape->path] = snapshot;
  return snapshot;
}

void RenderCache::removeSnapshot(ID assetID, const tgfx::Path& path) {
  auto snapshotCache = pathCaches.find(assetID);
  if (snapshotCache == pathCaches.end()) {
    return;
  }
  auto snapshot = snapshotCache->second.find(path);
  if (snapshot == snapshotCache->second.end()) {
    return;
  }
  removeSnapshotFromLRU(snapshot->second);
  graphicsMemory -= snapshot->second->memoryUsage();
  delete snapshot->second;
  snapshotCache->second.erase(snapshot);
  if (snapshotCache->second.empty()) {
    pathCaches.erase(assetID);
  }
}

void RenderCache::removePathSnapshots(ID assetID) {
  auto snapshotCache = pathCaches.find(assetID);
  if (snapshotCache != pathCaches.end()) {
    for (const auto& pair : snapshotCache->second) {
      removeSnapshotFromLRU(pair.second);
      graphicsMemory -= pair.second->memoryUsage();
      delete pair.second;
    }
    pathCaches.erase(assetID);
  }
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
}

void RenderCache::removeSnapshotFromLRU(Snapshot* snapshot) {
  auto position = std::find(snapshotLRU.begin(), snapshotLRU.end(), snapshot);
  if (position != snapshotLRU.end()) {
    snapshotLRU.erase(position);
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
  for (auto& item : pathCaches) {
    for (auto& snapshot : item.second) {
      graphicsMemory -= snapshot.second->memoryUsage();
      delete snapshot.second;
    }
  }
  pathCaches.clear();
  snapshotLRU.clear();
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
        graphicsMemory + releaseMemory < PURGEABLE_GRAPHICS_MEMORY) {
      // 总显存占用未超过20M且所有缓存均未超过10帧未使用，跳过清理。
      continue;
    }
    releaseMemory += snapshot->memoryUsage();
    expiredSnapshots.push_back(snapshot);
  }
  for (const auto& snapshot : expiredSnapshots) {
    if (snapshot->path.isEmpty()) {
      removeSnapshot(snapshot->assetID);
    } else {
      removeSnapshot(snapshot->assetID, snapshot->path);
    }
  }
}

void RenderCache::prepareImage(ID assetID, std::shared_ptr<tgfx::ImageCodec> codec) {
  usedAssets.insert(assetID);
  if (imageTasks.count(assetID) != 0 || snapshotCaches.count(assetID) != 0) {
    return;
  }
  auto layer = stage->getLayerFromReferenceMap(assetID);
  auto task = ImageTask::MakeAndRun(std::move(codec), layer->getFile());
  if (task) {
    imageTasks[assetID] = task;
  }
}

std::shared_ptr<tgfx::ImageBuffer> RenderCache::getImageBuffer(ID assetID) {
  usedAssets.insert(assetID);
  auto result = imageTasks.find(assetID);
  if (result != imageTasks.end()) {
    auto executor = result->second->wait();
    auto buffer = static_cast<ImageTask*>(executor)->getBuffer();
    // 预测生成的 Bitmap 取了一次就应该销毁，上层会进行缓存。
    imageTasks.erase(result);
    return buffer;
  }
  return {};
}

void RenderCache::clearExpiredBitmaps() {
  std::vector<ID> expiredBitmaps = {};
  for (auto& item : imageTasks) {
    if (usedAssets.count(item.first) == 0) {
      expiredBitmaps.push_back(item.first);
    }
  }
  for (auto& bitmapID : expiredBitmaps) {
    imageTasks.erase(bitmapID);
  }
}

//===================================== sequence caches =====================================

void RenderCache::prepareSequence(Sequence* sequence, Frame targetFrame) {
  if (sequence == nullptr) {
    return;
  }
  auto composition = sequence->composition;
  if (composition->staticContent() && hasSnapshot(composition->uniqueID)) {
    // 静态的序列帧采用位图的缓存逻辑，如果上层缓存过 Snapshot 就不需要预测。
    return;
  }
  auto reader = getSequenceReader(sequence, targetFrame);
  if (reader) {
    reader->prepare(targetFrame);
  }
}

std::shared_ptr<tgfx::Texture> RenderCache::getSequenceFrame(Sequence* sequence,
                                                             Frame targetFrame) {
  if (sequence == nullptr) {
    return nullptr;
  }
  auto reader = getSequenceReader(sequence, targetFrame);
  if (reader == nullptr) {
    return nullptr;
  }
  auto composition = sequence->composition;
  auto texture = reader->readTexture(targetFrame, this);
  if (composition->staticContent()) {
    // There is no need to cache a reader for the static sequence, it has already been cached as
    // a snapshot. We get here because the reader was created by prepare() methods.
    auto result = sequenceCaches.find(composition->uniqueID);
    if (result != sequenceCaches.end()) {
      for (auto item : result->second) {
        delete item;
      }
      sequenceCaches.erase(result);
    }
  }
  return texture;
}

SequenceReader* RenderCache::getSequenceReader(Sequence* sequence, Frame targetFrame) {
  if (sequence->composition->staticContent()) {
    targetFrame = 0;
  }
  auto assetID = sequence->composition->uniqueID;
  usedAssets.insert(assetID);
  auto& sequenceMap = usedSequences[assetID];
  auto readerResult = sequenceMap.find(targetFrame);
  if (readerResult != sequenceMap.end()) {
    return readerResult->second;
  }
  auto reader = findNearestSequenceReader(sequence, targetFrame);
  if (reader == nullptr) {
    reader = makeSequenceReader(sequence);
  }
  if (reader != nullptr) {
    sequenceMap[targetFrame] = reader;
  }
  return reader;
}

SequenceReader* RenderCache::findNearestSequenceReader(Sequence* sequence, Frame targetFrame) {
  auto assetID = sequence->composition->uniqueID;
  auto result = sequenceCaches.find(assetID);
  if (result == sequenceCaches.end()) {
    return nullptr;
  }
  auto& sequenceMap = usedSequences[assetID];
  std::unordered_set<SequenceReader*> usedReaders = {};
  for (auto& item : sequenceMap) {
    usedReaders.insert(item.second);
  }
  std::vector<SequenceReader*> freeReaders = {};
  for (auto& item : result->second) {
    if (usedReaders.count(item) == 0) {
      freeReaders.push_back(item);
    }
  }
  if (freeReaders.empty()) {
    return nullptr;
  }
  SequenceReader* reader = nullptr;
  Frame minDistance = INT64_MAX;
  for (auto& item : freeReaders) {
    if (item->lastFrame == targetFrame) {
      // use the last cached texture directly.
      reader = item;
      break;
    }
    auto distance = targetFrame - item->preparedFrame;
    if (distance < 0) {
      distance += sequence->duration();
    }
    if (distance >= 0 && distance < minDistance) {
      reader = item;
      minDistance = distance;
    }
  }
  if (reader == nullptr) {
    reader = freeReaders.front();
  }
  return reader;
}

SequenceReader* RenderCache::makeSequenceReader(Sequence* sequence) {
  SequenceReader* reader = nullptr;
  auto composition = sequence->composition;
  if (!_videoEnabled && composition->type() == CompositionType::Video) {
    return reader;
  }
  auto layer = stage->getLayerFromReferenceMap(composition->uniqueID);
  if (composition->type() == CompositionType::Bitmap) {
    reader = new BitmapSequenceReader(layer->getFile(), static_cast<BitmapSequence*>(sequence));
  } else {
    auto videoSequence = static_cast<VideoSequence*>(sequence);
#ifdef PAG_BUILD_FOR_WEB
    if (VideoDecoder::HasExternalSoftwareDecoder()) {
      auto demuxer =
          std::make_unique<VideoSequenceDemuxer>(layer->rootFile->getFile(), videoSequence);
      reader = new VideoReader(std::move(demuxer));
    } else {
      reader = new VideoSequenceReader(layer, videoSequence);
    }
#else
    auto demuxer = std::make_unique<VideoSequenceDemuxer>(layer->getFile(), videoSequence);
    reader = new VideoReader(std::move(demuxer));
#endif
  }
  auto assetID = sequence->composition->uniqueID;
  auto result = sequenceCaches.find(assetID);
  if (result == sequenceCaches.end()) {
    sequenceCaches[assetID] = {reader};
  } else {
    result->second.push_back(reader);
  }
  return reader;
}

void RenderCache::clearAllSequenceCaches() {
  for (auto& item : sequenceCaches) {
    removeSnapshot(item.first);
    for (auto reader : item.second) {
      delete reader;
    }
  }
  sequenceCaches.clear();
}

void RenderCache::clearSequenceCache(ID uniqueID) {
  auto result = sequenceCaches.find(uniqueID);
  if (result != sequenceCaches.end()) {
    removeSnapshot(result->first);
    for (auto reader : result->second) {
      delete reader;
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

Transform3DFilter* RenderCache::getTransform3DFilter() {
  if (transform3DFilter == nullptr) {
    transform3DFilter = new Transform3DFilter();
    if (!initFilter(transform3DFilter)) {
      delete transform3DFilter;
      transform3DFilter = nullptr;
    }
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
