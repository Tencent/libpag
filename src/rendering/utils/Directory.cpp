/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "Directory.h"
#if defined(__APPLE__) || defined(__ANDROID__) || defined(ANDROID)
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#else
#include <filesystem>
#endif

namespace pag {
#ifdef _WIN32
static constexpr char PathSeparator = '\\';
#else
static constexpr char PathSeparator = '/';
#endif

std::string Directory::GetFileName(const std::string& filePath) {
  auto fileName = filePath;
  auto pos = fileName.find_last_of(PathSeparator);
  if (pos != std::string::npos) {
    fileName = fileName.substr(pos + 1);
  }
  pos = fileName.find_last_of('.');
  if (pos != std::string::npos) {
    fileName = fileName.substr(0, pos);
  }
  return fileName;
}

std::string Directory::GetParentDirectory(const std::string& filePath) {
  auto parentDirectory = filePath;
  auto pos = parentDirectory.find_last_of(PathSeparator);
  if (pos != std::string::npos) {
    parentDirectory = parentDirectory.substr(0, pos);
  }
  return parentDirectory;
}

std::string Directory::JoinPath(const std::string& folder, const std::string& file) {
  std::string path = folder;
  if (path.back() != PathSeparator) {
    path += PathSeparator;
  }
  path.append(file);
  return path;
}

#if defined(__APPLE__) || defined(__ANDROID__) || defined(ANDROID)

bool Directory::CreateRecursively(const std::string& folder) {
  if (folder.empty()) {
    return false;
  }
  std::string path;
  std::string::size_type pos = 1;
  std::string::size_type lastPos = 0;
  while (pos < folder.size()) {
    pos = folder.find(PathSeparator, pos);
    if (pos == std::string::npos) {
      pos = folder.size();
    }
    path.append(folder.substr(lastPos, pos - lastPos));
    if (mkdir(path.c_str(), 0777) == -1 && errno != EEXIST) {
      return false;
    }
    path += PathSeparator;
    pos++;
    lastPos = pos;
  }
  return true;
}

void Directory::VisitFiles(const std::string& folder,
                           std::function<void(const std::string&, size_t)> callback) {
  DIR* dir = opendir(folder.c_str());
  if (dir == nullptr) {
    return;
  }
  struct dirent* ent = nullptr;
  struct stat stats = {};
  while ((ent = readdir(dir)) != nullptr) {
    std::string str = ent->d_name;
    if (str == "." || str == "..") {
      continue;
    }
    auto path = folder + PathSeparator;
    path.append(str);
    size_t fileSize = 0;
    if (!stat(path.c_str(), &stats)) {
      fileSize = static_cast<size_t>(stats.st_size);
    }
    callback(path, fileSize);
  }
  closedir(dir);
}

#else

bool Directory::CreateRecursively(const std::string& folder) {
  return std::filesystem::create_directories(folder);
}

void Directory::VisitFiles(const std::string& folder,
                           std::function<void(const std::string&, size_t)> callback) {
  if (!std::filesystem::exists(folder)) {
    Directory::CreateRecursively(folder);
  }
  for (const auto& entry : std::filesystem::directory_iterator(folder)) {
    if (entry.is_regular_file()) {
      std::string str = entry.path().filename().string();
      if (str == "." || str == "..") {
        continue;
      }
      auto fileSize = static_cast<size_t>(entry.file_size());
      callback(entry.path().string(), fileSize);
    }
  }
}
#endif

std::vector<std::string> Directory::FindFiles(const std::string& folder, const std::string& ext) {
  std::vector<std::string> files = {};
  VisitFiles(folder, [&](const std::string& path, size_t) {
    auto index = path.find(ext);
    if (index != std::string::npos) {
      files.push_back(path);
    }
  });
  return files;
}

}  // namespace pag
