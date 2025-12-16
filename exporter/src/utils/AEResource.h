/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include <AEGP_SuiteHandler.h>
#include <AE_GeneralPlug.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "codec/tags/ImageFillRule.h"

namespace exporter {

enum class AEResourceType { Unknown, Folder, Composition, Image };

class AEResource {
 public:
  static std::vector<std::shared_ptr<AEResource>> GetAEResourceList();

  AEResource();
  void setSavePath(const std::string& savePath);

  struct FileStructureRelationship {
    AEResource* parent = nullptr;
    std::vector<std::shared_ptr<AEResource>> children = {};
  };

  struct PlaceholderImageFlags {
    bool isEditable = true;
    pag::PAGScaleMode scaleMode = pag::PAGScaleMode::None;
  };

  struct Layer {
    A_long layerID = 0;
    std::string name = "";
    AEGP_LayerH layerHandle = nullptr;
  };

  struct CompositionRelationship {
    std::unordered_map<A_long, bool> exportAsBmpMap = {};
    std::unordered_map<A_long, bool> textLayerFlagMap = {};
    std::unordered_map<A_long, PlaceholderImageFlags> imagesLayerFlagMap = {};

    // Only save the current object's own resources, not the children's resources
    std::vector<Layer> textLayers = {};
    std::vector<Layer> imageLayers = {};
    std::vector<std::shared_ptr<AEResource>> children = {};
  };

  bool isExport = false;
  bool isExportAsBmp = false;
  pag::PAGTimeStretchMode stretchMode = pag::PAGTimeStretchMode::Repeat;
  pag::Frame stretchStartTime = 0;
  pag::Frame stretchDuration = 0;
  AEResourceType type = AEResourceType::Unknown;
  A_long ID = -1;
  std::string name = "";
  std::string savePath = "";
  AEGP_ItemH itemHandle = nullptr;
  FileStructureRelationship file = {};
  CompositionRelationship composition = {};

 private:
  void initSavePath();
};

bool HasCompositionResource();

AEResourceType GetAEItemResourceType(const AEGP_ItemH& item);

}  // namespace exporter
