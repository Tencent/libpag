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

#include "base/utils/MatrixUtil.h"
#include "base/utils/TimeUtil.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/caches/CompositionCache.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/renderers/LayerRenderer.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/ScopedLock.h"

namespace pag {
std::shared_ptr<PAGComposition> PAGComposition::Make(int width, int height) {
  auto pagComposition = std::shared_ptr<PAGComposition>(new PAGComposition(width, height));
  pagComposition->weakThis = pagComposition;
  return pagComposition;
}

PAGComposition::PAGComposition(int width, int height)
    : PAGLayer(nullptr, nullptr), _width(width), _height(height) {
  emptyComposition = new VectorComposition();
  emptyComposition->width = width;
  emptyComposition->height = height;
  emptyComposition->duration = INT64_MAX;
  layer = PreComposeLayer::Wrap(emptyComposition).release();
  layerCache = LayerCache::Get(layer);
  rootLocker = std::make_shared<std::mutex>();
  contentVersion = 1;
}

PAGComposition::PAGComposition(std::shared_ptr<File> file, PreComposeLayer* layer)
    : PAGLayer(file, layer) {
  if (layer != nullptr) {
    _width = layer->composition->width;
    _height = layer->composition->height;
    if (file != nullptr) {
      _frameRate = file->frameRate();
    }
    _frameDuration = layer->duration;
  }
}

PAGComposition::~PAGComposition() {
  removeAllLayers();
  if (emptyComposition) {
    delete emptyComposition;  // created by PAGComposition(width, height).
    delete layer;             // created by PAGComposition(width, height).
  }
}

int PAGComposition::width() const {
  LockGuard autoLock(rootLocker);
  return _width;
}

int PAGComposition::height() const {
  LockGuard autoLock(rootLocker);
  return _height;
}

void PAGComposition::setContentSize(int width, int height) {
  LockGuard autoLock(rootLocker);
  setContentSizeInternal(width, height);
}

int PAGComposition::widthInternal() const {
  return _width;
}

int PAGComposition::heightInternal() const {
  return _height;
}

void PAGComposition::setContentSizeInternal(int width, int height) {
  if (_width == width && _height == height) {
    return;
  }
  _width = width;
  _height = height;
  notifyModified(true);
}

int PAGComposition::numChildren() const {
  LockGuard autoLock(rootLocker);
  return static_cast<int>(layers.size());
}

std::shared_ptr<PAGLayer> PAGComposition::getLayerAt(int index) const {
  LockGuard autoLock(rootLocker);
  if (index >= 0 && static_cast<size_t>(index) < layers.size()) {
    return layers[index];
  }
  LOGE("An index specified for a parameter was out of range.");
  return nullptr;
}

int PAGComposition::getLayerIndexInternal(std::shared_ptr<PAGLayer> child) const {
  int index = 0;
  for (auto& layer : layers) {
    if (layer == child) {
      return index;
    }
    index++;
  }
  return -1;
}

int PAGComposition::getLayerIndex(std::shared_ptr<PAGLayer> pagLayer) const {
  LockGuard autoLock(rootLocker);
  return getLayerIndexInternal(pagLayer);
}

void PAGComposition::setLayerIndex(std::shared_ptr<PAGLayer> pagLayer, int index) {
  LockGuard autoLock(rootLocker);
  doSetLayerIndex(pagLayer, index);
}

void PAGComposition::doSetLayerIndex(std::shared_ptr<pag::PAGLayer> pagLayer, int index) {
  if (index < 0 || static_cast<size_t>(index) >= layers.size()) {
    index = static_cast<int>(layers.size()) - 1;
  }
  auto oldIndex = getLayerIndexInternal(pagLayer);
  if (oldIndex < 0) {
    LOGE("The supplied layer must be a child layer of the caller.");
    return;
  }
  if (oldIndex == index) {
    return;
  }
  layers.erase(layers.begin() + oldIndex);
  layers.insert(layers.begin() + index, pagLayer);
  notifyModified(true);
}

bool PAGComposition::addLayer(std::shared_ptr<PAGLayer> pagLayer) {
  if (pagLayer == nullptr) {
    return false;
  }
  ScopedLock autoLock(rootLocker, pagLayer->rootLocker);
  auto index = layers.size();
  if (pagLayer->_parent == this) {
    index--;
  }
  return doAddLayer(pagLayer, static_cast<int>(index));
}

bool PAGComposition::addLayerAt(std::shared_ptr<PAGLayer> pagLayer, int index) {
  if (pagLayer == nullptr) {
    return false;
  }
  ScopedLock autoLock(rootLocker, pagLayer->rootLocker);
  if (index < 0 || static_cast<size_t>(index) >= layers.size()) {
    index = static_cast<int>(layers.size());
    if (pagLayer->_parent == this) {
      index--;
    }
  }
  return doAddLayer(pagLayer, static_cast<int>(index));
}

bool PAGComposition::doAddLayer(std::shared_ptr<PAGLayer> pagLayer, int index) {
  if (pagLayer.get() == this) {
    LOGE("A layer cannot be added as a child of itself.");
    return false;
  } else if (pagLayer->layerType() == LayerType::PreCompose &&
             std::static_pointer_cast<PAGComposition>(pagLayer)->doContains(this)) {
    LOGE(
        "A layer cannot be added as a child to one of its children "
        "(or children's children, etc.).");
    return false;
  } else if (pagLayer->stage == pagLayer.get()) {
    LOGE("A stage cannot be added as a child to a layer.");
    return false;
  }
  auto oldParent = pagLayer->_parent;
  if (oldParent == this) {
    doSetLayerIndex(pagLayer, index);
    return true;
  }
  pagLayer->removeFromParentOrOwner();
  pagLayer->attachToTree(rootLocker, stage);
  if (rootFile && file == pagLayer->file) {
    pagLayer->onAddToRootFile(rootFile);
  }
  this->layers.insert(this->layers.begin() + index, pagLayer);
  pagLayer->_parent = this;
  notifyModified(true);
  if (emptyComposition) {
    updateDurationAndFrameRate();
  }
  return true;
}

bool PAGComposition::contains(std::shared_ptr<PAGLayer> pagLayer) const {
  if (pagLayer == nullptr) {
    return false;
  }
  ScopedLock autoLock(rootLocker, pagLayer->rootLocker);
  return doContains(pagLayer.get());
}

bool PAGComposition::doContains(pag::PAGLayer* target) const {
  while (target) {
    if (target == this) {
      return true;
    }
    target = target->_parent;
  }
  return false;
}

std::shared_ptr<PAGLayer> PAGComposition::removeLayer(std::shared_ptr<PAGLayer> pagLayer) {
  LockGuard autoLock(rootLocker);
  auto index = getLayerIndexInternal(pagLayer);
  if (index < 0) {
    LOGE("The supplied layer must be a child of the caller.");
    return nullptr;
  }
  return doRemoveLayer(index);
}

std::shared_ptr<PAGLayer> PAGComposition::removeLayerAt(int index) {
  LockGuard autoLock(rootLocker);
  if (index < 0 || static_cast<size_t>(index) >= layers.size()) {
    LOGE("An index specified for a parameter was out of range.");
    return nullptr;
  }
  return doRemoveLayer(index);
}

std::shared_ptr<PAGLayer> PAGComposition::doRemoveLayer(int index) {
  auto layer = layers[index];
  if (rootFile && file == layer->file) {
    layer->onRemoveFromRootFile();
  }
  layer->_parent = nullptr;
  layers.erase(layers.begin() + index);
  notifyModified(true);
  if (emptyComposition) {
    updateDurationAndFrameRate();
  }
  layer->detachFromTree();
  return layer;
}

void PAGComposition::removeAllLayers() {
  LockGuard autoLock(rootLocker);
  for (int i = static_cast<int>(layers.size() - 1); i >= 0; i--) {
    doRemoveLayer(i);
  }
}

void PAGComposition::swapLayer(std::shared_ptr<PAGLayer> pagLayer1,
                               std::shared_ptr<PAGLayer> pagLayer2) {
  LockGuard autoLock(rootLocker);
  auto index1 = getLayerIndexInternal(pagLayer1);
  auto index2 = getLayerIndexInternal(pagLayer2);
  if (index1 == -1 || index2 == -1) {
    LOGE("The supplied layer must be a child of the caller.");
    return;
  }
  doSwapLayerAt(index1, index2);
}

void PAGComposition::swapLayerAt(int index1, int index2) {
  LockGuard autoLock(rootLocker);
  auto size = this->layers.size();
  if (index1 >= 0 && static_cast<size_t>(index1) < size && index2 >= 0 &&
      static_cast<size_t>(index2) < size) {
    doSwapLayerAt(index1, index2);
  } else {
    LOGE("An index specified for a parameter was out of range.");
    return;
  }
}

void PAGComposition::doSwapLayerAt(int index1, int index2) {
  if (index1 > index2) {
    int temp = index2;
    index2 = index1;
    index1 = temp;
  } else if (index1 == index2) {
    return;
  }
  auto layer1 = layers[index1];
  auto layer2 = layers[index2];
  layers[index1] = layer2;
  layers[index2] = layer1;
  notifyModified(true);
}

ByteData* PAGComposition::audioBytes() const {
  return static_cast<PreComposeLayer*>(layer)->composition->audioBytes;
}

std::vector<const Marker*> PAGComposition::audioMarkers() const {
  std::vector<const Marker*> result = {};
  for (auto marker : static_cast<PreComposeLayer*>(layer)->composition->audioMarkers) {
    result.push_back(marker);
  }
  return result;
}

Frame PAGComposition::audioStartTime() const {
  auto preComposeLayer = static_cast<PreComposeLayer*>(layer);
  auto compositionOffset = preComposeLayer->compositionStartTime - layer->startTime + startFrame;
  auto frame = preComposeLayer->composition->audioStartTime + compositionOffset;
  return FrameToTime(frame, frameRateInternal());
}

std::vector<std::shared_ptr<PAGLayer>> PAGComposition::getLayersBy(
    std::function<bool(PAGLayer* pagLayer)> filterFunc) {
  std::vector<std::shared_ptr<PAGLayer>> result = {};
  FindLayers(filterFunc, &result, weakThis.lock());
  return result;
}

void PAGComposition::FindLayers(std::function<bool(PAGLayer* pagLayer)> filterFunc,
                                std::vector<std::shared_ptr<PAGLayer>>* result,
                                std::shared_ptr<PAGLayer> pagLayer) {
  if (filterFunc(pagLayer.get())) {
    result->push_back(pagLayer);
  }
  if (pagLayer->_trackMatteLayer) {
    FindLayers(filterFunc, result, pagLayer->_trackMatteLayer);
  }
  if (pagLayer->layerType() == LayerType::PreCompose) {
    for (auto& childLayer : static_cast<PAGComposition*>(pagLayer.get())->layers) {
      FindLayers(filterFunc, result, childLayer);
    }
  }
}

Frame PAGComposition::childFrameToLocal(Frame childFrame, float childFrameRate) const {
  auto localFrame = PAGLayer::childFrameToLocal(childFrame, childFrameRate);
  auto compositionOffset =
      static_cast<PreComposeLayer*>(layer)->compositionStartTime - layer->startTime;
  return localFrame + compositionOffset;
}

Frame PAGComposition::localFrameToChild(Frame localFrame, float childFrameRate) const {
  auto compositionOffset =
      static_cast<PreComposeLayer*>(layer)->compositionStartTime - layer->startTime;
  auto childFrame = localFrame - compositionOffset;
  return PAGLayer::localFrameToChild(childFrame, childFrameRate);
}

bool PAGComposition::hasClip() const {
  return _width > 0 && _height > 0;
}

Frame PAGComposition::frameDuration() const {
  return _frameDuration;
}

float PAGComposition::frameRateInternal() const {
  return _frameRate;
}

bool PAGComposition::gotoTime(int64_t layerTime) {
  auto changed = PAGLayer::gotoTime(layerTime);
  auto compositionOffset =
      static_cast<PreComposeLayer*>(layer)->compositionStartTime - layer->startTime + startFrame;
  /// 这里用floor取帧数是为了防止compositionOffsetTime变大导致layerTime变小导致显示帧数不正确。
  /// 比如layerTime = 100000 , compositionOffset = 1, frameRate = 30
  /// 此时如果取ceil compositionOffsetTime = 33334， subLayerTime = 66666，subLayerFrame = 1.9998
  /// 即显示第1帧， 但纯用frame计算时，gotoFrame = 2，因此此时不能取ceil，而要取floor来保证
  /// layerTime时间足够
  auto compositionOffsetTime =
      static_cast<Frame>(floor(compositionOffset * 1000000.0 / frameRateInternal()));
  for (auto& layer : layers) {
    if (layer->_excludedFromTimeline) {
      continue;
    }
    if (layer->gotoTime(layerTime - compositionOffsetTime)) {
      changed = true;
    }
  }
  return changed;
}

void PAGComposition::draw(Recorder* recorder) {
  if (!contentModified() && layerCache->contentStatic()) {
    // 子项未发生任何修改且内容是静态的，可以使用缓存快速跳过所有子项绘制。
    getContent()->draw(recorder);
    return;
  }
  auto preComposeLayer = static_cast<PreComposeLayer*>(layer);
  auto composition = preComposeLayer->composition;
  if (composition->type() == CompositionType::Bitmap ||
      composition->type() == CompositionType::Video) {
    auto layerFrame = layer->startTime + contentFrame;
    auto compositionFrame = preComposeLayer->getCompositionFrame(layerFrame);
    auto graphic = stage->getSequenceGraphic(composition, compositionFrame);
    recorder->drawGraphic(graphic);
  }
  if (layers.empty()) {
    return;
  }
  if (hasClip()) {
    recorder->saveClip(0, 0, static_cast<float>(_width), static_cast<float>(_height));
  }
  auto count = static_cast<int>(layers.size());
  for (int i = 0; i < count; i++) {
    auto& childLayer = layers[i];
    if (!childLayer->layerVisible) {
      continue;
    }
    DrawChildLayer(recorder, childLayer.get());
  }
  if (hasClip()) {
    recorder->restore();
  }
}

void PAGComposition::DrawChildLayer(Recorder* recorder, PAGLayer* childLayer) {
  auto filterModifier = childLayer->cacheFilters() ? nullptr : FilterModifier::Make(childLayer);
  auto trackMatte = TrackMatteRenderer::Make(childLayer);
  Transform extraTransform = {ToTGFX(childLayer->layerMatrix), childLayer->layerAlpha};
  LayerRenderer::DrawLayer(recorder, childLayer->layer,
                           childLayer->contentFrame + childLayer->layer->startTime, filterModifier,
                           trackMatte.get(), childLayer, &extraTransform);
}

void PAGComposition::measureBounds(tgfx::Rect* bounds) {
  if (!contentModified() && layerCache->contentStatic()) {
    getContent()->measureBounds(bounds);
    return;
  }
  bounds->setEmpty();
  auto composition = static_cast<PreComposeLayer*>(layer)->composition;
  if (composition->type() == CompositionType::Bitmap ||
      composition->type() == CompositionType::Video) {
    getContent()->measureBounds(bounds);
  }
  for (auto& childLayer : layers) {
    if (!childLayer->layerVisible) {
      continue;
    }
    tgfx::Rect layerBounds = {};
    MeasureChildLayer(&layerBounds, childLayer.get());
    bounds->join(layerBounds);
  }
  if (hasClip() && !bounds->isEmpty()) {
    auto clipBounds = tgfx::Rect::MakeXYWH(0, 0, _width, _height);
    if (!bounds->intersect(clipBounds)) {
      bounds->setEmpty();
    }
  }
}

void PAGComposition::MeasureChildLayer(tgfx::Rect* bounds, PAGLayer* childLayer) {
  std::unique_ptr<tgfx::Rect> trackMatteBounds = nullptr;
  if (childLayer->_trackMatteLayer != nullptr) {
    trackMatteBounds = std::make_unique<tgfx::Rect>();
    trackMatteBounds->setEmpty();
    auto trackMatteLayer = childLayer->_trackMatteLayer;
    auto layerFrame = trackMatteLayer->contentFrame + trackMatteLayer->layer->startTime;
    auto filterModifier = FilterModifier::Make(trackMatteLayer.get());
    Transform extraTransform = {ToTGFX(trackMatteLayer->layerMatrix), trackMatteLayer->layerAlpha};
    LayerRenderer::MeasureLayerBounds(trackMatteBounds.get(), trackMatteLayer->layer, layerFrame,
                                      filterModifier, nullptr, trackMatteLayer.get(),
                                      &extraTransform);
  }
  auto layerFrame = childLayer->contentFrame + childLayer->layer->startTime;
  auto filterModifier = FilterModifier::Make(childLayer->layer, layerFrame);
  Transform extraTransform = {ToTGFX(childLayer->layerMatrix), childLayer->layerAlpha};
  LayerRenderer::MeasureLayerBounds(bounds, childLayer->layer, layerFrame, filterModifier,
                                    trackMatteBounds.get(), childLayer, &extraTransform);
}

std::vector<std::shared_ptr<PAGLayer>> PAGComposition::getLayersByName(
    const std::string& layerName) {
  LockGuard autoLock(rootLocker);
  if (layerName.empty()) {
    return {};
  }
  std::vector<std::shared_ptr<PAGLayer>> result =
      getLayersBy([=](PAGLayer* pagLayer) -> bool { return pagLayer->layerName() == layerName; });
  return result;
}

std::vector<std::shared_ptr<PAGLayer>> PAGComposition::getLayersUnderPoint(float localX,
                                                                           float localY) {
  LockGuard autoLock(rootLocker);
  std::vector<std::shared_ptr<PAGLayer>> results;
  getLayersUnderPointInternal(localX, localY, &results);
  return results;
}

bool PAGComposition::GetTrackMatteLayerAtPoint(PAGLayer* childLayer, float x, float y,
                                               std::vector<std::shared_ptr<PAGLayer>>* results) {
  // trackMatteLayer 处于时间轴不可见的情况下也要当做空区域处理，永远不取消遮罩作用。当 trackMatte
  // 为空时，配合 inverted 的遮罩类型，表示保留全部目标图层。
  bool contains = false;
  Transform trackMatteTransform = {};
  auto trackMatteLayer = childLayer->_trackMatteLayer.get();
  if (trackMatteLayer->getTransform(&trackMatteTransform)) {
    tgfx::Point local = {x, y};
    MapPointInverted(trackMatteTransform.matrix, &local);
    tgfx::Rect trackMatteBounds = {};
    trackMatteLayer->measureBounds(&trackMatteBounds);
    contains = trackMatteBounds.contains(local.x, local.y);
    if (contains) {
      results->push_back(childLayer->_trackMatteLayer);
    }
  }
  auto trackMatteType = childLayer->layer->trackMatteType;
  auto inverse = (trackMatteType == TrackMatteType::AlphaInverted ||
                  trackMatteType == TrackMatteType::LumaInverted);
  return (contains != inverse);
}

bool PAGComposition::GetChildLayerAtPoint(PAGLayer* childLayer, float x, float y,
                                          std::vector<std::shared_ptr<PAGLayer>>* results) {
  Transform layerTransform = {};
  if (!childLayer->getTransform(&layerTransform)) {
    return false;
  }
  tgfx::Point localPoint = {x, y};
  MapPointInverted(layerTransform.matrix, &localPoint);
  auto mask = childLayer->layerCache->getMasks(childLayer->contentFrame);
  if (mask) {
    auto maskBounds = mask->getBounds();
    auto inverse = mask->isInverseFillType();
    if (maskBounds.contains(localPoint.x, localPoint.y) == inverse) {
      return false;
    }
  }

  bool success = false;
  if (childLayer->layerType() == LayerType::PreCompose) {
    success = static_cast<PAGComposition*>(childLayer)
                  ->getLayersUnderPointInternal(localPoint.x, localPoint.y, results);
  }
  if (!success) {
    tgfx::Rect childBounds = {};
    childLayer->measureBounds(&childBounds);
    success = childBounds.contains(localPoint.x, localPoint.y);
  }
  return success;
}

bool PAGComposition::getLayersUnderPointInternal(float x, float y,
                                                 std::vector<std::shared_ptr<PAGLayer>>* results) {
  auto bounds = tgfx::Rect::MakeWH(_width, _height);
  if (hasClip() && !bounds.contains(x, y)) {
    return false;
  }
  bool found = false;
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; i--) {
    auto childLayer = layers[i];
    if (!childLayer->layerVisible) {
      continue;
    }
    if (childLayer->_trackMatteLayer &&
        !GetTrackMatteLayerAtPoint(childLayer.get(), x, y, results)) {
      continue;
    }
    auto success = GetChildLayerAtPoint(childLayer.get(), x, y, results);
    if (success) {
      results->push_back(childLayer);
      found = true;
    }
  }
  return found;
}

