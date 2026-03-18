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

#include "cli/CommandLint.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"

namespace pagx::cli {

static std::string LayerLabel(const Layer* layer) {
  if (!layer->name.empty()) {
    return layer->name;
  }
  if (!layer->id.empty()) {
    return layer->id;
  }
  return "(unnamed layer)";
}

// Tolerance for floating-point alignment checks (10^-4 pixels).
static constexpr float ALIGNMENT_EPSILON = 1e-4f;

// Returns true if the value is aligned to a 0.5px grid (i.e. value * 2 is an integer).
static bool IsHalfPixelAligned(float value) {
  float scaled = value * 2.0f;
  return std::fabs(scaled - std::round(scaled)) < ALIGNMENT_EPSILON;
}

// Returns true only when the value is on a strict half-pixel boundary (0.5, 1.5, 2.5, ...),
// i.e. value * 2 is an odd integer. Integer values (0, 1, 2, ...) return false.
static bool IsStrictHalfPixel(float value) {
  float scaled = value * 2.0f;
  float nearest = std::round(scaled);
  if (std::fabs(scaled - nearest) >= ALIGNMENT_EPSILON) {
    return false;
  }
  return std::fmod(std::fabs(nearest), 2.0f) > 0.5f;
}

// Checks pixel alignment for a single coordinate value. Any value not on the 0.5px grid
// (including floating-point noise values) is reported as pixel-alignment. A separate
// coord-precision rule is unnecessary: values with excessive decimal places will also
// fail this check, so pixel-alignment subsumes both concerns.
static void CheckCoordinate(float value, const std::string& name, const std::string& location,
                            std::vector<LintIssue>& issues) {
  if (!IsHalfPixelAligned(value)) {
    issues.push_back({"pixel-alignment", location,
                      name + " (" + std::to_string(value) +
                          ") is not aligned to 0.5px grid — may cause blurry rendering"});
  }
}

// Collects all Stroke nodes from a flat element list, recursing into Groups.
static void CollectStrokes(const std::vector<Element*>& elements, std::vector<const Stroke*>& out) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Stroke) {
      out.push_back(static_cast<const Stroke*>(element));
    } else if (element->nodeType() == NodeType::Group) {
      CollectStrokes(static_cast<const Group*>(element)->elements, out);
    }
  }
}

// Collects all Fill nodes from a flat element list, recursing into Groups.
static void CollectFills(const std::vector<Element*>& elements, std::vector<const Fill*>& out) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Fill) {
      out.push_back(static_cast<const Fill*>(element));
    } else if (element->nodeType() == NodeType::Group) {
      CollectFills(static_cast<const Group*>(element)->elements, out);
    }
  }
}

// CollectGeometryBounds: Computes the union bounding box of all geometry in the element list.
//
// Handles three geometry node types:
//   Rectangle / Ellipse: both use a center-based coordinate model (position = center point,
//     size = width/height). Converted to LTRB by subtracting/adding half-dimensions.
//   Path: bounds are read directly from PathData::getBounds(). Skipped if data is null or empty.
//   Group: recursed into; the group's child bounds are unioned into the running result.
//
// Returns an empty Rect{} (width == height == 0) if no geometry is found in the list.
// Used by CheckSafeZone to determine whether the layer's content falls within the safe-zone
// inset. The const_cast on PathData is required because getBounds() is non-const
// (it caches the result internally) but we hold a const pointer from the document model.
static Rect CollectGeometryBounds(const std::vector<Element*>& elements) {
  bool found = false;
  Rect bounds = {};
  for (auto* element : elements) {
    Rect elementBounds = {};
    bool hasBounds = false;
    if (element->nodeType() == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width / 2.0f;
      float halfH = rect->size.height / 2.0f;
      elementBounds = Rect::MakeLTRB(rect->position.x - halfW, rect->position.y - halfH,
                                     rect->position.x + halfW, rect->position.y + halfH);
      hasBounds = true;
    } else if (element->nodeType() == NodeType::Ellipse) {
      auto* ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width / 2.0f;
      float halfH = ellipse->size.height / 2.0f;
      elementBounds = Rect::MakeLTRB(ellipse->position.x - halfW, ellipse->position.y - halfH,
                                     ellipse->position.x + halfW, ellipse->position.y + halfH);
      hasBounds = true;
    } else if (element->nodeType() == NodeType::Path) {
      auto* path = static_cast<const Path*>(element);
      if (path->data != nullptr && !path->data->isEmpty()) {
        elementBounds = const_cast<PathData*>(path->data)->getBounds();
        hasBounds = true;
      }
    } else if (element->nodeType() == NodeType::Group) {
      auto groupBounds = CollectGeometryBounds(static_cast<const Group*>(element)->elements);
      if (groupBounds.width > 0 || groupBounds.height > 0) {
        elementBounds = groupBounds;
        hasBounds = true;
      }
    }
    if (hasBounds) {
      if (!found) {
        bounds = elementBounds;
        found = true;
      } else {
        float left = std::min(bounds.x, elementBounds.x);
        float top = std::min(bounds.y, elementBounds.y);
        float right = std::max(bounds.x + bounds.width, elementBounds.x + elementBounds.width);
        float bottom = std::max(bounds.y + bounds.height, elementBounds.y + elementBounds.height);
        bounds = Rect::MakeLTRB(left, top, right, bottom);
      }
    }
  }
  return found ? bounds : Rect{};
}

