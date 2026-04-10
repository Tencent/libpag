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
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"

namespace pagx::cli {

struct ResolveOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  bool svgExpandUse = true;
  bool svgFlattenTransforms = false;
  bool svgPreserveUnknown = false;
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
            << "SVG options (see `pagx import` for details):\n"
            << "  --svg-no-expand-use         Do not expand <use> references\n"
            << "  --svg-flatten-transforms    Flatten nested transforms into single matrices\n"
            << "  --svg-preserve-unknown      Preserve unsupported SVG elements as Unknown nodes\n"
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
    } else if (arg == "--svg-no-expand-use") {
      options->svgExpandUse = false;
    } else if (arg == "--svg-flatten-transforms") {
      options->svgFlattenTransforms = true;
    } else if (arg == "--svg-preserve-unknown") {
      options->svgPreserveUnknown = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintResolveUsage();
      return -1;
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

  return 0;
}

//--------------------------------------------------------------------------------------------------
// Resolve logic
//--------------------------------------------------------------------------------------------------

static std::string InferFormatFromContent(const std::string& content) {
  auto pos = content.find('<');
  while (pos != std::string::npos) {
    if (pos + 1 < content.size() && content[pos + 1] != '/' && content[pos + 1] != '!' &&
        content[pos + 1] != '?') {
      auto tagEnd = content.find_first_of(" \t\n/>", pos + 1);
      if (tagEnd != std::string::npos) {
        auto tagName = content.substr(pos + 1, tagEnd - pos - 1);
        for (auto& ch : tagName) {
          ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return tagName;
      }
    }
    pos = content.find('<', pos + 1);
  }
  return {};
}

static void InlinePathData(PAGXDocument* svgDoc) {
  for (auto& node : svgDoc->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      node->id.clear();
    }
  }
}

