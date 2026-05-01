// Copyright (C) 2026 Tencent. All rights reserved.
#include "PAGXBuilder.h"
#include <utility>

namespace pagx::test {

LayerHandle LayerHandle::AddChild() {
  pagx::Layer* child = owner_->MakeRawLayer();
  layer_->children.push_back(child);
  return LayerHandle(owner_, child);
}

PAGXBuilder PAGXBuilder::Make(float width, float height) {
  auto doc = pagx::PAGXDocument::Make(width, height);
  return PAGXBuilder(std::move(doc));
}

std::shared_ptr<pagx::PAGXDocument> PAGXBuilder::Done() {
  doc_->applyLayout();
  return doc_;
}

LayerHandle PAGXBuilder::AddLayer() {
  pagx::Layer* layer = doc_->makeNode<pagx::Layer>();
  doc_->layers.push_back(layer);
  return LayerHandle(this, layer);
}

LayerHandle PAGXBuilder::AddCompositionRefLayer(pagx::Composition* target) {
  pagx::Layer* layer = doc_->makeNode<pagx::Layer>();
  layer->composition = target;
  doc_->layers.push_back(layer);
  return LayerHandle(this, layer);
}

pagx::Composition* PAGXBuilder::AddComposition(float width, float height) {
  auto* comp = doc_->makeNode<pagx::Composition>();
  comp->width = width;
  comp->height = height;
  return comp;
}

}  // namespace pagx::test