static constexpr float COLOR_NEAR_ONE_THRESHOLD = 0.99f;   // near-white / near-opaque
static constexpr float COLOR_NEAR_ZERO_THRESHOLD = 0.01f;  // near-black

// Returns true if the color is pure white (all RGB components >= 0.99, any color space).
static bool IsPureWhite(const Color& color) {
  return color.red >= COLOR_NEAR_ONE_THRESHOLD && color.green >= COLOR_NEAR_ONE_THRESHOLD &&
         color.blue >= COLOR_NEAR_ONE_THRESHOLD;
}

// Returns true if the color is a hardcoded opaque color (not referencing currentColor or a
// resource). White and black are the most problematic for theme compatibility.
static bool IsHardcodedOpaqueColor(const Color& color) {
  return color.alpha >= COLOR_NEAR_ONE_THRESHOLD;
}

// --- Per-layer rule checks ---

// Helper to check geometry coordinates in a flat element list, recursing into Groups.
static void CheckGeometryCoordinates(const std::vector<Element*>& elements,
                                     const std::string& location, std::vector<LintIssue>& issues) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      CheckCoordinate(rect->position.x, "Rectangle center.x", location, issues);
      CheckCoordinate(rect->position.y, "Rectangle center.y", location, issues);
      CheckCoordinate(rect->size.width, "Rectangle size.width", location, issues);
      CheckCoordinate(rect->size.height, "Rectangle size.height", location, issues);
    } else if (element->nodeType() == NodeType::Ellipse) {
      auto* ellipse = static_cast<const Ellipse*>(element);
      CheckCoordinate(ellipse->position.x, "Ellipse center.x", location, issues);
      CheckCoordinate(ellipse->position.y, "Ellipse center.y", location, issues);
      CheckCoordinate(ellipse->size.width, "Ellipse size.width", location, issues);
      CheckCoordinate(ellipse->size.height, "Ellipse size.height", location, issues);
    } else if (element->nodeType() == NodeType::Path) {
      auto* path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        // Check on-curve end points only; skip off-curve control points to reduce noise.
        // For Move/Line: the single point is on-curve. For Quad: last of 2 points. For Cubic: last
        // of 3 points. Close has no points. The on-curve end point is always the last point for
        // each verb.
        const auto& verbs = path->data->verbs();
        const auto& pts = path->data->points();
        size_t pointIndex = 0;
        for (auto verb : verbs) {
          int count = PathData::PointsPerVerb(verb);
          if (count > 0) {
            const Point& endPoint = pts[pointIndex + static_cast<size_t>(count) - 1];
            CheckCoordinate(endPoint.x, "Path point.x", location, issues);
            CheckCoordinate(endPoint.y, "Path point.y", location, issues);
          }
          pointIndex += static_cast<size_t>(count);
        }
      }
    } else if (element->nodeType() == NodeType::Group) {
      // Recurse into groups to check nested geometry coordinates.
      CheckGeometryCoordinates(static_cast<const Group*>(element)->elements, location, issues);
    }
  }
}

