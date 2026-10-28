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
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include "cli/CliUtils.h"
#include "cli/CommandResolve.h"
#include "cli/ImageStorage.h"
#include "cli/NodeTool.h"
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

// Returns true when `path[0..n)` matches `prefix[0..n)` after ASCII lower-casing. Caller
// passes the prefix length explicitly so we don't pay for `strlen` on a literal each call.
static bool StartsWithLower(const std::string& path, const char* prefix, size_t n) {
  if (path.size() < n) return false;
  for (size_t i = 0; i < n; ++i) {
    if (std::tolower(static_cast<unsigned char>(path[i])) != prefix[i]) {
      return false;
    }
  }
  return true;
}

// Returns true when `path` begins with `http://` or `https://` (case-insensitive). The
// html-snapshot script accepts URLs natively, so we bypass the on-disk file checks for
// these inputs.
static bool IsHttpUrl(const std::string& path) {
  if (path.size() < 7) {
    return false;
  }
  return StartsWithLower(path, "http://", 7) || StartsWithLower(path, "https://", 8);
}

#ifndef _WIN32
// POSIX shell quoting (single-quote wrap, escape any embedded single quotes). Windows bypasses
// the shell entirely and launches Node with CreateProcessW below.
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
#endif

struct SnapshotResult {
  std::string html = {};
  std::string error = {};
};

#ifdef _WIN32
static bool RunHTMLSnapshotProcess(const std::string& bin, const std::string& inputPath,
                                   std::string& html, int& exitCode, std::string& error) {
  std::wstring node = UTF8ToWide(NodeExecutablePath());
  std::wstring wideBin = UTF8ToWide(bin);
  std::wstring wideInput = UTF8ToWide(inputPath);
  if (wideBin.empty() || wideInput.empty()) {
    error = "failed to encode html-snapshot arguments as UTF-16";
    return false;
  }
  std::wstring command = QuoteWindowsArg(node) + L" " + QuoteWindowsArg(wideBin) + L" " +
                         QuoteWindowsArg(wideInput) + L" \"-o\" \"-\"";
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
    error = "failed to create html-snapshot output pipe (Windows error " +
            std::to_string(GetLastError()) + ")";
    return false;
  }
  if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
    error = "failed to configure html-snapshot output pipe (Windows error " +
            std::to_string(GetLastError()) + ")";
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }

  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = writePipe;
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION process = {};
  BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                nullptr, &startup, &process);
  CloseHandle(writePipe);
  if (!started) {
    error = "failed to spawn html-snapshot (Windows error " + std::to_string(GetLastError()) + ")";
    CloseHandle(readPipe);
    return false;
  }

  char buffer[4096];
  DWORD read = 0;
  while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
    html.append(buffer, read);
  }
  CloseHandle(readPipe);
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code);
  exitCode = static_cast<int>(code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}
#endif

// Spawn `node <snapshot.js> <input> -o -` and capture its stdout (the rendered HTML subset).
// snapshot.js routes progress and browser errors to stderr, which remains connected to the
// parent's stderr on both the POSIX popen path and the Windows CreateProcessW path.
static SnapshotResult RunHTMLSnapshot(const std::string& inputPath) {
  SnapshotResult result;
  auto bin = ResolveHTMLTool("PAGX_HTML_SNAPSHOT_BIN", "launch.js", "snapshot.js");
  if (bin.empty()) {
    result.error =
        "html-snapshot script not found; set PAGX_HTML_SNAPSHOT_BIN or run from a directory "
        "that contains tools/html-snapshot/snapshot.js";
    return result;
  }
#ifdef _WIN32
  int exitCode = 1;
  if (!RunHTMLSnapshotProcess(bin, inputPath, result.html, exitCode, result.error)) {
    return result;
  }
  if (exitCode != 0) {
    result.error = "html-snapshot failed (exit code " + std::to_string(exitCode) +
                   "); see stderr above for details";
    return result;
  }
#else
  std::string command = ShellQuote(NodeExecutablePath()) + " ";
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
    // `pclose` returns the wait(2) status on POSIX, so 127 lives in the high byte and means
    // "shell could not exec the command" (typically `node` not on PATH). Decode it so the
    // diagnostic distinguishes a missing runtime from a genuine snapshot failure.
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exitCode == 127) {
      result.error =
          "html-snapshot failed: `node` not found on PATH. Install Node.js (>=18) or set "
          "PATH to a shell where it is reachable, then re-run.";
    } else if (exitCode >= 0) {
      result.error = "html-snapshot failed (exit code " + std::to_string(exitCode) +
                     "); see stderr above for details";
    } else {
      result.error = "html-snapshot terminated abnormally (status " + std::to_string(status) +
                     "); see stderr above for details";
    }
    return result;
  }
