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

#include <string>

namespace pagx {

class Node;
class PAGXDocument;

namespace detail {

/**
 * Internal bridge for importers that register addressable nodes while building a document. The
 * declaration lives in the public header so PAGXDocument can befriend it, but the definition is
 * kept here: anything outside the import implementation has no reason to reach for it.
 */
struct DocumentAccess {
  static void RegisterNode(PAGXDocument& document, Node* node, const std::string& id);
};

}  // namespace detail
}  // namespace pagx
