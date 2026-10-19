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

// Controls the snapshot pre-pass for single-document library imports, including resolve.
// Enabled by default; PAGX_HTML_SNAPSHOT=0/false/no/off disables it for prepared input.
// The public import workflow selects prepared input explicitly through ImportPreparedSource.
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
static bool RunHTMLSnapshotProcess(const std::string& bin, const std::string& inputPath,
                                   const ImportFormatOptions& options, std::string& html,
                                   int& exitCode, std::string& error) {
  std::wstring node = UTF8ToWide(NodeExecutablePath());
  std::wstring wideBin = UTF8ToWide(bin);
  std::wstring wideInput = UTF8ToWide(inputPath);
  if (wideBin.empty() || wideInput.empty()) {
    error = "failed to encode html-snapshot arguments as UTF-16";
    return false;
  }
  std::wstring command = QuoteWindowsArg(node) + L" " + QuoteWindowsArg(wideBin) + L" " +
                         QuoteWindowsArg(wideInput) + L" \"-o\" \"-\"";
  if (options.captureAnimations) command += L" \"--capture-animations\"";
  if (!options.scenario.empty()) {
    command += L" --scenario " + QuoteWindowsArg(UTF8ToWide(options.scenario));
  }
  if (!options.checkpoint.empty()) {
    command += L" --checkpoint " + QuoteWindowsArg(UTF8ToWide(options.checkpoint));
  }

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
static SnapshotResult RunHTMLSnapshot(const std::string& inputPath,
                                      const ImportFormatOptions& options) {
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
  if (!RunHTMLSnapshotProcess(bin, inputPath, options, result.html, exitCode, result.error)) {
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
  // Opt into animation capture; snapshot.js is static-by-default, so the flag is
  // only appended when the caller asked for it.
  if (options.captureAnimations) {
    command += " --capture-animations";
  }
  if (!options.scenario.empty()) command += " --scenario " + ShellQuote(options.scenario);
  if (!options.checkpoint.empty()) command += " --checkpoint " + ShellQuote(options.checkpoint);

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
  if (!formatOptions.checkpoint.empty() && formatOptions.scenario.empty()) {
    result.error = "--checkpoint requires --scenario";
    return result;
  }
  if (!formatOptions.scenario.empty() &&
      (effectiveFormat != "html" || !snapshotEnabled || formatOptions.captureAnimations)) {
    result.error =
        "--scenario requires HTML with the snapshot pre-pass enabled and cannot be "
        "combined with --capture-animations";
    return result;
  }
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
      auto snap = RunHTMLSnapshot(filePath, formatOptions);
      if (!snap.error.empty()) {
        result.error = snap.error;
        return result;
      }
      result.document = HTMLImporter::ParseString(snap.html, htmlOptions);
    } else {
      // Snapshot disabled (PAGX_HTML_SNAPSHOT): the caller already handed us a flat subset
      // (e.g. an internal prepared-source caller), so import the file directly.
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

}  // namespace pagx::cli
