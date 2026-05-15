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

#include "pagx/HTMLSubsetTransformer.h"
#include <memory>
#include <utility>
#include <vector>
#include "pagx/html/HTMLTransformContext.h"
#include "pagx/html/HTMLTransformPasses.h"

namespace pagx {

//==================================================================================================
// HTMLTransformContext
//==================================================================================================

HTMLTransformContext::HTMLTransformContext(const HTMLSubsetTransformer::Options& options)
    : _options(options) {
}

HTMLTransformContext::~HTMLTransformContext() = default;

bool HTMLTransformContext::warn(const std::string& code, const std::string& message,
                                const std::shared_ptr<DOMNode>& node) {
  HTMLSubsetTransformer::Diagnostic diag = {};
  diag.severity = _options.strict ? HTMLSubsetTransformer::Severity::Error
                                  : HTMLSubsetTransformer::Severity::Warning;
  diag.code = code;
  diag.message = message;
  if (node) {
    diag.nodeName = node->name;
    diag.line = node->line;
  }
  _diagnostics.push_back(std::move(diag));
  if (_options.strict) {
    _hasFatal = true;
    return false;
  }
  return true;
}

void HTMLTransformContext::error(const std::string& code, const std::string& message,
                                 const std::shared_ptr<DOMNode>& node) {
  HTMLSubsetTransformer::Diagnostic diag = {};
  diag.severity = HTMLSubsetTransformer::Severity::Error;
  diag.code = code;
  diag.message = message;
  if (node) {
    diag.nodeName = node->name;
    diag.line = node->line;
  }
  _diagnostics.push_back(std::move(diag));
  _hasFatal = true;
}

void HTMLTransformContext::addClassRule(const std::string& className,
                                        const PropertyMap& declarations) {
  auto& slot = _classRules[className];
  for (const auto& kv : declarations) {
    slot[kv.first] = kv.second;
  }
}

void HTMLTransformContext::addElementRule(const std::string& tagName,
                                          const PropertyMap& declarations) {
  auto& slot = _elementRules[tagName];
  for (const auto& kv : declarations) {
    slot[kv.first] = kv.second;
  }
}

PropertyMap* HTMLTransformContext::findResolved(const DOMNode* node) {
  auto it = _resolved.find(node);
  if (it == _resolved.end()) return nullptr;
  return &it->second;
}

void HTMLTransformContext::setResolved(const DOMNode* node, PropertyMap value) {
  _resolved[node] = std::move(value);
}

//==================================================================================================
// HTMLSubsetTransformer::Builder
//==================================================================================================

struct HTMLSubsetTransformer::Builder::Impl {
  Options options = {};
  std::vector<std::unique_ptr<HTMLTransformPass>> passes = {};
};

HTMLSubsetTransformer::Builder::Builder() : _impl(std::make_unique<Impl>()) {
}

HTMLSubsetTransformer::Builder::~Builder() = default;

HTMLSubsetTransformer::Builder::Builder(Builder&&) noexcept = default;
HTMLSubsetTransformer::Builder& HTMLSubsetTransformer::Builder::operator=(Builder&&) noexcept =
    default;

HTMLSubsetTransformer::Builder& HTMLSubsetTransformer::Builder::withOptions(
    const Options& options) {
  _impl->options = options;
  return *this;
}

HTMLSubsetTransformer::Builder& HTMLSubsetTransformer::Builder::addDefaultPasses() {
  _impl->passes.push_back(std::make_unique<html_passes::DocumentSkeletonPass>());
  _impl->passes.push_back(std::make_unique<html_passes::StyleSheetCollectorPass>());
  _impl->passes.push_back(std::make_unique<html_passes::ComputedStylePass>());
  _impl->passes.push_back(std::make_unique<html_passes::PropertyFilterPass>());
  // AbsoluteToFlexInference is always wired in; it self-disables when
  // `Options::inferFlexFromAbsolute` is false so the default pipeline behaviour is unchanged.
  _impl->passes.push_back(std::make_unique<html_passes::AbsoluteToFlexInferencePass>());
  _impl->passes.push_back(std::make_unique<html_passes::StructureNormalizationPass>());
  _impl->passes.push_back(std::make_unique<html_passes::InlineStyleEmitterPass>());
  return *this;
}

HTMLSubsetTransformer::Builder& HTMLSubsetTransformer::Builder::addPass(
    std::unique_ptr<HTMLTransformPass> pass) {
  if (pass) {
    _impl->passes.push_back(std::move(pass));
  }
  return *this;
}

HTMLSubsetTransformer::Result HTMLSubsetTransformer::Builder::run(
    const std::shared_ptr<DOMNode>& root) {
  HTMLTransformContext ctx(_impl->options);
  for (auto& pass : _impl->passes) {
    pass->apply(root, ctx);
    if (ctx.hasFatal()) break;
  }
  Result result = {};
  result.diagnostics = ctx.takeDiagnostics();
  result.ok = !ctx.hasFatal();
  return result;
}

//==================================================================================================
// HTMLSubsetTransformer::Transform (default pipeline)
//==================================================================================================

HTMLSubsetTransformer::Result HTMLSubsetTransformer::Transform(
    const std::shared_ptr<DOMNode>& root, const Options& options) {
  Builder builder;
  builder.withOptions(options).addDefaultPasses();
  return builder.run(root);
}

}  // namespace pagx