static bool ResolveOneLayer(Layer* layer, const std::string& baseDir,
                            const SVGImporter::Options& svgOptions, PAGXDocument* doc) {
  bool hasImportSource = !layer->importSource.empty();
  bool hasImportContent = !layer->importContent.empty();

  if (!hasImportSource && !hasImportContent) {
    return false;  // Nothing to resolve.
  }

  // Check for conflicting contents or children.
  if (!layer->contents.empty() || !layer->children.empty()) {
    std::string desc =
        hasImportSource ? ("import=\"" + layer->importSource + "\"") : "inline <svg>";
    std::cerr << "pagx resolve: warning: Layer";
    if (!layer->id.empty()) {
      std::cerr << " '" << layer->id << "'";
    }
    std::cerr << " has " << desc << " but also contains other contents or children"
              << " — skipping resolve for this layer\n";
    return false;
  }

  std::string format = layer->importFormat;
  std::string resolvedFromDesc;
  std::shared_ptr<PAGXDocument> svgDoc;

  if (hasImportSource) {
    // External mode: read from file.
    auto filePath = baseDir + layer->importSource;
    if (format.empty()) {
      format = GetFileExtension(filePath);
    }
    if (format != "svg") {
      std::cerr << "pagx resolve: error: unsupported import format '" << format << "' for '"
                << layer->importSource << "'\n";
      return false;
    }
    svgDoc = SVGImporter::Parse(filePath, svgOptions);
    if (svgDoc == nullptr) {
      std::cerr << "pagx resolve: error: failed to parse '" << filePath << "'\n";
      return false;
    }
    resolvedFromDesc = layer->importSource;
  } else {
    // Inline mode: parse embedded content.
    if (format.empty()) {
      format = InferFormatFromContent(layer->importContent);
    }
    if (format != "svg") {
      std::cerr << "pagx resolve: error: unsupported inline import format '" << format << "'\n";
      return false;
    }
    svgDoc = SVGImporter::ParseString(layer->importContent, svgOptions);
    if (svgDoc == nullptr) {
      std::cerr << "pagx resolve: error: failed to parse inline SVG content\n";
      return false;
    }
    resolvedFromDesc = "inline svg";
  }

  if (!svgDoc->errors.empty()) {
    for (auto& error : svgDoc->errors) {
      std::cerr << "pagx resolve: warning: " << error << "\n";
    }
  }

  // Update Layer dimensions from SVG source.
  if (std::isnan(layer->width) && svgDoc->width > 0) {
    layer->width = svgDoc->width;
  }
  if (std::isnan(layer->height) && svgDoc->height > 0) {
    layer->height = svgDoc->height;
  }

  InlinePathData(svgDoc.get());

  // Collect the effective element layers from the SVG conversion result. SVG root elements
  // (<svg>) become wrapper Layers whose actual content is in their children. If such a wrapper
  // is a plain shell (no own contents, no extra attributes), unwrap it and use its children
  // as the effective element layers. Otherwise, keep the svgLayer itself.
  std::vector<Layer*> elementLayers;
  for (auto* svgLayer : svgDoc->layers) {
    bool isPlainWrapper = svgLayer->contents.empty() && !svgLayer->children.empty() &&
                          svgLayer->matrix.isIdentity() && svgLayer->alpha == 1.0f &&
                          svgLayer->blendMode == BlendMode::Normal && svgLayer->mask == nullptr &&
                          svgLayer->styles.empty() && svgLayer->filters.empty();
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
    canUnpack = single->matrix.isIdentity() && single->alpha == 1.0f &&
                single->blendMode == BlendMode::Normal && single->mask == nullptr &&
                single->styles.empty() && single->filters.empty() && single->children.empty();
  } else if (elementLayers.size() > 1) {
    canDowngradeAll = true;
    for (auto* el : elementLayers) {
      if (!el->children.empty() || !el->styles.empty() || !el->filters.empty() ||
          el->mask != nullptr || el->composition != nullptr) {
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
            float rot = std::atan2(m.b, m.a) * 180.0f / 3.14159265358979323846f;
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

  // Transfer ownership of all nodes from SVG document to target document.
  for (auto& node : svgDoc->nodes) {
    doc->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  // Clear import fields and set resolved marker.
  layer->importSource.clear();
  layer->importFormat.clear();
  layer->importContent.clear();
  layer->resolvedFrom = resolvedFromDesc;

  return true;
}

static void ResolveLayers(const std::vector<Layer*>& layers, const std::string& baseDir,
                          const SVGImporter::Options& svgOptions, PAGXDocument* doc,
                          int& resolvedCount, int& errorCount) {
  for (auto* layer : layers) {
    bool hasImport = !layer->importSource.empty() || !layer->importContent.empty();
    if (hasImport) {
      if (ResolveOneLayer(layer, baseDir, svgOptions, doc)) {
        resolvedCount++;
      } else if (!layer->importSource.empty() || !layer->importContent.empty()) {
        // ResolveOneLayer returned false and fields are still set — this is an error,
        // not a skip (skips clear the fields themselves or leave them for re-resolve).
        errorCount++;
      }
    }
    // Recurse into child layers.
    ResolveLayers(layer->children, baseDir, svgOptions, doc, resolvedCount, errorCount);
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

  auto doc = PAGXImporter::FromFile(options.inputFile);
  if (doc == nullptr) {
    std::cerr << "pagx resolve: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!doc->errors.empty()) {
    for (auto& error : doc->errors) {
      std::cerr << "pagx resolve: warning: " << error << "\n";
    }
  }

  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = options.svgExpandUse;
  svgOptions.flattenTransforms = options.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = options.svgPreserveUnknown;

  auto baseDir = GetDirectory(options.inputFile);
  int resolvedCount = 0;
  int errorCount = 0;

  ResolveLayers(doc->layers, baseDir, svgOptions, doc.get(), resolvedCount, errorCount);

  // Also resolve in Composition layers.
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      ResolveLayers(comp->layers, baseDir, svgOptions, doc.get(), resolvedCount, errorCount);
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
