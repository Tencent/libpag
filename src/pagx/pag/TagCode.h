// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 TagCode registry — authoritative source: design doc §6.1.
// Segment layout (each with ~50% headroom for future additions):
//
//   0         : End (stream terminator)
//   1-9       : top-level (FileHeader / AssetTable / Composition / AssetItem)
//   10-19     : Layer block + its sub-tags (Transform / MaskRef / Filters / Styles)
//   20-39     : Payload (Shape / Text / Image / Solid / Vector / Mesh / CompositionRef)
//   40-119    : VectorElement (14 types + 3D / Motion Path / AE modifiers)
//   120-139   : LayerFilter
//   140-159   : LayerStyle
//   160-239   : animation-only (keyframes / interpolation curves / RangeSelector v2)
//   240-299   : resource extensions (SVG fragment / Lottie / HDR color, ...)
//   900-1021  : experimental / third-party (16-byte UUID prefixed bodies)
//   1022      : ErrorMarker (Baker fatal abort marker)
//   1023      : reserved — never allocated (aligns with v1 10-bit field upper bound)
#pragma once

#include <cstdint>

namespace pagx::pag {

enum class TagCode : uint16_t {
  End = 0,

  // ---- Top-level 1-9 ----
  FileHeader = 1,
  ImageAssetTable = 2,
  FontAssetTable = 3,
  CompositionList = 4,
  Composition = 5,
  ImageAssetItem = 6,
  FontAssetItem = 7,
  // Phase 17 (v2.23): path-based embedded fonts owned by the PAG document
  // itself. Mirrors PAGX <Font id="..."><Glyph advance="..." path="..."/></Font>.
  // Not a ttf/otf subset; ttf font files are never embedded in PAG.
  EmbeddedFontTable = 8,
  EmbeddedFontItem = 9,

  // ---- Layer block + sub-tags 10-19 ----
  LayerBlock = 10,
  LayerMaskRef = 12,
  LayerFilters = 13,
  LayerStyles = 14,
  LayerTransform = 15,

  // ---- Payload 20-39 ----
  ShapePayload = 20,
  TextPayload = 21,
  ImagePayload = 22,
  SolidPayload = 23,
  VectorPayload = 24,
  MeshPayload = 25,
  CompositionRefPayload = 26,

  // ---- VectorElement 40-119 (14 element types in use; 40-53, pinned by §D.11) ----
  ElementRectangle = 40,
  ElementEllipse = 41,
  ElementPolystar = 42,
  ElementShapePath = 43,
  ElementFillStyle = 44,
  ElementStrokeStyle = 45,
  ElementTrimPath = 46,
  ElementRoundCorner = 47,
  ElementMergePath = 48,
  ElementRepeater = 49,
  ElementText = 50,
  ElementTextPath = 51,
  ElementTextModifier = 52,
  ElementVectorGroup = 53,

  // ---- LayerFilter 120-139 (pinned by §D.12) ----
  FilterBlur = 120,
  FilterDropShadow = 121,
  FilterInnerShadow = 122,
  FilterColorMatrix = 123,
  FilterBlend = 124,

  // ---- LayerStyle 140-159 (pinned by §D.12) ----
  StyleDropShadow = 140,
  StyleInnerShadow = 141,
  StyleBackgroundBlur = 142,

  // ---- Special 1022 ----
  ErrorMarker = 1022,
};

}  // namespace pagx::pag
