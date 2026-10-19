// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include "cli/CommandImport.h"
#include "cli/CommandRender.h"
#include "cli/ImportPlan.h"
#include "cli/ImportRuntime.h"
#include "cli/ImportSource.h"
#include "cli/NodeTool.h"
#include "pagx/PAGXExporter.h"
#include "pagx/nodes/Image.h"
#include "pagx/utils/ImageFormatUtils.h"

namespace pagx::cli {
namespace fs = std::filesystem;
using JSON = nlohmann::json;
namespace {
void CheckCancellation() {
  if (ToolCancellationCode())
    throw std::runtime_error(ToolCancellationCode() == 130 ? "import cancelled (SIGINT)"
                                                           : "import cancelled (SIGTERM)");
}
std::string ReadBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot read file: " + path.u8string());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string Relative(const std::string& file, const ImportPlan& plan) {
  if (file.empty() || plan.manifest.empty()) return file;
  return fs::u8path(file)
      .lexically_relative(fs::u8path(plan.manifest).parent_path())
      .generic_u8string();
}
JSON BrowserOptions(const ImportRequest& o) {
  JSON options = {{"browserEngine", o.browserEngine},
                  {"captureAnimations", o.captureAnimations},
                  {"scrollReveal", o.scrollReveal},
                  {"inlineIconFonts", o.inlineIconFonts},
                  {"downloadFonts", o.downloadFonts || o.embedFonts || o.render},
                  {"cookies", o.cookies},
                  {"headers", o.headers}};
  if (!o.scenario.empty()) options["scenario"] = fs::absolute(fs::u8path(o.scenario)).u8string();
  if (!o.checkpoint.empty()) options["checkpoint"] = o.checkpoint;
  for (const auto& pair : o.browserOptions) {
    if (pair.first == "selector") options["selector"] = pair.second;
    else
      options[pair.first == "viewport-width"    ? "viewportWidth"
              : pair.first == "viewport-height" ? "viewportHeight"
                                                : "waitMs"] = std::stoi(pair.second);
  }
  return options;
}
JSON PlanResult(const ImportPlan& plan) {
  JSON data = {{"version", 1},
               {"status", "planned"},
               {"outputs", JSON::array()},
               {"errors", JSON::array()}};
  for (const auto& item : plan.items) {
    JSON entry = {{"input", item.input},
                  {"format", item.format},
                  {"status", "pending"},
                  {"pagx", Relative(item.output, plan)},
                  {"warnings", JSON::array()}};
    if (!item.checkpoint.empty()) entry["checkpoint"] = item.checkpoint;
    if (!item.png.empty()) entry["png"] = Relative(item.png, plan);
    if (!item.snapshot.empty()) entry["snapshot"] = Relative(item.snapshot, plan);
    data["outputs"].push_back(std::move(entry));
  }
  if (!plan.previewDir.empty()) data["preview"] = {{"status", "pending"}, {"pages", JSON::array()}};
  return data;
}

void ExportItem(const ImportRequest& options, const ImportPlan& plan, const ImportItem& item,
                const JSON& captured, JSON* entry) {
  auto snapshot = captured.is_null() ? item.input : captured.at("snapshot").get<std::string>();
  auto base = IsImportURL(item.input) ? "" : fs::u8path(item.input).parent_path().u8string();
  auto fonts = captured.is_null() ? std::vector<std::string>{}
                                  : captured.at("fonts").get<std::vector<std::string>>();
  if (!captured.is_null()) {
    (*entry)["url"] = captured.at("url");
    (*entry)["warnings"] = captured.at("warnings");
  }
  auto result = ImportPreparedSource(item, snapshot, base, options, fonts);
  if (!result.error.empty()) throw std::runtime_error(result.error);
  for (const auto& warning : result.warnings) (*entry)["warnings"].push_back(warning);
  if (options.verbose)
    for (const auto& warning : (*entry)["warnings"])
      std::cerr << "pagx import: warning: " << warning.get<std::string>() << "\n";
  CheckCancellation();
  auto parent = fs::u8path(item.output).parent_path();
  fs::create_directories(parent);
  ImportTempDirectory stage(parent);
  std::vector<std::pair<fs::path, fs::path>> files;
  auto stageBytes = [&](const fs::path& target, const std::string& bytes) {
    auto local = stage.path / target.lexically_relative(parent);
    WriteImportFile(local, bytes, false);
    files.emplace_back(local, target);
    return local;
  };
  JSON assets = JSON::array(), savedFonts = JSON::array();
  int index = 0;
  if (options.images == ImageStorageMode::External) {
    for (const auto& node : result.document->nodes) {
      if (node->nodeType() != NodeType::Image) continue;
      auto image = static_cast<Image*>(node.get());
      if (!image->data) continue;
      auto data = image->data;
      int width = 0, height = 0;
      auto extension = IsJPEG(data->bytes(), data->size())                              ? ".jpg"
                       : IsWebP(data->bytes(), data->size())                            ? ".webp"
                       : GetPNGDimensions(data->bytes(), data->size(), &width, &height) ? ".png"
                                                                                        : ".bin";
      auto target = fs::u8path(item.assets) / ("image-" + std::to_string(++index) + extension);
      stageBytes(target, std::string(static_cast<const char*>(data->data()), data->size()));
      image->data = nullptr;
      image->filePath = target.lexically_relative(parent).generic_u8string();
      assets.push_back(Relative(target.u8string(), plan));
    }
  }
  if (options.downloadFonts) {
    index = 0;
    for (const auto& font : fonts) {
      auto target = fs::u8path(item.fonts) /
                    (std::to_string(++index) + "-" + fs::u8path(font).filename().u8string());
      stageBytes(target, ReadBytes(fs::u8path(font)));
      savedFonts.push_back(Relative(target.u8string(), plan));
    }
  }
  auto document = stageBytes(fs::u8path(item.output), PAGXExporter::ToXML(*result.document));
  // Publish the main document last, after every requested artifact is ready.
  auto documentFile = files.back();
  files.pop_back();
  if (!item.png.empty()) {
    auto png = stage.path / fs::u8path(item.png).filename();
    if (!RenderPNG(document.u8string(), png.u8string(), static_cast<float>(options.scale), fonts))
      throw std::runtime_error("PAGX PNG rendering failed");
    files.emplace_back(png, fs::u8path(item.png));
  }
  if (!item.snapshot.empty())
    stageBytes(fs::u8path(item.snapshot), ReadBytes(fs::u8path(snapshot)));
  files.push_back(documentFile);
  CheckCancellation();
  for (const auto& file : files) {
    CheckCancellation();
    fs::create_directories(file.second.parent_path());
    PublishImportFile(file.first, file.second, options.overwrite);
  }
  (*entry)["width"] = result.document->width;
  (*entry)["height"] = result.document->height;
  (*entry)["assets"] = assets;
  (*entry)["fonts"] = savedFonts;
  (*entry)["status"] = "complete";
  std::cout << "pagx import: wrote " << item.output << "\n";
  if (!item.png.empty()) std::cout << "pagx import: wrote " << item.png << "\n";
}

void ExportPreview(const ImportRequest& options, const ImportPlan& plan, ImportAdapter* adapter,
                   JSON* data) {
  auto target = fs::u8path(plan.previewDir);
  fs::create_directories(target.parent_path());
  ImportTempDirectory stage(target.parent_path());
  JSON items = JSON::array();
  for (const auto& item : plan.items) {
    auto name = item.checkpoint.empty()
                    ? (options.batch.empty()
                           ? fs::u8path(item.input).filename().u8string()
                           : fs::relative(fs::u8path(item.input), fs::u8path(options.batch))
                                 .generic_u8string())
                    : item.checkpoint;
    items.push_back({{"name", name}, {"png", item.png}});
  }
  JSON request = {{"operation", "preview"},
                  {"preview",
                   {{"items", items},
                    {"outputDir", (stage.path / "sheets").u8string()},
                    {"pagxBin", CurrentExecutablePath()}}}};
  if (options.previewColumns) request["preview"]["columns"] = options.previewColumns;
  JSON response;
  auto code = adapter->run(request, &response);
  CheckCancellation();
  if (code || response.value("status", "") != "complete")
    throw std::runtime_error("preview generation failed");
  auto pages = response.at("pages");
  if (!pages.is_array() || pages.empty()) throw std::runtime_error("preview returned no pages");
  fs::create_directories(target);
  for (auto& page : pages) {
    auto source = fs::u8path(page.at("file").get<std::string>());
    if (source.parent_path() != stage.path / "sheets")
      throw std::runtime_error("invalid preview output path");
    auto output = target / source.filename();
    PublishImportFile(source, output, options.overwrite);
    page["file"] = Relative(output.u8string(), plan);
  }
  (*data)["preview"] = {{"status", "complete"}, {"pages", pages}};
}
}  // namespace

