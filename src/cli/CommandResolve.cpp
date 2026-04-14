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

#include "cli/CommandResolve.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "base/utils/MathUtil.h"
#include "cli/CliUtils.h"
#include "cli/CommandImport.h"
#include "pagx/PAGXExporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LayoutNode.h"

namespace pagx::cli {

struct ResolveOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  ImportFormatOptions formatOptions = {};
};

static void PrintResolveUsage() {
  std::cout << "Usage: pagx resolve <file> [options]\n"
            << "\n"
            << "Resolve all import directives in a PAGX file into native PAGX nodes.\n"
            << "Expands inline <svg> elements and external file references (import attribute)\n"
            << "within Layers.\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <file>         Output PAGX file (default: overwrite input)\n"
            << "\n"
            << "Format-specific options are shared with `pagx import`;\n"
            << "see `pagx import --help` for details.\n"
            << "\n"
            << "Examples:\n"
            << "  pagx resolve design.pagx                # resolve in place\n"
            << "  pagx resolve design.pagx -o out.pagx    # resolve to new file\n";
}

static int ParseResolveOptions(int argc, char* argv[], ResolveOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintResolveUsage();
      return -1;
    } else if (arg == "--svg-no-expand-use" || arg == "--svg-flatten-transforms" ||
               arg == "--svg-preserve-unknown") {
      // Format-specific options — handled by ParseFormatOptions below.
    } else if (arg[0] == '-') {
      std::cerr << "pagx resolve: error: unknown option '" << arg << "'\n";
      return 1;
    } else if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx resolve: error: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx resolve: error: missing input file\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }

  ParseFormatOptions(argc, argv, &options->formatOptions);
  return 0;
}

//--------------------------------------------------------------------------------------------------
// Resolve logic
//--------------------------------------------------------------------------------------------------

