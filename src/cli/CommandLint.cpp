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
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx_xsd.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/RRect.h"
#include "tgfx/core/Rect.h"

namespace pagx::cli {

// --- Diagnostic structures ---

struct LintDiagnostic {
  int line = 0;
  std::string category = {};
  std::string message = {};
};

// --- XSD validation (from CommandValidator) ---

#if LIBXML_VERSION >= 21200
static void CollectStructuredError(void* context, const xmlError* xmlError) {
#else
static void CollectStructuredError(void* context, xmlError* xmlError) {
#endif
  if (xmlError == nullptr) {
    return;
  }
  auto* diagnostics = static_cast<std::vector<LintDiagnostic>*>(context);
  LintDiagnostic diag = {};
  diag.line = xmlError->line;
  diag.category = "error";
  std::string msg(xmlError->message != nullptr ? xmlError->message : "Unknown error");
  while (!msg.empty() && msg.back() == '\n') {
    msg.pop_back();
  }
  diag.message = std::move(msg);
  diagnostics->push_back(std::move(diag));
}

static void ValidateXsdAndSemantic(const std::string& filePath,
                                   std::vector<LintDiagnostic>& diagnostics) {
  xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_NONET);
  if (doc == nullptr) {
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Failed to parse XML document";
    diagnostics.push_back(std::move(diag));
    return;
  }

  const auto& xsdContent = PagxXsdContent();
  xmlSchemaParserCtxtPtr parserCtxt =
      xmlSchemaNewMemParserCtxt(xsdContent.c_str(), static_cast<int>(xsdContent.size()));
  if (parserCtxt == nullptr) {
    xmlFreeDoc(doc);
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to create schema parser context";
    diagnostics.push_back(std::move(diag));
    return;
  }

  xmlSchemaPtr schema = xmlSchemaParse(parserCtxt);
  xmlSchemaFreeParserCtxt(parserCtxt);
  if (schema == nullptr) {
    xmlFreeDoc(doc);
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to parse XSD schema";
    diagnostics.push_back(std::move(diag));
    return;
  }

  xmlSchemaValidCtxtPtr validCtxt = xmlSchemaNewValidCtxt(schema);
  if (validCtxt == nullptr) {
    xmlSchemaFree(schema);
    xmlFreeDoc(doc);
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to create validation context";
    diagnostics.push_back(std::move(diag));
    return;
  }

  xmlSchemaSetValidStructuredErrors(validCtxt, CollectStructuredError, &diagnostics);
  xmlSchemaValidateDoc(validCtxt, doc);

  xmlSchemaFreeValidCtxt(validCtxt);
  xmlSchemaFree(schema);
  xmlFreeDoc(doc);

  auto pagxDoc = pagx::PAGXImporter::FromFile(filePath);
  if (pagxDoc != nullptr) {
    for (const auto& errorStr : pagxDoc->errors) {
      LintDiagnostic diag = {};
      diag.category = "error";
      if (errorStr.compare(0, 5, "line ") == 0) {
        auto colonPos = errorStr.find(':', 5);
        if (colonPos != std::string::npos) {
          auto lineStr = errorStr.substr(5, colonPos - 5);
          char* endPtr = nullptr;
          long lineNum = strtol(lineStr.c_str(), &endPtr, 10);
          diag.line =
              (endPtr != lineStr.c_str() && *endPtr == '\0') ? static_cast<int>(lineNum) : 0;
          diag.message = errorStr.substr(colonPos + 2);
        } else {
          diag.message = errorStr;
        }
      } else {
        diag.message = errorStr;
      }
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Helper functions ---

static bool HasConstraintAttributes(float left, float right, float top, float bottom, float centerX,
                                    float centerY) {
  return !std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
         !std::isnan(centerX) || !std::isnan(centerY);
}

// --- Pass 1: Empty node detection ---

static bool IsEmptyLayer(const Layer* layer) {
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  return layer->contents.empty() && layer->children.empty() && layer->composition == nullptr &&
         layer->styles.empty() && layer->filters.empty();
}

static bool IsEmptyElement(const Element* element) {
  if (element->nodeType() == NodeType::Stroke) {
    auto stroke = static_cast<const Stroke*>(element);
    return stroke->width <= 0.0f;
  }
  if (element->nodeType() == NodeType::Group) {
    auto group = static_cast<const Group*>(element);
    return group->elements.empty();
  }
  return false;
}

static int CountEmptyNodes(const PAGXDocument* document) {
  int count = 0;

  std::function<int(const std::vector<Layer*>&)> countEmptyLayers =
      [&](const std::vector<Layer*>& layers) {
        int c = 0;
        for (auto* layer : layers) {
          if (IsEmptyLayer(layer)) {
            c++;
          }
        }
        return c;
      };

  std::function<int(const std::vector<Element*>&)> countEmptyElements =
      [&](const std::vector<Element*>& elements) {
        int c = 0;
        for (auto* element : elements) {
          if (IsEmptyElement(element)) {
            c++;
          }
        }
        return c;
      };

  count += countEmptyLayers(document->layers);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<const Layer*>(node.get());
      count += countEmptyLayers(layer->children);
      count += countEmptyElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      auto group = static_cast<const Group*>(node.get());
      count += countEmptyElements(group->elements);
    } else if (node->nodeType() == NodeType::Composition) {
      auto composition = static_cast<const Composition*>(node.get());
      count += countEmptyLayers(composition->layers);
    }
  }
  return count;
}

// --- Pass 7: Full canvas clip mask detection ---

static bool IsFullCanvasClipMask(const Layer* maskLayer, float canvasWidth, float canvasHeight) {
  if (!maskLayer->visible) {
    return false;
  }
  if (maskLayer->x != 0 || maskLayer->y != 0) {
    return false;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return false;
  }
  if (maskLayer->contents.size() != 1) {
    return false;
  }
  auto element = maskLayer->contents[0];
  if (element->nodeType() != NodeType::Rectangle) {
    return false;
  }
  auto rect = static_cast<const Rectangle*>(element);
  auto left = rect->position.x - rect->size.width * 0.5f;
  auto top = rect->position.y - rect->size.height * 0.5f;
  return left <= 0 && top <= 0 && rect->size.width >= canvasWidth &&
         rect->size.height >= canvasHeight;
}

static void CollectMaskReferences(const std::vector<Layer*>& layers,
                                  std::unordered_set<const Layer*>& maskedLayers) {
  for (auto* layer : layers) {
    if (layer->mask != nullptr) {
      maskedLayers.insert(layer->mask);
    }
    if (layer->composition != nullptr) {
      CollectMaskReferences(layer->composition->layers, maskedLayers);
    }
    CollectMaskReferences(layer->children, maskedLayers);
  }
}

static int CountFullCanvasClipMasks(const PAGXDocument* document) {
  int count = 0;
  float canvasWidth = document->width;
  float canvasHeight = document->height;

  std::unordered_set<const Layer*> maskedLayers = {};
  CollectMaskReferences(document->layers, maskedLayers);

  std::function<void(const std::vector<Layer*>&)> checkLayers =
      [&](const std::vector<Layer*>& layers) {
        for (auto* layer : layers) {
          if (layer->mask != nullptr &&
              maskedLayers.find(layer->mask) == maskedLayers.end() &&
              IsFullCanvasClipMask(layer->mask, canvasWidth, canvasHeight)) {
            count++;
          }
          if (layer->composition != nullptr) {
            checkLayers(layer->composition->layers);
          }
          checkLayers(layer->children);
        }
      };

  checkLayers(document->layers);
  return count;
}

// --- Pass 13: Unreferenced resource detection ---

static bool IsGradient(NodeType type) {
  return type == NodeType::LinearGradient || type == NodeType::RadialGradient ||
         type == NodeType::ConicGradient || type == NodeType::DiamondGradient;
}

static void CollectReferencedNodes(const std::vector<Element*>& elements,
                                   std::unordered_set<const Node*>& referenced);

static void CollectColorStops(const ColorSource* source,
                              std::unordered_set<const Node*>& referenced) {
  const std::vector<ColorStop*>* stops = nullptr;
  auto type = source->nodeType();
  if (type == NodeType::LinearGradient) {
    stops = &static_cast<const LinearGradient*>(source)->colorStops;
  } else if (type == NodeType::RadialGradient) {
    stops = &static_cast<const RadialGradient*>(source)->colorStops;
  } else if (type == NodeType::ConicGradient) {
    stops = &static_cast<const ConicGradient*>(source)->colorStops;
  } else if (type == NodeType::DiamondGradient) {
    stops = &static_cast<const DiamondGradient*>(source)->colorStops;
  }
  if (stops != nullptr) {
    for (auto* stop : *stops) {
      referenced.insert(stop);
    }
  }
}

static void CollectReferencedNodesFromColorSource(const ColorSource* source,
                                                  std::unordered_set<const Node*>& referenced) {
  if (source == nullptr) {
    return;
  }
  referenced.insert(source);
  if (IsGradient(source->nodeType())) {
    CollectColorStops(source, referenced);
  } else if (source->nodeType() == NodeType::ImagePattern) {
    auto pattern = static_cast<const ImagePattern*>(source);
    if (pattern->image != nullptr) {
      referenced.insert(pattern->image);
    }
  }
}

static void CollectReferencedNodes(const std::vector<Element*>& elements,
                                   std::unordered_set<const Node*>& referenced) {
  for (auto* element : elements) {
    if (element == nullptr) {
      continue;
    }
    referenced.insert(element);
    auto type = element->nodeType();
    if (type == NodeType::Fill) {
      auto fill = static_cast<const Fill*>(element);
      CollectReferencedNodesFromColorSource(fill->color, referenced);
    } else if (type == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(element);
      CollectReferencedNodesFromColorSource(stroke->color, referenced);
    } else if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        referenced.insert(path->data);
      }
    } else if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      for (auto* run : text->glyphRuns) {
        referenced.insert(run);
        if (run->font != nullptr) {
          referenced.insert(run->font);
          for (auto* glyph : run->font->glyphs) {
            referenced.insert(glyph);
            if (glyph->path != nullptr) {
              referenced.insert(glyph->path);
            }
            if (glyph->image != nullptr) {
              referenced.insert(glyph->image);
            }
          }
        }
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      CollectReferencedNodes(group->elements, referenced);
    } else if (type == NodeType::TextPath) {
      auto textPath = static_cast<const TextPath*>(element);
      if (textPath->path != nullptr) {
        referenced.insert(textPath->path);
      }
    } else if (type == NodeType::TextModifier) {
      auto modifier = static_cast<const TextModifier*>(element);
      for (auto* selector : modifier->selectors) {
        referenced.insert(selector);
      }
    }
  }
}