#endif
#ifndef _WIN32
  result.html = std::move(html);
#endif
  if (result.html.empty()) {
    result.error = "html-snapshot produced empty output";
    return result;
  }
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

// HTML import behaviour is fixed in code (no longer exposed via CLI/ImportFormatOptions): the
// importer always normalizes the subset, prefers the <body> intrinsic size, recovers flex from
// absolute layout, and downgrades unsupported constructs to warnings rather than hard errors.
static HTMLImporter::Options ToHTMLOptions(float targetWidth = NAN, float targetHeight = NAN) {
  HTMLImporter::Options options = {};
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
                        float targetHeight, HTMLSource htmlSource) {
  ImportResult result = {};
  // URL inputs (http/https) are routed through html-snapshot (the snapshot renderer fetches
  // the page) — the SVG/HTML importers can't fetch them on their own. Force the effective
  // format to html for URLs (the extension heuristic would otherwise pick up nonsense like
  // "com" from the hostname).
  bool isUrl = IsHttpUrl(filePath);
  std::string effectiveFormat =
      isUrl ? NormalizeFormat(format, "html") : NormalizeFormat(format, GetFileExtension(filePath));
  bool snapshotEnabled = htmlSource == HTMLSource::Page;
  if (isUrl && (!snapshotEnabled || effectiveFormat != "html")) {
    result.error = "URL inputs require HTML browser snapshotting";
    return result;
  }
  if (effectiveFormat == "svg") {
    result.document =
        SVGImporter::Parse(filePath, ToSVGOptions(formatOptions, targetWidth, targetHeight));
  } else if (effectiveFormat == "html") {
    auto htmlOptions = ToHTMLOptions(targetWidth, targetHeight);
    if (snapshotEnabled) {
      // Run snapshot.js as a subprocess and feed its stdout (a flat, absolute-positioned
      // subset HTML) straight to the importer. No temp file touches the disk; the HTML
      // lives in this string for the duration of the call.
      auto snap = RunHTMLSnapshot(filePath);
      if (!snap.error.empty()) {
        result.error = snap.error;
        return result;
      }
      result.document = HTMLImporter::ParseString(snap.html, htmlOptions);
    } else {
      // The caller explicitly supplied a prepared subset; no browser is needed.
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
    result.document = HTMLImporter::ParseString(content, ToHTMLOptions(targetWidth, targetHeight));
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

struct HTMLImportOptions {
  bool noSnapshot = false;
  bool keepSubset = false;
  bool inlineIconFonts = true;
  bool captureAnimations = false;
  bool scrollReveal = false;
  bool downloadFonts = false;
  bool embedFonts = false;
  bool downloadImages = false;
  std::string scenario;
  std::string checkpoint;
  std::string browserEngine;
  std::string fontDir;
  std::string imageDir;
  std::string selector;
  std::string batchDir;
  std::vector<std::string> cookies;
  std::vector<std::string> headers;
  double viewportWidth = -1;
  double viewportHeight = -1;
  double waitMs = -1;
};

struct ImportOptions {
  std::string inputFile;
  std::string outputFile;
  std::string outputDir;
  std::string outputName;
  std::string format;
  ImportFormatOptions formatOptions;
  HTMLImportOptions html;
  bool verbose = false;
  bool resolve = true;
  bool render = false;
  bool preview = false;
  bool dryRun = false;
  bool skipExisting = false;
  double renderScale = 1;
  double previewColumns = -1;
  ImageStorageMode imageStorage = ImageStorageMode::External;
  std::string imageBaseDir;
};

static void PrintUsage() {
  std::cout << R"(Usage: pagx import [input] [options]

Import SVG or HTML into PAGX. HTML pages and URLs are rendered in a browser;
interaction scenarios export independent static checkpoints in one session.

Input and output:
  --input <file|url>              Alternative to a single positional input
  -o, --output <file>             One PAGX file (default: beside local input)
  --output-dir <dir>              Artifact directory; required for multiple results
  --output-name <name>            Base name with --output-dir; required for URLs
  --format <svg|html>             Force input format (otherwise inferred)
  --images <external|embed>       Image storage (default: external)
  --image-base-dir <dir>          Base directory for relative image resources
  --no-resolve                    Keep import directives; incompatible with rendering
  --verbose, -v                   Print conversion warnings

Artifacts (SVG and HTML):
  --render                        Render each imported PAGX to PNG
  --render-scale <number>         Positive PNG scale (default: 1; requires rendering)
  --preview                       Render PNGs and assemble contact sheets
  --preview-columns <1..32>       Columns in contact sheets; requires --preview

HTML options:
  --html-scenario <file.json>     Replay and export all named capture steps
  --html-checkpoint <name>        Export only this state, replaying preceding actions
  --html-browser-engine <name>    puppeteer or playwright (env: HTML_SNAPSHOT_BROWSER)
  --html-no-snapshot              Convert a prepared local subset without a browser
  --html-keep-subset              Keep intermediate <name>.subset.html files
  --html-capture-animations       Capture page animations (default: static frame)
  --html-viewport-width <px>      Browser viewport width (positive integer)
  --html-viewport-height <px>     Browser viewport height (positive integer)
  --html-wait-ms <ms>             Additional readiness delay (nonnegative integer)
  --html-selector <css>           Wait for a selector before capture
  --html-scroll-reveal            Scroll to reveal lazy content before capture
  --html-no-inline-icon-fonts     Disable icon-font glyph conversion to SVG
  --html-download-fonts           Download web fonts for native font fallbacks
  --html-embed-fonts              Embed fonts; implies --html-download-fonts
  --html-font-dir <dir>           Downloaded font directory
  --html-download-images          Download page images instead of snapshot inlining
  --html-image-dir <dir>          Downloaded image directory
  --html-cookie <name=value>      URL cookie (repeatable)
  --html-header <Key: Value>      URL request header (repeatable)
  --html-batch <dir>              Recursively import HTML files; requires --output-dir
  --skip-existing                 Batch: skip complete sets of requested artifacts
  --dry-run                       Batch: list work without creating outputs or a browser

SVG options (also apply to nested SVG imports):
  --svg-no-expand-use             Do not expand <use> references
  --svg-flatten-transforms        Flatten nested transforms
  --svg-preserve-unknown          Preserve unsupported SVG elements

Examples:
  pagx import icon.svg -o icon.pagx --preview
  pagx import page.html --html-scenario states.json --output-dir out --preview
  pagx import page.html --html-scenario states.json --html-checkpoint ready -o ready.pagx
  pagx import https://example.com --output-dir out --output-name demo
  pagx import --html-batch pages --output-dir out --preview
)";
}

static int ImportArgumentError(const std::string& message) {
  std::cerr << "pagx import: error: " << message << "\n";
  return 2;
}

static int ParseOptions(int argc, char* argv[], ImportOptions* options) {
  auto& o = *options;
  auto& h = o.html;
  std::map<std::string, std::string*> strings = {{"--input", &o.inputFile},
                                                 {"--output", &o.outputFile},
                                                 {"-o", &o.outputFile},
                                                 {"--output-dir", &o.outputDir},
                                                 {"--output-name", &o.outputName},
                                                 {"--format", &o.format},
                                                 {"--image-base-dir", &o.imageBaseDir},
                                                 {"--html-scenario", &h.scenario},
                                                 {"--html-checkpoint", &h.checkpoint},
                                                 {"--html-browser-engine", &h.browserEngine},
                                                 {"--html-font-dir", &h.fontDir},
                                                 {"--html-image-dir", &h.imageDir},
                                                 {"--html-selector", &h.selector},
                                                 {"--html-batch", &h.batchDir}};
  std::map<std::string, bool*> switches = {
      {"--verbose", &o.verbose},
      {"-v", &o.verbose},
      {"--render", &o.render},
      {"--preview", &o.preview},
      {"--dry-run", &o.dryRun},
      {"--skip-existing", &o.skipExisting},
      {"--html-no-snapshot", &h.noSnapshot},
      {"--html-keep-subset", &h.keepSubset},
      {"--html-capture-animations", &h.captureAnimations},
      {"--html-scroll-reveal", &h.scrollReveal},
      {"--html-download-fonts", &h.downloadFonts},
      {"--html-embed-fonts", &h.embedFonts},
      {"--html-download-images", &h.downloadImages},
      {"--svg-flatten-transforms", &o.formatOptions.svgFlattenTransforms},
      {"--svg-preserve-unknown", &o.formatOptions.svgPreserveUnknown}};
  std::map<std::string, double*> numbers = {{"--render-scale", &o.renderScale},
                                            {"--preview-columns", &o.previewColumns},
                                            {"--html-viewport-width", &h.viewportWidth},
                                            {"--html-viewport-height", &h.viewportHeight},
                                            {"--html-wait-ms", &h.waitMs}};
  bool htmlOptions = false;
  bool snapshotOptions = false;
  bool scaleSpecified = false;
  bool positionalOnly = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (!positionalOnly && arg == "--") {
      positionalOnly = true;
      continue;
    }
    if (!positionalOnly && (arg == "--help" || arg == "-h")) {
      PrintUsage();
      return -1;
    }
    if (positionalOnly || arg.empty() || arg[0] != '-') {
      if (arg.empty()) return ImportArgumentError("input must not be empty");
      if (!o.inputFile.empty()) return ImportArgumentError("only one input may be specified");
      o.inputFile = arg;
      continue;
    }
    if (arg.rfind("--html-", 0) == 0) {
      htmlOptions = true;
      snapshotOptions |= arg != "--html-no-snapshot";
    }
    auto flag = switches.find(arg);
    if (flag != switches.end()) {
      *flag->second = true;
      continue;
    }
    if (arg == "--no-resolve") {
      o.resolve = false;
      continue;
    }
    if (arg == "--svg-no-expand-use") {
      o.formatOptions.svgExpandUse = false;
      continue;
    }
    if (arg == "--html-no-inline-icon-fonts") {
      h.inlineIconFonts = false;
      continue;
    }
    auto str = strings.find(arg);
    auto num = numbers.find(arg);
    if (str == strings.end() && num == numbers.end() && arg != "--images" &&
        arg != "--html-cookie" && arg != "--html-header") {
      return ImportArgumentError("unknown option '" + arg + "'");
    }
    if (i + 1 >= argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-') {
      return ImportArgumentError(arg + " requires a value");
    }
    std::string value = argv[++i];
    if (str != strings.end()) {
      if (arg == "--input" && !o.inputFile.empty()) {
        return ImportArgumentError("only one input may be specified");
      }
      *str->second = value;
    } else if (num != numbers.end()) {
      double parsed = 0;
      try {
        size_t consumed = 0;
        parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed)) throw std::invalid_argument("");
      } catch (const std::exception&) {
        return ImportArgumentError(arg + " requires a finite number");
      }
      if (arg == "--render-scale") {
        if (parsed <= 0) return ImportArgumentError(arg + " must be positive");
        scaleSpecified = true;
      } else if (std::floor(parsed) != parsed || parsed < (arg == "--html-wait-ms" ? 0 : 1) ||
                 parsed > (arg == "--preview-columns" ? 32 : 2147483647)) {
        return ImportArgumentError(arg + " is outside the supported integer range");
      }
      *num->second = parsed;
    } else if (arg == "--images") {
      if (!ParseImageStorageMode(value, &o.imageStorage)) {
        return ImportArgumentError("invalid --images value '" + value +
                                   "' (expected external or embed)");
      }
    } else if (arg == "--html-cookie")
      h.cookies.push_back(value);
    else
      h.headers.push_back(value);
  }
  if (o.inputFile.empty() && h.batchDir.empty()) return ImportArgumentError("missing input");
  if (!h.batchDir.empty() &&
      (!o.inputFile.empty() || !o.outputFile.empty() || !o.outputName.empty())) {
    return ImportArgumentError(
        "--html-batch cannot be combined with an input, --output or --output-name");
  }
  if ((!o.outputFile.empty() && !o.outputDir.empty()) ||
      (!o.outputName.empty() && o.outputDir.empty())) {
    return ImportArgumentError("use --output or --output-dir [--output-name], exclusively");
  }
  if (!h.batchDir.empty() && o.outputDir.empty())
    return ImportArgumentError("--html-batch requires --output-dir");
  if (h.batchDir.empty() && (o.dryRun || o.skipExisting))
    return ImportArgumentError("--dry-run and --skip-existing require --html-batch");
  o.format = NormalizeFormat(o.format, !h.batchDir.empty() || IsHttpUrl(o.inputFile)
                                           ? "html"
                                           : GetFileExtension(o.inputFile));
  if (o.format != "html" && o.format != "svg")
    return ImportArgumentError("unsupported format '" + o.format + "'");
  if (htmlOptions && o.format != "html")
    return ImportArgumentError("--html-* options require HTML input");
  if (IsHttpUrl(o.inputFile) && (o.format != "html" || h.noSnapshot))
    return ImportArgumentError("URL inputs require HTML browser snapshotting");
  if (h.noSnapshot && snapshotOptions)
    return ImportArgumentError("--html-no-snapshot cannot be combined with HTML capture options");
  if (!h.checkpoint.empty() && h.scenario.empty())
    return ImportArgumentError("--html-checkpoint requires --html-scenario");
  if (!h.scenario.empty() &&
      (!h.batchDir.empty() || h.captureAnimations || h.scrollReveal || h.viewportWidth >= 0 ||
       h.viewportHeight >= 0 || h.waitMs >= 0 || !h.selector.empty())) {
    return ImportArgumentError(
        "--html-scenario owns viewport and readiness; cannot combine with batch, animation, "
        "viewport, wait, selector or scroll-reveal options");
  }
  if (!h.browserEngine.empty() && h.browserEngine != "puppeteer" && h.browserEngine != "playwright")
    return ImportArgumentError("unsupported --html-browser-engine");
  if ((!h.cookies.empty() || !h.headers.empty()) && !IsHttpUrl(o.inputFile))
    return ImportArgumentError("HTML cookies/headers require a URL input");
  h.downloadFonts |= h.embedFonts;
  if (!h.fontDir.empty() && !h.downloadFonts)
    return ImportArgumentError(
        "--html-font-dir requires --html-download-fonts or --html-embed-fonts");
  if (!h.imageDir.empty() && !h.downloadImages)
    return ImportArgumentError("--html-image-dir requires --html-download-images");
  o.render |= o.preview;
  if (o.render && !o.resolve)
    return ImportArgumentError("rendering and --preview cannot be combined with --no-resolve");
  if (o.previewColumns >= 0 && !o.preview)
    return ImportArgumentError("--preview-columns requires --preview");
  if (scaleSpecified && !o.render)
    return ImportArgumentError("--render-scale requires --render or --preview");
  if (!o.outputName.empty() && (o.outputName == "." || o.outputName == ".." ||
                                o.outputName.find_first_of("/\\") != std::string::npos))
    return ImportArgumentError("--output-name must be a filename, not a path");
  if (IsHttpUrl(o.inputFile) && o.outputFile.empty() &&
      (o.outputDir.empty() || o.outputName.empty()))
    return ImportArgumentError("URL input requires --output or --output-dir with --output-name");
  // Scenarios determine cardinality and checkpoint names in the HTML runtime before any side effects.
  if (h.scenario.empty() && h.batchDir.empty() && o.outputFile.empty()) {
    if (o.outputDir.empty()) o.outputFile = ReplaceExtension(o.inputFile, "pagx");
    else {
      if (o.outputName.empty())
        o.outputName = std::filesystem::u8path(o.inputFile).stem().u8string();
      o.outputFile =
          (std::filesystem::u8path(o.outputDir) / std::filesystem::u8path(o.outputName + ".pagx"))
              .u8string();
    }
  }
  std::error_code ec;
  if (!o.outputFile.empty() &&
      std::filesystem::is_directory(std::filesystem::u8path(o.outputFile), ec))
    return ImportArgumentError("--output must name a file, not a directory");
  if (!o.outputFile.empty() && !IsHttpUrl(o.inputFile)) {
    const auto input = std::filesystem::u8path(o.inputFile);
    const auto output = std::filesystem::u8path(o.outputFile);
    if (std::filesystem::absolute(input).lexically_normal() ==
            std::filesystem::absolute(output).lexically_normal() ||
        std::filesystem::equivalent(input, output, ec)) {
      return ImportArgumentError("output must differ from input");
    }
  }
  return 0;
}

