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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "PAGXExternalFiles.h"
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/types/Data.h"

namespace pagx {
namespace {

constexpr int kMaxSubDocumentInstances = 1024;
constexpr int kMaxReferenceDepth = 64;

std::string DirName(const std::string& path) {
  auto lastSlash = path.find_last_of("/");
  if (lastSlash == std::string::npos) {
    return {};
  }
  return path.substr(0, lastSlash + 1);
}

std::string ResolvePath(const std::string& baseDir, const std::string& path) {
  if (path.empty() || path[0] == '/' || path.find("://") != std::string::npos) {
    return path;
  }
  return baseDir + path;
}

std::shared_ptr<Data> ReadFileData(const std::string& path,
                                   std::unordered_map<std::string, std::shared_ptr<Data>>* cache) {
  auto it = cache->find(path);
  if (it != cache->end()) {
    return it->second;
  }
  auto data = std::shared_ptr<Data>(nullptr);
  FILE* file = fopen(path.c_str(), "rb");
  if (file != nullptr) {
    fseek(file, 0, SEEK_END);
    auto size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size >= 0) {
      std::vector<uint8_t> bytes(static_cast<size_t>(size));
      if (size > 0) {
        fread(bytes.data(), 1, static_cast<size_t>(size), file);
      }
      data = Data::MakeWithCopy(bytes.data(), bytes.size());
    }
    fclose(file);
  }
  (*cache)[path] = data;
  return data;
}

struct LoadContext {
  std::unordered_map<std::string, std::shared_ptr<Data>> fileCache = {};
  int subDocumentCount = 0;
  bool allFound = true;
};

void LoadInDocument(PAGXDocument* document, const std::string& baseDir, int depth,
                    LoadContext* context) {
  if (depth > kMaxReferenceDepth || context->subDocumentCount >= kMaxSubDocumentInstances) {
    context->allFound = false;
    return;
  }
  // Image nodes with local file paths: batched so a single pass over the nodes feeds them all.
  std::unordered_map<std::string, std::shared_ptr<Data>> imageData = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->data != nullptr || image->filePath.empty() || image->filePath.find("data:") == 0 ||
        image->filePath.find("://") != std::string::npos) {
      continue;
    }
    auto bytes = ReadFileData(ResolvePath(baseDir, image->filePath), &context->fileCache);
    if (bytes == nullptr) {
      context->allFound = false;
      continue;
    }
    imageData[image->filePath] = bytes;
  }
  if (!imageData.empty()) {
    document->loadFileDataMap(imageData);
  }
  // Composition layers: loadFileData parses one fresh sub-document per layer and appends
  // wrapper nodes to the document, so snapshot the pending layers first.
  std::vector<Layer*> layers = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    auto* layer = static_cast<Layer*>(node.get());
    if (layer->composition == nullptr && layer->externalDoc == nullptr &&
        !layer->compositionFilePath.empty()) {
      layers.push_back(layer);
    }
  }
  for (auto* layer : layers) {
    if (context->subDocumentCount >= kMaxSubDocumentInstances) {
      context->allFound = false;
      break;
    }
    auto absolutePath = ResolvePath(baseDir, layer->compositionFilePath);
    auto bytes = ReadFileData(absolutePath, &context->fileCache);
    if (bytes == nullptr) {
      context->allFound = false;
      continue;
    }
    if (!document->loadFileData(layer->compositionFilePath, bytes)) {
      continue;
    }
    context->subDocumentCount++;
    // The sub-document's own references resolve against its own directory.
    LoadInDocument(layer->externalDoc.get(), DirName(absolutePath), depth + 1, context);
  }
}

}  // namespace

bool PAGXExternalFiles::Load(PAGXDocument* document, const std::string& baseDir) {
  if (document == nullptr) {
    return false;
  }
  auto dir = baseDir;
  if (!dir.empty() && dir.back() != '/') {
    dir += '/';
  }
  LoadContext context = {};
  LoadInDocument(document, dir, 0, &context);
  return context.allFound;
}

}  // namespace pagx
