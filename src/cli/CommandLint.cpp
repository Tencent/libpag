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

// --- Helpers ---

static std::string LayerLabel(const Layer* layer) {
  if (!layer->name.empty()) {
    return layer->name;
  }
  if (!layer->id.empty()) {
    return layer->id;
  }
  return "(unnamed layer)";
}

// Returns true if the value is aligned to a 0.5px grid (i.e. value * 2 is an integer).
static bool IsHalfPixelAligned(float value) {
  float scaled = value * 2.0f;
  return std::fabs(scaled - std::round(scaled)) < 1e-4f;
}

// Returns true if the value has more than 4 decimal places of precision.
static bool ExceedsPrecision(float value) {
  float rounded = std::round(value * 1e4f) / 1e4f;
  return std::fabs(value - rounded) > 1e-6f;
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

// Returns the bounding box of all geometry elements (Rectangle, Ellipse, Path) in the element
// list, recursing into Groups. Returns an empty Rect if no geometry is found.
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

// Returns true if the color is pure white (all RGB components >= 0.99, any color space).
static bool IsPureWhite(const Color& color) {
  return color.red >= 0.99f && color.green >= 0.99f && color.blue >= 0.99f;
}

// Returns true if the color is a hardcoded opaque color (not referencing currentColor or a
// resource). White and black are the most problematic for theme compatibility.
static bool IsHardcodedOpaqueColor(const Color& color) {
  return color.alpha >= 0.99f;
}

// --- Per-layer rule checks ---

// VIS-001/002/003: Pixel alignment and coordinate precision.
static void CheckPixelAlignment(const Layer* layer, std::vector<LintIssue>& issues) {
  std::string location = LayerLabel(layer);

  auto checkCoord = [&](float value, const std::string& name) {
    if (ExceedsPrecision(value)) {
      issues.push_back({"VIS-003", location,
                        name + " (" + std::to_string(value) +
                            ") has more than 4 decimal places — likely floating-point noise"});
    } else if (!IsHalfPixelAligned(value)) {
      issues.push_back({"VIS-001", location,
                        name + " (" + std::to_string(value) +
                            ") is not aligned to 0.5px grid — may cause blurry rendering"});
    }
  };

  // Check layer position
  checkCoord(layer->x, "Layer x");
  checkCoord(layer->y, "Layer y");

  // Check geometry elements
  for (auto* element : layer->contents) {
    if (element->nodeType() == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      checkCoord(rect->position.x, "Rectangle center.x");
      checkCoord(rect->position.y, "Rectangle center.y");
      checkCoord(rect->size.width, "Rectangle size.width");
      checkCoord(rect->size.height, "Rectangle size.height");
    } else if (element->nodeType() == NodeType::Ellipse) {
      auto* ellipse = static_cast<const Ellipse*>(element);
      checkCoord(ellipse->position.x, "Ellipse center.x");
      checkCoord(ellipse->position.y, "Ellipse center.y");
      checkCoord(ellipse->size.width, "Ellipse size.width");
      checkCoord(ellipse->size.height, "Ellipse size.height");
    }
  }

  // VIS-002: For strokes, check that stroke center sits on the correct boundary.
  std::vector<const Stroke*> strokes;
  CollectStrokes(layer->contents, strokes);
  for (auto* stroke : strokes) {
    float width = stroke->width;
    bool isOddWidth = std::fmod(std::round(width), 2.0f) != 0.0f;
    if (isOddWidth) {
      // Odd stroke width: stroke center must be on 0.5px boundary (layer at half-pixel)
      if (!IsHalfPixelAligned(layer->x) || !IsHalfPixelAligned(layer->y)) {
        issues.push_back({"VIS-002", location,
                          "Stroke width " + std::to_string(width) +
                              "px is odd — layer position should be on 0.5px boundary for "
                              "crisp rendering"});
      }
    }
  }
}

// VIS-010/011/012/013: Stroke width constraints.
static void CheckStrokeWidth(const Layer* layer, float canvasSize, std::vector<LintIssue>& issues) {
  std::string location = LayerLabel(layer);
  std::vector<const Stroke*> strokes;
  CollectStrokes(layer->contents, strokes);

  if (strokes.empty()) {
    return;
  }

  // VIS-010: Minimum stroke width.
  for (auto* stroke : strokes) {
    if (stroke->width < 0.5f) {
      issues.push_back({"VIS-010", location,
                        "Stroke width (" + std::to_string(stroke->width) +
                            "px) is below 0.5px and may disappear at small icon sizes"});
    }
  }

  // VIS-011: Stroke width consistency across all strokes in this layer.
  if (strokes.size() > 1) {
    float referenceWidth = strokes[0]->width;
    for (size_t i = 1; i < strokes.size(); ++i) {
      float delta = std::fabs(strokes[i]->width - referenceWidth);
      if (delta > 0.25f) {
        issues.push_back({"VIS-011", location,
                          "Stroke widths are inconsistent: " + std::to_string(referenceWidth) +
                              "px vs " + std::to_string(strokes[i]->width) +
                              "px (tolerance ±0.25px)"});
        break;
      }
    }
  }

  // VIS-012: Recommended stroke width range for this canvas size.
  if (canvasSize > 0) {
    float minSafe = 0.0f;
    float maxSafe = 0.0f;
    if (canvasSize <= 16.0f) {
      minSafe = 1.0f;
      maxSafe = 1.5f;
    } else if (canvasSize <= 24.0f) {
      minSafe = 1.0f;
      maxSafe = 2.5f;
    } else if (canvasSize <= 32.0f) {
      minSafe = 1.5f;
      maxSafe = 3.0f;
    } else if (canvasSize <= 48.0f) {
      minSafe = 2.0f;
      maxSafe = 4.0f;
    }
    if (maxSafe > 0.0f) {
      for (auto* stroke : strokes) {
        if (stroke->width < minSafe || stroke->width > maxSafe) {
          issues.push_back({"VIS-012", location,
                            "Stroke width (" + std::to_string(stroke->width) +
                                "px) is outside recommended range [" + std::to_string(minSafe) +
                                ", " + std::to_string(maxSafe) + "]px for " +
                                std::to_string(static_cast<int>(canvasSize)) + "x" +
                                std::to_string(static_cast<int>(canvasSize)) + " canvas"});
        }
      }
    }
  }

  // VIS-013: Corner radius to stroke width ratio must not exceed 4.
  for (auto* element : layer->contents) {
    if (element->nodeType() == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      if (rect->roundness > 0.0f && !strokes.empty()) {
        float referenceWidth = strokes[0]->width;
        if (referenceWidth > 0.0f && (rect->roundness / referenceWidth) > 4.0f) {
          issues.push_back({"VIS-013", location,
                            "Corner radius (" + std::to_string(rect->roundness) +
                                "px) to stroke width (" + std::to_string(referenceWidth) +
                                "px) ratio exceeds 4 — may produce visual overlapping artifacts"});
        }
      }
    }
  }
}

// VIS-020/021/022: Safe zone / margins.
static void CheckSafeZone(const Layer* layer, float canvasWidth, float canvasHeight,
                          std::vector<LintIssue>& issues) {
  if (canvasWidth <= 0.0f || canvasHeight <= 0.0f) {
    return;
  }

  std::string location = LayerLabel(layer);
  float minPadding = canvasWidth * 0.083f;  // VIS-021 formula

  Rect bounds = CollectGeometryBounds(layer->contents);
  if (bounds.width <= 0.0f && bounds.height <= 0.0f) {
    return;
  }

  float boundsRight = bounds.x + bounds.width;
  float boundsBottom = bounds.y + bounds.height;

  // VIS-022: No edge must touch or exceed canvas boundary.
  if (bounds.x <= 0.0f || bounds.y <= 0.0f || boundsRight >= canvasWidth ||
      boundsBottom >= canvasHeight) {
    issues.push_back({"VIS-022", location,
                      "Content touches or exceeds canvas boundary — all elements must stay "
                      "within canvas bounds"});
    return;
  }

  // VIS-020: Content must respect minimum safe padding.
  float actualLeft = bounds.x;
  float actualTop = bounds.y;
  float actualRight = canvasWidth - boundsRight;
  float actualBottom = canvasHeight - boundsBottom;
  float minActual = std::min({actualLeft, actualTop, actualRight, actualBottom});

  if (minActual < minPadding) {
    issues.push_back({"VIS-020", location,
                      "Content is too close to canvas edge (min padding " +
                          std::to_string(minActual) +
                          "px, recommended >= " + std::to_string(minPadding) + "px)"});
  }
}

// VIS-100/101: Color rules — hardcoded colors break dark/light theme compatibility.
static void CheckColorRules(const Layer* layer, std::vector<LintIssue>& issues) {
  std::string location = LayerLabel(layer);

  std::vector<const Fill*> fills;
  CollectFills(layer->contents, fills);
  std::vector<const Stroke*> strokes;
  CollectStrokes(layer->contents, strokes);

  auto checkColorSource = [&](const ColorSource* source, const std::string& painterType) {
    if (source == nullptr || source->nodeType() != NodeType::SolidColor) {
      return;
    }
    auto* solid = static_cast<const SolidColor*>(source);
    if (!IsHardcodedOpaqueColor(solid->color)) {
      return;
    }
    // VIS-100/101: Warn on hardcoded white or black as the sole color.
    bool isWhite = IsPureWhite(solid->color);
    bool isBlack =
        solid->color.red <= 0.01f && solid->color.green <= 0.01f && solid->color.blue <= 0.01f;
    if (isWhite || isBlack) {
      std::string colorName = isWhite ? "white (#FFFFFF)" : "black (#000000)";
      issues.push_back(
          {"VIS-101", location,
           painterType + " uses hardcoded " + colorName +
               " — use currentColor or a theme-aware resource reference for theme compatibility"});
    }
  };

  for (auto* fill : fills) {
    checkColorSource(fill->color, "Fill");
  }
  for (auto* stroke : strokes) {
    checkColorSource(stroke->color, "Stroke");
  }
}

// --- Document-level traversal ---

static void CheckLayer(const Layer* layer, float canvasWidth, float canvasHeight,
                       std::vector<LintIssue>& issues) {
  float canvasSize = std::min(canvasWidth, canvasHeight);
  CheckPixelAlignment(layer, issues);
  CheckStrokeWidth(layer, canvasSize, issues);
  CheckSafeZone(layer, canvasWidth, canvasHeight, issues);
  CheckColorRules(layer, issues);

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
    return {{"", "", "Failed to load file: " + filePath}};
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
