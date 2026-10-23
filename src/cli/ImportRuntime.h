// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#pragma once
#include <filesystem>
#include "nlohmann/json.hpp"

namespace pagx::cli {

class ImportTempDirectory {
 public:
  explicit ImportTempDirectory(
      const std::filesystem::path& parent = std::filesystem::temp_directory_path());
  ~ImportTempDirectory();
  ImportTempDirectory(const ImportTempDirectory&) = delete;
  ImportTempDirectory& operator=(const ImportTempDirectory&) = delete;
  const std::filesystem::path path;
};

// Atomic per-file publication. replace=false never overwrites an existing destination.
void PublishImportFile(const std::filesystem::path& source, const std::filesystem::path& target,
                       bool replace);
void WriteImportFile(const std::filesystem::path& target, const std::string& bytes,
                     bool replace = true);
nlohmann::json ReadImportJSON(const std::filesystem::path& path);

// A request is data, not a command-line string. stdout/stderr are inherited for progress;
// the only result channel is a versioned JSON file. The native process remains in control.
class ImportAdapter {
 public:
  int run(nlohmann::json request, nlohmann::json* response);

 private:
  std::unique_ptr<ImportTempDirectory> directory = nullptr;
};
}  // namespace pagx::cli
