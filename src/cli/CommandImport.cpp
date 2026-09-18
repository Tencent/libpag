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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include "cli/CliUtils.h"
#include "cli/CommandResolve.h"
#include "cli/ImageStorage.h"
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

// Resolve the path to the html-snapshot driver script. The path is fixed in code; resolution
// order (first hit wins):
//   1. `PAGX_HTML_SNAPSHOT_BIN` environment variable,
//   2. the relative path `tools/html-snapshot/snapshot.js` when it resolves from cwd,
//   3. the repository root's `tools/html-snapshot/snapshot.js`, when a `.git` marker can be found
//      within eight parent levels.
// Every candidate must be a regular file. Searching only after locating the repository root also
// avoids executing a same-named script planted in an arbitrary writable ancestor directory.
// Returns empty when nothing matched; the caller surfaces a clear error in that case.
static std::string ResolveSnapshotBin() {
  namespace fs = std::filesystem;
  auto isRegularFile = [](const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && !ec;
  };

  if (const char* env = std::getenv("PAGX_HTML_SNAPSHOT_BIN")) {
    if (env[0] != '\0') {
      return isRegularFile(env) ? std::string(env) : std::string();
    }
  }

  const std::string relativeDefault = "tools/html-snapshot/snapshot.js";
  if (isRegularFile(relativeDefault)) {
    return relativeDefault;
  }

  std::error_code ec;
  auto cur = fs::current_path(ec);
  if (ec) {
    return {};
  }
  for (int depth = 0; depth < 8; ++depth) {
    std::error_code markerEc;
    if (fs::exists(cur / ".git", markerEc) && !markerEc) {
      auto candidate = cur / "tools" / "html-snapshot" / "snapshot.js";
      return isRegularFile(candidate) ? candidate.string() : std::string();
    }
    auto parent = cur.parent_path();
    if (parent == cur) {
      break;
    }
    cur = parent;
  }
  return {};
}

// Whether the html-snapshot pre-pass runs before the HTML importer. Enabled by default; set
// the `PAGX_HTML_SNAPSHOT` environment variable to a falsy value (`0`, `false`, `no`, `off`,
// case-insensitive) to disable it. The html-snapshot tooling (`html2pagx`, the snapshot
// server) sets this when it has already rendered the page to a flat subset and hands that
// directly to the importer — a second in-importer snapshot would be redundant (and would
// require a browser the tool already ran).
static bool HTMLSnapshotEnabled() {
  const char* env = std::getenv("PAGX_HTML_SNAPSHOT");
  if (env == nullptr || env[0] == '\0') {
    return true;
  }
  std::string value = env;
  for (auto& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return !(value == "0" || value == "false" || value == "no" || value == "off");
}

struct SnapshotResult {
  std::string html = {};
  std::string error = {};
};

#ifdef _WIN32
static std::wstring UTF8ToWide(const std::string& value) {
  if (value.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                      wide.data(), size);
  return wide;
}

// Quotes one argv item using the CommandLineToArgvW / Microsoft C runtime rules. CreateProcessW
// receives this command line directly, so cmd.exe metacharacters never get a chance to execute.
static std::wstring QuoteWindowsArg(const std::wstring& value) {
  std::wstring out = L"\"";
  size_t backslashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      backslashes++;
      continue;
    }
    if (ch == L'\"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'\"');
    } else {
      out.append(backslashes, L'\\');
      out.push_back(ch);
    }
    backslashes = 0;
  }
  out.append(backslashes * 2, L'\\');
  out.push_back(L'\"');
  return out;
}