static void CollectReferencedNodesFromLayer(const Layer* layer,
                                            std::unordered_set<const Node*>& referenced) {
  if (layer == nullptr) {
    return;
  }
  referenced.insert(layer);
  CollectReferencedNodes(layer->contents, referenced);
  for (auto* style : layer->styles) {
    referenced.insert(style);
  }
  for (auto* filter : layer->filters) {
    referenced.insert(filter);
  }
  if (layer->mask != nullptr) {
    CollectReferencedNodesFromLayer(layer->mask, referenced);
  }
  if (layer->composition != nullptr) {
    referenced.insert(layer->composition);
    for (auto* child : layer->composition->layers) {
      CollectReferencedNodesFromLayer(child, referenced);
    }
  }
  for (auto* child : layer->children) {
    CollectReferencedNodesFromLayer(child, referenced);
  }
}

static int CountUnreferencedResources(const PAGXDocument* document) {
  std::unordered_set<const Node*> referenced = {};
  for (auto* layer : document->layers) {
    CollectReferencedNodesFromLayer(layer, referenced);
  }

  int count = 0;
  for (auto& node : document->nodes) {
    if (referenced.find(node.get()) == referenced.end()) {
      count++;
    }
  }
  return count;
}

// --- Lint hints (performance warnings) ---

