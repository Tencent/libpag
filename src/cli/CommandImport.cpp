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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include "cli/CliUtils.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/SVGImporter.h"

namespace pagx::cli {

//--------------------------------------------------------------------------------------------------
// Format-specific option parsing
//--------------------------------------------------------------------------------------------------

void ParseFormatOptions(int argc, char* argv[], ImportFormatOptions* options) {
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--svg-no-expand-use") {
      options->svgExpandUse = false;
    } else if (arg == "--svg-flatten-transforms") {
      options->svgFlattenTransforms = true;
    } else if (arg == "--svg-preserve-unknown") {
      options->svgPreserveUnknown = true;
    } else if (arg == "--html-strict") {
      options->htmlStrict = true;
    } else if (arg == "--html-preserve-unknown") {
      options->htmlPreserveUnknown = true;
    } else if (arg == "--html-no-prefer-body-size") {
      options->htmlPreferBodySize = false;
    } else if (arg == "--html-no-normalize") {
      options->htmlAutoNormalize = false;
    } else if (arg == "--html-infer-flex") {
      options->htmlInferFlex = true;
    } else if (arg == "--html-snapshot") {
      options->htmlSnapshot = true;
    } else if (arg == "--html-snapshot-bin" && i + 1 < argc) {
      options->htmlSnapshotBin = argv[++i];
    }
  }
}

//--------------------------------------------------------------------------------------------------
// Format inference
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
        if (tagName == "html" || tagName == "body") {
          return "html";
        }
        return tagName;
      }
    }
    pos = content.find('<', pos + 1);
  }
  return {};
}

static std::string NormalizeFormat(const std::string& format, const std::string& fallback) {
  std::string f = format.empty() ? fallback : format;
  for (auto& ch : f) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (f == "htm" || f == "xhtml") f = "html";
  return f;
}

//--------------------------------------------------------------------------------------------------
// html-snapshot bridge
//--------------------------------------------------------------------------------------------------

// Returns true when `path` begins with `http://` or `https://` (case-insensitive). The
// html-snapshot script accepts URLs natively, so we bypass the on-disk file checks for
// these inputs.
static bool IsHttpUrl(const std::string& path) {
  if (path.size() < 7) {
    return false;
  }
  auto starts = [&](const char* prefix, size_t n) {
    if (path.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
      if (std::tolower(static_cast<unsigned char>(path[i])) != prefix[i]) {
        return false;
      }
    }
    return true;
  };
  return starts("http://", 7) || starts("https://", 8);
}

// POSIX shell quoting (single-quote wrap, escape any embedded single quotes). The
// snapshot.js path and input path are user-controlled, so we route them through this
// before they hit `popen`'s shell.
static std::string ShellQuote(const std::string& value) {
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    } else {
      out += ch;
    }
  }
  out += "'";
  return out;
}

// Resolve the path to `tools/html-snapshot/snapshot.js` for `--html-snapshot`. Resolution
// order (first non-empty hit wins):
//   1. `--html-snapshot-bin <path>` (explicit override),
//   2. `PAGX_HTML_SNAPSHOT_BIN` environment variable,
//   3. Upward search from cwd for `tools/html-snapshot/snapshot.js` (max 8 levels — covers
//      the common case of running `pagx` from anywhere under the repo).
// Returns empty when nothing matched; the caller surfaces a clear error in that case.
static std::string ResolveSnapshotBin(const std::string& explicitPath) {
  if (!explicitPath.empty()) {
    return explicitPath;
  }
  if (const char* env = std::getenv("PAGX_HTML_SNAPSHOT_BIN")) {
    if (env[0] != '\0') {
      return env;
    }
  }
  namespace fs = std::filesystem;
  std::error_code ec;
  auto cur = fs::current_path(ec);
  if (ec) {
    return {};
  }
  for (int depth = 0; depth < 8; ++depth) {
    auto candidate = cur / "tools" / "html-snapshot" / "snapshot.js";
    if (fs::exists(candidate, ec) && !ec) {
      return candidate.string();
    }
    auto parent = cur.parent_path();
    if (parent == cur) {
      break;
    }
    cur = parent;
  }
  return {};
}

struct SnapshotResult {
  std::string html = {};
  std::string error = {};
};

