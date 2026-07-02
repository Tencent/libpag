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

#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/nodes/Layer.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

void HTMLIdAllocator::collectAll(const std::shared_ptr<DOMNode>& node, HTMLDiagnosticSink& sink,
                                 int depth) {
  if (!node) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    sink.warn("html: maximum recursion depth reached during id collection; subtree skipped");
    return;
  }
  if (node->type == DOMNodeType::Element) {
    auto* idAttr = node->findAttribute("id");
    if (idAttr && !idAttr->empty()) {
      _existingIds.insert(*idAttr);
    }
  }
  auto child = node->getFirstChild();
  while (child) {
    collectAll(child, sink, depth + 1);
    child = child->getNextSibling();
  }
}

std::string HTMLIdAllocator::consume(const std::shared_ptr<DOMNode>& element) {
  if (!element) return {};
  auto* idAttr = element->findAttribute("id");
  if (!idAttr || idAttr->empty()) return {};
  // The PAGX layer registry is populated lazily by `makeNode<>`, never with an explicit id from
  // this importer, so collisions can only come from the source DOM itself. `_existingIds` already
  // tracks every DOM-side id discovered by `collectAll`, so we trust the author and forward
  // the id verbatim.
  return *idAttr;
}

void HTMLIdAllocator::assign(Layer* layer, const std::shared_ptr<DOMNode>& element) {
  if (layer == nullptr) return;
  std::string id = consume(element);
  if (!id.empty()) layer->id = id;
}

std::string HTMLIdAllocator::generateUnique(const std::string& prefix) {
  std::string id;
  do {
    id = prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0);
  _existingIds.insert(id);
  return id;
}

}  // namespace pagx