// Pixel alignment and coordinate precision checks.
static void CheckPixelAlignment(const Layer* layer, const std::string& location,
                                const std::vector<const Stroke*>& strokes,
                                std::vector<LintIssue>& issues) {
  // Check layer position
  CheckCoordinate(layer->x, "Layer x", location, issues);
  CheckCoordinate(layer->y, "Layer y", location, issues);

  // Check geometry elements (recursing into Groups)
  CheckGeometryCoordinates(layer->contents, location, issues);

  // Odd-stroke alignment: stroke center must sit on the correct pixel boundary.
  for (auto* stroke : strokes) {
    float width = stroke->width;
    bool isIntegerWidth = std::fabs(width - std::round(width)) < ALIGNMENT_EPSILON;
    bool isOddWidth = isIntegerWidth && (std::fmod(std::round(width), 2.0f) != 0.0f);
    if (isOddWidth) {
      // Odd stroke width: stroke center must be on strict 0.5px boundary (layer at half-pixel)
      if (!IsStrictHalfPixel(layer->x) || !IsStrictHalfPixel(layer->y)) {
        issues.push_back({"odd-stroke-alignment", location,
                          "Stroke width " + std::to_string(width) +
                              "px is odd — layer position should be on 0.5px boundary for "
                              "crisp rendering"});
        break;
      }
    }
  }
}

// Helper to check corner ratio in a flat element list, recursing into Groups.
static void CheckCornerRadiusRatio(const std::vector<Element*>& elements,
                                   const std::string& location,
                                   const std::vector<const Stroke*>& strokes,
                                   std::vector<LintIssue>& issues) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      if (rect->roundness > 0.0f && !strokes.empty()) {
        float referenceWidth = strokes[0]->width;
        if (referenceWidth > 0.0f && (rect->roundness / referenceWidth) > 4.0f) {
          issues.push_back({"stroke-corner-ratio", location,
                            "Corner radius (" + std::to_string(rect->roundness) +
                                "px) to stroke width (" + std::to_string(referenceWidth) +
                                "px) ratio exceeds 4 — may produce visual overlapping artifacts"});
        }
      }
    } else if (element->nodeType() == NodeType::Group) {
      CheckCornerRadiusRatio(static_cast<const Group*>(element)->elements, location, strokes,
                             issues);
    }
  }
}

// Stroke width constraint checks.
static void CheckStrokeWidth(const Layer* layer, float canvasSize, const std::string& location,
                             const std::vector<const Stroke*>& strokes,
                             std::vector<LintIssue>& issues) {
  if (strokes.empty()) {
    return;
  }

  // Minimum stroke width.
  for (auto* stroke : strokes) {
    if (stroke->width < 0.5f) {
      issues.push_back({"stroke-too-thin", location,
                        "Stroke width (" + std::to_string(stroke->width) +
                            "px) is below 0.5px and may disappear at small icon sizes"});
    }
  }

  // Stroke width consistency across all strokes in this layer.
  if (strokes.size() > 1) {
    float referenceWidth = strokes[0]->width;
    for (size_t i = 1; i < strokes.size(); ++i) {
      float delta = std::fabs(strokes[i]->width - referenceWidth);
      if (delta > 0.25f) {
        issues.push_back({"stroke-inconsistent", location,
                          "Stroke widths are inconsistent: " + std::to_string(referenceWidth) +
                              "px vs " + std::to_string(strokes[i]->width) +
                              "px (tolerance ±0.25px)"});
        break;
      }
    }
  }

  // Recommended stroke width ranges by canvas size.
  // Values derived from visual balance guidelines for icon design:
  //   - Minimum: thin enough to render cleanly at the given pixel density.
  //   - Maximum: thick enough to stay visually light; exceeding this makes strokes dominate.
  // Entries are ordered by increasing maxCanvasSize; the first matching entry is used.
  // Canvas sizes above 48px have no enforced range (icons at that scale have more freedom).
  struct StrokeRange {
    float maxCanvasSize;
    float minSafe;
    float maxSafe;
  };
  static constexpr StrokeRange STROKE_RANGES[] = {
      {16.0f, 1.0f, 1.5f},
      {24.0f, 1.0f, 2.5f},
      {32.0f, 1.5f, 3.0f},
      {48.0f, 2.0f, 4.0f},
  };
  if (canvasSize > 0) {
    float minSafe = 0.0f;
    float maxSafe = 0.0f;
    for (const auto& range : STROKE_RANGES) {
      if (canvasSize <= range.maxCanvasSize) {
        minSafe = range.minSafe;
        maxSafe = range.maxSafe;
        break;
      }
    }
    if (maxSafe > 0.0f) {
      for (auto* stroke : strokes) {
        if (stroke->width < minSafe || stroke->width > maxSafe) {
          issues.push_back({"stroke-out-of-range", location,
                            "Stroke width (" + std::to_string(stroke->width) +
                                "px) is outside recommended range [" + std::to_string(minSafe) +
                                ", " + std::to_string(maxSafe) + "]px for " +
                                std::to_string(static_cast<int>(canvasSize)) + "x" +
                                std::to_string(static_cast<int>(canvasSize)) + " canvas"});
        }
      }
    }
  }

  // Corner radius to stroke width ratio must not exceed 4 (recurse into Groups).
  CheckCornerRadiusRatio(layer->contents, location, strokes, issues);
}