static bool CanDowngradeToGroup(const Layer* layer) {
  if (!layer->styles.empty() || !layer->filters.empty() || !layer->children.empty()) {
    return false;
  }
  if (layer->mask != nullptr || layer->composition != nullptr) {
    return false;
  }
  if (layer->blendMode != BlendMode::Normal) {
    return false;
  }
  if (layer->hasScrollRect || !layer->visible) {
    return false;
  }
  if (!layer->matrix.isIdentity() || !layer->matrix3D.isIdentity()) {
    return false;
  }
  if (layer->preserve3D || layer->groupOpacity || !layer->passThroughBackground) {
    return false;
  }
  if (!layer->antiAlias) {
    return false;
  }
  if (!layer->id.empty() || !layer->name.empty()) {
    return false;
  }
  if (!layer->customData.empty()) {
    return false;
  }
  if (layer->layout != LayoutMode::None || layer->flex != 0.0f) {
    return false;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  if (HasConstraintAttributes(layer->left, layer->right, layer->top, layer->bottom, layer->centerX,
                              layer->centerY)) {
    return false;
  }
  if (!layer->includeInLayout) {
    return false;
  }
  return true;
}

static void CollectLintHintsFromContentsHelper(const std::vector<Element*>& elements,
                                               float outerProduct, const Layer* layer,
                                               std::vector<LintDiagnostic>& diagnostics);

static void CollectLintHintsFromContents(const Layer* layer,
                                         std::vector<LintDiagnostic>& diagnostics,
                                         float repeaterProductSoFar) {
  CollectLintHintsFromContentsHelper(layer->contents, repeaterProductSoFar, layer, diagnostics);
}

static void CollectLintHintsFromContentsHelper(const std::vector<Element*>& elements,
                                               float outerProduct, const Layer* layer,
                                               std::vector<LintDiagnostic>& diagnostics) {
  float localProduct = outerProduct;
  int repeaterCount = 0;
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Repeater) {
      auto repeater = static_cast<const Repeater*>(element);
      localProduct *= repeater->copies;
      repeaterCount++;
    }
  }

  bool hasRepeater = localProduct > outerProduct;
  bool hasStrokeWithDashes = false;
  auto name = layer->name.empty() ? layer->id : layer->name;

  for (auto* element : elements) {
    NodeType type = element->nodeType();

    if (type == NodeType::Repeater) {
      auto repeater = static_cast<const Repeater*>(element);
      if (repeater->copies > 200.0f) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": Repeater with high copies count (" +
                       std::to_string(static_cast<int>(repeater->copies)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
      if (localProduct > 500.0f && (outerProduct > 1.0f || repeaterCount > 1)) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": Nested Repeater with product " +
                       std::to_string(static_cast<int>(localProduct)) +
                       " (> 500) may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (type == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(element);
      if (hasRepeater && stroke->align != StrokeAlign::Center) {
        std::string alignStr = (stroke->align == StrokeAlign::Inside) ? "Inside" : "Outside";
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": Stroke with non-center alignment (" + alignStr +
                       ") in Repeater scope requires expensive CPU path operations";
        diagnostics.push_back(std::move(diag));
      }
      if (!stroke->dashes.empty()) {
        hasStrokeWithDashes = true;
      }
    }

    if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        auto verbCount = path->data->verbs().size();
        if (verbCount > 15) {
          LintDiagnostic diag = {};
          diag.category = "warning";
          diag.message = name + ": Path with high complexity (" + std::to_string(verbCount) +
                         " verbs) may cause rendering performance issues";
          diagnostics.push_back(std::move(diag));
        }
      }
    }

    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      CollectLintHintsFromContentsHelper(group->elements, localProduct, layer, diagnostics);
    }
  }

  if (hasRepeater && hasStrokeWithDashes) {
    LintDiagnostic diag = {};
    diag.category = "warning";
    diag.message = name +
                   ": Dashed Stroke within Repeater scope may cause performance "
                   "issues due to complex rendering calculations";
    diagnostics.push_back(std::move(diag));
  }
}

static void CollectBlurHints(const Layer* layer, std::vector<LintDiagnostic>& diagnostics) {
  auto name = layer->name.empty() ? layer->id : layer->name;

  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      auto blur = static_cast<const BlurFilter*>(filter);
      if (blur->blurX > 30.0f || blur->blurY > 30.0f) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": BlurFilter with high blur radius (X:" +
                       std::to_string(static_cast<int>(blur->blurX)) +
                       " Y:" + std::to_string(static_cast<int>(blur->blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }
  }

  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle) {
      auto shadow = static_cast<const DropShadowStyle*>(style);
      if (shadow->blurX > 30.0f || shadow->blurY > 30.0f) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": DropShadowStyle with high blur radius (X:" +
                       std::to_string(static_cast<int>(shadow->blurX)) +
                       " Y:" + std::to_string(static_cast<int>(shadow->blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    } else if (type == NodeType::BackgroundBlurStyle) {
      auto bgBlur = static_cast<const BackgroundBlurStyle*>(style);
      if (bgBlur->blurX > 30.0f || bgBlur->blurY > 30.0f) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = name + ": BackgroundBlurStyle with high blur radius (X:" +
                       std::to_string(static_cast<int>(bgBlur->blurX)) +
                       " Y:" + std::to_string(static_cast<int>(bgBlur->blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }
  }
}

static bool HasHighCostElementsHelper(const std::vector<Element*>& elements);

static bool HasHighCostElements(const Layer* layer) {
  if (HasHighCostElementsHelper(layer->contents)) {
    return true;
  }
  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      return true;
    }
  }
  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle || type == NodeType::BackgroundBlurStyle ||
        type == NodeType::InnerShadowStyle) {
      return true;
    }
  }
  return false;
}

static bool HasHighCostElementsHelper(const std::vector<Element*>& elements) {
  for (auto* element : elements) {
    NodeType type = element->nodeType();
    if (type == NodeType::Repeater) {
      return true;
    }
    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      if (HasHighCostElementsHelper(group->elements)) {
        return true;
      }
    }
  }
  return false;
}

static bool IsMaskASimpleRectangle(const Layer* mask) {
  if (mask == nullptr) {
    return false;
  }
  if (mask->contents.size() != 2) {
    return false;
  }
  if (mask->contents[0]->nodeType() != NodeType::Rectangle) {
    return false;
  }
  if (mask->contents[1]->nodeType() != NodeType::Fill) {
    return false;
  }
  auto fill = static_cast<const Fill*>(mask->contents[1]);
  if (fill->color == nullptr || fill->color->nodeType() != NodeType::SolidColor) {
    return false;
  }
  if (fill->alpha < 0.999f) {
    return false;
  }
  if (!mask->matrix.isIdentity() || !mask->matrix3D.isIdentity()) {
    return false;
  }
  if (!mask->styles.empty() || !mask->filters.empty()) {
    return false;
  }
  if (mask->blendMode != BlendMode::Normal) {
    return false;
  }
  return true;
}

