// Copyright (C) 2026 Tencent. All rights reserved.
//
// TextBaker — translates PAGX Text / TextPath / TextModifier nodes into their
// PAGDocument counterparts (ElementTextData / ElementTextPathData /
// ElementTextModifierData).
//
// Phase 16 (v2.20): runtime-shape mode. TextBaker flattens pagx::Text fields
// (text / fontFamily / fontStyle / fontSize / letterSpacing / fauxBold / ...)
// directly into ElementTextData; the Inflater re-shapes at load time through
// FontProvider + TextShaper. Pre-shaped glyphRuns are dropped (per-glyph
// xform information is lost) and the Baker emits TextGlyphRunsDowngraded=208
// so callers can see the downgrade explicitly.
//
// TextBaker stays a class rather than a free function because future Phases
// reintroducing richer Text access may need the `friend class pag::TextBaker`
// grant already declared inside include/pagx/nodes/Text.h.
#pragma once

#include <memory>

namespace pagx {
class Text;
class TextPath;
class TextModifier;
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
};

}  // namespace pagx::pag
