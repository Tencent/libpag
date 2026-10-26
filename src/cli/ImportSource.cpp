// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include "cli/ImportSource.h"
#include <filesystem>
#include <fstream>
#include "cli/CliUtils.h"
#include "cli/CommandResolve.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/nodes/Image.h"
#include "pagx/utils/Base64.h"

namespace pagx::cli {
namespace fs = std::filesystem;

ImportResult ImportPreparedSource(const ImportItem& item, const std::string& snapshot,
                                  const std::string& baseDir, const ImportRequest& options,
                                  const std::vector<std::string>& fonts) {
  ImportResult result;
  std::ifstream stream(fs::u8path(snapshot), std::ios::binary);
  if (!stream) {
    result.error = "cannot read prepared source: " + snapshot;
    return result;
  }
  std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  auto imageBase = fs::u8path(options.imageBaseDir.empty() ? baseDir : options.imageBaseDir);
  if (!imageBase.empty()) imageBase = fs::absolute(imageBase).lexically_normal();
  // Captured web fonts and caller-supplied fallbacks are both shaping sources. The captured files
  // come first: they are the ones the page was laid out with.
  std::vector<std::string> shapingFonts = fonts;
  shapingFonts.insert(shapingFonts.end(), options.fontFallbacks.begin(),
                      options.fontFallbacks.end());
  if (item.format == "html") {
    FontConfig fontConfig;
    if (!LoadFontConfig(&fontConfig, fonts, options.fontFallbacks, options.fontAliases,
                        "pagx import")) {
      result.error = "cannot load the requested fonts";
      return result;
    }
    HTMLImporter::Options htmlOptions;
    htmlOptions.fontConfig = &fontConfig;
    htmlOptions.basePath = imageBase.empty() ? "" : imageBase.generic_u8string() + "/";
    // Baking replaces the font sources with the glyphs they produced, so the resource references
    // are dropped in the same pass that arranges for the fonts to be resolvable.
    htmlOptions.emitFontFileResources = !options.embedFonts;
    htmlOptions.fontReplacements = options.formatOptions.fontReplacements;
    result.document = HTMLImporter::ParseString(content, htmlOptions);
    if (!result.document) {
      result.error = "failed to parse prepared HTML: " + snapshot;
      return result;
    }
    for (const auto& node : result.document->nodes)
      if (node->nodeType() == NodeType::PathData) node->id.clear();
    result.warnings = std::move(result.document->errors);
  } else {
    // Preserve authored relative paths until the image base override is applied.
    result = ImportString(content, "svg", options.formatOptions);
  }
  if (!result.error.empty()) return result;
  auto document = result.document.get();
  const auto base = baseDir.empty() ? std::string() : fs::u8path(baseDir).u8string() + "/";
  if (options.resolve) {
    auto resolved = ResolveDocument(document, base, options.formatOptions);
    if (resolved.errorCount) {
      result.error =
          "failed to resolve " + std::to_string(resolved.errorCount) + " import directive(s)";
      return result;
    }
  }
  auto optimized = PAGXOptimizer::Optimize(document);
  if (!optimized.converged) result.warnings.push_back("PAGXOptimizer did not converge");
  // Resolve and load every present image in both --resolve and --no-resolve modes.
  // The workflow decides how to publish these bytes (external sidecars or embedded XML).
  for (const auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Image) continue;
    auto image = static_cast<Image*>(node.get());
    if (image->data) continue;
    if (image->filePath.empty()) continue;  // An explicit empty image slot is not a missing file.
    if (image->filePath.rfind("data:", 0) == 0) image->data = DecodeBase64DataURI(image->filePath);
    else if (!IsImportURL(image->filePath)) {
      auto path = fs::u8path(image->filePath);
      if (path.is_relative()) path = imageBase / path;
      std::ifstream stream(path, std::ios::binary);
      if (stream) {
        std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        if (!bytes.empty()) image->data = Data::MakeWithCopy(bytes.data(), bytes.size());
      }
    }
    if (!image->data) {
      result.error = "cannot load image resource: " + image->filePath;
      return result;
    }
    image->filePath.clear();
  }
  if (options.embedFonts &&
      !EmbedFonts(document, shapingFonts, options.fontAliases, "pagx import")) {
    result.error = "font embedding failed";
  }
  return result;
}
}  // namespace pagx::cli