static void CollectLintHints(const Layer* layer, std::vector<LintDiagnostic>& diagnostics,
                             bool isRoot) {
  auto name = layer->name.empty() ? layer->id : layer->name;

  if (!isRoot && !layer->children.empty() && layer->layout == LayoutMode::None) {
    bool allDowngradable = true;
    for (auto* child : layer->children) {
      if (!CanDowngradeToGroup(child)) {
        allDowngradable = false;
        break;
      }
    }
    if (allDowngradable) {
      auto count = static_cast<int>(layer->children.size());
      LintDiagnostic diag = {};
      diag.category = "warning";
      diag.message = name + ": " + std::to_string(count) +
                     " child Layer(s) use no Layer-exclusive "
                     "features and could be downgraded to Groups";
      diagnostics.push_back(std::move(diag));
    }
  }

  CollectLintHintsFromContents(layer, diagnostics, 1.0f);
  CollectBlurHints(layer, diagnostics);

  if (layer->alpha < 0.2f && layer->alpha > 0.0f && HasHighCostElements(layer)) {
    LintDiagnostic diag = {};
    diag.category = "warning";
    diag.message = name + ": Low opacity (" + std::to_string(static_cast<int>(layer->alpha * 100)) +
                   "%) with high-cost elements (Repeater/Blur) may indicate unnecessary rendering";
    diagnostics.push_back(std::move(diag));
  }

  if (layer->mask != nullptr && layer->maskType == MaskType::Alpha &&
      IsMaskASimpleRectangle(layer->mask)) {
    LintDiagnostic diag = {};
    diag.category = "warning";
    diag.message =
        name +
        ": Using rectangular mask could be replaced with clipToBounds for better performance";
    diagnostics.push_back(std::move(diag));
  }

  for (auto* child : layer->children) {
    CollectLintHints(child, diagnostics, false);
  }
}

static void CollectAllLintHints(const PAGXDocument* document,
                                std::vector<LintDiagnostic>& diagnostics) {
  for (auto* layer : document->layers) {
    CollectLintHints(layer, diagnostics, true);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto comp = static_cast<const Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CollectLintHints(layer, diagnostics, true);
      }
    }
  }
}

// --- Pass 2: PathData deduplication detection ---

static bool PathDataEqual(const PathData* a, const PathData* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return a->verbs() == b->verbs() && a->points() == b->points();
}

static int CountDuplicatePathData(const PAGXDocument* document) {
  std::vector<const PathData*> uniquePaths = {};
  int duplicateCount = 0;

  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::PathData) {
      continue;
    }
    auto pathData = static_cast<const PathData*>(node.get());
    bool found = false;
    for (auto* existing : uniquePaths) {
      if (PathDataEqual(existing, pathData)) {
        duplicateCount++;
        found = true;
        break;
      }
    }
    if (!found) {
      uniquePaths.push_back(pathData);
    }
  }
  return duplicateCount;
}

// --- Pass 3: Gradient deduplication detection ---

static bool ColorStopsEqual(const std::vector<ColorStop*>& a, const std::vector<ColorStop*>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i]->offset != b[i]->offset || a[i]->color != b[i]->color) {
      return false;
    }
  }
  return true;
}

