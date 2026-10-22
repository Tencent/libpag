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

#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

// Export-time stripping (design doc §4.6). Runs after layout flattening (T2) and before
// resource collection / encoding. Destructive: operates in place on the given document.
class Stripper {
 public:
  // Collects every id that must survive stripping: Layer ids (PAGLayer::id() is a public
  // API), ids referenced by animation objects and data binds (runtime string lookups),
  // Animation and StateMachine ids, and StateMachine input / state / region names.
  static std::unordered_set<std::string> CollectRetainedIds(const PAGXDocument* document);

  // Removes data-* from every node except ViewModelProperty, the document root, and
  // ImagePattern (C27); clears sourceLine, import directives, and Font.file paths.
  static void StripMetadata(PAGXDocument* document);

  // Clears the id of every node whose id is not in the retained set.
  static void StripIds(PAGXDocument* document, const std::unordered_set<std::string>& retainedIds);

  // Removes nodes the renderer can never reach: invisible layers that are not animation
  // or data-bind targets and not masks, empty Groups with no referenced children, and
  // geometry elements that no painter can consume.
  static void RemoveDeadNodes(PAGXDocument* document,
                              const std::unordered_set<std::string>& retainedIds);

  // All stripping steps in pipeline order.
  static void Strip(PAGXDocument* document);
};

}  // namespace pagx
