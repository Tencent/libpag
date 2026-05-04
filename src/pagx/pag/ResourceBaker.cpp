// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/ResourceBaker.h"

namespace pagx::pag {

namespace {

// Generic three-tier intern. Promotes the asset into the destination vector
// and writes back into all three lookup maps so the next call short-circuits
// at whichever tier matches.
template <typename Asset>
uint32_t InternAsset(std::vector<std::unique_ptr<Asset>>& destination,
                     std::unordered_map<const void*, uint32_t>& byNode,
                     std::unordered_map<const tgfx::Data*, uint32_t>& byDataPtr,
                     std::unordered_map<std::string, uint32_t>& byKey, std::unique_ptr<Asset> asset,
                     const void* nodePtr, std::string semanticKey) {
  // Tier 1 — pointer-equal node already interned this run.
  if (nodePtr != nullptr) {
    auto it = byNode.find(nodePtr);
    if (it != byNode.end()) {
      return it->second;
    }
  }
  // Tier 2 — same embedded Data instance reused from another node.
  const tgfx::Data* dataPtr = asset != nullptr ? asset->data.get() : nullptr;
  if (dataPtr != nullptr) {
    auto it = byDataPtr.find(dataPtr);
    if (it != byDataPtr.end()) {
      uint32_t hit = it->second;
      if (nodePtr != nullptr) {
        byNode.emplace(nodePtr, hit);
      }
      return hit;
    }
  }
  // Tier 3 — semantic key (URI / abs path / "system\0family\0style").
  if (!semanticKey.empty()) {
    auto it = byKey.find(semanticKey);
    if (it != byKey.end()) {
      uint32_t hit = it->second;
      if (nodePtr != nullptr) {
        byNode.emplace(nodePtr, hit);
      }
      if (dataPtr != nullptr) {
        byDataPtr.emplace(dataPtr, hit);
      }
      return hit;
    }
  }
  // Miss — insert a fresh slot and back-fill every applicable cache tier.
  uint32_t newIndex = static_cast<uint32_t>(destination.size());
  destination.push_back(std::move(asset));
  if (nodePtr != nullptr) {
    byNode.emplace(nodePtr, newIndex);
  }
  if (dataPtr != nullptr) {
    byDataPtr.emplace(dataPtr, newIndex);
  }
  if (!semanticKey.empty()) {
    byKey.emplace(std::move(semanticKey), newIndex);
  }
  return newIndex;
}

}  // namespace

uint32_t RegisterImage(BakeContext& ctx, PAGDocument& doc, std::unique_ptr<ImageAsset> asset,
                       const void* nodePtr, std::string semanticKey) {
  return InternAsset(doc.images, ctx.imageIndexByNode, ctx.imageIndexByDataPtr, ctx.imageIndexByKey,
                     std::move(asset), nodePtr, std::move(semanticKey));
}

}  // namespace pagx::pag