static bool GradientEqual(const ColorSource* a, const ColorSource* b) {
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  auto type = a->nodeType();
  if (type == NodeType::LinearGradient) {
    auto ga = static_cast<const LinearGradient*>(a);
    auto gb = static_cast<const LinearGradient*>(b);
    return ga->startPoint == gb->startPoint && ga->endPoint == gb->endPoint &&
           ga->matrix == gb->matrix && ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::RadialGradient) {
    auto ga = static_cast<const RadialGradient*>(a);
    auto gb = static_cast<const RadialGradient*>(b);
    return ga->center == gb->center && ga->radius == gb->radius && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::ConicGradient) {
    auto ga = static_cast<const ConicGradient*>(a);
    auto gb = static_cast<const ConicGradient*>(b);
    return ga->center == gb->center && ga->startAngle == gb->startAngle &&
           ga->endAngle == gb->endAngle && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::DiamondGradient) {
    auto ga = static_cast<const DiamondGradient*>(a);
    auto gb = static_cast<const DiamondGradient*>(b);
    return ga->center == gb->center && ga->radius == gb->radius && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  return false;
}

static int CountDuplicateGradients(const PAGXDocument* document) {
  std::vector<const ColorSource*> uniqueSources = {};
  int duplicateCount = 0;

  for (auto& node : document->nodes) {
    if (!IsGradient(node->nodeType())) {
      continue;
    }
    auto source = static_cast<const ColorSource*>(node.get());
    bool found = false;
    for (auto* existing : uniqueSources) {
      if (GradientEqual(existing, source)) {
        duplicateCount++;
        found = true;
        break;
      }
    }
    if (!found) {
      uniqueSources.push_back(source);
    }
  }
  return duplicateCount;
}

// --- Pass 4: Mergeable Group detection ---

static bool IsGeometry(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
         type == NodeType::Path;
}

static bool IsPainter(NodeType type) {
  return type == NodeType::Fill || type == NodeType::Stroke;
}

static bool IsModifier(NodeType type) {
  return type == NodeType::TrimPath || type == NodeType::RoundCorner || type == NodeType::MergePath;
}

static bool HasDefaultGroupTransform(const Group* group) {
  if (group->anchor.x != 0 || group->anchor.y != 0 || group->position.x != 0 ||
      group->position.y != 0 || group->rotation != 0 || group->scale.x != 1 ||
      group->scale.y != 1 || group->skew != 0 || group->skewAxis != 0 || group->alpha != 1) {
    return false;
  }
  if (HasConstraintAttributes(group->left, group->right, group->top, group->bottom, group->centerX,
                              group->centerY)) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  return true;
}

static bool SplitGeometryAndPainters(const Group* group, std::vector<Element*>& geometry,
                                     std::vector<Element*>& painters) {
  bool seenPainter = false;
  for (auto* element : group->elements) {
    auto type = element->nodeType();
    if (IsModifier(type)) {
      return false;
    }
    if (type == NodeType::Group || type == NodeType::Repeater || type == NodeType::Text ||
        type == NodeType::TextBox || type == NodeType::TextPath || type == NodeType::TextModifier) {
      return false;
    }
    if (IsPainter(type)) {
      seenPainter = true;
      painters.push_back(element);
    } else if (IsGeometry(type)) {
      if (seenPainter) {
        return false;
      }
      geometry.push_back(element);
    } else {
      return false;
    }
  }
  return !geometry.empty() && !painters.empty();
}

static bool ElementsStructurallyEqual(const Element* a, const Element* b);

static bool PainterColorsEqual(const ColorSource* a, const ColorSource* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  if (a->nodeType() == NodeType::SolidColor) {
    return static_cast<const SolidColor*>(a)->color == static_cast<const SolidColor*>(b)->color;
  }
  return false;
}

static bool ElementsStructurallyEqual(const Element* a, const Element* b) {
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  auto type = a->nodeType();
  if (type == NodeType::Rectangle) {
    auto ra = static_cast<const Rectangle*>(a);
    auto rb = static_cast<const Rectangle*>(b);
    return ra->position == rb->position && ra->size == rb->size && ra->roundness == rb->roundness &&
           ra->reversed == rb->reversed;
  }
  if (type == NodeType::Ellipse) {
    auto ea = static_cast<const Ellipse*>(a);
    auto eb = static_cast<const Ellipse*>(b);
    return ea->position == eb->position && ea->size == eb->size && ea->reversed == eb->reversed;
  }
  if (type == NodeType::Polystar) {
    auto pa = static_cast<const Polystar*>(a);
    auto pb = static_cast<const Polystar*>(b);
    return pa->position == pb->position && pa->type == pb->type &&
           pa->pointCount == pb->pointCount && pa->outerRadius == pb->outerRadius &&
           pa->innerRadius == pb->innerRadius && pa->rotation == pb->rotation &&
           pa->outerRoundness == pb->outerRoundness && pa->innerRoundness == pb->innerRoundness &&
           pa->reversed == pb->reversed;
  }
  if (type == NodeType::Path) {
    auto pathA = static_cast<const Path*>(a);
    auto pathB = static_cast<const Path*>(b);
    if (pathA->reversed != pathB->reversed) {
      return false;
    }
    return PathDataEqual(pathA->data, pathB->data);
  }
  if (type == NodeType::Fill) {
    auto fa = static_cast<const Fill*>(a);
    auto fb = static_cast<const Fill*>(b);
    return fa->alpha == fb->alpha && fa->blendMode == fb->blendMode &&
           fa->fillRule == fb->fillRule && fa->placement == fb->placement &&
           PainterColorsEqual(fa->color, fb->color);
  }
  if (type == NodeType::Stroke) {
    auto sa = static_cast<const Stroke*>(a);
    auto sb = static_cast<const Stroke*>(b);
    return sa->width == sb->width && sa->alpha == sb->alpha && sa->blendMode == sb->blendMode &&
           sa->cap == sb->cap && sa->join == sb->join && sa->miterLimit == sb->miterLimit &&
           sa->dashes == sb->dashes && sa->dashOffset == sb->dashOffset &&
           sa->dashAdaptive == sb->dashAdaptive && sa->align == sb->align &&
           sa->placement == sb->placement && PainterColorsEqual(sa->color, sb->color);
  }
  if (type == NodeType::Group) {
    auto ga = static_cast<const Group*>(a);
    auto gb = static_cast<const Group*>(b);
    if (ga->position != gb->position || ga->anchor != gb->anchor || ga->rotation != gb->rotation ||
        ga->scale != gb->scale || ga->skew != gb->skew || ga->skewAxis != gb->skewAxis ||
        ga->alpha != gb->alpha) {
      return false;
    }
    if (ga->elements.size() != gb->elements.size()) {
      return false;
    }
    for (size_t i = 0; i < ga->elements.size(); i++) {
      if (!ElementsStructurallyEqual(ga->elements[i], gb->elements[i])) {
        return false;
      }
    }
    return true;
  }
  if (type == NodeType::Text) {
    auto ta = static_cast<const Text*>(a);
    auto tb = static_cast<const Text*>(b);
    if (ta->text != tb->text || ta->fontFamily != tb->fontFamily ||
        ta->fontStyle != tb->fontStyle || ta->fontSize != tb->fontSize ||
        ta->letterSpacing != tb->letterSpacing || ta->fauxBold != tb->fauxBold ||
        ta->fauxItalic != tb->fauxItalic || ta->textAnchor != tb->textAnchor ||
        ta->glyphRuns.size() != tb->glyphRuns.size()) {
      return false;
    }
    for (size_t i = 0; i < ta->glyphRuns.size(); i++) {
      auto ra = ta->glyphRuns[i];
      auto rb = tb->glyphRuns[i];
      if (ra->font != rb->font || ra->fontSize != rb->fontSize || ra->glyphs != rb->glyphs ||
          ra->x != rb->x || ra->y != rb->y || ra->xOffsets != rb->xOffsets ||
          ra->positions != rb->positions || ra->anchors != rb->anchors ||
          ra->scales != rb->scales || ra->rotations != rb->rotations || ra->skews != rb->skews) {
        return false;
      }
    }
    return true;
  }
  if (type == NodeType::TextBox) {
    auto tba = static_cast<const TextBox*>(a);
    auto tbb = static_cast<const TextBox*>(b);
    return tba->position == tbb->position &&
           (tba->width == tbb->width || (std::isnan(tba->width) && std::isnan(tbb->width))) &&
           (tba->height == tbb->height || (std::isnan(tba->height) && std::isnan(tbb->height))) &&
           tba->textAlign == tbb->textAlign && tba->paragraphAlign == tbb->paragraphAlign &&
           tba->writingMode == tbb->writingMode && tba->lineHeight == tbb->lineHeight &&
           tba->wordWrap == tbb->wordWrap && tba->overflow == tbb->overflow;
  }
  return false;
}

static bool PaintersEqual(const std::vector<Element*>& a, const std::vector<Element*>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (!ElementsStructurallyEqual(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

static int CountMergeableGroupsInElements(const std::vector<Element*>& elements) {
  int count = 0;
  size_t i = 0;
  while (i < elements.size()) {
    auto* element = elements[i];
    if (element->nodeType() != NodeType::Group) {
      i++;
      continue;
    }
    auto* group = static_cast<const Group*>(element);
    if (!HasDefaultGroupTransform(group)) {
      i++;
      continue;
    }
    std::vector<Element*> geometry = {};
    std::vector<Element*> painters = {};
    if (!SplitGeometryAndPainters(group, geometry, painters)) {
      i++;
      continue;
    }
    size_t j = i + 1;
    while (j < elements.size()) {
      auto* next = elements[j];
      if (next->nodeType() != NodeType::Group) {
        break;
      }
      auto* nextGroup = static_cast<const Group*>(next);
      if (!HasDefaultGroupTransform(nextGroup)) {
        break;
      }
      std::vector<Element*> nextGeometry = {};
      std::vector<Element*> nextPainters = {};
      if (!SplitGeometryAndPainters(nextGroup, nextGeometry, nextPainters)) {
        break;
      }
      if (!PaintersEqual(painters, nextPainters)) {
        break;
      }
      count++;
      j++;
    }
    i = j;
  }
  return count;
}

static int CountMergeableGroups(const PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<const Layer*>(node.get());
      count += CountMergeableGroupsInElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      auto group = static_cast<const Group*>(node.get());
      count += CountMergeableGroupsInElements(group->elements);
    }
  }
  return count;
}

// --- Pass 5: Redundant first-child Group detection ---

static bool CanUnwrapFirstChildGroup(const Group* group) {
  if (group->nodeType() != NodeType::Group) {
    return false;
  }
  if (group->alpha != 1.0f) {
    return false;
  }
  if (group->rotation != 0 || group->scale.x != 1 || group->scale.y != 1 || group->skew != 0 ||
      group->skewAxis != 0) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  if (HasConstraintAttributes(group->left, group->right, group->top, group->bottom, group->centerX,
                              group->centerY)) {
    return false;
  }
  for (auto* element : group->elements) {
    auto* layout = LayoutNode::AsLayoutNode(element);
    if (layout != nullptr) {
      if (!std::isnan(layout->right) || !std::isnan(layout->bottom) ||
          !std::isnan(layout->centerX) || !std::isnan(layout->centerY)) {
        return false;
      }
    }
  }
  return true;
}

static int CountUnwrappableFirstChildGroupsInElements(const std::vector<Element*>& elements) {
  int count = 0;
  if (elements.empty()) {
    return count;
  }
  auto* first = elements[0];
  if (first->nodeType() == NodeType::Group) {
    auto* group = static_cast<const Group*>(first);
    if (CanUnwrapFirstChildGroup(group)) {
      count++;
    }
  }
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      count += CountUnwrappableFirstChildGroupsInElements(group->elements);
    }
  }
  return count;
}

