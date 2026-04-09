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

#include "cli/CommandImport.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "base/utils/MathUtil.h"
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Import.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"

namespace pagx::cli {

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  bool svgExpandUse = true;
  bool svgFlattenTransforms = false;
  bool svgPreserveUnknown = false;
  // Resolve mode: expand all <Import> nodes in a PAGX file.
  std::string resolveFile = {};
};

static void PrintUsage() {
  std::cout << "Usage: pagx import [options]\n"
            << "\n"
            << "Import a file from another format and convert it to PAGX, or resolve\n"
            << "<Import> nodes in an existing PAGX file.\n"
            << "\n"
            << "Standard mode (--input):\n"
            << "  --input <file>              Input file to import (required)\n"
            << "  --output <file>             Output PAGX file (default: <input>.pagx)\n"
            << "  --format <format>           Force input format (svg)\n"
            << "\n"
            << "Resolve mode (--resolve):\n"
            << "  --resolve <file>            Expand all <Import> nodes in a PAGX file\n"
            << "  --output <file>             Output PAGX file (default: overwrite input)\n"
            << "\n"
            << "SVG options (both modes):\n"
            << "  --svg-no-expand-use         Do not expand <use> references\n"
            << "  --svg-flatten-transforms    Flatten nested transforms into single matrices\n"
            << "  --svg-preserve-unknown      Preserve unsupported SVG elements as Unknown nodes\n"
            << "\n"
            << "Examples:\n"
            << "  pagx import --input icon.svg                     # SVG to icon.pagx\n"
            << "  pagx import --input icon.svg --output out.pagx   # SVG to out.pagx\n"
            << "  pagx import --resolve design.pagx                # resolve in place\n"
            << "  pagx import --resolve design.pagx -o out.pagx    # resolve to new file\n";
}

static int ParseOptions(int argc, char* argv[], ImportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFile = argv[++i];
    } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--resolve" && i + 1 < argc) {
      options->resolveFile = argv[++i];
    } else if (arg == "--svg-no-expand-use") {
      options->svgExpandUse = false;
    } else if (arg == "--svg-flatten-transforms") {
      options->svgFlattenTransforms = true;
    } else if (arg == "--svg-preserve-unknown") {
      options->svgPreserveUnknown = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx import: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx import: error: unexpected argument '" << arg
                << "', use --input or --resolve to specify the input file\n";
      return 1;
    }
    i++;
  }

  if (!options->inputFile.empty() && !options->resolveFile.empty()) {
    std::cerr << "pagx import: error: --input and --resolve are mutually exclusive\n";
    return 1;
  }

  if (options->inputFile.empty() && options->resolveFile.empty()) {
    std::cerr << "pagx import: error: missing --input or --resolve\n";
    return 1;
  }

  if (!options->resolveFile.empty()) {
    // Resolve mode.
    if (options->outputFile.empty()) {
      options->outputFile = options->resolveFile;
    }
  } else {
    // Standard mode.
    if (options->format.empty()) {
      options->format = GetFileExtension(options->inputFile);
    }
    if (options->outputFile.empty()) {
      options->outputFile = ReplaceExtension(options->inputFile, "pagx");
    }
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------
// Convert mode: import a file and write a new PAGX.
//--------------------------------------------------------------------------------------------------

static int ImportFromSVG(const ImportOptions& options) {
  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = options.svgExpandUse;
  svgOptions.flattenTransforms = options.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = options.svgPreserveUnknown;

  auto document = SVGImporter::Parse(options.inputFile, svgOptions);
  if (document == nullptr) {
    std::cerr << "pagx import: error: failed to parse '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx import: warning: " << error << "\n";
    }
  }

  auto xml = PAGXExporter::ToXML(*document);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx import: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;

  std::cout << "pagx import: wrote " << options.outputFile << "\n";
  return 0;
}

//--------------------------------------------------------------------------------------------------
// Resolve mode: expand all <Import> nodes in a PAGX file.
//--------------------------------------------------------------------------------------------------

static std::string GetDirectory(const std::string& path) {
  auto slash = path.rfind('/');
  if (slash != std::string::npos) {
    return path.substr(0, slash + 1);
  }
  return "./";
}

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

