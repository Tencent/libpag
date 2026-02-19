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

#include "CommandFormat.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"

namespace pagx::cli {

static void PrintFormatUsage() {
  std::cout << "Usage: pagx format [options] <file.pagx>\n"
            << "\n"
            << "Formats (pretty-prints) a PAGX file with consistent indentation and attribute\n"
            << "ordering. By default, also applies structural optimizations.\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --indent <n>          Indentation spaces (default: 2)\n"
            << "  --no-optimize         Only format, skip optimizations\n"
            << "  -h, --help            Show this help message\n";
}

// Collects all Node pointers that are directly referenced by the document tree.
static void CollectReferencedNodes(const std::vector<Element*>& elements,
                                   std::unordered_set<const Node*>& referenced);

static void CollectReferencedNodesFromColorSource(const ColorSource* source,
                                                  std::unordered_set<const Node*>& referenced) {
  if (source == nullptr) {
    return;
  }
  referenced.insert(source);
  auto type = source->nodeType();
  if (type == NodeType::LinearGradient) {
    auto gradient = static_cast<const LinearGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::RadialGradient) {
    auto gradient = static_cast<const RadialGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::ConicGradient) {
    auto gradient = static_cast<const ConicGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::DiamondGradient) {
    auto gradient = static_cast<const DiamondGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::ImagePattern) {
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
    } else if (type == NodeType::Group) {
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

static void RemoveUnreferencedResources(PAGXDocument* document) {
  std::unordered_set<const Node*> referenced = {};
  for (auto* layer : document->layers) {
    CollectReferencedNodesFromLayer(layer, referenced);
  }
  auto& nodes = document->nodes;
  nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                              [&referenced](const std::unique_ptr<Node>& node) {
                                return referenced.find(node.get()) == referenced.end();
                              }),
              nodes.end());
}

static bool IsEmptyLayer(const Layer* layer) {
  return layer->contents.empty() && layer->children.empty() && layer->composition == nullptr &&
         layer->styles.empty() && layer->filters.empty();
}

static void RemoveEmptyLayers(std::vector<Layer*>& layers) {
  layers.erase(
      std::remove_if(layers.begin(), layers.end(),
                     [](const Layer* layer) { return IsEmptyLayer(layer); }),
      layers.end());
}

static void RemoveEmptyElements(std::vector<Element*>& elements) {
  elements.erase(
      std::remove_if(elements.begin(), elements.end(),
                     [](const Element* element) {
                       if (element->nodeType() == NodeType::Stroke) {
                         auto stroke = static_cast<const Stroke*>(element);
                         return stroke->width == 0;
                       }
                       if (element->nodeType() == NodeType::Group) {
                         auto group = static_cast<const Group*>(element);
                         return group->elements.empty();
                       }
                       return false;
                     }),
      elements.end());
}

static void RemoveEmptyNodes(PAGXDocument* document) {
  RemoveEmptyLayers(document->layers);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<Layer*>(node.get());
      RemoveEmptyLayers(layer->children);
      RemoveEmptyElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group) {
      auto group = static_cast<Group*>(node.get());
      RemoveEmptyElements(group->elements);
    } else if (node->nodeType() == NodeType::Composition) {
      auto composition = static_cast<Composition*>(node.get());
      RemoveEmptyLayers(composition->layers);
    }
  }
}

static bool PathDataEqual(const PathData* a, const PathData* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return a->verbs() == b->verbs() && a->points() == b->points();
}

