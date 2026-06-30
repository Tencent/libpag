/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <memory>
#include <string>
#include "pagx/PAGImage.h"
#include "pagx/types/Data.h"
#include "pagx/nodes/Node.h"

namespace pagx {

class LayerBuilder;

/**
 * Image represents an image resource that can be referenced by other nodes. The image source can
 * be a file path, a URL, or a base64-encoded data URI. A host-supplied decoded image (provided at
 * runtime via PAGXDocument::loadFileData) is cached on the node but never serialized.
 */
class Image : public Node {
 public:
  /**
   * Image binary data (decoded from base64 or loaded via loadFileData).
   */
  std::shared_ptr<Data> data = nullptr;

  /**
   * External file path (mutually exclusive with data, data has priority).
   */
  std::string filePath = {};

  NodeType nodeType() const override {
    return NodeType::Image;
  }

 private:
  Image() = default;

  // Host-supplied ready-to-render image for this resource (set via loadFileData(path, PAGImage)).
  // Runtime-only state; never serialized. Takes priority over decoding from data / filePath.
  std::shared_ptr<PAGImage> runtimeImage = nullptr;

  friend class PAGXDocument;
  friend class LayerBuilder;
};

}  // namespace pagx