// Only normalized fields cross the process boundary. The Node runtime has no public CLI parser.
static std::string ImportRequest(const ImportOptions& o, const std::string& executable) {
  std::ostringstream json;
  json.imbue(std::locale::classic());
  json << std::setprecision(std::numeric_limits<double>::max_digits10);
  json << "{\"version\":1";
  auto str = [&](const char* key, const std::string& value) {
    json << ",\"" << key << "\":\"" << EscapeJson(value) << "\"";
  };
  auto flag = [&](const char* key, bool value) {
    json << ",\"" << key << "\":" << (value ? "true" : "false");
  };
  auto number = [&](const char* key, double value) {
    if (value >= 0) json << ",\"" << key << "\":" << value;
  };
  auto array = [&](const char* key, const std::vector<std::string>& values) {
    json << ",\"" << key << "\":[";
    bool first = true;
    for (const auto& value : values) {
      if (!first) json << ',';
      json << '"' << EscapeJson(value) << '"';
      first = false;
    }
    json << ']';
  };
  str("pagxBin", executable);
  str("input", o.inputFile);
  str("format", o.format);
  str("outputFile", o.outputFile);
  str("outputDir", o.outputDir);
  str("outputName", o.outputName);
  str("pagxImages", o.imageStorage == ImageStorageMode::Embed ? "embed" : "external");
  str("pagxImageBaseDir", o.imageBaseDir);
  flag("doResolve", o.resolve);
  flag("doRender", o.render);
  flag("verbose", o.verbose);
  flag("preview", o.preview);
  flag("dryRun", o.dryRun);
  flag("skipExisting", o.skipExisting);
  number("scale", o.renderScale);
  number("previewColumns", o.previewColumns);
  const auto& h = o.html;
  str("scenario", h.scenario);
  str("checkpoint", h.checkpoint);
  str("batchDir", h.batchDir);
  str("browserEngine", h.browserEngine);
  str("fontDir", h.fontDir);
  str("imageDir", h.imageDir);
  str("selector", h.selector);
  flag("htmlNoSnapshot", h.noSnapshot);
  flag("keepSubsetHtml", h.keepSubset);
  flag("inlineIconFonts", h.inlineIconFonts);
  flag("captureAnimations", h.captureAnimations);
  flag("scrollReveal", h.scrollReveal);
  flag("downloadFonts", h.downloadFonts);
  flag("embedFonts", h.embedFonts);
  flag("downloadImages", h.downloadImages);
  number("viewportWidth", h.viewportWidth);
  number("viewportHeight", h.viewportHeight);
  number("waitMs", h.waitMs);
  array("cookies", h.cookies);
  array("headers", h.headers);
  std::vector<std::string> svg;
  if (!o.formatOptions.svgExpandUse) svg.emplace_back("--svg-no-expand-use");
  if (o.formatOptions.svgFlattenTransforms) svg.emplace_back("--svg-flatten-transforms");
  if (o.formatOptions.svgPreserveUnknown) svg.emplace_back("--svg-preserve-unknown");
  array("svgOptions", svg);
  json << '}';
  return json.str();
}

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if ((options.format == "html" && !options.html.noSnapshot) || options.render) {
    auto script = ResolveHTMLTool("PAGX_IMPORT_BIN", "launch-import.js", "import.js");
    if (script.empty()) {
      std::cerr << "pagx import: runtime not found; install the complete @libpag/pagx package "
                   "or set PAGX_IMPORT_BIN\n";
      return 1;
    }
    auto executable = CurrentExecutablePath();
    if (executable.empty()) {
      std::cerr << "pagx import: cannot locate the invoking native binary\n";
      return 1;
    }
    return RunNodeTool(script, {"--request", ImportRequest(options, executable)});
  }

  auto result = ImportFile(options.inputFile, options.format, options.formatOptions, NAN, NAN,
                           options.html.noSnapshot ? HTMLSource::Subset : HTMLSource::Page);
  if (!result.error.empty()) {
    std::cerr << "pagx import: error: " << result.error << "\n";
    return 1;
  }
  if (options.verbose) {
    for (auto& warning : result.warnings) {
      std::cerr << "pagx import: warning: " << warning << "\n";
    }
  }

  if (options.resolve) {
    // The HTML importer records external SVG `<img>` sources as already-resolved paths and
    // inline `<svg>` as directive content, so no base directory prefix is needed here.
    auto resolveStats = ResolveDocument(result.document.get(), "", options.formatOptions);
    if (resolveStats.errorCount > 0) {
      // A directive that cannot be resolved must not abort the whole conversion. This happens for
      // an external SVG `<img>` whose source the snapshot pass could not inline into a
      // `data:`/local reference — e.g. a site-absolute `/icon.svg` path from a URL import, a 404 /
      // cross-origin fetch, or an image that mounted only after the inline pass (common on
      // JS-rendered pages, where the animation-capture virtual clock defers DOM mounts). Such a
      // source ends up as a bare `import` directive that resolve then tries to read as a local
      // file and fails. Treat this like the non-fatal handling of an unresolvable raster `<img>`
      // (preserved as an empty image slot): one missing external asset must not discard an
      // otherwise-complete document. Drop the leftover directive so the Layer becomes a plain
      // (empty) box and the output stays fully flattened — `pagx render` rejects any document that
      // still carries an unresolved directive.
      int dropped = DropUnresolvedDirectives(result.document.get());
      std::cerr << "pagx import: warning: failed to resolve " << resolveStats.errorCount
                << " import directive(s); dropped " << dropped
                << " unresolvable reference(s) from the output\n";
    }
  }

  auto optimizeResult = PAGXOptimizer::Optimize(result.document.get());
  if (options.verbose && !optimizeResult.converged) {
    std::cerr << "pagx import: warning: PAGXOptimizer did not converge within "
              << optimizeResult.iterationsUsed << " iteration(s); output may be sub-optimal\n";
  }

  auto outputParent = std::filesystem::u8path(options.outputFile).parent_path();
  std::error_code ec;
  if (!outputParent.empty()) std::filesystem::create_directories(outputParent, ec);
  if (ec) {
    std::cerr << "pagx import: error: cannot create output directory: " << ec.message() << "\n";
    return 1;
  }

  // Normalise image resources to the requested storage mode. Relative image paths recorded by the
  // importer are anchored at the input document's directory, so that is the default base; copied
  // files land next to the output PAGX. URL inputs have no local base, so image relocation is
  // skipped for them.
  {
    ImageStorageOptions imageOptions = {};
    imageOptions.mode = options.imageStorage;
    std::string inputDir =
        IsHttpUrl(options.inputFile) ? std::string() : GetDirectory(options.inputFile);
    imageOptions.baseDir = !options.imageBaseDir.empty() ? options.imageBaseDir : inputDir;
    // The importer anchors relative `<img>` paths at the input file's directory, so that is the
    // prefix to strip back off when recovering the authored `source`.
    imageOptions.documentDir = inputDir;
    imageOptions.outputDir = GetDirectory(options.outputFile);
    ApplyImageStorage(result.document.get(), imageOptions, "pagx import");
  }

  auto xml = PAGXExporter::ToXML(*result.document);
  if (!WriteStringToFile(xml, options.outputFile, "pagx import")) {
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
