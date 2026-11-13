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

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/types/Data.h"

namespace pagx {

class Image;
class PAGXDocument;

// Collects the image entries produced while PAGX XML is serialized, then assembles them with the
// document into a single ZIP archive. The writer owns entry naming and payloads, so the serializer
// only asks for the path an image should be referenced by.
class PAGXArchiveWriter {
 public:
  explicit PAGXArchiveWriter(const PAGXDocument& document);

  PAGXArchiveWriter(const PAGXArchiveWriter&) = delete;
  PAGXArchiveWriter& operator=(const PAGXArchiveWriter&) = delete;

  std::string AddImage(const Image* image);

  std::shared_ptr<Data> Finish(std::string_view documentXML, std::string* errorMsg);

  // Replaces warnings with one line per image the archive could not absorb, so a caller can log or
  // inspect them without parsing a single concatenated message. Leaves it empty when the archive
  // is self-contained.
  void CollectWarnings(std::vector<std::string>* warnings) const;

 private:
  std::string addEntry(const Image* image, const std::shared_ptr<Data>& data);
  std::string allocatePath(const std::string& baseName, const std::string& extension);
  void reserveKeptPath(const std::string& key);
  void recordExternalReference(const std::string& reference, const char* reason);
  void recordUnsupportedFormat(const std::string& entryPath, const char* mime);

  std::unordered_map<const Image*, std::string> _entryPaths = {};
  std::unordered_set<std::string> _usedPaths = {};
  std::vector<std::pair<std::string, std::shared_ptr<Data>>> _entries = {};
  std::vector<std::pair<std::string, std::string>> _unresolvedDetails = {};
  std::vector<std::pair<std::string, std::string>> _unsupportedFormats = {};
  std::unordered_set<std::string> _reportedReferences = {};
  size_t _nextImageIndex = 1;
};

}  // namespace pagx
