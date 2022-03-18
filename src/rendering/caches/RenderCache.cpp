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
#include <map>
#include "base/utils/TimeUtil.h"
#include "base/utils/USE.h"
#include "base/utils/UniqueID.h"
#include "rendering/caches/ImageContentCache.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/renderers/FilterRenderer.h"

#ifdef FILE_MOVIE

#include "rendering/readers/MovieReader.h"

#endif

namespace pag {
// 300M设置的大一些用于兜底，通常在大于20M时就开始随时清理。
#define MAX_GRAPHICS_MEMORY 314572800
#define PURGEABLE_GRAPHICS_MEMORY 20971520  // 20M
#define PURGEABLE_EXPIRED_FRAME 10
#define SCALE_FACTOR_PRECISION 0.001f
#define DECODING_VISIBLE_DISTANCE 500000  // 提前 500ms 秒开始解码。
#define MIN_HARDWARE_PREPARE_TIME 100000  // 距离当前时刻小于100ms的视频启动软解转硬解优化。

class ImageTask : public Executor {
 public:
  static std::shared_ptr<Task> MakeAndRun(std::shared_ptr<tgfx::Image> image,
                                          std::shared_ptr<File> file) {
    if (image == nullptr) {
      return nullptr;
    }
    auto bitmap = new ImageTask(std::move(image), file);
    auto task = Task::Make(std::unique_ptr<ImageTask>(bitmap));
    task->run();
    return task;
  }

  std::shared_ptr<tgfx::TextureBuffer> getBuffer() const {
    return buffer;
  }

 private:
  std::shared_ptr<tgfx::TextureBuffer> buffer = {};
  std::shared_ptr<tgfx::Image> image = nullptr;
  // Make a reference to file when image made from imageByte of file.
  std::shared_ptr<File> file = nullptr;

  explicit ImageTask(std::shared_ptr<tgfx::Image> image, std::shared_ptr<File> file)
      : image(std::move(image)), file(std::move(file)) {
  }