static int CountUnwrappableFirstChildGroups(const PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<const Layer*>(node.get());
      count += CountUnwrappableFirstChildGroupsInElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      auto group = static_cast<const Group*>(node.get());
      count += CountUnwrappableFirstChildGroupsInElements(group->elements);
    }
  }
  return count;
}

// --- Pass 6: Path to primitive detection ---

static int CountPathsToPrimitives(const PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    std::vector<Element*>* elements = nullptr;
    if (node->nodeType() == NodeType::Layer) {
      elements = &static_cast<Layer*>(node.get())->contents;
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      elements = &static_cast<Group*>(node.get())->elements;
    }
    if (elements == nullptr) {
      continue;
    }
    for (auto* element : *elements) {
      if (element->nodeType() != NodeType::Path) {
        continue;
      }
      auto path = static_cast<const Path*>(element);
      if (path->data == nullptr) {
        continue;
      }
      if (HasConstraintAttributes(path->left, path->right, path->top, path->bottom, path->centerX,
                                  path->centerY)) {
        continue;
      }
      auto tgfxPath = ToTGFX(*path->data);
      tgfx::RRect rrect = {};
      tgfx::Rect rect = {};
      if (tgfxPath.isRRect(&rrect) || tgfxPath.isOval(&rect) || tgfxPath.isRect(&rect)) {
        count++;
      }
    }
  }
  return count;
}

// --- Pass 9: Coordinate localization detection ---

static bool ShouldSkipLocalization(const Layer* layer) {
  if (!layer->matrix.isIdentity()) {
    return true;
  }
  if (layer->composition != nullptr) {
    return true;
  }
  if (layer->contents.empty()) {
    return true;
  }
  if (HasConstraintAttributes(layer->left, layer->right, layer->top, layer->bottom, layer->centerX,
                              layer->centerY)) {
    return true;
  }
  for (auto* element : layer->contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(element);
      if (HasConstraintAttributes(rect->left, rect->right, rect->top, rect->bottom, rect->centerX,
                                  rect->centerY)) {
        return true;
      }
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<const Ellipse*>(element);
      if (HasConstraintAttributes(ellipse->left, ellipse->right, ellipse->top, ellipse->bottom,
                                  ellipse->centerX, ellipse->centerY)) {
        return true;
      }
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<const Polystar*>(element);
      if (HasConstraintAttributes(polystar->left, polystar->right, polystar->top, polystar->bottom,
                                  polystar->centerX, polystar->centerY)) {
        return true;
      }
    } else if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (HasConstraintAttributes(path->left, path->right, path->top, path->bottom, path->centerX,
                                  path->centerY)) {
        return true;
      }
    } else if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      if (HasConstraintAttributes(text->left, text->right, text->top, text->bottom, text->centerX,
                                  text->centerY)) {
        return true;
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      if (HasConstraintAttributes(group->left, group->right, group->top, group->bottom,
                                  group->centerX, group->centerY)) {
        return true;
      }
    }
  }
  return false;
}

static int CountLocalizableCoordinates(const PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    auto layer = static_cast<const Layer*>(node.get());
    if (ShouldSkipLocalization(layer)) {
      continue;
    }
    if (layer->x != 0 || layer->y != 0) {
      count++;
    }
  }
  return count;
}

// --- Pass 10: PathData localization detection ---

static void CollectPaths(const std::vector<Element*>& elements, std::vector<const Path*>& paths) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Path) {
      paths.push_back(static_cast<const Path*>(element));
    } else if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      CollectPaths(static_cast<const Group*>(element)->elements, paths);
    }
  }
}

static int CountLocalizablePathData(const PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    std::vector<const Path*> paths = {};
    if (node->nodeType() == NodeType::Layer) {
      CollectPaths(static_cast<const Layer*>(node.get())->contents, paths);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      CollectPaths(static_cast<const Group*>(node.get())->elements, paths);
    }
    for (auto* path : paths) {
      if (path->data == nullptr || path->data->isEmpty()) {
        continue;
      }
      auto bounds = path->data->getBounds();
      if (std::abs(bounds.x) >= 0.001f || std::abs(bounds.y) >= 0.001f) {
        count++;
      }
    }
  }
  return count;
}

// --- Pass 12: Extractable Composition detection ---

