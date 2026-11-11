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

#include "pagx/utils/PAGXArchiveWriter.h"
#include <string>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Node.h"
#include "pagx/utils/ImageMime.h"
#include "pagx/utils/MemZipWriter.h"

namespace pagx {

namespace {

constexpr const char* DOCUMENT_ENTRY_PATH = "document.pagx";
constexpr size_t IMAGES_PREFIX_LENGTH = 7;  // strlen("images/")

// Most filesystems cap a single file name at 255 bytes. Truncating the stem keeps room for the
// extension and for the "_<n>" suffix that resolves name collisions.
constexpr size_t MAX_ENTRY_STEM_LENGTH = 128;

std::string ToLowerAscii(std::string value) {
  for (char& c : value) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return value;
}

// Windows resolves these names as devices before looking at the extension, so an entry such as
// "con.png" or "COM1.png" cannot be created or opened there even though the archive is valid.
bool IsWindowsDeviceName(const std::string& stem) {
  auto lower = ToLowerAscii(stem);
  if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul") {
    return true;
  }
  return lower.size() == 4 && lower[3] >= '1' && lower[3] <= '9' &&
         (lower.rfind("com", 0) == 0 || lower.rfind("lpt", 0) == 0);
}

// Keeps archive entry names free of directory separators and other characters that would let a
// document id escape the images/ directory. Only the entry name is sanitized; the document keeps
// the original id and @id references.
std::string SanitizeEntryName(const std::string& name) {
  std::string sanitized;
  sanitized.reserve(name.size());
  for (char c : name) {
    bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '-';
    sanitized += allowed ? c : '_';
  }
  return sanitized;
}

// Entry extensions follow the sniffed format; payloads outside the formats an <Image> may carry
// keep the same PNG fallback that the data URI mime label uses for unrecognized bytes.
std::string ImageEntryExtension(const uint8_t* bytes, size_t size) {
  const char* mime = DetectImageMime(bytes, size);
  if (mime == nullptr) {
    return "png";
  }
  std::string label(mime);
  if (label == "image/jpeg") {
    return "jpg";
  }
  if (label == "image/webp") {
    return "webp";
  }
  if (label == "image/gif") {
    return "gif";
  }
  return "png";
}

// Folds a relative resource path into the form a loader would resolve inside the document
// directory: backslashes become separators, "." segments and repeated separators disappear, and
// ".." segments cancel the segment before them when possible. Paths that cannot name a file in the
// archive (absolute paths, URLs, scheme-qualified ids) yield a form that never starts with
// "images/", so the caller's prefix test drops them.
std::string NormalizeRelativePath(const std::string& path) {
  if (path.empty() || path[0] == '/' || path[0] == '\\') {
    return {};
  }
  std::vector<std::string> segments;
  std::string segment;
  for (size_t i = 0; i <= path.size(); i++) {
    char c = i < path.size() ? path[i] : '/';
    if (c != '/' && c != '\\') {
      segment += c;
      continue;
    }
    if (!segment.empty() && segment != ".") {
      if (segment == ".." && !segments.empty() && segments.back() != "..") {
        segments.pop_back();
      } else {
        segments.push_back(segment);
      }
    }
    segment.clear();
  }
  std::string normalized;
  for (const auto& part : segments) {
    if (!normalized.empty()) {
      normalized += '/';
    }
    normalized += part;
  }
  return normalized;
}

}  // namespace

PAGXArchiveWriter::PAGXArchiveWriter(const PAGXDocument& document) {
  // Reservations must happen before any entry is allocated, otherwise a packed entry could be
  // written at a path that an unresolved image still refers to and silently replace it inside the
  // archive.
  for (const auto& node : document.nodes) {
    if (node == nullptr || node->nodeType() != NodeType::Image) {
      continue;
    }
    const auto* image = static_cast<const Image*>(node.get());
    if (image->data != nullptr || image->filePath.empty()) {
      continue;
    }
    auto key = ToLowerAscii(NormalizeRelativePath(image->filePath));
    if (key.rfind("images/", 0) == 0) {
      reserveKeptPath(key);
    }
  }
}

std::string PAGXArchiveWriter::AddImage(const Image* image) {
  if (image->data == nullptr) {
    return image->filePath;
  }
  auto cached = _entryPaths.find(image);
  if (cached != _entryPaths.end()) {
    return cached->second;
  }
  std::string baseName;
  if (!image->id.empty()) {
    baseName = SanitizeEntryName(image->id);
  } else if (image->filePath.rfind("emoji:", 0) == 0) {
    baseName = "emoji_" + SanitizeEntryName(image->filePath.substr(6));
  }
  auto path =
      allocatePath(baseName, ImageEntryExtension(image->data->bytes(), image->data->size()));
  _entryPaths[image] = path;
  _entries.emplace_back(path, image->data);
  return path;
}

std::shared_ptr<Data> PAGXArchiveWriter::Finish(std::string_view documentXML,
                                                std::string* errorMsg) {
  MemZipWriter writer;
  if (!writer.write(DOCUMENT_ENTRY_PATH, documentXML.data(), documentXML.size(), errorMsg)) {
    return nullptr;
  }
  for (const auto& entry : _entries) {
    if (!writer.write(entry.first, entry.second->bytes(), entry.second->size(), errorMsg)) {
      return nullptr;
    }
  }
  return writer.finish(errorMsg);
}

// Reserves a unique path under images/. Comparison keys are ASCII-lowercased so an archive that
// lands on a case-insensitive filesystem cannot extract one entry over another; the returned path
// keeps the original case for the XML and the archive itself.
std::string PAGXArchiveWriter::allocatePath(const std::string& baseName,
                                            const std::string& extension) {
  std::string stem = baseName.empty() ? "img_" + std::to_string(_nextImageIndex++) : baseName;
  if (stem.size() > MAX_ENTRY_STEM_LENGTH) {
    stem.resize(MAX_ENTRY_STEM_LENGTH);
  }
  if (IsWindowsDeviceName(stem)) {
    stem = "img_" + stem;
  }
  std::string prefix = "images/" + stem;
  std::string path = prefix + "." + extension;
  for (size_t suffix = 2; _usedPaths.count(ToLowerAscii(path)) > 0; suffix++) {
    path = prefix + "_" + std::to_string(suffix) + "." + extension;
  }
  _usedPaths.insert(ToLowerAscii(path));
  return path;
}

// Reserves a kept external reference and every directory above it below images/. A reference such
// as "images/foo.png/photo.png" needs "images/foo.png" to exist as a directory after extraction,
// so a packed entry must not take that name as a file: a file/directory clash makes the archive
// fail to extract. The images/ root itself stays available for packed entries.
void PAGXArchiveWriter::reserveKeptPath(const std::string& key) {
  _usedPaths.insert(key);
  for (auto slash = key.find_last_of('/');
       slash != std::string::npos && slash >= IMAGES_PREFIX_LENGTH;
       slash = key.find_last_of('/', slash - 1)) {
    _usedPaths.insert(key.substr(0, slash));
  }
}

}  // namespace pagx
