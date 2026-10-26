// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include "cli/ImportRuntime.h"
#include <fstream>
#include <random>
#include "cli/NodeTool.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace pagx::cli {
namespace fs = std::filesystem;
namespace {
fs::path CreateDirectory(const fs::path& parent) {
  std::random_device random;
  for (int i = 0; i < 100; ++i) {
    auto candidate =
        parent / (".pagx-import-" + std::to_string(random()) + "-" + std::to_string(random()));
    if (fs::create_directory(candidate)) return candidate;
  }
  throw std::runtime_error("cannot create import working directory");
}
}  // namespace

ImportTempDirectory::ImportTempDirectory(const fs::path& parent) : path(CreateDirectory(parent)) {
}
ImportTempDirectory::~ImportTempDirectory() {
  std::error_code ignored;
  fs::remove_all(path, ignored);
}

void PublishImportFile(const fs::path& source, const fs::path& target, bool replace) {
#ifdef _WIN32
  auto flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
  if (!MoveFileExW(source.c_str(), target.c_str(), flags))
    throw std::runtime_error("cannot publish output: " + target.u8string());
#else
  if (replace) fs::rename(source, target);
  else {
    // An atomic exclusive link avoids the exists()+rename() overwrite race.
    fs::create_hard_link(source, target);
    fs::remove(source);
  }
#endif
}

void WriteImportFile(const fs::path& target, const std::string& bytes, bool replace) {
  auto parent = target.parent_path().empty() ? fs::path(".") : target.parent_path();
  fs::create_directories(parent);
  ImportTempDirectory temp(parent);
  auto file = temp.path / "content";
  std::ofstream stream(file, std::ios::binary);
  if (!stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size())) || !stream.flush())
    throw std::runtime_error("cannot write output: " + target.u8string());
  stream.close();
  if (!stream) throw std::runtime_error("cannot close output: " + target.u8string());
  PublishImportFile(file, target, replace);
}

nlohmann::json ReadImportJSON(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot read import protocol file: " + path.u8string());
  return nlohmann::json::parse(input);
}

int ImportAdapter::run(nlohmann::json request, nlohmann::json* response) {
  auto script = ResolveHTMLTool("PAGX_HTML_IMPORT_BIN", "launch-import.js", "import-adapter.js");
  if (script.empty())
    throw std::runtime_error(
        "HTML import runtime not found; install the complete @libpag/pagx package or set "
        "PAGX_HTML_IMPORT_BIN");
  if (!directory) directory = std::make_unique<ImportTempDirectory>();
  auto input = directory->path / "request.json", output = directory->path / "response.json";
  std::error_code ignored;
  fs::remove(output, ignored);
  request["version"] = 1;
  if (request.value("operation", "") == "capture") request["workDir"] = directory->path.u8string();
  WriteImportFile(input, request.dump());
  auto code = RunNodeToolWait(script, {input.u8string(), output.u8string()});
  *response = nullptr;
  if (fs::exists(output)) {
    *response = ReadImportJSON(output);
    if (!response->is_object() || response->value("version", 0) != 1)
      throw std::runtime_error("invalid HTML import protocol response");
  }
  if (!code && response->is_null())
    throw std::runtime_error("HTML import runtime returned no result");
  return code;
}
}  // namespace pagx::cli
