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
#include <unordered_set>

namespace pagx {

struct DOMNode;
class HTMLDiagnosticSink;
class Layer;

/**
 * Owns the document-wide pool of id strings used to disambiguate generated layers from
 * author-supplied ones. Every author-supplied id discovered by `collectAll` is reserved so
 * that subsequent `generateUnique` calls cannot collide; `assign` writes the element's
 * id (when present) onto a Layer.
 */
class HTMLIdAllocator {
 public:
  /**
   * Walks the DOM and reserves every element's `id` attribute. Recursion is bounded by
   * `MAX_HTML_RECURSION_DEPTH`; oversize subtrees are skipped after a diagnostic on `sink`.
   */
  void collectAll(const std::shared_ptr<DOMNode>& node, HTMLDiagnosticSink& sink, int depth = 0);

  /**
   * Returns the element's `id` attribute verbatim, or an empty string when absent. The
   * pre-walk by `collectAll` guarantees the returned id is already in `_existingIds` so
   * subsequent `generateUnique` calls steer clear.
   */
  std::string consume(const std::shared_ptr<DOMNode>& element);

  /**
   * Copies the element's id (when present) onto `layer->id`. No-op when either is null or
   * when the element has no id attribute.
   */
  void assign(Layer* layer, const std::shared_ptr<DOMNode>& element);

  /**
   * Allocates a fresh id of the form `<prefix><n>` whose value does not collide with any
   * previously reserved id (author-supplied or previously generated).
   */
  std::string generateUnique(const std::string& prefix);

 private:
  std::unordered_set<std::string> _existingIds = {};
  int _nextGeneratedId = 0;
};

}  // namespace pagx