int RunImport(int argc, char* argv[]) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  ImportRequest options;
  bool help = false;
  std::string error;
  if (!ParseImportRequest(args, &options, &help, &error)) {
    std::cerr << "pagx import: " << error << "\n";
    return 2;
  }
  if (help) {
    std::cout << ImportUsage();
    return 0;
  }
  ToolProcessScope lifetime;
  ImportAdapter adapter;
  ImportPlan plan;
  JSON data;
  bool ownsManifest = false;
  auto writeManifest = [&]() {
    if (ownsManifest) WriteImportFile(fs::u8path(plan.manifest), data.dump(2) + "\n");
  };
  try {
    JSON request = {{"operation", "inspect"},
                    {"options", BrowserOptions(options)},
                    {"inputs", JSON::array()}};
    JSON response;
    std::vector<std::string> checkpoints;
    if (!options.scenario.empty()) {
      request["inputs"].push_back({{"id", "0"}, {"input", options.input}});
      auto code = adapter.run(request, &response);
      CheckCancellation();
      if (code) return 2;
      checkpoints = response.at("checkpoints").get<std::vector<std::string>>();
    }
    if (!BuildImportPlan(options, checkpoints, &plan, &error)) {
      std::cerr << "pagx import: " << error << "\n";
      return 2;
    }
    std::map<std::string, std::string> sourceIDs;
    request["inputs"] = JSON::array();
    for (const auto& item : plan.items) {
      if (item.format != "html" || options.htmlSource != "browser" || sourceIDs.count(item.input))
        continue;
      auto id = std::to_string(sourceIDs.size());
      sourceIDs[item.input] = id;
      request["inputs"].push_back({{"id", id}, {"input", item.input}});
    }
    if (!sourceIDs.empty() && options.scenario.empty()) {
      auto code = adapter.run(request, &response);
      CheckCancellation();
      if (code) return 2;
    }
    data = PlanResult(plan);
    if (options.dryRun) {
      std::cout << data.dump(2) << "\n";
      return 0;
    }
    data["status"] = "running";
    if (!plan.manifest.empty()) {
      WriteImportFile(fs::u8path(plan.manifest), data.dump(2) + "\n", options.overwrite);
      ownsManifest = true;
    }
    std::map<std::string, JSON> captures;
    if (!sourceIDs.empty()) {
      request["operation"] = "capture";
      auto code = adapter.run(request, &response);
      CheckCancellation();
      if (!response.is_null()) {
        for (const auto& captured : response.value("outputs", JSON::array())) {
          auto key = captured.at("id").get<std::string>() + ":" +
                     captured.at("checkpoint").get<std::string>();
          if (!captures.emplace(key, captured).second)
            throw std::runtime_error("duplicate captured checkpoint");
        }
        for (const auto& failure : response.value("errors", JSON::array())) {
          data["errors"].push_back(failure);
          std::cerr << "pagx import: " << failure.at("error").get<std::string>() << "\n";
        }
      }
      if (code && data["errors"].empty())
        data["errors"].push_back(
            {{"error", "HTML capture failed (exit " + std::to_string(code) + ")"}});
    }
    for (size_t i = 0; i < plan.items.size(); ++i) {
      CheckCancellation();
      const auto& item = plan.items[i];
      auto& entry = data["outputs"][i];
      entry["status"] = "running";
      writeManifest();
      try {
        JSON captured;
        auto id = sourceIDs.find(item.input);
        if (id != sourceIDs.end()) {
          auto found = captures.find(id->second + ":" + item.checkpoint);
          if (found == captures.end())
            throw std::runtime_error("HTML source/checkpoint was not captured");
          captured = found->second;
        }
        ExportItem(options, plan, item, captured, &entry);
      } catch (const std::exception& e) {
        CheckCancellation();
        entry["status"] = "failed";
        entry["error"] = e.what();
        data["errors"].push_back(
            {{"input", item.input}, {"checkpoint", item.checkpoint}, {"error", e.what()}});
        std::cerr << "pagx import: " << item.input << ": " << e.what() << "\n";
      }
      writeManifest();
    }
    if (options.preview) {
      if (!data["errors"].empty())
        throw std::runtime_error("preview requires all requested outputs to succeed");
      ExportPreview(options, plan, &adapter, &data);
    }
    CheckCancellation();
    data["status"] = data["errors"].empty() ? "complete" : "failed";
    writeManifest();
    return data["errors"].empty() ? 0 : 1;
  } catch (const std::exception& e) {
    auto cancelled = ToolCancellationCode();
    std::cerr << "pagx import: " << e.what() << "\n";
    if (ownsManifest) {
      data["status"] = cancelled ? "cancelled" : "failed";
      data["errors"].push_back({{"error", e.what()}});
      for (auto& entry : data["outputs"])
        if (entry["status"] == "pending" || entry["status"] == "running") {
          entry["status"] = cancelled ? "cancelled" : "failed";
          entry["error"] = e.what();
        }
      if (options.preview && data["preview"]["status"] == "pending") {
        data["preview"]["status"] = cancelled ? "cancelled" : "failed";
        data["preview"]["error"] = e.what();
      }
      try {
        writeManifest();
      } catch (const std::exception& failure) {
        std::cerr << "pagx import: cannot update manifest: " << failure.what() << "\n";
      }
    }
    return cancelled ? cancelled : 1;
  }
}
}  // namespace pagx::cli
