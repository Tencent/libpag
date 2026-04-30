// Copyright (C) 2026 Tencent. All rights reserved.
#include "StructBuilders.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace pagx::test {

using namespace pagx::pag;

namespace {

std::unique_ptr<Layer> MakeEmptyLayer() {
  auto l = std::make_unique<Layer>();
  l->type = LayerType::Layer;
  l->name = "";
  l->startTime = 0;
  l->duration = 1;
  l->stretch = {1, 1};
  return l;
}

std::unique_ptr<Composition> MakeEmptyComposition(std::string id, uint32_t w, uint32_t h) {
  auto c = std::make_unique<Composition>();
  c->id = std::move(id);
  c->width = w;
  c->height = h;
  return c;
}

std::unique_ptr<PAGDocument> MakeBaseDocument() {
  auto doc = std::make_unique<PAGDocument>();
  doc->header.width = 1920.0f;
  doc->header.height = 1080.0f;
  doc->header.backgroundColor = Color{};
  doc->header.frameRate = {24, 1};
  doc->header.duration = 1;
  return doc;
}

}  // namespace

std::unique_ptr<PAGDocument> MakeDeepLayerStack(uint32_t depth) {
  auto doc = MakeBaseDocument();
  auto comp = MakeEmptyComposition("deep", 1920, 1080);

  if (depth == 0) {
    doc->compositions.push_back(std::move(comp));
    return doc;
  }

  // Build the chain bottom-up so each parent owns its single child slot.
  std::unique_ptr<Layer> tail;
  for (uint32_t i = 0; i < depth; ++i) {
    auto layer = MakeEmptyLayer();
    layer->name = "L" + std::to_string(i);
    if (tail) {
      layer->children.push_back(std::move(tail));
    }
    tail = std::move(layer);
  }

  comp->layers.push_back(std::move(tail));
  doc->compositions.push_back(std::move(comp));
  return doc;
}

std::unique_ptr<PAGDocument> MakeWideSiblingTree(uint32_t width) {
  auto doc = MakeBaseDocument();
  auto comp = MakeEmptyComposition("wide", 1920, 1080);
  auto root = MakeEmptyLayer();
  root->name = "root";
  root->children.reserve(width);
  for (uint32_t i = 0; i < width; ++i) {
    auto child = MakeEmptyLayer();
    child->name = "C" + std::to_string(i);
    root->children.push_back(std::move(child));
  }
  comp->layers.push_back(std::move(root));
  doc->compositions.push_back(std::move(comp));
  return doc;
}

std::unique_ptr<PAGDocument> MakeLayerWithKMasks(uint32_t k) {
  auto doc = MakeBaseDocument();
  auto comp = MakeEmptyComposition("kmasks", 1920, 1080);

  // layers[0] is the mask target. layers[1] is the host carrying k mask refs.
  auto target = MakeEmptyLayer();
  target->name = "target";

  auto host = MakeEmptyLayer();
  host->name = "host";
  // Single mask field on Layer in the current model — the K-mask scenario is
  // simulated by repeating the path k times in the host's vector (the
  // Inflater's pending bookkeeping cap is what we want to stress, not the
  // wire layout). Each element is the path to layers[0].
  host->maskLayerPath = {0};
  host->maskType = LayerMaskType::Alpha;
  // Stash k via a dedicated children-of-host expansion so the cap test can
  // cause k pending entries by walking host k times. Inflater logic uses one
  // entry per (layer, mask) pair; we duplicate child layers each holding the
  // mask reference to drive the count predictably.
  for (uint32_t i = 0; i < k; ++i) {
    auto sibling = MakeEmptyLayer();
    sibling->name = "m" + std::to_string(i);
    sibling->maskLayerPath = {0};
    sibling->maskType = LayerMaskType::Alpha;
    host->children.push_back(std::move(sibling));
  }

  comp->layers.push_back(std::move(target));
  comp->layers.push_back(std::move(host));
  doc->compositions.push_back(std::move(comp));
  return doc;
}

std::unique_ptr<PAGDocument> MakeCompositionRefCycle() {
  auto doc = MakeBaseDocument();

  // comp[0] ── ref ──> comp[1] ── ref ──> comp[0]
  auto compA = MakeEmptyComposition("compA", 1920, 1080);
  auto refA = MakeEmptyLayer();
  refA->type = LayerType::CompositionRef;
  refA->compositionIndex = 1;
  compA->layers.push_back(std::move(refA));

  auto compB = MakeEmptyComposition("compB", 1920, 1080);
  auto refB = MakeEmptyLayer();
  refB->type = LayerType::CompositionRef;
  refB->compositionIndex = 0;
  compB->layers.push_back(std::move(refB));

  doc->compositions.push_back(std::move(compA));
  doc->compositions.push_back(std::move(compB));
  return doc;
}

}  // namespace pagx::test