  void execute() override {
    buffer = image->makeBuffer();
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
  auto startTime = GetTimer();
  auto result = filter->initialize(getContext());
  programCompilingTime += GetTimer() - startTime;
  return result;
}

void RenderCache::preparePreComposeLayer(PreComposeLayer* layer, DecodingPolicy policy) {
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
  auto sequence = Sequence::Get(composition);
  auto sequenceFrame = sequence->toSequenceFrame(compositionFrame);
  if (prepareSequenceReader(sequence, sequenceFrame, policy)) {
    return;
  }
  auto result = sequenceCaches.find(composition->uniqueID);
  if (result != sequenceCaches.end()) {
    // 循环预测
    result->second->prepareAsync(sequenceFrame);
  }
}

void RenderCache::prepareImageLayer(PAGImageLayer* pagLayer, DecodingPolicy policy) {
  auto pagImage = static_cast<PAGImageLayer*>(pagLayer)->getPAGImage();
  if (pagImage == nullptr) {
    auto imageBytes = static_cast<ImageLayer*>(pagLayer->layer)->imageBytes;
    auto image = ImageContentCache::GetImage(imageBytes);
    if (image) {
      prepareImage(imageBytes->uniqueID, image);
    }
    return;
  }
  if (pagImage->isMovie()) {
#ifdef FILE_MOVIE
    auto movie = static_cast<PAGMovie*>(pagImage.get());
    if (movie->isFile()) {
      auto fileMovie = static_cast<FileMovie*>(movie);
      auto prepared = prepareMovieReader(fileMovie->uniqueID(), fileMovie->getInfo(),
                                         fileMovie->startTime, policy);
      if (!prepared) {
        auto result = movieCaches.find(fileMovie->uniqueID());
        if (result != movieCaches.end()) {
          result->second->prepareAsync(fileMovie->startTime);
        }
      }
    }
#else
    USE(policy);
#endif
  } else {
    auto image = pagImage->getImage();
    if (image) {
      prepareImage(pagImage->uniqueID(), image);
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

void RenderCache::prepareFrame() {
  usedAssets = {};
  resetPerformance();
  auto layerDistances = stage->findNearlyVisibleLayersIn(DECODING_VISIBLE_DISTANCE);
  for (auto& item : layerDistances) {
    auto policy = SoftwareToHardwareEnabled() && item.first < MIN_HARDWARE_PREPARE_TIME
                      ? DecodingPolicy::SoftwareToHardware
                      : DecodingPolicy::Hardware;
    for (auto pagLayer : item.second) {
      if (pagLayer->layerType() == LayerType::PreCompose) {
        preparePreComposeLayer(static_cast<PreComposeLayer*>(pagLayer->layer), policy);
      } else if (pagLayer->layerType() == LayerType::Image) {
        prepareImageLayer(static_cast<PAGImageLayer*>(pagLayer), policy);
      }
    }
  }
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
    imageTasks.erase(assetID);
    clearSequenceCache(assetID);
    clearFilterCache(assetID);
    removeTextAtlas(assetID);
#ifdef FILE_MOVIE
    clearMovieCache(assetID);
#endif
  }
}

void RenderCache::releaseAll() {
  clearAllSnapshots();
  clearAllTextAtlas();
  graphicsMemory = 0;
  clearAllSequenceCaches();
#ifdef FILE_MOVIE
  clearAllMovieCaches();
#endif
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
#ifdef FILE_MOVIE
  clearExpiredMovieCaches();
#endif
  clearExpiredBitmaps();
  clearExpiredSnapshots();
  auto currentTimestamp = GetTimer();
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
    snapshot->idleFrames = 0;
    auto position = std::find(snapshotLRU.begin(), snapshotLRU.end(), snapshot);
    if (position != snapshotLRU.end()) {
      snapshotLRU.erase(position);
    }
    snapshotLRU.push_front(snapshot);
    return snapshot;
  }
  if (scaleFactor < SCALE_FACTOR_PRECISION || graphicsMemory >= MAX_GRAPHICS_MEMORY) {
    return nullptr;
  }
  auto newSnapshot = image->makeSnapshot(this, scaleFactor);
  if (newSnapshot == nullptr) {
    return nullptr;
  }
  snapshot = newSnapshot.release();
  snapshot->assetID = image->assetID;
  snapshot->makerKey = image->uniqueKey;
  graphicsMemory += snapshot->memoryUsage();
  snapshotLRU.push_front(snapshot);
  snapshotCaches[image->assetID] = snapshot;
  return snapshot;
}

void RenderCache::removeSnapshot(ID assetID) {
  auto snapshot = snapshotCaches.find(assetID);
  if (snapshot == snapshotCaches.end()) {
    return;
  }
  auto position = std::find(snapshotLRU.begin(), snapshotLRU.end(), snapshot->second);
  if (position != snapshotLRU.end()) {
    snapshotLRU.erase(position);
  }
  graphicsMemory -= snapshot->second->memoryUsage();
  delete snapshot->second;
  snapshotCaches.erase(assetID);
}

TextAtlas* RenderCache::getTextAtlas(ID assetID) const {
  auto textAtlas = textAtlases.find(assetID);
  if (textAtlas == textAtlases.end()) {
    return nullptr;
  }
  return textAtlas->second;
}

TextAtlas* RenderCache::getTextAtlas(const TextGlyphs* textGlyphs) {
  auto maxScaleFactor = stage->getAssetMaxScale(textGlyphs->assetID());
  auto textAtlas = getTextAtlas(textGlyphs->assetID());
  if (textAtlas && (textAtlas->textGlyphsID() != textGlyphs->id() ||
                    fabsf(textAtlas->scaleFactor() - maxScaleFactor) > SCALE_FACTOR_PRECISION)) {
    removeTextAtlas(textGlyphs->assetID());
    textAtlas = nullptr;
  }
  if (textAtlas) {
    return textAtlas;
  }
  if (maxScaleFactor < SCALE_FACTOR_PRECISION) {
    return nullptr;
  }
  textAtlas = TextAtlas::Make(textGlyphs, this, maxScaleFactor).release();
  if (textAtlas) {
    graphicsMemory += textAtlas->memoryUsage();
    textAtlases[textGlyphs->assetID()] = textAtlas;
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
}

void RenderCache::clearExpiredSnapshots() {
  while (!snapshotLRU.empty()) {
    auto snapshot = snapshotLRU.back();
    // 只有 Snapshot 数量可能会比较多，使用 LRU
    // 来避免遍历完整的列表，遇到第一个用过的就可以取消遍历。
    if (usedAssets.count((snapshot->assetID) > 0)) {
      break;
    }
    snapshot->idleFrames++;
    if (snapshot->idleFrames < PURGEABLE_EXPIRED_FRAME &&
        graphicsMemory < PURGEABLE_GRAPHICS_MEMORY) {
      // 总显存占用未超过20M且所有缓存均未超过10帧未使用，跳过清理。
      break;
    }
    removeSnapshot(snapshot->assetID);
  }
}

void RenderCache::prepareImage(ID assetID, std::shared_ptr<tgfx::Image> image) {
  usedAssets.insert(assetID);
  if (imageTasks.count(assetID) != 0 || snapshotCaches.count(assetID) != 0) {
    return;
  }
  auto task = ImageTask::MakeAndRun(std::move(image), stage->getFileFromReferenceMap(assetID));
  if (task) {
    imageTasks[assetID] = task;
  }
}

std::shared_ptr<tgfx::TextureBuffer> RenderCache::getImageBuffer(ID assetID) {
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

static std::shared_ptr<SequenceReader> MakeSequenceReader(std::shared_ptr<File> file,
                                                          Sequence* sequence,
                                                          DecodingPolicy policy) {
  std::shared_ptr<SequenceReader> reader = nullptr;
  if (sequence->composition->type() == CompositionType::Video) {
    if (sequence->composition->staticContent()) {
      // 全静态的序列帧强制软件解码。
      policy = DecodingPolicy::Software;
    }
    reader = SequenceReader::Make(std::move(file), static_cast<VideoSequence*>(sequence), policy);
  } else {
    reader = std::make_shared<BitmapSequenceReader>(std::move(file),
                                                    static_cast<BitmapSequence*>(sequence));
  }
  return reader;
}

//===================================== sequence caches =====================================

bool RenderCache::prepareSequenceReader(Sequence* sequence, Frame targetFrame,
                                        DecodingPolicy policy) {
  auto composition = sequence->composition;
  if (!_videoEnabled && composition->type() == CompositionType::Video) {
    return false;
  }
  usedAssets.insert(composition->uniqueID);
  auto staticComposition = composition->staticContent();
  if (sequenceCaches.count(composition->uniqueID) != 0) {
#ifdef PAG_BUILD_FOR_WEB
    sequenceCaches[composition->uniqueID]->prepareAsync(targetFrame);
#endif
    return false;
  }
  if (staticComposition && hasSnapshot(composition->uniqueID)) {
    // 静态的序列帧采用位图的缓存逻辑，如果上层缓存过 Snapshot 就不需要预测。
    return false;
  }
  auto file = stage->getFileFromReferenceMap(composition->uniqueID);
  auto reader = MakeSequenceReader(file, sequence, policy);
  sequenceCaches[composition->uniqueID] = reader;
  reader->prepareAsync(targetFrame);
  return true;
}

std::shared_ptr<SequenceReader> RenderCache::getSequenceReader(Sequence* sequence) {
  if (sequence == nullptr) {
    return nullptr;
  }
  auto composition = sequence->composition;
  if (!_videoEnabled && composition->type() == CompositionType::Video) {
    return nullptr;
  }
  auto compositionID = composition->uniqueID;
  usedAssets.insert(compositionID);
  auto staticComposition = sequence->composition->staticContent();
  std::shared_ptr<SequenceReader> reader = nullptr;
  auto result = sequenceCaches.find(compositionID);
  if (result != sequenceCaches.end()) {
    reader = result->second;
    if (reader->getSequence() != sequence) {
      clearSequenceCache(compositionID);
      reader = nullptr;
    } else if (staticComposition) {
      // 完全静态的序列帧是预测生成的，第一次访问时就可以移除，上层会进行缓存。
      sequenceCaches.erase(result);
    }
  }
  if (reader == nullptr) {
    auto file = stage->getFileFromReferenceMap(composition->uniqueID);
    reader = MakeSequenceReader(file, sequence, DecodingPolicy::SoftwareToHardware);
    if (reader && !staticComposition) {
      // 完全静态的序列帧不用缓存。
      sequenceCaches[compositionID] = reader;
    }
  }
  return reader;
}

void RenderCache::clearAllSequenceCaches() {
  for (auto& item : sequenceCaches) {
    removeSnapshot(item.first);
  }
  sequenceCaches.clear();
}

void RenderCache::clearSequenceCache(ID uniqueID) {
  auto result = sequenceCaches.find(uniqueID);
  if (result != sequenceCaches.end()) {
    removeSnapshot(result->first);
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

#ifdef FILE_MOVIE

bool RenderCache::prepareMovieReader(ID movieID, MovieInfo* movieInfo, int64_t targetTime,
                                     DecodingPolicy policy) {
  usedAssets.insert(movieID);
  if (movieCaches.count(movieID) != 0) {
    return false;
  }
  auto reader = std::make_shared<MovieReader>(movieInfo, policy);
  movieCaches[movieID] = reader;
  reader->prepareAsync(targetTime);
  return true;
}

std::shared_ptr<MovieReader> RenderCache::getMovieReader(ID movieID, MovieInfo* movieInfo) {
  if (movieInfo == nullptr) {
    return nullptr;
  }
  usedAssets.insert(movieID);
  std::shared_ptr<MovieReader> reader = nullptr;
  auto result = movieCaches.find(movieID);
  if (result != movieCaches.end()) {
    reader = result->second;
  } else {
    reader = std::make_shared<MovieReader>(movieInfo, DecodingPolicy::Hardware);
    movieCaches[movieID] = reader;
  }
  return reader;
}

void RenderCache::clearAllMovieCaches() {
  for (auto& item : movieCaches) {
    removeSnapshot(item.first);
  }
  movieCaches.clear();
}

void RenderCache::clearMovieCache(ID movieID) {
  auto result = movieCaches.find(movieID);
  if (result != movieCaches.end()) {
    removeSnapshot(result->first);
    movieCaches.erase(result);
  }
}

void RenderCache::clearExpiredMovieCaches() {
  std::vector<ID> expiredMovies = {};
  for (auto& item : movieCaches) {
    if (usedAssets.count(item.first) == 0) {
      expiredMovies.push_back(item.first);
    }
  }
  for (auto& id : expiredMovies) {
    clearMovieCache(id);
  }
}

#endif

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
