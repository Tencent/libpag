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

#include "pagx/PAGFile.h"
#include "pagx/animation/Animation.h"

namespace pagx {

std::shared_ptr<PAGFile> PAGFile::Make(std::shared_ptr<PAGXDocument> document) {
  if (document == nullptr) {
    return nullptr;
  }
  auto file = std::shared_ptr<PAGFile>(new PAGFile());
  file->document = document;
  // PR1 only registers the file itself; node-level reverse mapping is populated when the runtime
  // tree is actually built (PR6+). Pass an empty referenced-node list for now.
  document->registerLiveFile(file, {});
  return file;
}

PAGFile::~PAGFile() {
  if (document != nullptr) {
    document->unregisterLiveFile(this);
  }
}

std::vector<std::string> PAGFile::getTimelineNames() const {
  std::vector<std::string> names = {};
  if (document == nullptr) {
    return names;
  }
  names.reserve(document->animations.size());
  for (auto* animation : document->animations) {
    if (animation != nullptr) {
      names.push_back(animation->name);
    }
  }
  return names;
}

std::shared_ptr<PAGTimeline> PAGFile::getTimeline(const std::string& name) {
  if (document == nullptr) {
    return nullptr;
  }
  Animation* matched = nullptr;
  for (auto* animation : document->animations) {
    if (animation != nullptr && animation->name == name) {
      matched = animation;
      break;
    }
  }
  if (matched == nullptr) {
    return nullptr;
  }
  auto it = timelinesByAnimation.find(matched);
  if (it != timelinesByAnimation.end()) {
    return it->second;
  }
  // Construct via private ctor; std::make_shared cannot access it.
  auto timeline = std::shared_ptr<PAGTimeline>(
      new PAGTimeline(std::weak_ptr<PAGFile>(shared_from_this()), matched));
  timelinesByAnimation.emplace(matched, timeline);
  return timeline;
}

std::shared_ptr<PAGTimeline> PAGFile::getDefaultTimeline() {
  if (document == nullptr || document->animations.empty()) {
    return nullptr;
  }
  auto* first = document->animations.front();
  if (first == nullptr) {
    return nullptr;
  }
  return getTimeline(first->name);
}

bool PAGFile::draw(const std::shared_ptr<PAGSurface>& /*surface*/) {
  // TODO(PR5/PR6): wire to tgfx::DisplayList draw against the surface backend.
  return false;
}

float PAGFile::getWidth() const {
  return document != nullptr ? document->width : 0.0f;
}

float PAGFile::getHeight() const {
  return document != nullptr ? document->height : 0.0f;
}

void PAGFile::onNodesChanged(const std::vector<Node*>& /*dirtyNodes*/) {
  // TODO(PR11): rebuild affected runtime sub-trees and reset relevant timelines.
}

}  // namespace pagx
