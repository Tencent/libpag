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

#include <optional>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

class FontConfig;

/**
 * Controls how content SVG cannot represent natively is handled. VectorOnly forbids new effect
 * bitmaps but keeps authored images and bitmap glyphs; PreserveAppearance may bake a complete
 * isolated region and fails instead of silently degrading; BestEffort reports every approximation
 * through the diagnostics sink.
 */
enum class SVGStaticExportPolicy {
  VectorOnly,
  PreserveAppearance,
  BestEffort,
};

/**
 * Controls text emission. Fidelity uses the layout-resolved glyph data (WOFF2 / outline / bitmap),
 * while Native keeps readable text that the target may reshape.
 */
enum class SVGTextExportMode {
  Native,
  Fidelity,
};

/**
 * Selects which document animations are compiled into SMIL. Default follows the runtime's
 * getDefaultTimeline (the first declared Animation); Named selects one id; All exports every
 * top-level Animation in declaration order.
 */
enum class SVGAnimationSelection {
  Default,
  All,
  Named,
};

/**
 * Controls whether animations with channels SVG cannot express are rejected or exported with
 * explicit warnings. Strict fails the export; BestEffort keeps the expressible subset.
 */
enum class SVGAnimationPolicy {
  Strict,
  BestEffort,
};

/**
 * Animation export options.
 */
struct SVGAnimationOptions {
  /**
   * Which animations to export. The default is Default.
   */
  SVGAnimationSelection selection = SVGAnimationSelection::Default;

  /**
   * The Animation id used when selection is Named. Ignored for the other modes.
   */
  std::string animationId = {};

  /**
   * The animation policy. The default is Strict.
   */
  SVGAnimationPolicy policy = SVGAnimationPolicy::Strict;
};

/**
 * The action taken for a diagnostic entry.
 */
enum class SVGDiagnosticAction {
  Exact,
  Approximated,
  Frozen,
  Dropped,
  Rejected,
};

/**
 * A structured export diagnostic. reasonCode is stable for programmatic handling; the message is
 * for humans. Empty fields mean the diagnostic is not tied to a specific node/channel.
 */
struct SVGDiagnostic {
  /**
   * Stable reason code, for example "unsupported-feature" or "budget-exceeded".
   */
  std::string reasonCode = {};

  /**
   * Stable path of the document instance that produced the diagnostic.
   */
  std::string instancePath = {};

  /**
   * Authored node id when the diagnostic is tied to a node.
   */
  std::string nodeId = {};

  /**
   * Channel name or affected output element.
   */
  std::string channelOrOutput = {};

  /**
   * The output class produced for the affected content: native, expanded, baked, or none.
   */
  std::string outputClass = {};

  /**
   * The state source used for evaluation, for example "template-static".
   */
  std::string stateSource = {};

  /**
   * Consumed draw range or the frozen export time, when the action is approximated or frozen.
   */
  std::string consumedRangeOrFreezeTime = {};

  /**
   * Human-readable message.
   */
  std::string message = {};

  /**
   * The action taken for this entry.
   */
  SVGDiagnosticAction action = SVGDiagnosticAction::Dropped;
};

/**
 * Hard export budgets. A zero field selects the built-in fixed limit; the limit cannot be raised
 * to infinity. Callers may pass smaller values to make a rejection testable.
 */
struct SVGBudget {
  /**
   * Maximum reference/instance nesting depth.
   */
  size_t maxReferenceDepth = 128;

  /**
   * Maximum number of expanded document instances.
   */
  size_t maxInstances = 10000;

  /**
   * Maximum total geometry points across the export session.
   */
  size_t maxGeometryPoints = 1000000;

  /**
   * Maximum total final glyphs across the export session.
   */
  size_t maxGlyphs = 1000000;

  /**
   * Maximum generated draw elements produced by modifiers such as Repeater.
   */
  size_t maxGeneratedElements = 200000;

  /**
   * Maximum samples emitted for one animation track.
   */
  size_t maxSamplesPerTrack = 4096;

  /**
   * Maximum samples emitted across the whole export session.
   */
  size_t maxTotalSamples = 65536;

  /**
   * Maximum subdivision depth used when certifying a continuous track.
   */
  size_t maxSubdivisionDepth = 24;

  /**
   * Maximum number of segments expanded from a bounded common animation period.
   */
  size_t maxCommonPeriodSegments = 10000;

  /**
   * Maximum total raster pixels produced by bake patches.
   */
  size_t maxRasterPixels = 67108864;

  /**
   * Maximum width or height of one bake surface.
   */
  size_t maxRasterEdge = 8192;

  /**
   * Maximum encoded image bytes across the export session.
   */
  size_t maxEncodedBytes = 67108864;

  /**
   * Maximum size of the produced SVG document.
   */
  size_t maxOutputBytes = 67108864;
};

/**
 * Export options for SVGExporter.
 */
struct SVGExportOptions {
  /**
   * Indentation spaces for the output SVG. The default value is 2.
   */
  int indent = 2;

  /**
   * The static export policy. The default is PreserveAppearance: native output is preferred, and
   * content that cannot be represented natively is baked only when a complete isolated composition
   * can be produced; otherwise the export fails. VectorOnly rejects content that would need a new
   * effect bitmap while keeping authored images and bitmap glyphs.
   */
  SVGStaticExportPolicy staticPolicy = SVGStaticExportPolicy::PreserveAppearance;

