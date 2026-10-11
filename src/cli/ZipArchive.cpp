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

#include "cli/ZipArchive.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <system_error>
#include <vector>
#include "unzip.h"

namespace pagx::cli {

namespace fs = std::filesystem;

// macOS Finder adds resource-fork shadows under this folder when it compresses a directory; they
// are never part of the page.
static const char* const MAC_METADATA_DIR = "__MACOSX";

static void SetError(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
}

static std::string ToLowerASCII(std::string text) {
  for (auto& ch : text) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return text;
}

static bool CreateUniqueTempDirectory(std::string* directory) {
  std::error_code ec;
  auto tempRoot = fs::temp_directory_path(ec);
  if (ec) {
    return false;
  }
  std::random_device device;
  std::mt19937_64 generator(device());
  for (int attempt = 0; attempt < 16; attempt++) {
    auto candidate = tempRoot / ("pagx-import-" + std::to_string(generator()));
    if (fs::create_directory(candidate, ec) && !ec) {
      *directory = candidate.string();
      return true;
    }
  }
  return false;
}

// Normalises an entry name to forward slashes and checks that it stays inside the extraction
// directory: no absolute path, no drive letter and no `..` component (the "zip slip" attack).
static bool IsSafeEntryName(std::string* name) {
  std::replace(name->begin(), name->end(), '\\', '/');
  if (name->empty() || (*name)[0] == '/' || (name->size() >= 2 && (*name)[1] == ':')) {
    return false;
  }
  size_t start = 0;
  while (start <= name->size()) {
    auto end = name->find('/', start);
    auto component =
        name->substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (component == "..") {
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return true;
}

static bool IsMacMetadataEntry(const std::string& name) {
  return name.rfind(std::string(MAC_METADATA_DIR) + "/", 0) == 0;
}

static bool ExtractCurrentEntry(unzFile archive, const fs::path& destination, std::string* error) {
  std::error_code ec;
  fs::create_directories(destination.parent_path(), ec);
  if (ec) {
    SetError(error, "failed to create directory '" + destination.parent_path().string() + "'");
    return false;
  }
  if (unzOpenCurrentFile(archive) != UNZ_OK) {
    SetError(error, "failed to open zip entry '" + destination.string() + "'");
    return false;
  }
  std::ofstream out(destination, std::ios::binary);
  std::vector<char> buffer(64 * 1024);
  int readSize = 0;
  while (out.good() && (readSize = unzReadCurrentFile(archive, buffer.data(),
                                                      static_cast<unsigned>(buffer.size()))) > 0) {
    out.write(buffer.data(), readSize);
  }
  bool written = out.good() && readSize == 0;
  out.close();
  // Closing the entry validates its CRC, so a truncated or corrupted entry is reported here.
  bool closed = unzCloseCurrentFile(archive) == UNZ_OK;
  if (!written || !closed) {
    SetError(error, "failed to extract zip entry '" + destination.string() + "'");
    return false;
  }
  return true;
}

ExtractedArchive::~ExtractedArchive() {
  if (!_directory.empty()) {
    std::error_code ec;
    fs::remove_all(_directory, ec);
  }
}

bool ExtractedArchive::extract(const std::string& zipPath, std::string* error) {
  if (!_directory.empty()) {
    SetError(error, "archive already extracted");
    return false;
  }
  unzFile archive = unzOpen64(zipPath.c_str());
  if (archive == nullptr) {
    SetError(error, "failed to open zip archive '" + zipPath + "'");
    return false;
  }
  if (!CreateUniqueTempDirectory(&_directory)) {
    unzClose(archive);
    SetError(error, "failed to create a temporary directory to extract '" + zipPath + "'");
    return false;
  }
  bool ok = true;
  int status = unzGoToFirstFile(archive);
  while (ok && status == UNZ_OK) {
    unz_file_info64 info = {};
    if (unzGetCurrentFileInfo64(archive, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
      SetError(error, "failed to read zip entry metadata in '" + zipPath + "'");
      ok = false;
      break;
    }
    std::vector<char> nameBuffer(info.size_filename + 1, '\0');
    if (unzGetCurrentFileInfo64(archive, &info, nameBuffer.data(),
                                static_cast<uLong>(nameBuffer.size()), nullptr, 0, nullptr,
                                0) != UNZ_OK) {
      SetError(error, "failed to read zip entry name in '" + zipPath + "'");
      ok = false;
      break;
    }
    std::string name = nameBuffer.data();
    if (!IsSafeEntryName(&name)) {
      SetError(error, "zip entry '" + name + "' points outside the archive; refusing to extract");
      ok = false;
      break;
    }
    if (!IsMacMetadataEntry(name)) {
      auto destination = fs::path(_directory) / fs::u8path(name);
      if (name.back() == '/') {
        std::error_code ec;
        fs::create_directories(destination, ec);
      } else {
        ok = ExtractCurrentEntry(archive, destination, error);
      }
    }
    if (ok) {
      status = unzGoToNextFile(archive);
    }
  }
  if (ok && status != UNZ_END_OF_LIST_OF_FILE) {
    SetError(error, "failed to read the zip directory of '" + zipPath + "'");
    ok = false;
  }
  unzClose(archive);
  return ok;
}

std::string ExtractedArchive::findEntryHTML(std::string* error) const {
  std::vector<fs::path> htmlFiles = {};
  std::error_code ec;
  fs::recursive_directory_iterator iterator(_directory, ec);
  for (auto end = fs::recursive_directory_iterator(); !ec && iterator != end;
       iterator.increment(ec)) {
    const auto& entry = *iterator;
    if (entry.is_directory(ec) && entry.path().filename() == MAC_METADATA_DIR) {
      iterator.disable_recursion_pending();
      continue;
    }
    auto extension = ToLowerASCII(entry.path().extension().string());
    if (entry.is_regular_file(ec) && (extension == ".html" || extension == ".htm")) {
      htmlFiles.push_back(entry.path());
    }
  }
  std::sort(htmlFiles.begin(), htmlFiles.end());
  const fs::path* best = nullptr;
  size_t bestDepth = 0;
  for (const auto& file : htmlFiles) {
    if (ToLowerASCII(file.filename().string()) != "index.html") {
      continue;
    }
    auto relative = file.lexically_relative(_directory);
    auto depth = static_cast<size_t>(std::distance(relative.begin(), relative.end()));
    if (best == nullptr || depth < bestDepth) {
      best = &file;
      bestDepth = depth;
    }
  }
  if (best != nullptr) {
    return best->string();
  }
  if (htmlFiles.size() == 1) {
    return htmlFiles.front().string();
  }
  SetError(error, htmlFiles.empty() ? "zip archive contains no HTML file"
                                    : "zip archive contains several HTML files but no index.html");
  return {};
}

}  // namespace pagx::cli