static bool ResolveOneLayer(Layer* layer, const std::string& baseDir,
                            const ImportFormatOptions& formatOptions, PAGXDocument* doc) {
  bool hasImportSource = !layer->importDirective.source.empty();
  bool hasImportContent = !layer->importDirective.content.empty();

  if (!hasImportSource && !hasImportContent) {
    return false;  // Nothing to resolve.
  }

  // Check for conflicting contents or children.
  if (!layer->contents.empty() || !layer->children.empty()) {
    std::string desc =
        hasImportSource ? ("import=\"" + layer->importDirective.source + "\"") : "inline <svg>";
    std::cerr << "pagx resolve: warning: Layer";
    if (!layer->id.empty()) {
      std::cerr << " '" << layer->id << "'";
    }
    std::cerr << " has " << desc << " but also contains other contents or children"
              << " — skipping resolve for this layer\n";
    return false;
  }

  ImportResult result;
  std::string resolvedFromDesc;

  if (hasImportSource) {
    auto filePath = baseDir + layer->importDirective.source;
    result = ImportFile(filePath, layer->importDirective.format, formatOptions, layer->width,
                        layer->height);
    resolvedFromDesc = layer->importDirective.source;
  } else {
    result = ImportString(layer->importDirective.content, layer->importDirective.format,
                          formatOptions, layer->width, layer->height);
    resolvedFromDesc = "inline svg";
  }

  if (!result.error.empty()) {
    std::cerr << "pagx resolve: error: " << result.error << "\n";
    return false;
  }
  for (auto& warning : result.warnings) {
    std::cerr << "pagx resolve: warning: " << warning << "\n";
  }

  auto& svgDoc = result.document;

  // Update Layer dimensions from source.
  if (std::isnan(layer->width) && svgDoc->width > 0) {
    layer->width = svgDoc->width;
  }
  if (std::isnan(layer->height) && svgDoc->height > 0) {
    layer->height = svgDoc->height;
  }

  // Collect the effective element layers from the conversion result. Root elements (e.g. <svg>)
  // become wrapper Layers whose actual content is in their children. If such a wrapper is a plain
  // shell (no own contents, no extra attributes), unwrap it and use its children as the effective
  // element layers. Otherwise, keep the wrapper itself.
  std::vector<Layer*> elementLayers;
  for (auto* svgLayer : svgDoc->layers) {
    bool isPlainWrapper =
        IsLayerShell(svgLayer) && svgLayer->contents.empty() && !svgLayer->children.empty();
    if (isPlainWrapper) {
      for (auto* child : svgLayer->children) {
        elementLayers.push_back(child);
      }
    } else {
      elementLayers.push_back(svgLayer);
    }
  }

  // Determine how to embed element layers into the parent layer.
  // - Single clean layer (no extra attributes): unpack its contents directly.
  // - Multiple layers, all downgradable to Group: wrap each in a Group for painter isolation.
  // - Otherwise: keep ALL as child Layers (partial downgrade forbidden — contents render behind
  //   children, so mixing would change the paint order).
  bool canUnpack = false;
  bool canDowngradeAll = false;
  if (elementLayers.size() == 1) {
    auto* single = elementLayers[0];
    canUnpack = IsLayerShell(single) && single->children.empty();
  } else if (elementLayers.size() > 1) {
    canDowngradeAll = true;
    for (auto* el : elementLayers) {
      if (!el->children.empty() || HasLayerOnlyFeatures(el)) {
        canDowngradeAll = false;
        break;
      }
    }
  }

  if (canUnpack) {
    for (auto* element : elementLayers[0]->contents) {
      layer->contents.push_back(element);
    }
  } else if (canDowngradeAll) {
    for (size_t i = 0; i < elementLayers.size(); i++) {
      auto* elemLayer = elementLayers[i];
      bool unpackFirst = false;
      if (i == 0 && elemLayer->matrix.isIdentity() && elemLayer->alpha == 1.0f) {
        unpackFirst = true;
        for (auto* child : elemLayer->contents) {
          auto* layoutNode = LayoutNode::AsLayoutNode(child);
          if (layoutNode != nullptr &&
              (!std::isnan(layoutNode->right) || !std::isnan(layoutNode->bottom) ||
               !std::isnan(layoutNode->centerX) || !std::isnan(layoutNode->centerY))) {
            unpackFirst = false;
            break;
          }
        }
      }
      if (unpackFirst) {
        for (auto* element : elemLayer->contents) {
          layer->contents.push_back(element);
        }
      } else {
        auto m = elemLayer->matrix;
        bool hasSkew = !pag::FloatNearlyEqual(m.a * m.c + m.b * m.d, 0.0f);
        if (hasSkew) {
          // Matrix contains skew which cannot be represented by Group's
          // position/scale/rotation. Keep it as a Layer instead of downgrading.
          layer->children.push_back(elemLayer);
          continue;
        }
        auto* group = doc->makeNode<Group>();
        group->elements = std::move(elemLayer->contents);
        if (!elemLayer->matrix.isIdentity()) {
          group->position = {m.tx, m.ty};
          if (m.a != 1 || m.b != 0 || m.c != 0 || m.d != 1) {
            float sx = std::sqrt(m.a * m.a + m.b * m.b);
            float sy = std::sqrt(m.c * m.c + m.d * m.d);
            float det = m.a * m.d - m.b * m.c;
            if (det < 0) {
              sy = -sy;
            }
            float rot = pag::RadiansToDegrees(std::atan2(m.b, m.a));
            group->scale = {sx, sy};
            group->rotation = rot;
          }
        }
        group->alpha = elemLayer->alpha;
        layer->contents.push_back(group);
      }
    }
  } else {
    for (auto* elemLayer : elementLayers) {
      layer->children.push_back(elemLayer);
    }
  }

  // Transfer ownership of all nodes from imported document to target document.
  for (auto& node : svgDoc->nodes) {
    doc->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  // Clear import fields and set resolved marker.
  layer->importDirective.source.clear();
  layer->importDirective.format.clear();
  layer->importDirective.content.clear();
  layer->importDirective.resolvedFrom = resolvedFromDesc;

  return true;
}

static void ResolveLayers(const std::vector<Layer*>& layers, const std::string& baseDir,
                          const ImportFormatOptions& formatOptions, PAGXDocument* doc,
                          int& resolvedCount, int& errorCount) {
  for (auto* layer : layers) {
    bool hasImport =
        !layer->importDirective.source.empty() || !layer->importDirective.content.empty();
    if (hasImport) {
      if (ResolveOneLayer(layer, baseDir, formatOptions, doc)) {
        resolvedCount++;
      } else if (!layer->importDirective.source.empty() ||
                 !layer->importDirective.content.empty()) {
        // ResolveOneLayer returned false and fields are still set — this is an error,
        // not a skip (skips clear the fields themselves or leave them for re-resolve).
        errorCount++;
      }
    }
    // Recurse into child layers.
    ResolveLayers(layer->children, baseDir, formatOptions, doc, resolvedCount, errorCount);
  }
}

//--------------------------------------------------------------------------------------------------
// Entry point
//--------------------------------------------------------------------------------------------------

int RunResolve(int argc, char* argv[]) {
  ResolveOptions options = {};
  auto parseResult = ParseResolveOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto doc = LoadDocument(options.inputFile, "pagx resolve");
  if (doc == nullptr) {
    return 1;
  }

  auto baseDir = GetDirectory(options.inputFile);
  int resolvedCount = 0;
  int errorCount = 0;

  ResolveLayers(doc->layers, baseDir, options.formatOptions, doc.get(), resolvedCount, errorCount);

  // Also resolve in Composition layers.
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      ResolveLayers(comp->layers, baseDir, options.formatOptions, doc.get(), resolvedCount,
                    errorCount);
    }
  }

  if (resolvedCount == 0 && errorCount == 0) {
    return 0;
  }

  auto xml = PAGXExporter::ToXML(*doc);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx resolve: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;

  std::cout << "pagx resolve: resolved " << resolvedCount << " import(s)";
  if (errorCount > 0) {
    std::cout << ", " << errorCount << " error(s)";
  }
  std::cout << ", wrote " << options.outputFile << "\n";
  return errorCount > 0 ? 1 : 0;
}

}  // namespace pagx::cli
