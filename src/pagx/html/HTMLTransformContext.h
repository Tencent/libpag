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
#include <unordered_map>
#include <vector>
#include "pagx/HTMLSubsetTransformer.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

/**
 * Resolved property map for a single element. Keys are lower-case CSS property names; values
 * are trimmed but otherwise verbatim. Order is irrelevant inside the map; the serialiser sorts
 * keys deterministically when emitting `style="…"`.
 */
using PropertyMap = std::unordered_map<std::string, std::string>;

class HTMLTransformContext {
 public:
  explicit HTMLTransformContext(const HTMLSubsetTransformer::Options& options);
  ~HTMLTransformContext();
  HTMLTransformContext(const HTMLTransformContext&) = delete;
  HTMLTransformContext& operator=(const HTMLTransformContext&) = delete;

  const HTMLSubsetTransformer::Options& options() const {
    return _options;
  }

  // Diagnostics ----------------------------------------------------------------
  // Appends a warning. Returns true when processing should continue, false when `strict` mode
  // requires aborting.
  bool warn(const std::string& code, const std::string& message,
            const std::shared_ptr<DOMNode>& node = nullptr);

  // Appends a hard error and signals abort.
  void error(const std::string& code, const std::string& message,
             const std::shared_ptr<DOMNode>& node = nullptr);

  bool hasFatal() const {
    return _hasFatal;
  }

  const std::vector<HTMLSubsetTransformer::Diagnostic>& diagnostics() const {
    return _diagnostics;
  }

  std::vector<HTMLSubsetTransformer::Diagnostic> takeDiagnostics() {
    return std::move(_diagnostics);
  }

  // CSS rule tables ------------------------------------------------------------
  // Adds a rule to the class registry (class name without leading `.`).
  void addClassRule(const std::string& className, const PropertyMap& declarations);

  // Adds a rule to the element registry (tag name, lower-cased).
  void addElementRule(const std::string& tagName, const PropertyMap& declarations);

  const std::unordered_map<std::string, PropertyMap>& classRules() const {
    return _classRules;
  }
  const std::unordered_map<std::string, PropertyMap>& elementRules() const {
    return _elementRules;
  }

  // Resolved style cache -------------------------------------------------------
  // Returns the resolved style of `node` (may be a previously computed cached value, or
  // nullptr when none has been recorded).
  PropertyMap* findResolved(const DOMNode* node);

  // Stores or updates the resolved style for `node`.
  void setResolved(const DOMNode* node, PropertyMap value);

  // Canvas size (mirrors the importer's options). NaN when unknown.
  float canvasWidth() const {
    return _options.canvasWidth;
  }
  float canvasHeight() const {
    return _options.canvasHeight;
  }

 private:
  HTMLSubsetTransformer::Options _options;
  std::vector<HTMLSubsetTransformer::Diagnostic> _diagnostics = {};
  std::unordered_map<std::string, PropertyMap> _classRules = {};
  std::unordered_map<std::string, PropertyMap> _elementRules = {};
  std::unordered_map<const DOMNode*, PropertyMap> _resolved = {};
  bool _hasFatal = false;
};

/**
 * Base class for transformer pipeline stages. Forward-declared in `pagx/HTMLSubsetTransformer.h`
 * to keep the public header thin; full definition lives here.
 */
class HTMLTransformPass {
 public:
  HTMLTransformPass() = default;
  virtual ~HTMLTransformPass() = default;
  HTMLTransformPass(const HTMLTransformPass&) = delete;
  HTMLTransformPass& operator=(const HTMLTransformPass&) = delete;

  virtual const char* name() const = 0;
  virtual void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) = 0;
};

}  // namespace pagx