static size_t HashElement(const Element* element) {
  size_t h = std::hash<int>{}(static_cast<int>(element->nodeType()));
  auto type = element->nodeType();
  if (type == NodeType::Rectangle) {
    auto rect = static_cast<const Rectangle*>(element);
    h ^= std::hash<float>{}(rect->position.x) * 29;
    h ^= std::hash<float>{}(rect->position.y) * 31;
    h ^= std::hash<float>{}(rect->size.width) * 37;
    h ^= std::hash<float>{}(rect->size.height) * 41;
    h ^= std::hash<float>{}(rect->roundness) * 43;
    h ^= std::hash<bool>{}(rect->reversed) * 47;
  } else if (type == NodeType::Ellipse) {
    auto ellipse = static_cast<const Ellipse*>(element);
    h ^= std::hash<float>{}(ellipse->position.x) * 29;
    h ^= std::hash<float>{}(ellipse->position.y) * 31;
    h ^= std::hash<float>{}(ellipse->size.width) * 37;
    h ^= std::hash<float>{}(ellipse->size.height) * 41;
    h ^= std::hash<bool>{}(ellipse->reversed) * 43;
  } else if (type == NodeType::Polystar) {
    auto polystar = static_cast<const Polystar*>(element);
    h ^= std::hash<float>{}(polystar->position.x) * 29;
    h ^= std::hash<float>{}(polystar->position.y) * 31;
    h ^= std::hash<int>{}(static_cast<int>(polystar->type)) * 37;
    h ^= std::hash<float>{}(polystar->pointCount) * 41;
    h ^= std::hash<float>{}(polystar->outerRadius) * 43;
    h ^= std::hash<float>{}(polystar->innerRadius) * 47;
    h ^= std::hash<float>{}(polystar->rotation) * 53;
    h ^= std::hash<float>{}(polystar->outerRoundness) * 59;
    h ^= std::hash<float>{}(polystar->innerRoundness) * 61;
    h ^= std::hash<bool>{}(polystar->reversed) * 67;
  } else if (type == NodeType::Path) {
    auto path = static_cast<const Path*>(element);
    if (path->data != nullptr) {
      h ^= std::hash<size_t>{}(path->data->verbs().size()) * 31;
      h ^= std::hash<size_t>{}(path->data->points().size()) * 37;
    }
    h ^= std::hash<bool>{}(path->reversed) * 43;
  } else if (type == NodeType::Fill) {
    auto fill = static_cast<const Fill*>(element);
    h ^= std::hash<float>{}(fill->alpha) * 31;
    h ^= std::hash<int>{}(static_cast<int>(fill->blendMode)) * 37;
    h ^= std::hash<int>{}(static_cast<int>(fill->fillRule)) * 41;
    if (fill->color != nullptr) {
      h ^= std::hash<int>{}(static_cast<int>(fill->color->nodeType())) * 43;
    }
  } else if (type == NodeType::Stroke) {
    auto stroke = static_cast<const Stroke*>(element);
    h ^= std::hash<float>{}(stroke->width) * 31;
    h ^= std::hash<float>{}(stroke->alpha) * 37;
    h ^= std::hash<int>{}(static_cast<int>(stroke->cap)) * 41;
    h ^= std::hash<int>{}(static_cast<int>(stroke->join)) * 43;
  } else if (type == NodeType::Group) {
    auto group = static_cast<const Group*>(element);
    h ^= std::hash<float>{}(group->position.x) * 29;
    h ^= std::hash<float>{}(group->position.y) * 31;
    h ^= std::hash<float>{}(group->rotation) * 37;
    h ^= std::hash<float>{}(group->scale.x) * 41;
    h ^= std::hash<float>{}(group->scale.y) * 43;
    h ^= std::hash<float>{}(group->alpha) * 47;
    h ^= std::hash<size_t>{}(group->elements.size()) * 53;
  }
  return h;
}

static size_t HashLayerStructure(const Layer* layer) {
  size_t h = std::hash<size_t>{}(layer->contents.size());
  h ^= std::hash<size_t>{}(layer->children.size()) * 17;
  for (auto* element : layer->contents) {
    h = h * 131 + HashElement(element);
  }
  for (auto* child : layer->children) {
    h = h * 131 + HashLayerStructure(child);
  }
  return h;
}

static bool LayersStructurallyEqual(const Layer* a, const Layer* b) {
  if (a->contents.size() != b->contents.size()) {
    return false;
  }
  if (a->children.size() != b->children.size()) {
    return false;
  }
  if (a->composition != nullptr || b->composition != nullptr) {
    return false;
  }
  if (a->styles.size() != b->styles.size() || a->filters.size() != b->filters.size()) {
    return false;
  }
  if (a->alpha != b->alpha || a->blendMode != b->blendMode || a->visible != b->visible ||
      a->maskType != b->maskType || a->antiAlias != b->antiAlias ||
      a->groupOpacity != b->groupOpacity) {
    return false;
  }
  if (!a->matrix.isIdentity() || !b->matrix.isIdentity()) {
    return false;
  }
  for (size_t i = 0; i < a->contents.size(); i++) {
    if (!ElementsStructurallyEqual(a->contents[i], b->contents[i])) {
      return false;
    }
  }
  for (size_t i = 0; i < a->children.size(); i++) {
    if (!LayersStructurallyEqual(a->children[i], b->children[i])) {
      return false;
    }
  }
  return true;
}

static void CollectCandidateLayers(
    const Layer* layer, std::unordered_map<size_t, std::vector<const Layer*>>& hashGroups,
    bool parentHasContainerLayout) {
  bool skipDueToLayout = parentHasContainerLayout && layer->includeInLayout;
  if (!skipDueToLayout && !layer->contents.empty() && layer->composition == nullptr &&
      layer->matrix.isIdentity() && layer->children.empty() && layer->styles.empty() &&
      layer->filters.empty() && layer->flex == 0.0f && std::isnan(layer->right) &&
      std::isnan(layer->bottom) && std::isnan(layer->centerX) && std::isnan(layer->centerY)) {
    auto hash = HashLayerStructure(layer);
    hashGroups[hash].push_back(layer);
  }
  bool thisHasContainerLayout = layer->layout != LayoutMode::None;
  for (auto* child : layer->children) {
    CollectCandidateLayers(child, hashGroups, thisHasContainerLayout);
  }
}

static int CountExtractableCompositions(const PAGXDocument* document) {
  std::unordered_map<size_t, std::vector<const Layer*>> hashGroups = {};
  for (auto* layer : document->layers) {
    CollectCandidateLayers(layer, hashGroups, false);
  }

  int count = 0;
  for (auto& pair : hashGroups) {
    auto& layers = pair.second;
    if (layers.size() < 2) {
      continue;
    }
    std::vector<bool> used(layers.size(), false);
    for (size_t i = 0; i < layers.size(); i++) {
      if (used[i]) {
        continue;
      }
      int groupSize = 1;
      for (size_t j = i + 1; j < layers.size(); j++) {
        if (used[j]) {
          continue;
        }
        if (LayersStructurallyEqual(layers[i], layers[j])) {
          used[j] = true;
          groupSize++;
        }
      }
      if (groupSize >= 2) {
        count += groupSize;
      }
    }
  }
  return count;
}

