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

#include "cli/ImageStorage.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>
#include <vector>
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"

namespace pagx::cli {

namespace {

// A `source` is "external-relocatable" only when it is a plain relative filesystem path. Absolute
// paths, drive letters, URI schemes (http:, https:, file:, data:, ...) are all treated as fixed
// references we must not rewrite as relative-path copies.
bool IsRelativeFilePath(const std::string& src) {
  if (src.empty()) {
    return false;
  }
  if (src[0] == '/' || src[0] == '\\') {
    return false;  // POSIX absolute or UNC.
  }
  // scheme:// or drive letter "C:" — any ':' before the first '/' means a non-relative reference.
  auto colon = src.find(':');
  if (colon != std::string::npos) {
    auto slash = src.find('/');
    if (slash == std::string::npos || colon < slash) {
      return false;
    }
  }
  return true;
}

bool IsDataUri(const std::string& src) {
  return src.compare(0, 5, "data:") == 0;
}

// Resolves a relative Image `source` to an existing file on disk. `<img>` sources arrive already
// prefixed with the input directory (so they resolve as-is), while inline-SVG <image> hrefs are
// raw relative paths that must be joined with baseDir; try both and return the first that exists.
std::string FindExistingSource(const std::string& rel, const std::string& baseDir) {
  std::error_code ec;
  if (std::filesystem::exists(rel, ec) && !ec) {
    return rel;
  }
  if (!baseDir.empty()) {
    std::filesystem::path joined = std::filesystem::path(baseDir) / rel;
    if (std::filesystem::exists(joined, ec) && !ec) {
      return joined.string();
    }
  }
  return {};
}

// Recovers the *authored* relative source from a possibly prefixed `filePath`.
// PAGXImporter::FromFile (and the HTML importer) rewrite every relative Image source to
// `<documentDir>/<authored>` so the renderer can locate the file, which turns a clean
// `../assets/x.png` into an absolute or `..`-laden string. Both dirs follow the `GetDirectory`
// convention (trailing slash), so a leading string match strips exactly what was prepended;
// `documentDir` is the true prefix, while `baseDir` (the loose-file search root, which may point
// elsewhere) is tried as a fallback. The remainder is normalised to collapse redundant separators
// while preserving intentional leading `..`. Returns an empty string when the path sits under
// none of these dirs (e.g. a genuinely authored absolute path or remote URL), signalling the caller to
// leave the reference untouched.
std::string AuthoredRelativeSource(const std::string& filePath, const std::string& documentDir,
                                   const std::string& baseDir, const std::string& archiveDir) {
  std::string rel = filePath;
  for (const std::string& prefix : {documentDir, baseDir, archiveDir}) {
    if (!prefix.empty() && rel.rfind(prefix, 0) == 0) {
      rel = rel.substr(prefix.size());
      break;
    }
  }
  if (rel.empty()) {
    return {};
  }
  std::string normalized = std::filesystem::path(rel).lexically_normal().generic_string();
  return IsRelativeFilePath(normalized) ? normalized : std::string{};
}

// Copies the local file a `filePath` reference points at under `options.outputDir`, at the
// authored relative path recovered from the reference, and returns that relative path for the
// rewritten reference ("path names unchanged"). Returns an empty string when the reference must
// stay as it is: genuinely absolute paths and remote URLs are not relocatable, and a missing file
// or failed copy is reported as a warning naming the resource `kind`.
std::string RelocateExternalFile(const std::string& filePath, const ImageStorageOptions& options,
                                 const std::string& kind, const std::string& command) {
  std::string baseDir = options.baseDir.empty() ? options.outputDir : options.baseDir;
  // FromFile may have prefixed the document directory, so a clean `assets/x.png` can arrive as an
  // absolute or `..`-laden string; strip that back out so the copy destination and rewritten
  // reference keep the original name.
  std::string relSource =
      AuthoredRelativeSource(filePath, options.documentDir, baseDir, options.archiveDir);
  if (relSource.empty()) {
    return {};
  }
  // Locate the physical file: the (prefixed) filePath usually resolves directly; otherwise fall
  // back to joining the recovered relative path with baseDir.
  std::string resolvedSrc;
  std::error_code exists_ec;
  if (std::filesystem::exists(filePath, exists_ec) && !exists_ec) {
    resolvedSrc = filePath;
  } else {
    resolvedSrc = FindExistingSource(relSource, baseDir);
  }
  if (resolvedSrc.empty()) {
    std::cerr << command << ": warning: " << kind
              << " source not found, leaving reference unchanged: '" << filePath << "'\n";
    return {};
  }
  std::filesystem::path src(resolvedSrc);
  std::filesystem::path dst = std::filesystem::path(options.outputDir) / relSource;
  std::error_code ec;
  auto canonSrc = std::filesystem::weakly_canonical(src, ec);
  auto canonDst = std::filesystem::weakly_canonical(dst, ec);
  if (!ec && canonSrc == canonDst) {
    return relSource;  // Already co-located; just clean up the reference.
  }
  auto parent = dst.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      std::cerr << command << ": warning: failed to create " << kind << " directory '"
                << parent.string() << "'\n";
      return {};
    }
  }
  std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    std::cerr << command << ": warning: failed to copy " << kind << " '" << src.string() << "' -> '"
              << dst.string() << "': " << ec.message() << "\n";
    return {};
  }
  return relSource;
}