  /**
   * The text export mode. The default is Fidelity: the layout-resolved glyphs are authoritative and
   * are emitted as WOFF2 @font-face text, outlines, or the original bitmap glyphs. Native keeps a
   * readable string that the target may reshape with a different font.
   */
  SVGTextExportMode textMode = SVGTextExportMode::Fidelity;

  /**
   * Whether to include the XML declaration (<?xml ...?>) at the top. The default value is true.
   */
  bool xmlDeclaration = true;

  /**
   * Optional FontConfig for text layout. When provided, registered and fallback fonts from the
   * FontConfig are used for text shaping and layout. When nullptr, the exporter falls back to
   * system fonts via platform-native font lookup.
   */
  FontConfig* fontConfig = nullptr;

  /**
   * Optional structured diagnostics sink. Entries mirror the warnings that affect output loss or
   * approximation and additionally carry a stable reason code and the action taken.
   */
  std::vector<SVGDiagnostic>* diagnostics = nullptr;

  /**
   * Hard budgets for generated geometry and output bytes.
   */
  SVGBudget budget = {};

  /**
   * Oversampling factor applied when a layer or tiled pattern has to be baked to PNG. The Surface
   * behind every bake is sized at `ceil(logicalSize * rasterScale)` physical pixels, while the
   * placed &lt;image&gt; keeps using logical dimensions so consumers stretch the denser bitmap
   * over the same visible extent, yielding a crisper result.
   *
   * Valid range: (0, 4]. Values outside this range are clamped at the entry point — passing 0 or
   * a negative value would otherwise silently disable rasterization (the bake would produce a
   * zero-pixel surface and the exporter would fall through to the vector path, contradicting the
   * PreserveAppearance policy). The default is 2.0 (i.e. @2x assets), matching the
   * default of HTMLExportOptions::rasterScale.
   */
  float rasterScale = 2.0f;

  /**
   * Whether to embed vector Font resources as WOFF2 @font-face rules with base64 data URIs and
   * render their Text elements via &lt;text&gt; with PUA Unicode characters. When enabled, Text
   * nodes whose GlyphRun references an embeddable vector Font become real &lt;text&gt; elements —
   * selectable, searchable, and animatable per character — instead of opaque outline &lt;path&gt;
   * elements. Bitmap (CBDT) fonts and GlyphRuns that carry per-glyph scales / skews remain on the
   * outline path because plain SVG &lt;text&gt; cannot express them. When disabled, every Text
   * with GlyphRun data is emitted as &lt;path&gt;. This is a glyph encoding preference only: it does
   * not allow TextBox to bypass the authoritative layout glyphs. The default value is true.
   */
  bool embedFontsAsWoff2 = true;

  /**
   * Whether to export PAGX animations as SVG SMIL elements. A document without animations produces
   * byte-identical output either way, because the exporter only changes structure for elements that
   * actually carry an exportable channel. Channels without a native SMIL equivalent (text content,
   * blend modes, path data) are reported through the warnings sink and dropped.
   *
   * The export is template-static: it reads the document's authored state and serializes tracks, but
   * never advances a timeline, evaluates a StateMachine, or applies a default DataBind. State that
   * only exists at runtime (host input history, a scene snapshot) cannot be recovered here.
   * The default value is true.
   */
  bool exportAnimation = true;

  /**
   * Animation selection options. Ignored when exportAnimation is false.
   */
  SVGAnimationOptions animation = {};
};

/**
 * SVGExporter converts a PAGXDocument into SVG format.
 * This is the reverse of SVGImporter: it maps PAGX node types back to SVG elements.
 * GPU locking: callers must not hold the same GPU device's context lock while calling export
 * methods. The exporter acquires and releases device locks internally when rasterizing assets.
 */
class SVGExporter {
 public:
  using Options = SVGExportOptions;

  /**
   * Exports a PAGXDocument to an SVG string.
   * @param document the PAGXDocument to export. Passed as non-const because applyLayout() is run
   *        on first use to resolve renderPosition() for layers and groups.
   * @param options export options controlling text rendering, modifier resolution, and rasterizer
   *        behaviour.
   * @param warnings optional pointer to receive human-readable warnings whenever a layer or asset
   *        had to be degraded — image data could not be loaded, a tiled pattern bake failed, a
   *        layer rasterization failed, or an SVG-unsupported feature was emitted as vector
   *        because `bakeUnsupported` is false. Useful for CI / batch pipelines to detect silent
   *        degradation. May be nullptr to suppress collection.
   */
  static std::string ToSVG(PAGXDocument& document, const Options& options = {},
                           std::vector<std::string>* warnings = nullptr);

  /**
   * Exports a PAGXDocument to an SVG file.
   * @param document the PAGXDocument to export. Passed as non-const because applyLayout() is run
   *        on first use to resolve renderPosition() for layers and groups.
   * @param warnings same semantics as ToSVG(); see that method for details.
   * @return true on success. The file is written through a sibling temporary file and renamed into
   *         place, so a failed export leaves an existing file untouched. This is not a crash
   *         durability guarantee.
   */
  static bool ToFile(PAGXDocument& document, const std::string& filePath,
                     const Options& options = {}, std::vector<std::string>* warnings = nullptr);
};

}  // namespace pagx
