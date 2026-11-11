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
#include "pagx/PAGXExporter.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Node.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/ImageMime.h"
#include "pagx/utils/MemZipWriter.h"
#include "pagx/utils/StringUtils.h"

namespace pagx {

namespace {

constexpr size_t IMAGES_PREFIX_LENGTH = 7;  // strlen(PAGXExporter::IMAGES_DIRECTORY)
constexpr size_t MAX_REPORTED_REFERENCES = 16;

// Most filesystems cap a single file name at 255 bytes. Truncating the stem keeps room for the
// extension and for the "_<n>" suffix that resolves name collisions.
constexpr size_t MAX_ENTRY_STEM_LENGTH = 128;

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

const char* ExternalReferenceReason(const std::string& filePath) {
  if (filePath.rfind("hash:", 0) == 0) {
    return "host-supplied hash reference";
  }
  if (filePath.find("://") != std::string::npos) {
    return "remote URL";
  }
  return "local file path";
}

bool IsAbsolutePath(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return path.size() >= 2 && path[1] == ':' &&
         ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'));
}

std::string JoinPathSegments(const std::string& path) {
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

// Folds a resource path into the form a loader would resolve inside the document directory:
// backslashes become separators, "." segments and repeated separators disappear, and ".."
// segments cancel the segment before them when possible. An absolute path keeps only the suffix
// that starts at its last "images/" segment: the archive can only write below images/, so that
// suffix is the part an extracted entry could collide with, while the rest of the host path means
// nothing here. Anything that yields no images/ path is dropped by the caller's prefix test.
size_t FindImagesSegment(const std::string& lowerPath) {
  size_t searchEnd = std::string::npos;
  while (true) {
    auto slashIndex = lowerPath.rfind("images/", searchEnd);
    auto backslashIndex = lowerPath.rfind("images\\", searchEnd);
    size_t index = std::string::npos;
    if (slashIndex != std::string::npos) {
      index = slashIndex;
    }
    if (backslashIndex != std::string::npos &&
        (index == std::string::npos || backslashIndex > index)) {
      index = backslashIndex;
    }
    if (index == std::string::npos) {
      return std::string::npos;
    }
    // The segment must start a path component: "myimages/x.png" is not below images/.
    if (index == 0 || lowerPath[index - 1] == '/' || lowerPath[index - 1] == '\\') {
      return index;
    }
    searchEnd = index - 1;
  }
}

std::string NormalizeRelativePath(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  if (!IsAbsolutePath(path)) {
    return JoinPathSegments(path);
  }
  auto index = FindImagesSegment(ToLowerAscii(path));
  if (index == std::string::npos) {
    return {};
  }
  return JoinPathSegments(path.substr(index));
}

std::string JoinDetails(const std::vector<std::string>& details) {
  size_t count =
      details.size() < MAX_REPORTED_REFERENCES ? details.size() : MAX_REPORTED_REFERENCES;
  std::string joined;
  for (size_t i = 0; i < count; i++) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += details[i];
  }
  if (details.size() > count) {
    joined += ", and " + std::to_string(details.size() - count) + " more";
  }
  return joined;
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
  // One Image can be referenced by many ImagePattern / Glyph nodes, so the cache has to be
  // consulted before the data URI is decoded: decoding first would make every reference pay for
  // a full Base64 decode whose result is then thrown away.
  auto cached = _entryPaths.find(image);
  if (cached != _entryPaths.end()) {
    return cached->second;
  }
  if (image->data != nullptr) {
    return addEntry(image, image->data);
  }
  if (image->filePath.empty()) {
    return {};
  }
  if (image->filePath.rfind("data:", 0) == 0) {
    auto decoded = DecodeBase64DataURI(image->filePath);
    if (decoded != nullptr && decoded->size() > 0) {
      return addEntry(image, decoded);
    }
    recordExternalReference(image->filePath, "undecodable data URI");
    return image->filePath;
  }
  recordExternalReference(image->filePath, ExternalReferenceReason(image->filePath));
  return image->filePath;
}

std::string PAGXArchiveWriter::addEntry(const Image* image, const std::shared_ptr<Data>& data) {
  std::string baseName;
  if (!image->id.empty()) {
    baseName = SanitizeEntryName(image->id);
  } else if (image->filePath.rfind("emoji:", 0) == 0) {
    baseName = "emoji_" + SanitizeEntryName(image->filePath.substr(6));
  }
  const char* mime = DetectImageMime(data->bytes(), data->size());
  auto path = allocatePath(baseName, ImageMimeToExtension(mime));
  if (!IsSupportedImageMime(mime)) {
    recordUnsupportedFormat(path, mime);
  }
  _entryPaths[image] = path;
  _entries.emplace_back(path, data);
  return path;
}

std::string PAGXArchiveWriter::DescribeUnresolvedReferences() const {
  std::string description;
  if (!_unresolvedDetails.empty()) {
    description = "archive is not self-contained; " + std::to_string(_unresolvedDetails.size()) +
                  " image reference(s) kept external: " + JoinDetails(_unresolvedDetails);
  }
  if (!_unsupportedFormats.empty()) {
    if (!description.empty()) {
      description += "; ";
    }
    description +=
        std::to_string(_unsupportedFormats.size()) +
        " packed image(s) use a format <Image> cannot render: " + JoinDetails(_unsupportedFormats);
  }
  return description;
}

void PAGXArchiveWriter::recordExternalReference(const std::string& reference, const char* reason) {
  if (reference.empty() || !_reportedReferences.insert("external:" + reference).second) {
    return;
  }
  _unresolvedDetails.push_back(reference + " (" + reason + ")");
}

void PAGXArchiveWriter::recordUnsupportedFormat(const std::string& entryPath, const char* mime) {
  auto detail = entryPath + " (" +
                (mime != nullptr ? std::string(mime) : std::string("unrecognized format")) + ")";
  if (!_reportedReferences.insert("format:" + detail).second) {
    return;
  }
  _unsupportedFormats.push_back(detail);
}

std::shared_ptr<Data> PAGXArchiveWriter::Finish(std::string_view documentXML,
                                                std::string* errorMsg) {
  MemZipWriter writer;
  if (!writer.write(PAGXExporter::DOCUMENT_ENTRY_PATH, documentXML.data(), documentXML.size(),
                    errorMsg)) {
    return nullptr;
  }
  for (const auto& entry : _entries) {
    if (!writer.write(entry.first, entry.second->bytes(), entry.second->size(), errorMsg,
                      Z_NO_COMPRESSION)) {
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
  // Device names are at most four bytes, so deciding before the truncation keeps the prefix edge
  // from pushing the stem past the length limit above.
  if (IsWindowsDeviceName(stem)) {
    stem = "img_" + stem;
  }
  if (stem.size() > MAX_ENTRY_STEM_LENGTH) {
    stem.resize(MAX_ENTRY_STEM_LENGTH);
  }
  std::string prefix = std::string(PAGXExporter::IMAGES_DIRECTORY) + stem;
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