static bool ResolveOneImport(Import* imp, const std::string& baseDir,
                             const SVGImporter::Options& svgOptions, Layer* parentLayer,
                             PAGXDocument* doc, std::vector<Element*>& replacements) {
  std::string format = imp->format;
  std::shared_ptr<PAGXDocument> svgDoc;

  if (!imp->source.empty()) {
    // External mode: read from file.
    auto filePath = baseDir + imp->source;
    if (format.empty()) {
      format = GetFileExtension(filePath);
    }
    if (format != "svg") {
      std::cerr << "pagx import: error: unsupported import format '" << format << "' for '"
                << imp->source << "'\n";
      return false;
    }
    svgDoc = SVGImporter::Parse(filePath, svgOptions);
    if (svgDoc == nullptr) {
      std::cerr << "pagx import: error: failed to parse '" << filePath << "'\n";
      return false;
    }
  } else if (!imp->rawContent.empty()) {
    // Inline mode: parse embedded content.
    if (format.empty()) {
      format = InferFormatFromContent(imp->rawContent);
    }
    if (format != "svg") {
      std::cerr << "pagx import: error: unsupported inline import format '" << format << "'\n";
      return false;
    }
    svgDoc = SVGImporter::ParseString(imp->rawContent, svgOptions);
    if (svgDoc == nullptr) {
      std::cerr << "pagx import: error: failed to parse inline SVG content\n";
      return false;
    }
  } else {
    std::cerr << "pagx import: error: <Import> has no source and no inline content\n";
    return false;
  }

  if (!svgDoc->errors.empty()) {
    for (auto& error : svgDoc->errors) {
      std::cerr << "pagx import: warning: " << error << "\n";
    }
  }

  // Update parent Layer dimensions from SVG source.
  if (std::isnan(parentLayer->width) && svgDoc->width > 0) {
    parentLayer->width = svgDoc->width;
  }
  if (std::isnan(parentLayer->height) && svgDoc->height > 0) {
    parentLayer->height = svgDoc->height;
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
    for (auto* layer : elementLayers) {
      if (!layer->children.empty() || !layer->styles.empty() || !layer->filters.empty() ||
          layer->mask != nullptr || layer->composition != nullptr) {
        canDowngradeAll = false;
        break;
      }
    }
  }

  if (canUnpack) {
    for (auto* element : elementLayers[0]->contents) {
      replacements.push_back(element);
    }
  } else if (canDowngradeAll) {
    for (size_t i = 0; i < elementLayers.size(); i++) {
      auto* layer = elementLayers[i];
      // The first element layer can be unpacked directly (no preceding geometry to isolate from),
      // provided it has no extra attributes that would require a Group wrapper. This mirrors the
      // CanUnwrapFirstChildGroup lint check: identity transform, alpha=1, no constraints, no
      // explicit size, and no child elements using right/bottom/centerX/centerY constraints.
      bool unpackFirst = false;
      if (i == 0 && layer->matrix.isIdentity() && layer->alpha == 1.0f) {
        unpackFirst = true;
        for (auto* child : layer->contents) {
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
        for (auto* element : layer->contents) {
          replacements.push_back(element);
        }
      } else {
        auto m = layer->matrix;
        bool hasSkew = !pag::FloatNearlyEqual(m.a * m.c + m.b * m.d, 0.0f);
        if (hasSkew) {
          // Matrix contains skew which cannot be represented by Group's
          // position/scale/rotation. Keep it as a Layer instead of downgrading.
          parentLayer->children.push_back(layer);
          continue;
        }
        auto* group = doc->makeNode<Group>();
        group->elements = std::move(layer->contents);
        if (!layer->matrix.isIdentity()) {
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
        group->alpha = layer->alpha;
        replacements.push_back(group);
      }
    }
  } else {
    for (auto* layer : elementLayers) {
      parentLayer->children.push_back(layer);
    }
  }

  // Transfer ownership of all nodes from SVG document to target document.
  for (auto& node : svgDoc->nodes) {
    doc->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  return true;
}

static void ResolveLayerImports(Layer* layer, const std::string& baseDir,
                                const SVGImporter::Options& svgOptions, PAGXDocument* doc,
                                int& resolvedCount, int& errorCount) {
  std::vector<Element*> newContents;
  for (auto* element : layer->contents) {
    if (element->nodeType() != NodeType::Import) {
      newContents.push_back(element);
      continue;
    }
    auto* imp = static_cast<Import*>(element);
    std::vector<Element*> replacements;
    if (ResolveOneImport(imp, baseDir, svgOptions, layer, doc, replacements)) {
      newContents.insert(newContents.end(), replacements.begin(), replacements.end());
      resolvedCount++;
    } else {
      newContents.push_back(element);
      errorCount++;
    }
  }
  layer->contents = newContents;

  // Recurse into child layers.
  for (auto* child : layer->children) {
    ResolveLayerImports(child, baseDir, svgOptions, doc, resolvedCount, errorCount);
  }
}

static int ResolveImports(const ImportOptions& options) {
  auto doc = PAGXImporter::FromFile(options.resolveFile);
  if (doc == nullptr) {
    std::cerr << "pagx import: error: failed to load '" << options.resolveFile << "'\n";
    return 1;
  }
  if (!doc->errors.empty()) {
    for (auto& error : doc->errors) {
      std::cerr << "pagx import: warning: " << error << "\n";
    }
  }

  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = options.svgExpandUse;
  svgOptions.flattenTransforms = options.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = options.svgPreserveUnknown;

  auto baseDir = GetDirectory(options.resolveFile);
  int resolvedCount = 0;
  int errorCount = 0;

  for (auto* layer : doc->layers) {
    ResolveLayerImports(layer, baseDir, svgOptions, doc.get(), resolvedCount, errorCount);
  }

  // Also resolve in Composition layers.
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      for (auto* layer : comp->layers) {
        ResolveLayerImports(layer, baseDir, svgOptions, doc.get(), resolvedCount, errorCount);
      }
    }
  }

  if (resolvedCount == 0 && errorCount == 0) {
    return 0;
  }

  auto xml = PAGXExporter::ToXML(*doc);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx import: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;

  std::cout << "pagx import: resolved " << resolvedCount << " import(s)";
  if (errorCount > 0) {
    std::cout << ", " << errorCount << " error(s)";
  }
  std::cout << ", wrote " << options.outputFile << "\n";
  return errorCount > 0 ? 1 : 0;
}

//--------------------------------------------------------------------------------------------------
// Entry point
//--------------------------------------------------------------------------------------------------

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (!options.resolveFile.empty()) {
    return ResolveImports(options);
  }

  if (options.format == "svg") {
    return ImportFromSVG(options);
  }

  std::cerr << "pagx import: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