// Safe-zone minimum padding: 8.3% (1/12) of canvas edge.
static constexpr float SAFE_ZONE_PADDING_RATIO = 0.083f;

// Safe zone / canvas margin checks.
static void CheckSafeZone(const Layer* layer, float canvasWidth, float canvasHeight,
                          const std::string& location, std::vector<LintIssue>& issues) {
  if (canvasWidth <= 0.0f || canvasHeight <= 0.0f) {
    return;
  }
  float minPaddingX = canvasWidth * SAFE_ZONE_PADDING_RATIO;
  float minPaddingY = canvasHeight * SAFE_ZONE_PADDING_RATIO;

  Rect bounds = CollectGeometryBounds(layer->contents);
  if (bounds.isEmpty()) {
    return;
  }

  // Geometry coordinates are relative to the layer origin. Translate to canvas space by adding
  // the layer's x/y offset so that safe-zone checks are against the correct absolute positions.
  bounds.x += layer->x;
  bounds.y += layer->y;

  float boundsRight = bounds.x + bounds.width;
  float boundsBottom = bounds.y + bounds.height;

  // No edge must touch or exceed canvas boundary.
  if (bounds.x <= 0.0f || bounds.y <= 0.0f || boundsRight >= canvasWidth ||
      boundsBottom >= canvasHeight) {
    issues.push_back({"touches-canvas-edge", location,
                      "Content touches or exceeds canvas boundary — all elements must stay "
                      "within canvas bounds"});
    return;
  }

  // Content must respect minimum safe padding (separate checks for X and Y directions).
  float actualLeft = bounds.x;
  float actualTop = bounds.y;
  float actualRight = canvasWidth - boundsRight;
  float actualBottom = canvasHeight - boundsBottom;

  if (actualLeft < minPaddingX || actualRight < minPaddingX) {
    issues.push_back({"outside-safe-zone", location,
                      "Content is too close to canvas left/right edge (min padding " +
                          std::to_string(std::min(actualLeft, actualRight)) +
                          "px, recommended >= " + std::to_string(minPaddingX) + "px)"});
    return;
  }

  if (actualTop < minPaddingY || actualBottom < minPaddingY) {
    issues.push_back({"outside-safe-zone", location,
                      "Content is too close to canvas top/bottom edge (min padding " +
                          std::to_string(std::min(actualTop, actualBottom)) +
                          "px, recommended >= " + std::to_string(minPaddingY) + "px)"});
  }
}

// Helper to check a single color source for hardcoded theme colors.
static void CheckColorSource(const ColorSource* source, const std::string& painterType,
                             const std::string& location, std::vector<LintIssue>& issues) {
  // Only check SolidColor nodes. Gradients, patterns, and resource references (other
  // ColorSource subtypes) are intentional design choices and are not flagged.
  if (source == nullptr || source->nodeType() != NodeType::SolidColor) {
    return;
  }
  auto* solid = static_cast<const SolidColor*>(source);
  // Only flag fully opaque hardcoded colors. Semi-transparent colors (e.g. overlays) are
  // legitimate uses that don't break theme switching in the same way.
  if (!IsHardcodedOpaqueColor(solid->color)) {
    return;
  }
  // Pure white and pure black are the worst offenders for theme incompatibility:
  // white is invisible on light backgrounds, black is invisible on dark backgrounds.
  // currentColor or a theme-aware resource reference should be used instead.
  bool isWhite = IsPureWhite(solid->color);
  bool isBlack = solid->color.red <= COLOR_NEAR_ZERO_THRESHOLD &&
                 solid->color.green <= COLOR_NEAR_ZERO_THRESHOLD &&
                 solid->color.blue <= COLOR_NEAR_ZERO_THRESHOLD;
  if (isWhite || isBlack) {
    std::string colorName = isWhite ? "white (#FFFFFF)" : "black (#000000)";
    issues.push_back(
        {"hardcoded-theme-color", location,
         painterType + " uses hardcoded " + colorName +
             " — use currentColor or a theme-aware resource reference for theme compatibility"});
  }
}