bool PAGComposition::cacheFilters() const {
  return layerCache->cacheFilters() && !contentModified() && layerCache->contentStatic();
}

void PAGComposition::invalidateCacheScale() {
  if (stage == nullptr) {
    return;
  }
  PAGLayer::invalidateCacheScale();
  for (auto& layer : layers) {
    if (layer->_trackMatteLayer) {
      layer->_trackMatteLayer->invalidateCacheScale();
    }
    layer->invalidateCacheScale();
  }
}

void PAGComposition::onAddToStage(PAGStage* pagStage) {
  PAGLayer::onAddToStage(pagStage);
  for (auto& layer : layers) {
    layer->onAddToStage(pagStage);
  }
}

void PAGComposition::onRemoveFromStage() {
  PAGLayer::onRemoveFromStage();
  for (auto& layer : layers) {
    layer->onRemoveFromStage();
  }
}

void PAGComposition::onAddToRootFile(PAGFile* pagFile) {
  PAGLayer::onAddToRootFile(pagFile);
  for (auto& layer : layers) {
    if (layer->file == file) {
      layer->onAddToRootFile(pagFile);
    }
  }
}

void PAGComposition::onRemoveFromRootFile() {
  PAGLayer::onRemoveFromRootFile();
  for (auto& layer : layers) {
    if (layer->file == file) {
      layer->onRemoveFromRootFile();
    }
  }
}