// Spawn `node <snapshot.js> <input> -o -` and capture its stdout (the rendered HTML
// subset). snapshot.js routes its `wrote ...` progress line and any browser errors to
// stderr, which `popen` leaves connected to the parent's stderr — so the user still sees
// progress and diagnostics while we get a clean HTML payload back on stdout.
static SnapshotResult RunHTMLSnapshot(const std::string& inputPath, const std::string& binPath) {
  SnapshotResult result;
  auto bin = ResolveSnapshotBin(binPath);
  if (bin.empty()) {
    result.error =
        "html-snapshot script not found; pass --html-snapshot-bin <path>, set "
        "PAGX_HTML_SNAPSHOT_BIN, or run from a directory that contains "
        "tools/html-snapshot/snapshot.js";
    return result;
  }
  std::string command = "node ";
  command += ShellQuote(bin);
  command += " ";
  command += ShellQuote(inputPath);
  command += " -o -";

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    result.error = "failed to spawn html-snapshot (popen failed)";
    return result;
  }
  std::string html;
  char buffer[4096];
  while (true) {
    auto n = std::fread(buffer, 1, sizeof(buffer), pipe);
    if (n == 0) break;
    html.append(buffer, n);
  }
  int status = pclose(pipe);
  if (status != 0) {
    result.error = "html-snapshot failed (exit status " + std::to_string(status) +
                   "); see stderr above for details";
    return result;
  }
  if (html.empty()) {
    result.error = "html-snapshot produced empty output";
    return result;
  }
  result.html = std::move(html);
  return result;
}

//--------------------------------------------------------------------------------------------------
// Import functions
//--------------------------------------------------------------------------------------------------

static SVGImporter::Options ToSVGOptions(const ImportFormatOptions& formatOptions,
                                         float targetWidth = NAN, float targetHeight = NAN) {
  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = formatOptions.svgExpandUse;
  svgOptions.flattenTransforms = formatOptions.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = formatOptions.svgPreserveUnknown;
  svgOptions.targetWidth = targetWidth;
  svgOptions.targetHeight = targetHeight;
  return svgOptions;
}

static HTMLImporter::Options ToHTMLOptions(const ImportFormatOptions& formatOptions,
                                           float targetWidth = NAN, float targetHeight = NAN) {
  HTMLImporter::Options options = {};
  options.strict = formatOptions.htmlStrict;
  options.preserveUnknownElements = formatOptions.htmlPreserveUnknown;
  options.preferBodySize = formatOptions.htmlPreferBodySize;
  options.autoNormalize = formatOptions.htmlAutoNormalize;
  options.inferFlexFromAbsolute = formatOptions.htmlInferFlex;
  options.targetWidth = targetWidth;
  options.targetHeight = targetHeight;
  return options;
}

static void InlinePathData(PAGXDocument* doc) {
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      node->id.clear();
    }
  }
}

ImportResult ImportFile(const std::string& filePath, const std::string& format,
                        const ImportFormatOptions& formatOptions, float targetWidth,
                        float targetHeight) {
  ImportResult result = {};
  // URL inputs (http/https) are only meaningful when html-snapshot is enabled — the
  // SVG/HTML importers can't fetch them on their own. Force the effective format to
  // html for URLs (the extension heuristic would otherwise pick up nonsense like
  // "com" from the hostname).
  bool isUrl = IsHttpUrl(filePath);
  std::string effectiveFormat =
      isUrl ? NormalizeFormat(format, "html") : NormalizeFormat(format, GetFileExtension(filePath));
  if (isUrl && !formatOptions.htmlSnapshot) {
    result.error = "URL inputs require --html-snapshot (the importer cannot fetch http(s) URLs)";
    return result;
  }
  if (effectiveFormat == "svg") {
    result.document =
        SVGImporter::Parse(filePath, ToSVGOptions(formatOptions, targetWidth, targetHeight));
  } else if (effectiveFormat == "html") {
    auto htmlOptions = ToHTMLOptions(formatOptions, targetWidth, targetHeight);
    if (formatOptions.htmlSnapshot) {
      // Run snapshot.js as a subprocess and feed its stdout (a flat,
      // absolute-positioned subset HTML) straight to the importer. No temp file
      // touches the disk; the HTML lives in this string for the duration of the
      // call.
      auto snap = RunHTMLSnapshot(filePath, formatOptions.htmlSnapshotBin);
      if (!snap.error.empty()) {
        result.error = snap.error;
        return result;
      }
      result.document = HTMLImporter::ParseString(snap.html, htmlOptions);
    } else {
      result.document = HTMLImporter::Parse(filePath, htmlOptions);
    }
  } else {
    result.error = "unsupported format '" + effectiveFormat + "'";
    return result;
  }
  if (result.document == nullptr) {
    result.error = "failed to parse '" + filePath + "'";
    return result;
  }
  InlinePathData(result.document.get());
  result.warnings = std::move(result.document->errors);
  return result;
}