// --- Output formatting ---

static void PrintDiagnosticsText(const std::vector<LintDiagnostic>& diagnostics,
                                 const std::string& file) {
  for (const auto& diag : diagnostics) {
    if (diag.line > 0) {
      std::cerr << file << ":" << diag.line << ": " << diag.message << "\n";
    } else {
      std::cerr << file << ": " << diag.message << "\n";
    }
  }
}

static void PrintDiagnosticsJson(const std::vector<LintDiagnostic>& diagnostics,
                                 const std::string& file) {
  std::cout << "{\n";
  std::cout << "  \"file\": \"" << EscapeJson(file) << "\",\n";
  std::cout << "  \"ok\": " << (diagnostics.empty() ? "true" : "false") << ",\n";
  std::cout << "  \"diagnostics\": [";
  for (size_t i = 0; i < diagnostics.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\n    {\"line\": " << diagnostics[i].line << ", \"category\": \""
              << EscapeJson(diagnostics[i].category) << "\", \"message\": \""
              << EscapeJson(diagnostics[i].message) << "\"}";
  }
  if (!diagnostics.empty()) {
    std::cout << "\n  ";
  }
  std::cout << "]\n";
  std::cout << "}\n";
}

static void PrintUsage() {
  std::cout << "Usage: pagx lint [options] <file.pagx>\n"
            << "\n"
            << "Check a PAGX file for errors, structural issues, and optimization opportunities.\n"
            << "This command performs validation and detection without modifying the file.\n"
            << "\n"
            << "Options:\n"
            << "  --json                 Output in JSON format\n"
            << "  -h, --help             Show this help message\n";
}

int RunLint(int argc, char* argv[]) {
  bool jsonOutput = false;
  std::string filePath = {};

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--json") == 0) {
      jsonOutput = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx lint: unknown option '" << argv[i] << "'\n";
      return 1;
    } else {
      filePath = argv[i];
    }
  }

  if (filePath.empty()) {
    std::cerr << "pagx lint: missing input file\n";
    return 1;
  }

  std::vector<LintDiagnostic> diagnostics = {};

  // Part A: XSD validation + semantic checks
  ValidateXsdAndSemantic(filePath, diagnostics);

  // If XSD validation failed, skip structural analysis
  bool hasErrors = false;
  for (const auto& diag : diagnostics) {
    if (diag.category == "error") {
      hasErrors = true;
      break;
    }
  }

  if (!hasErrors) {
    auto document = PAGXImporter::FromFile(filePath);
    if (document != nullptr) {
      // Part B: Structural issue detection
      int emptyNodes = CountEmptyNodes(document.get());
      if (emptyNodes > 0) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = std::to_string(emptyNodes) + " empty node(s) can be removed";
        diagnostics.push_back(std::move(diag));
      }

      int fullCanvasClipMasks = CountFullCanvasClipMasks(document.get());
      if (fullCanvasClipMasks > 0) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message = std::to_string(fullCanvasClipMasks) +
                       " full-canvas clip mask(s) can be removed (no visible effect)";
        diagnostics.push_back(std::move(diag));
      }

      int unreferencedResources = CountUnreferencedResources(document.get());
      if (unreferencedResources > 0) {
        LintDiagnostic diag = {};
        diag.category = "warning";
        diag.message =
            std::to_string(unreferencedResources) + " unreferenced resource(s) can be removed";
        diagnostics.push_back(std::move(diag));
      }

      // Part C: Optimization suggestions
      int duplicatePathData = CountDuplicatePathData(document.get());
      if (duplicatePathData > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message = std::to_string(duplicatePathData) + " duplicate PathData(s) can be merged";
        diagnostics.push_back(std::move(diag));
      }

      int duplicateGradients = CountDuplicateGradients(document.get());
      if (duplicateGradients > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message = std::to_string(duplicateGradients) + " duplicate gradient(s) can be merged";
        diagnostics.push_back(std::move(diag));
      }

      int mergeableGroups = CountMergeableGroups(document.get());
      if (mergeableGroups > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message =
            std::to_string(mergeableGroups) + " adjacent Group(s) with same painters can be merged";
        diagnostics.push_back(std::move(diag));
      }

      int unwrappableGroups = CountUnwrappableFirstChildGroups(document.get());
      if (unwrappableGroups > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message =
            std::to_string(unwrappableGroups) + " redundant first-child Group(s) can be unwrapped";
        diagnostics.push_back(std::move(diag));
      }

      int pathsToPrimitives = CountPathsToPrimitives(document.get());
      if (pathsToPrimitives > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message = std::to_string(pathsToPrimitives) +
                       " Path(s) can be replaced with Rectangle/Ellipse primitives";
        diagnostics.push_back(std::move(diag));
      }

      int localizableCoordinates = CountLocalizableCoordinates(document.get());
      if (localizableCoordinates > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message =
            std::to_string(localizableCoordinates) + " Layer(s) have coordinates that can be moved";
        diagnostics.push_back(std::move(diag));
      }

      int localizablePathData = CountLocalizablePathData(document.get());
      if (localizablePathData > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message =
            std::to_string(localizablePathData) + " PathData(s) can be localized to origin";
        diagnostics.push_back(std::move(diag));
      }

      int extractableCompositions = CountExtractableCompositions(document.get());
      if (extractableCompositions > 0) {
        LintDiagnostic diag = {};
        diag.category = "info";
        diag.message = std::to_string(extractableCompositions) +
                       " structurally identical Layer(s) can be extracted to shared Composition";
        diagnostics.push_back(std::move(diag));
      }

      // Lint hints (performance warnings)
      CollectAllLintHints(document.get(), diagnostics);
    }
  }

  // Output results
  if (jsonOutput) {
    PrintDiagnosticsJson(diagnostics, filePath);
  } else if (!diagnostics.empty()) {
    PrintDiagnosticsText(diagnostics, filePath);
  } else {
    std::cout << filePath << ": ok\n";
  }

  // Return non-zero if there are errors or warnings
  for (const auto& diag : diagnostics) {
    if (diag.category == "error" || diag.category == "warning") {
      return 1;
    }
  }
  return 0;
}

}  // namespace pagx::cli
