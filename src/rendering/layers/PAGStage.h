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

#include <cfloat>
#include <map>
#include <optional>
#include <unordered_set>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
struct SequenceCache {
  std::shared_ptr<Graphic> graphic = nullptr;
  Frame compositionFrame = 0;
};

class PAGStage : public PAGComposition {
 public:
  static std::shared_ptr<PAGStage> Make(int width, int height);

  ~PAGStage() override;

  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
   * graphics memory which leads to better performance.
   * The default value is 1.0.
   */
  float cacheScale() const {
    return _cacheScale;
  }

  /**
   * Set the value of cacheScale property.
   */
  void setCacheScale(float value);

  /**
   * Returns the first root composition.
   */
  std::shared_ptr<PAGComposition> getRootComposition();

  /**
   * Returns the version of current content.
   */
  uint32_t getContentVersion() const;

  /**
   * Add a PAGLayer to this stage.
   */
  void addReference(PAGLayer* pagLayer);

  /**
   * Remove a PAGLayer from this stage.
   */
  void removeReference(PAGLayer* pagLayer);

  /**
   * Add a PAGImage to this stage.
   */
  void addReference(PAGImage* pagImage, PAGLayer* pagLayer);

  /**
   * Remove a PAGImage from this stage.
   */
  void removeReference(PAGImage* pagImage, PAGLayer* pagLayer);

  /**
   * Invalidate the content of a PAGLayer, it is usually called when a PAGLayer is edited.
   */
  void invalidateCacheScale(PAGLayer* pagLayer);

  std::shared_ptr<Graphic> getSequenceGraphic(Composition* composition, Frame compositionFrame);

  std::map<int64_t, std::vector<PAGLayer*>> findNearlyVisibleLayersIn(int64_t timeDistance);

  std::unordered_set<ID> getRemovedAssets();

  float getAssetMaxScale(ID assetID);

  float getAssetMinScale(ID assetID);

 protected:
  void invalidateCacheScale() override {
    PAGComposition::invalidateCacheScale();
  };

  void onAddToStage(PAGStage*) override{};

  void onRemoveFromStage() override{};

 private:
  float _cacheScale = 1.0f;
  int64_t rootVersion = -1;
  std::unordered_map<PAGLayer*, Frame> layerStartTimeMap = {};
  std::unordered_map<ID, std::vector<PAGLayer*>> layerReferenceMap = {};
  std::unordered_map<ID, std::pair<float, float>> scaleFactorCache = {};
  std::unordered_map<ID, SequenceCache> sequenceCache = {};
  std::unordered_set<ID> invalidAssets = {};
  std::unordered_map<ID, PAGImage*> pagImageMap = {};

  static tgfx::Point GetLayerContentScaleFactor(PAGLayer* pagLayer, bool isPAGImage);
  PAGStage(int width, int height);
  pag::PAGLayer* getLayerFromReferenceMap(ID uniqueID);
  void addToReferenceMap(ID uniqueID, PAGLayer* pagLayer);
  bool removeFromReferenceMap(ID uniqueID, PAGLayer* pagLayer);
  std::pair<float, float> getScaleFactor(ID referenceID);
  std::optional<std::pair<float, float>> calcScaleFactor(ID referenceID);
  static std::pair<float, float> GetLayerScaleFactor(PAGLayer* pagLayer, tgfx::Point scale);
  void updateLayerStartTime(PAGLayer* pagLayer);
  void updateChildLayerStartTime(PAGComposition* pagComposition);

  friend class RenderCache;
};
}  // namespace pag
