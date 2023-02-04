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
#include <queue>
#include <unordered_set>
#include "TextAtlas.h"
#include "TextBlock.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Performance.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/graphics/ImageProxy.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Shape.h"
#include "rendering/graphics/Snapshot.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/sequences/SequenceImageQueue.h"
#include "rendering/utils/PathHasher.h"
#include "tgfx/gpu/Device.h"

namespace pag {
using ShapeMap = std::unordered_map<tgfx::Path, std::shared_ptr<tgfx::Shape>, PathHasher>;

class RenderCache : public Performance {
 public:
  explicit RenderCache(PAGStage* stage);

  ~RenderCache() override;

  ID uniqueID() const {
    return _uniqueID;
  }

  void beginFrame();

  void attachToContext(tgfx::Context* current, bool forDrawing = true);

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
   * Prepares the nearly visible layers for the upcoming drawings, which collects all CPU tasks and
   * runs them asynchronously in parallel.
   */
  void prepareLayers();

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
   * Returns a snapshot cache of specified Picture. If there is no associated cache available,
   * a new cache will be created by the Picture. Returns null if the Picture fails to make a
   * new snapshot.
   */
  Snapshot* getSnapshot(const Picture* picture);

  /**
   * Frees the snapshot cache associated with specified asset ID immediately.
   */
  void removeSnapshot(ID assetID);

  std::shared_ptr<tgfx::Shape> getShape(ID assetID, const tgfx::Path& path);

  TextAtlas* getTextAtlas(const TextBlock* textBlock);

  /**
   * Prepares an image for next getAssetImage() call, which may schedule an asynchronous decoding
   * task immediately.
   */
  void prepareAssetImage(ID assetID, const ImageProxy* proxy);

  /**
   * Returns an image of the specified assetID.Returns a decoded or mipMapped image if available.
   * Otherwise, returns the original Image.
   */
  std::shared_ptr<tgfx::Image> getAssetImage(ID assetID, const ImageProxy* proxy);

  uint32_t getContentVersion() const;

  bool videoEnabled() const;

  void setVideoEnabled(bool value);

  void prepareSequenceImage(Sequence* sequence, Frame targetFrame);

  std::shared_ptr<tgfx::Image> getSequenceImage(Sequence* sequence, Frame targetFrame);

  LayerFilter* getFilterCache(LayerStyle* layerStyle);

  LayerFilter* getFilterCache(Effect* effect);

  MotionBlurFilter* getMotionBlurFilter();

  LayerStylesFilter* getLayerStylesFilter(Layer* layer);

  std::shared_ptr<File> getFileByAssetID(ID assetID);

  void recordImageDecodingTime(int64_t decodingTime);

  void recordTextureUploadingTime(int64_t time);

  void recordProgramCompilingTime(int64_t time);

  void releaseAll();

 private:
  ID _uniqueID = 0;
  PAGStage* stage = nullptr;
  uint32_t deviceID = 0;
  tgfx::Context* context = nullptr;
  std::queue<int64_t> timestamps = {};
  bool isDrawingFrame = false;
  size_t graphicsMemory = 0;
  bool _videoEnabled = true;
  bool _snapshotEnabled = true;
  std::unordered_set<ID> usedAssets = {};
  std::unordered_map<ID, Snapshot*> snapshotCaches = {};
  std::list<Snapshot*> snapshotLRU = {};
  std::unordered_map<Snapshot*, std::list<Snapshot*>::iterator> snapshotPositions = {};
  std::unordered_map<ID, TextAtlas*> textAtlases = {};
  std::unordered_map<ID, ShapeMap> shapeCaches = {};
  std::unordered_map<ID, std::shared_ptr<tgfx::Image>> assetImages = {};
  std::unordered_map<ID, std::shared_ptr<tgfx::Image>> decodedAssetImages = {};
  std::unordered_map<ID, std::vector<SequenceImageQueue*>> sequenceCaches = {};
  std::unordered_map<ID, std::unordered_map<Frame, SequenceImageQueue*>> usedSequences = {};
  std::unordered_map<ID, Filter*> filterCaches;
  MotionBlurFilter* motionBlurFilter = nullptr;

  // decoded image caches:
  void clearExpiredDecodedImages();

  // snapshot caches:
  void clearAllSnapshots();
  void clearExpiredSnapshots();
  void moveSnapshotToHead(Snapshot* snapshot);
  void removeSnapshotFromLRU(Snapshot* snapshot);

  // sequence caches:
  SequenceImageQueue* getSequenceImageQueue(Sequence* sequence, Frame targetFrame);
  SequenceImageQueue* findNearestSequenceImageQueue(Sequence* sequence, Frame targetFrame);
  SequenceImageQueue* makeSequenceImageQueue(Sequence* sequence);
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

  // shape caches:
  std::shared_ptr<tgfx::Shape> findShape(ID assetID, const tgfx::Path& path);
  void removeShape(ID assetID, const tgfx::Path& path);

  void preparePreComposeLayer(PreComposeLayer* layer);
  void prepareImageLayer(PAGImageLayer* layer);
  void prepareNextFrame();
  std::shared_ptr<tgfx::Image> getAssetImageInternal(ID assetID, const ImageProxy* proxy);
  void recordPerformance();

  friend class PAGPlayer;
};
}  // namespace pag