void PAGComposition::onTimelineChanged() {
  for (auto& layer : layers) {
    layer->onTimelineChanged();
    if (layer->_trackMatteLayer != nullptr) {
      layer->_trackMatteLayer->onTimelineChanged();
    }
  }
}

void PAGComposition::updateRootLocker(std::shared_ptr<std::mutex> locker) {
  PAGLayer::updateRootLocker(locker);
  for (auto& layer : layers) {
    layer->updateRootLocker(locker);
  }
}

void PAGComposition::updateDurationAndFrameRate() {
  int64_t layerMaxTimeDuration = 1;
  float layerMaxFrameRate = layers.empty() ? 60 : 1;
  for (auto& layer : layers) {
    auto layerTimeDuration = layer->startTimeInternal() + layer->durationInternal();
    if (layerTimeDuration > layerMaxTimeDuration) {
      layerMaxTimeDuration = layerTimeDuration;
    }
    auto layerFrameRate = layer->frameRateInternal();
    if (layerFrameRate > layerMaxFrameRate) {
      layerMaxFrameRate = layerFrameRate;
    }
  }
  bool changed = false;
  auto layerMaxFrameDuration = static_cast<Frame>(
      round(static_cast<double>(layerMaxTimeDuration) * layerMaxFrameRate / 1000000.0));
  if (_frameDuration != layerMaxFrameDuration) {
    _frameDuration = layerMaxFrameDuration;
    changed = true;
  }
  if (_frameRate != layerMaxFrameRate) {
    _frameRate = layerMaxFrameRate;
    changed = true;
  }
  if (changed && _parent && _parent->emptyComposition) {
    _parent->updateDurationAndFrameRate();
  }
}

bool PAGComposition::hasBitmapSequenceOnly() const {
  auto composition = static_cast<PreComposeLayer*>(layer)->composition;
  if (composition != nullptr) {
    auto type = composition->type();
    if (type == CompositionType::Bitmap || type == CompositionType::Video) {
      return true;
    }
  }

  bool hasSequenceContent = false;
  for (const auto& childLayer : layers) {
    auto layerType = childLayer->layerType();
    switch (layerType) {
      case LayerType::Shape:
      case LayerType::Text:
      case LayerType::Solid:
      case LayerType::Image:
        return false;
      case LayerType::PreCompose: {
        auto childComposition = std::static_pointer_cast<PAGComposition>(childLayer);
        if (!childComposition->hasBitmapSequenceOnly()) {
          return false;
        }
        hasSequenceContent = true;
        break;
      }
      case LayerType::Null:
      case LayerType::Camera:
        break;
      default:
        return false;
    }
  }

  return hasSequenceContent;
}

}  // namespace pag