static void DeduplicatePathData(PAGXDocument* document) {
  std::vector<PathData*> uniquePaths = {};
  std::unordered_map<const PathData*, PathData*> mergeMap = {};

  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::PathData) {
      continue;
    }
    auto pathData = static_cast<PathData*>(node.get());
    bool found = false;
    for (auto* existing : uniquePaths) {
      if (PathDataEqual(existing, pathData)) {
        mergeMap[pathData] = existing;
        found = true;
        break;
      }
    }
    if (!found) {
      uniquePaths.push_back(pathData);
    }
  }

  if (mergeMap.empty()) {
    return;
  }

  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Path) {
      auto path = static_cast<Path*>(node.get());
      auto it = mergeMap.find(path->data);
      if (it != mergeMap.end()) {
        path->data = it->second;
      }
    } else if (node->nodeType() == NodeType::TextPath) {
      auto textPath = static_cast<TextPath*>(node.get());
      auto it = mergeMap.find(textPath->path);
      if (it != mergeMap.end()) {
        textPath->path = it->second;
      }
    } else if (node->nodeType() == NodeType::Glyph) {
      auto glyph = static_cast<Glyph*>(node.get());
      auto it = mergeMap.find(glyph->path);
      if (it != mergeMap.end()) {
        glyph->path = it->second;
      }
    }
  }
}

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

static bool IsGradient(NodeType type) {
  return type == NodeType::LinearGradient || type == NodeType::RadialGradient ||
         type == NodeType::ConicGradient || type == NodeType::DiamondGradient;
}

static void DeduplicateColorSources(PAGXDocument* document) {
  std::vector<ColorSource*> uniqueSources = {};
  std::unordered_map<const ColorSource*, ColorSource*> mergeMap = {};

  for (auto& node : document->nodes) {
    if (!IsGradient(node->nodeType())) {
      continue;
    }
    auto source = static_cast<ColorSource*>(node.get());
    bool found = false;
    for (auto* existing : uniqueSources) {
      if (GradientEqual(existing, source)) {
        mergeMap[source] = existing;
        found = true;
        break;
      }
    }
    if (!found) {
      uniqueSources.push_back(source);
    }
  }

  if (mergeMap.empty()) {
    return;
  }

  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Fill) {
      auto fill = static_cast<Fill*>(node.get());
      auto it = mergeMap.find(fill->color);
      if (it != mergeMap.end()) {
        fill->color = it->second;
      }
    } else if (node->nodeType() == NodeType::Stroke) {
      auto stroke = static_cast<Stroke*>(node.get());
      auto it = mergeMap.find(stroke->color);
      if (it != mergeMap.end()) {
        stroke->color = it->second;
      }
    }
  }
}

static void OptimizeDocument(PAGXDocument* document) {
  RemoveEmptyNodes(document);
  DeduplicatePathData(document);
  DeduplicateColorSources(document);
  RemoveUnreferencedResources(document);
}

int RunFormat(int argc, char* argv[]) {
  std::string inputPath = {};
  std::string outputPath = {};
  bool optimize = true;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      PrintFormatUsage();
      return 0;
    }
    if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) &&
        i + 1 < argc) {
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--indent") == 0 && i + 1 < argc) {
      // Indent option is accepted but currently handled by the exporter default.
      ++i;
      continue;
    }
    if (std::strcmp(argv[i], "--no-optimize") == 0) {
      optimize = false;
      continue;
    }
    if (argv[i][0] == '-') {
      std::cerr << "pagx format: unknown option '" << argv[i] << "'\n";
      return 1;
    }
    if (inputPath.empty()) {
      inputPath = argv[i];
    } else {
      std::cerr << "pagx format: unexpected argument '" << argv[i] << "'\n";
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "pagx format: missing input file\n";
    PrintFormatUsage();
    return 1;
  }

  if (outputPath.empty()) {
    outputPath = inputPath;
  }

  auto document = PAGXImporter::FromFile(inputPath);
  if (document == nullptr) {
    std::cerr << "pagx format: failed to load '" << inputPath << "'\n";
    return 1;
  }

  if (optimize) {
    OptimizeDocument(document.get());
  }

  auto xml = PAGXExporter::ToXML(*document);

  std::ofstream out(outputPath);
  if (!out.is_open()) {
    std::cerr << "pagx format: failed to write '" << outputPath << "'\n";
    return 1;
  }
  out << xml;
  out.close();

  return 0;
}

}  // namespace pagx::cli
