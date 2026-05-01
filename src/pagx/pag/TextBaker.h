// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 8 TextBaker — translates PAGX Text / TextPath / TextModifier nodes
// into their PAGDocument counterparts (ElementTextData /
// ElementTextPathData / ElementTextModifierData). Lives behind its own
// header because the Text baker path has to reach into Text's private
// glyphData via a `friend class pag::TextBaker` declaration added in
// include/pagx/nodes/Text.h — keeping the baker class (rather than a free
// function) lets that friend grant survive.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace pagx {
class Text;
class TextPath;
class TextModifier;
class Font;
}  // namespace pagx

namespace pagx::pag {

struct BakeContext;
struct PAGDocument;
struct VectorElement;

class TextBaker {
 public:
  // Dispatchers invoked by VectorBaker::BakeElement. Each returns a fully
  // populated VectorElement of the matching VectorElementType; nullptr on
  // fatal (ctx->errors already populated).
  static std::unique_ptr<VectorElement> BakeText(BakeContext& ctx, PAGDocument& doc,
                                                 const pagx::Text& src);
  static std::unique_ptr<VectorElement> BakeTextPath(BakeContext& ctx, const pagx::TextPath& src);
  static std::unique_ptr<VectorElement> BakeTextModifier(BakeContext& ctx,
                                                         const pagx::TextModifier& src);

  // Interns a PAGX Font node into PAGDocument::fonts and returns its index.
  // Phase 8 registers every unique pagx::Font* as a System-kind placeholder
  // (family = synthetic "#embedded_<index>", no TTF bytes). Phase 10
  // PAGExporter will upgrade these to Embedded FontAssets by re-running
  // FontEmbedder. Returns UINT32_MAX when `font` is null.
  static uint32_t InternFont(BakeContext& ctx, PAGDocument& doc, const pagx::Font* font);

  // Interns a System-font (family + style) reference used by runtime
  // shaping Text nodes. Phase 8 writes the family/style strings verbatim —
  // Phase 9 Inflater resolves them at load time via platform font lookup.
  static uint32_t InternSystemFont(BakeContext& ctx, PAGDocument& doc, const std::string& family,
                                   const std::string& style);
};

}  // namespace pagx::pag