static bool RunHTMLSnapshotProcess(const std::string& bin, const std::string& inputPath,
                                   bool captureAnimations, std::string& html, int& exitCode,
                                   std::string& error) {
  std::wstring node = L"node";
  std::wstring wideBin = UTF8ToWide(bin);
  std::wstring wideInput = UTF8ToWide(inputPath);
  if (wideBin.empty() || wideInput.empty()) {
    error = "failed to encode html-snapshot arguments as UTF-16";
    return false;
  }
  std::wstring command = QuoteWindowsArg(node) + L" " + QuoteWindowsArg(wideBin) + L" " +
                         QuoteWindowsArg(wideInput) + L" \"-o\" \"-\"";
  if (captureAnimations) command += L" \"--capture-animations\"";

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
static SnapshotResult RunHTMLSnapshot(const std::string& inputPath, bool captureAnimations) {
  SnapshotResult result;
  auto bin = ResolveSnapshotBin();
  if (bin.empty()) {
    result.error =
        "html-snapshot script not found; set PAGX_HTML_SNAPSHOT_BIN or run from a directory "
        "that contains tools/html-snapshot/snapshot.js";
    return result;
  }
#ifdef _WIN32
  int exitCode = 1;
  if (!RunHTMLSnapshotProcess(bin, inputPath, captureAnimations, result.html, exitCode,
                              result.error)) {
    return result;
  }
  if (exitCode != 0) {
    result.error = "html-snapshot failed (exit code " + std::to_string(exitCode) +
                   "); see stderr above for details";
    return result;
  }
#else
  std::string command = "node ";
  command += ShellQuote(bin);
  command += " ";
  command += ShellQuote(inputPath);
  command += " -o -";
  // Opt into animation capture; snapshot.js is static-by-default, so the flag is
  // only appended when the caller asked for it.
  if (captureAnimations) {
    command += " --capture-animations";
  }

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
                        float targetHeight) {
  ImportResult result = {};
  // URL inputs (http/https) are routed through html-snapshot (the snapshot renderer fetches
  // the page) — the SVG/HTML importers can't fetch them on their own. Force the effective
  // format to html for URLs (the extension heuristic would otherwise pick up nonsense like
  // "com" from the hostname).
  bool isUrl = IsHttpUrl(filePath);
  std::string effectiveFormat =
      isUrl ? NormalizeFormat(format, "html") : NormalizeFormat(format, GetFileExtension(filePath));
  bool snapshotEnabled = HTMLSnapshotEnabled();
  if (isUrl && !snapshotEnabled) {
    result.error =
        "URL inputs require the html-snapshot pre-pass, but it is disabled via "
        "PAGX_HTML_SNAPSHOT; the importer cannot fetch http(s) URLs itself";
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
      auto snap = RunHTMLSnapshot(filePath, formatOptions.captureAnimations);
      if (!snap.error.empty()) {
        result.error = snap.error;
        return result;
      }
      result.document = HTMLImporter::ParseString(snap.html, htmlOptions);
    } else {
      // Snapshot disabled (PAGX_HTML_SNAPSHOT): the caller already handed us a flat subset
      // (e.g. html2pagx pre-snapshotted), so import the file directly.
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

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  ImportFormatOptions formatOptions = {};
  // Conversion warnings (e.g. flex inference fallbacks, unsupported constructs) are noisy and
  // non-fatal. They are suppressed by default; `--verbose`/`-v` opts back in. Errors are always
  // printed.
  bool verbose = false;
  // Some importers (notably HTML) leave `import` directives behind for external SVG `<img>`
  // references and inline `<svg>` elements. These are expanded into native PAGX nodes in the
  // same pass by default so the output is fully flattened; `--no-resolve` keeps the directives.
  bool resolve = true;
  // How image resources are stored in the exported PAGX. Defaults to writing image files next to
  // the output (External); `--images embed` inlines them as base64 data URIs instead.
  ImageStorageMode imageStorage = ImageStorageMode::External;
  // Directory used to resolve relative image `source` paths to real files. Defaults to the input
  // file's directory. Only meaningful for local (non-URL) inputs.
  std::string imageBaseDir = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx import [options]\n"
      << "\n"
      << "Import a file from another format and convert it to PAGX.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file|url>             Input file or URL to import (required)\n"
      << "  --output <file>                Output PAGX file (default: <input>.pagx)\n"
      << "  --format <format>              Force input format (svg, html)\n"
      << "  --no-resolve                   Keep import directives (external <svg> images, inline\n"
      << "                                 <svg>) instead of expanding them into native nodes\n"
      << "  --capture-animations           HTML/URL only: capture the page's animations (CSS\n"
      << "                                 @keyframes, Web Animations, GSAP, anime.js) into the\n"
      << "                                 output so they replay in PAGX. Default: a static frame\n"
      << "  --images <mode>                How image resources are stored: 'external' (default;\n"
      << "                                 write image files next to the output PAGX and keep the\n"
      << "                                 relative path) or 'embed' (inline as base64 data URIs)\n"
      << "  --image-base-dir <dir>         Directory to resolve relative image paths against\n"
      << "                                 (default: the input file's directory)\n"
      << "  --verbose, -v                  Print conversion warnings (suppressed by default)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-no-expand-use            Do not expand <use> references\n"
      << "  --svg-flatten-transforms       Flatten nested transforms into single matrices\n"
      << "  --svg-preserve-unknown         Preserve unsupported SVG elements as Unknown nodes\n"
      << "\n"
      << "Examples:\n"
      << "  pagx import --input icon.svg                      # SVG to icon.pagx\n"
      << "  pagx import --input layout.html                   # HTML to layout.pagx\n"
      << "  pagx import --input page.html --output card.pagx  # HTML to card.pagx\n"
      << "  pagx import --input page.html --no-resolve        # keep import directives\n"
      << "  pagx import --input page.html --capture-animations # replay page animations in PAGX\n"
      << "  pagx import --input https://example.com/demo --output demo.pagx  # URL input\n";
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
    } else if (arg == "--no-resolve") {
      options->resolve = false;
    } else if (arg == "--capture-animations") {
      options->formatOptions.captureAnimations = true;
    } else if (arg == "--images" && i + 1 < argc) {
      std::string mode = argv[++i];
      if (!ParseImageStorageMode(mode, &options->imageStorage)) {
        std::cerr << "pagx import: error: invalid --images value '" << mode
                  << "' (expected 'external' or 'embed')\n";
        return 1;
      }
    } else if (arg == "--image-base-dir" && i + 1 < argc) {
      options->imageBaseDir = argv[++i];
    } else if (arg == "--verbose" || arg == "-v") {
      options->verbose = true;
    } else if (arg == "--svg-no-expand-use" || arg == "--svg-flatten-transforms" ||
               arg == "--svg-preserve-unknown") {
      // Handled by ParseFormatOptions below.
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
