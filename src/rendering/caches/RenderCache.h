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

#pragma once

#include <list>
#include <memory>
#include <unordered_set>
#include "TextAtlas.h"
#include "TextBlock.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Performance.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/filters/Transform3DFilter.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Shape.h"
#include "rendering/graphics/Snapshot.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/sequences/SequenceReader.h"
#include "tgfx/gpu/Device.h"

namespace pag {
static constexpr int64_t DECODING_VISIBLE_DISTANCE = 500000;  // 提前 500ms 开始解码。

class RenderCache : public Performance {
 public:
  explicit RenderCache(PAGStage* stage);

  ~RenderCache() override;

  ID uniqueID() const {
    return _uniqueID;
  }

  void beginFrame();

  void attachToContext(tgfx::Context* current, bool forHitTest = false);

  void detachFromContext();

  /**
   * Returns the total memory usage of this cache.
   */
  size_t memoryUsage() const {
    return graphicsMemory;
  }

  /**
   * Returns the GPU context associated with this cache.
   */
  tgfx::Context* getContext() const {
    return context;
  }

  /**
   * If set to false, the getSnapshot() always returns nullptr. The default value is true.
   */
  bool snapshotEnabled() const;

  /**
   * Set the value of snapshotEnabled property.
   */
  void setSnapshotEnabled(bool value);

  /**
   * Returns true if there is snapshot cache available for specified asset ID.
   */
  bool hasSnapshot(ID assetID) const {
    return snapshotCaches.count(assetID) > 0;
  }

  /**
   * Returns a snapshot cache of specified asset id. Returns null if there is no associated cache
   * available. This is a read-only query which is used usually during hit testing.
   */
  Snapshot* getSnapshot(ID assetID) const;

  /**
   * Returns a snapshot cache of specified Image. If there is no associated cache available,
   * a new cache will be created by the image. Returns null if the image fails to make a
   * new snapshot.
   */
  Snapshot* getSnapshot(const Picture* image);

  /**
   * Frees the snapshot cache associated with specified asset ID immediately.
   */
  void removeSnapshot(ID assetID);

  Snapshot* getSnapshot(const Shape* shape);

  TextAtlas* getTextAtlas(const TextBlock* textBlock);

  /**
   * Prepares a bitmap task for next getImageBuffer() call.
   */
  void prepareImage(ID assetID, std::shared_ptr<tgfx::ImageCodec> codec);

  /**
   * Returns a texture buffer cache of specified asset id. Returns null if there is no associated
   * cache available.
   */
  std::shared_ptr<tgfx::ImageBuffer> getImageBuffer(ID assetID);

  uint32_t getContentVersion() const;

  bool videoEnabled() const;

  void setVideoEnabled(bool value);

  void prepareSequence(Sequence* sequence, Frame targetFrame);

  std::shared_ptr<tgfx::Texture> getSequenceFrame(Sequence* sequence, Frame targetFrame);

  LayerFilter* getFilterCache(LayerStyle* layerStyle);

  LayerFilter* getFilterCache(Effect* effect);

  MotionBlurFilter* getMotionBlurFilter();
  
  Transform3DFilter* getTransform3DFilter();

  LayerStylesFilter* getLayerStylesFilter(Layer* layer);

  void recordImageDecodingTime(int64_t decodingTime);

  void recordTextureUploadingTime(int64_t time);

  void recordProgramCompilingTime(int64_t time);

  void releaseAll();

 private:
  ID _uniqueID = 0;
  PAGStage* stage = nullptr;
  uint32_t deviceID = 0;
  tgfx::Context* context = nullptr;
  int64_t lastTimestamp = 0;
  bool hitTestOnly = false;
  size_t graphicsMemory = 0;
  bool _videoEnabled = true;
  bool _snapshotEnabled = true;
  std::unordered_set<ID> usedAssets = {};
  std::unordered_map<ID, Snapshot*> snapshotCaches = {};
  std::list<Snapshot*> snapshotLRU = {};
  std::unordered_map<ID, TextAtlas*> textAtlases = {};
  std::unordered_map<ID, std::shared_ptr<Task>> imageTasks;
  std::unordered_map<ID, std::vector<SequenceReader*>> sequenceCaches = {};
  std::unordered_map<ID, std::unordered_map<Frame, SequenceReader*>> usedSequences = {};
  std::unordered_map<ID, Filter*> filterCaches;
  MotionBlurFilter* motionBlurFilter = nullptr;
  Transform3DFilter* transform3DFilter = nullptr;
  std::unordered_map<ID, std::unordered_map<tgfx::Path, Snapshot*, tgfx::PathHash>> pathCaches;

  // bitmap caches:
  void clearExpiredBitmaps();

  // snapshot caches:
  void clearAllSnapshots();
  void clearExpiredSnapshots();

  // sequence caches:
  void clearAllSequenceCaches();
  void clearSequenceCache(ID uniqueID);
  void clearExpiredSequences();

  // filter caches:
  LayerFilter* getLayerFilterCache(ID uniqueID, const std::function<LayerFilter*()>& makeFilter);
  void clearFilterCache(ID uniqueID);
  bool initFilter(Filter* filter);

  // text atlas caches:
  void clearAllTextAtlas();
  void removeTextAtlas(ID assetID);
  TextAtlas* getTextAtlas(ID assetID) const;

  // path snapshot caches:
  Snapshot* getSnapshot(ID assetID, const tgfx::Path& path) const;
  void removeSnapshot(ID assetID, const tgfx::Path& path);
  void removePathSnapshots(ID assetID);

  void prepareLayers(int64_t timeDistance = DECODING_VISIBLE_DISTANCE);
  void preparePreComposeLayer(PreComposeLayer* layer);
  void prepareImageLayer(PAGImageLayer* layer);
  SequenceReader* getSequenceReader(Sequence* sequence, Frame targetFrame);
  SequenceReader* findNearestSequenceReader(Sequence* sequence, Frame targetFrame);
  SequenceReader* makeSequenceReader(Sequence* sequence);
  Snapshot* makeSnapshot(float scaleFactor, const std::function<Snapshot*()>& maker);
  void moveSnapshotToHead(Snapshot* snapshot);
  void removeSnapshotFromLRU(Snapshot* snapshot);

  friend class PAGPlayer;
};
}  // namespace pag