// Color rules — hardcoded colors break dark/light theme compatibility.
static void CheckColorRules(const std::string& location, const std::vector<const Fill*>& fills,
                            const std::vector<const Stroke*>& strokes,
                            std::vector<LintIssue>& issues) {
  for (auto* fill : fills) {
    CheckColorSource(fill->color, "Fill", location, issues);
  }
  for (auto* stroke : strokes) {
    CheckColorSource(stroke->color, "Stroke", location, issues);
  }
}

// --- Document-level traversal ---

static void CheckLayer(const Layer* layer, float canvasWidth, float canvasHeight,
                       std::vector<LintIssue>& issues) {
  float canvasSize = std::min(canvasWidth, canvasHeight);
  std::string location = LayerLabel(layer);
  std::vector<const Stroke*> strokes;
  CollectStrokes(layer->contents, strokes);
  std::vector<const Fill*> fills;
  CollectFills(layer->contents, fills);
  CheckPixelAlignment(layer, location, strokes, issues);
  CheckStrokeWidth(layer, canvasSize, location, strokes, issues);
  CheckSafeZone(layer, canvasWidth, canvasHeight, location, issues);
  CheckColorRules(location, fills, strokes, issues);

  for (auto* child : layer->children) {
    CheckLayer(child, canvasWidth, canvasHeight, issues);
  }
}

std::vector<LintIssue> LintDocument(const PAGXDocument* document) {
  std::vector<LintIssue> issues;
  for (auto* layer : document->layers) {
    CheckLayer(layer, document->width, document->height, issues);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CheckLayer(layer, comp->width, comp->height, issues);
      }
    }
  }
  return issues;
}

std::vector<LintIssue> LintFile(const std::string& filePath) {
  auto document = PAGXImporter::FromFile(filePath);
  if (document == nullptr) {
    return {{"ERR-LOAD", "", "Failed to load file: " + filePath}};
  }
  return LintDocument(document.get());
}

// --- Output ---

static void PrintIssuesText(const std::vector<LintIssue>& issues, const std::string& file) {
  std::cout << file << ": " << issues.size() << " lint issue(s)\n";
  for (const auto& issue : issues) {
    std::cout << "  [" << issue.ruleId << "]";
    if (!issue.location.empty()) {
      std::cout << " " << issue.location << ":";
    }
    std::cout << " " << issue.message << "\n";
  }
}

static void PrintIssuesJson(const std::vector<LintIssue>& issues, const std::string& file) {
  std::cout << "{\n";
  std::cout << "  \"file\": \"" << EscapeJson(file) << "\",\n";
  std::cout << "  \"issues\": [";
  for (size_t i = 0; i < issues.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\n    {\"ruleId\": \"" << EscapeJson(issues[i].ruleId) << "\", \"location\": \""
              << EscapeJson(issues[i].location) << "\", \"message\": \""
              << EscapeJson(issues[i].message) << "\"}";
  }
  if (!issues.empty()) {
    std::cout << "\n  ";
  }
  std::cout << "]\n";
  std::cout << "}\n";
}

static void PrintUsage() {
  std::cout << "Usage: pagx lint [options] <file.pagx>\n"
            << "\n"
            << "Check visual quality rules on a PAGX file and report issues.\n"
            << "Exit code is always 0 — lint results are advisory only.\n"
            << "\n"
            << "Options:\n"
            << "  --json                 Output in JSON format\n"
            << "  -h, --help             Show this help message\n";
}

int RunLint(int argc, char* argv[]) {
  bool jsonOutput = false;
  std::string filePath;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--json") == 0) {
      jsonOutput = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx lint: unknown option '" << argv[i] << "'\n";
      return 0;
    } else {
      if (!filePath.empty()) {
        std::cerr << "pagx lint: multiple input files specified; only one is supported\n";
        return 0;
      }
      filePath = argv[i];
    }
  }

  if (filePath.empty()) {
    std::cerr << "pagx lint: missing input file\n";
    PrintUsage();
    return 0;
  }

  auto issues = LintFile(filePath);

  if (jsonOutput) {
    PrintIssuesJson(issues, filePath);
  } else if (issues.empty()) {
    std::cout << filePath << ": no issues found\n";
  } else {
    PrintIssuesText(issues, filePath);
  }

  return 0;  // Always 0: lint is advisory, must not block the pipeline.
}

}  // namespace pagx::cli
