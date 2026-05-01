// Copyright (C) 2026 Tencent. All rights reserved.
//
// Fluent builder for synthesizing PAGX documents in unit tests. Wraps
// PAGXDocument::makeNode<T> + the public mutable fields on Layer / Composition
// so tests can describe DOM trees declaratively without rolling per-test
// scaffolding.
//
// Phase 2 originally promised this helper; we shipped it in Phase 3 instead
// because it intersects with LayerBaker the most naturally — every fluent
// method lands a Layer that LayerBaker has to bake, so the two evolve in
// lock-step.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"
#include "pagx/types/MaskType.h"

namespace pagx::test {

class PAGXBuilder;

// Layer-level fluent handle. Returned by AddLayer / AddCompositionRefLayer /
// inside AddChild closures. Methods return *this so callers can chain.
class LayerHandle {
 public:
  LayerHandle(PAGXBuilder* owner, pagx::Layer* layer) : owner_(owner), layer_(layer) {
  }

  pagx::Layer* layer() const {
    return layer_;
  }

  LayerHandle& Name(std::string n) {
    layer_->name = std::move(n);
    return *this;
  }
  LayerHandle& Visible(bool v) {
    layer_->visible = v;
    return *this;
  }
  LayerHandle& Alpha(float a) {
    layer_->alpha = a;
    return *this;
  }
  LayerHandle& BlendMode(pagx::BlendMode mode) {
    layer_->blendMode = mode;
    return *this;
  }
  LayerHandle& Position(float x, float y) {
    layer_->x = x;
    layer_->y = y;
    return *this;
  }
  LayerHandle& Preserve3D(bool flag) {
    layer_->preserve3D = flag;
    return *this;
  }
  LayerHandle& AntiAlias(bool flag) {
    layer_->antiAlias = flag;
    return *this;
  }
  LayerHandle& GroupOpacity(bool flag) {
    layer_->groupOpacity = flag;
    return *this;
  }
  LayerHandle& PassThroughBackground(bool flag) {
    layer_->passThroughBackground = flag;
    return *this;
  }
  LayerHandle& ScrollRect(float left, float top, float right, float bottom) {
    layer_->scrollRect = pagx::Rect::MakeLTRB(left, top, right, bottom);
    layer_->hasScrollRect = true;
    return *this;
  }
  LayerHandle& Mask(pagx::Layer* target, pagx::MaskType type = pagx::MaskType::Alpha) {
    layer_->mask = target;
    layer_->maskType = type;
    return *this;
  }
  LayerHandle& CompositionRef(pagx::Composition* target) {
    layer_->composition = target;
    return *this;
  }

  // Add a child layer beneath this one. The returned handle is itself
  // chainable so deep trees stay readable:
  //
  //   builder.AddLayer().Name("root").AddChild()
  //          .Name("child").Alpha(0.5f).EndChild();
  LayerHandle AddChild();
  LayerHandle& EndChild() {
    return *this;
  }

 private:
  PAGXBuilder* owner_;
  pagx::Layer* layer_;
};

// Top-level builder. Construct via Make(canvasW, canvasH); end with Done()
// (which calls applyLayout() once and yields the underlying PAGXDocument).
class PAGXBuilder {
 public:
  // Default canvas size is the standard 1080p frame; callers can override.
  static PAGXBuilder Make(float width = 1920.0f, float height = 1080.0f);

  PAGXBuilder(PAGXBuilder&&) = default;
  PAGXBuilder& operator=(PAGXBuilder&&) = default;
  PAGXBuilder(const PAGXBuilder&) = delete;
  PAGXBuilder& operator=(const PAGXBuilder&) = delete;

  // Returns the underlying document without running applyLayout — useful for
  // edge-case tests that want to verify the LayoutNotApplied=100 fatal path.
  std::shared_ptr<pagx::PAGXDocument> RawDocument() const {
    return doc_;
  }

  // Run applyLayout() once and hand back the document, ready for Bake().
  std::shared_ptr<pagx::PAGXDocument> Done();

  // ----- Layer / composition factories -----

  LayerHandle AddLayer();
  LayerHandle AddCompositionRefLayer(pagx::Composition* target);
  pagx::Composition* AddComposition(float width, float height);

  // Inject a custom-built node tree (handy for negative tests that need to
  // poison a specific field after creation).
  pagx::Layer* MakeRawLayer() {
    return doc_->makeNode<pagx::Layer>();
  }

 private:
  explicit PAGXBuilder(std::shared_ptr<pagx::PAGXDocument> doc) : doc_(std::move(doc)) {
  }

  std::shared_ptr<pagx::PAGXDocument> doc_;
};

}  // namespace pagx::test