std::shared_ptr<Data> ReadFileToData(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    return nullptr;
  }
  auto size = file.tellg();
  if (size <= 0) {
    return nullptr;
  }
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    return nullptr;
  }
  return Data::MakeWithCopy(buffer.data(), buffer.size());
}

}  // namespace

bool ParseImageStorageMode(const std::string& name, ImageStorageMode* mode) {
  std::string lower;
  lower.reserve(name.size());
  for (char c : name) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (lower == "external") {
    if (mode) *mode = ImageStorageMode::External;
    return true;
  }
  if (lower == "embed" || lower == "embedded" || lower == "inline") {
    if (mode) *mode = ImageStorageMode::Embed;
    return true;
  }
  return false;
}

void ApplyImageStorage(PAGXDocument* doc, const ImageStorageOptions& options,
                       const std::string& command) {
  if (doc == nullptr) {
    return;
  }
  std::string baseDir = options.baseDir.empty() ? options.outputDir : options.baseDir;
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(nodePtr.get());

    if (options.mode == ImageStorageMode::Embed) {
      // Already inline (embedded bytes or a data-URI path): nothing to do.
      if (image->data || IsDataUri(image->filePath)) {
        continue;
      }
      // Remote URLs cannot be inlined without network access; leave them alone.
      if (image->filePath.rfind("http://", 0) == 0 || image->filePath.rfind("https://", 0) == 0) {
        continue;
      }
      std::string resolved = IsRelativeFilePath(image->filePath)
                                 ? FindExistingSource(image->filePath, baseDir)
                                 : image->filePath;
      auto data = resolved.empty() ? nullptr : ReadFileToData(resolved);
      if (!data) {
        std::cerr << command << ": warning: cannot embed image, file not found: '"
                  << image->filePath << "'\n";
        continue;
      }
      image->data = std::move(data);
      image->filePath.clear();
      continue;
    }

    // External mode: relocate local file references only. Images authored as embedded bytes or as
    // data URIs are left exactly as they are — we never extract inline content out to a file.
    if (image->data || IsDataUri(image->filePath)) {
      continue;
    }
    if (options.outputDir.empty()) {
      // No place to write files; leave references as-is.
      continue;
    }
    auto relSource = RelocateExternalFile(image->filePath, options, "image", command);
    if (!relSource.empty()) {
      image->filePath = relSource;
    }
  }
}

void ApplyFontStorage(PAGXDocument* doc, const ImageStorageOptions& options,
                      const std::string& command) {
  if (doc == nullptr || options.outputDir.empty()) {
    return;
  }
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<Font*>(nodePtr.get());
    if (font->file.empty() || IsDataUri(font->file)) {
      continue;
    }
    auto relSource = RelocateExternalFile(font->file, options, "font", command);
    if (!relSource.empty()) {
      font->file = relSource;
      font->fileOriginal = relSource;
    }
  }
}

}  // namespace pagx::cli
