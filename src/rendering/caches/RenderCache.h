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

#pragma once

#include <list>
#include <memory>
#include <queue>
#include <unordered_set>
#include "TextBlock.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Performance.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/graphics/ImageProxy.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Shape.h"
#include "rendering/graphics/Snapshot.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/sequences/SequenceImageQueue.h"
#include "rendering/sequences/SequenceInfo.h"
#include "rendering/utils/PathHasher.h"
#include "tgfx/gpu/Device.h"

namespace pag {
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

  void prepareNextFrame();

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
   * If set to true, PAG will cache the associated rendering data into a disk file, such as the
   * decoded image frames of video compositions. This can help reduce memory usage and improve
   * rendering performance.
   */
  bool useDiskCache() const {
    return _useDiskCache;
  }

  /**
   * Set the value of useDiskCache property.
   */
  void setUseDiskCache(bool value) {
    _useDiskCache = value;
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

  /**
   * Prepares an image for the next getAssetImage() call, which may schedule an asynchronous
   * decoding task immediately.
   */
  void prepareAssetImage(ID assetID, const ImageProxy* proxy);

  /**
   * Returns an image of the specified assetID.Returns a decoded or mipmapped image if available.
   * Otherwise, returns the original Image.
   */
  std::shared_ptr<tgfx::Image> getAssetImage(ID assetID, const ImageProxy* proxy);

  uint32_t getContentVersion() const;

  bool videoEnabled() const;

  void setVideoEnabled(bool value);

  void prepareSequenceImage(std::shared_ptr<SequenceInfo> sequence, Frame targetFrame);

  std::shared_ptr<tgfx::Image> getSequenceImage(std::shared_ptr<SequenceInfo> sequence,
                                                Frame targetFrame);

  std::shared_ptr<File> getFileByAssetID(ID assetID);

  void recordImageDecodingTime(int64_t decodingTime);

  void recordTextureUploadingTime(int64_t time);

  void recordProgramCompilingTime(int64_t time);

  FilterResources* findFilterResources(ID type);

  void addFilterResources(ID type, std::unique_ptr<FilterResources> resources);

  void releaseAll();

 private:
  ID _uniqueID = 0;
  PAGStage* stage = nullptr;
  uint32_t contextID = 0;
  tgfx::Context* context = nullptr;
  std::queue<std::chrono::steady_clock::time_point> timestamps = {};
  bool isDrawingFrame = false;
  size_t graphicsMemory = 0;
  bool _videoEnabled = true;
  bool _snapshotEnabled = true;
  bool _useDiskCache = false;
  std::unordered_set<ID> usedAssets = {};
  std::unordered_map<ID, Snapshot*> snapshotCaches = {};
  std::list<Snapshot*> snapshotLRU = {};
  std::unordered_map<Snapshot*, std::list<Snapshot*>::iterator> snapshotPositions = {};
  std::unordered_map<ID, std::shared_ptr<tgfx::Image>> assetImages = {};
  std::unordered_map<ID, std::shared_ptr<tgfx::Image>> decodedAssetImages = {};
  std::unordered_map<ID, std::vector<SequenceImageQueue*>> sequenceCaches = {};
  std::unordered_map<ID, std::unordered_map<Frame, SequenceImageQueue*>> usedSequences = {};

  // decoded image caches:
  void clearExpiredDecodedImages();

  // snapshot caches:
  void clearAllSnapshots();
  void clearExpiredSnapshots();
  void moveSnapshotToHead(Snapshot* snapshot);
  void removeSnapshotFromLRU(Snapshot* snapshot);

  // sequence caches:
  SequenceImageQueue* getSequenceImageQueue(std::shared_ptr<SequenceInfo> sequence,
                                            Frame targetFrame);
  SequenceImageQueue* findNearestSequenceImageQueue(std::shared_ptr<SequenceInfo> sequence,
                                                    Frame targetFrame);
  SequenceImageQueue* makeSequenceImageQueue(std::shared_ptr<SequenceInfo> sequence);
  void clearAllSequenceCaches();
  void clearSequenceCache(ID uniqueID);
  void clearExpiredSequences();

  void preparePreComposeLayer(PreComposeLayer* layer);
  void prepareImageLayer(PAGImageLayer* layer);
  std::shared_ptr<tgfx::Image> getAssetImageInternal(ID assetID, const ImageProxy* proxy);
  void recordPerformance();

  // filter resources cache:
  std::unordered_map<ID, std::unique_ptr<FilterResources>> filterResourcesMap = {};

  friend class PAGPlayer;
};
}  // namespace pag