ImportResult ImportString(const std::string& content, const std::string& format,
                          const ImportFormatOptions& formatOptions, float targetWidth,
                          float targetHeight) {
  ImportResult result = {};
  std::string effectiveFormat = NormalizeFormat(format, InferFormatFromContent(content));
  if (effectiveFormat == "svg") {
    result.document =
        SVGImporter::ParseString(content, ToSVGOptions(formatOptions, targetWidth, targetHeight));
  } else if (effectiveFormat == "html") {
    result.document =
        HTMLImporter::ParseString(content, ToHTMLOptions(formatOptions, targetWidth, targetHeight));
  } else {
    result.error = "unsupported inline import format '" + effectiveFormat + "'";
    return result;
  }
  if (result.document == nullptr) {
    result.error = "failed to parse inline content";
    return result;
  }
  InlinePathData(result.document.get());
  result.warnings = std::move(result.document->errors);
  return result;
}

//--------------------------------------------------------------------------------------------------
// CLI entry point
//--------------------------------------------------------------------------------------------------

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  ImportFormatOptions formatOptions = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx import [options]\n"
      << "\n"
      << "Import a file from another format and convert it to PAGX.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file|url>             Input file or URL to import (required;\n"
      << "                                 URL inputs require --html-snapshot)\n"
      << "  --output <file>                Output PAGX file (default: <input>.pagx)\n"
      << "  --format <format>              Force input format (svg, html)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-no-expand-use            Do not expand <use> references\n"
      << "  --svg-flatten-transforms       Flatten nested transforms into single matrices\n"
      << "  --svg-preserve-unknown         Preserve unsupported SVG elements as Unknown nodes\n"
      << "\n"
      << "HTML options:\n"
      << "  --html-strict                  Treat HTML import warnings as errors\n"
      << "  --html-preserve-unknown        Keep unknown HTML tags as empty Layers\n"
      << "  --html-no-prefer-body-size     Prefer --target* over <body> intrinsic size\n"
      << "  --html-no-normalize            Skip the HTML subset normalizer (debug)\n"
      << "  --html-infer-flex              Recover display:flex from absolute layout (lossy)\n"
      << "  --html-snapshot                Pre-process the input through\n"
      << "                                 tools/html-snapshot/snapshot.js before import\n"
      << "                                 (renders JS/React-driven pages in a headless\n"
      << "                                 browser; typically paired with --html-infer-flex).\n"
      << "                                 Requires `node` on PATH and a snapshot.js install.\n"
      << "  --html-snapshot-bin <path>     Override the path to snapshot.js\n"
      << "                                 (default: $PAGX_HTML_SNAPSHOT_BIN, else search\n"
      << "                                  upward from cwd for tools/html-snapshot/snapshot.js)\n"
      << "\n"
      << "Examples:\n"
      << "  pagx import --input icon.svg                      # SVG to icon.pagx\n"
      << "  pagx import --input layout.html                   # HTML to layout.pagx\n"
      << "  pagx import --input page.html --output card.pagx  # HTML to card.pagx\n"
      << "  pagx import --input app.html --html-snapshot --html-infer-flex\n"
      << "                                                    # React/Tailwind page via snapshot\n"
      << "  pagx import --input https://example.com/demo --html-snapshot --output demo.pagx\n"
      << "                                                    # URL input requires snapshot\n";
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
    } else if (arg == "--svg-no-expand-use" || arg == "--svg-flatten-transforms" ||
               arg == "--svg-preserve-unknown" || arg == "--html-strict" ||
               arg == "--html-preserve-unknown" || arg == "--html-no-prefer-body-size" ||
               arg == "--html-no-normalize" || arg == "--html-infer-flex" ||
               arg == "--html-snapshot") {
      // Handled by ParseFormatOptions below.
    } else if (arg == "--html-snapshot-bin" && i + 1 < argc) {
      // Same — ParseFormatOptions will pick this up. We still need to skip the
      // value here so the trailing path doesn't trip the unknown-argument
      // branch below.
      ++i;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx import: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx import: error: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx import: error: missing --input\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    if (IsHttpUrl(options->inputFile)) {
      std::cerr << "pagx import: error: --output is required when --input is a URL\n";
      return 1;
    }
    options->outputFile = ReplaceExtension(options->inputFile, "pagx");
  }

  ParseFormatOptions(argc, argv, &options->formatOptions);
  return 0;
}

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto result = ImportFile(options.inputFile, options.format, options.formatOptions);
  if (!result.error.empty()) {
    std::cerr << "pagx import: error: " << result.error << "\n";
    return 1;
  }
  for (auto& warning : result.warnings) {
    std::cerr << "pagx import: warning: " << warning << "\n";
  }

  auto optimizeResult = PAGXOptimizer::Optimize(result.document.get());
  if (!optimizeResult.converged) {
    std::cerr << "pagx import: warning: PAGXOptimizer did not converge within "
              << optimizeResult.iterationsUsed << " iteration(s); output may be sub-optimal\n";
  }

  auto xml = PAGXExporter::ToXML(*result.document);
  if (!WriteStringToFile(xml, options.outputFile, "pagx import")) {
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
