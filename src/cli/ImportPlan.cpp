// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include "cli/ImportPlan.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace pagx::cli {
namespace fs = std::filesystem;
namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}
bool SafeName(const std::string& name) {
  auto device = Lower(name.substr(0, name.find('.')));
  if (device == "con" || device == "prn" || device == "aux" || device == "nul" ||
      (device.size() == 4 && (device.rfind("com", 0) == 0 || device.rfind("lpt", 0) == 0) &&
       device[3] >= '1' && device[3] <= '9'))
    return false;
  return !name.empty() && name != "." && name != ".." &&
         name.find_first_of("/\\<>:\"|?*\n\r\t") == std::string::npos &&
         std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }) &&
         name.back() != '.' && name.back() != ' ';
}
std::string Absolute(const std::string& file) {
  return fs::weakly_canonical(fs::absolute(fs::u8path(file))).u8string();
}
std::string Format(const std::string& input, const std::string& forced) {
  auto extension = fs::u8path(input).extension().u8string();
  auto format = forced.empty() ? (IsImportURL(input)  ? "html"
                                  : extension.empty() ? ""
                                                      : Lower(extension.substr(1)))
                               : forced;
  if (format == "htm") format = "html";
  Require(format == "html" || format == "svg", "unsupported input format: " + input);
  Require(!IsImportURL(input) || format == "html", "URL input requires HTML format");
  return format;
}
std::string PathKey(const std::string& value) {
  // Reject case-only collisions on every platform so an output set is portable.
  return Lower(Absolute(value));
}
}  // namespace

bool IsImportURL(const std::string& input) {
  return input.rfind("http://", 0) == 0 || input.rfind("https://", 0) == 0;
}

bool ParseImportRequest(const std::vector<std::string>& args, ImportRequest* options, bool* help,
                        std::string* error) {
  *options = {};
  *help = false;
  error->clear();
  auto& o = *options;
  try {
    std::set<std::string> seen;
    bool positionalOnly = false, htmlFlags = false, svgFlags = false;
    for (size_t i = 0; i < args.size(); ++i) {
      auto flag = args[i];
      if (!positionalOnly && (flag == "--help" || flag == "-h")) {
        *help = true;
        return true;
      }
      if (!positionalOnly && flag == "--") {
        positionalOnly = true;
        continue;
      }
      if (positionalOnly || flag.empty() || flag[0] != '-') {
        Require(o.input.empty(), "only one input may be specified");
        Require(!flag.empty(), "input must not be empty");
        o.input = flag;
        continue;
      }
      if (flag == "-o") flag = "--output";
      if (flag == "-v") flag = "--verbose";
      if (flag != "--html-cookie" && flag != "--html-header")
        Require(seen.insert(flag).second, "duplicate option: " + flag);
      auto value = [&]() {
        Require(i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != '-',
                flag + " requires a value (use ./ for paths starting with '-')");
        return args[++i];
      };
      auto number = [&](double min, double max, bool integral) {
        const auto text = value();
        size_t end = 0;
        double result;
        try {
          result = std::stod(text, &end);
        } catch (...) {
          throw std::runtime_error("invalid number for " + flag + ": " + text);
        }
        Require(end == text.size() && std::isfinite(result) && result >= min && result <= max &&
                    (!integral || std::floor(result) == result),
                "invalid number for " + flag + ": " + text);
        return result;
      };
      if (flag == "--input") {
        Require(o.input.empty(), "only one input may be specified");
        o.input = value();
      } else if (flag == "--output")
        o.output = value();
      else if (flag == "--output-dir")
        o.outputDir = value();
      else if (flag == "--output-name")
        o.outputName = value();
      else if (flag == "--batch")
        o.batch = value();
      else if (flag == "--format")
        o.format = value();
      else if (flag == "--images") {
        auto mode = value();
        Require(mode == "external" || mode == "embed", "--images expects external or embed");
        ParseImageStorageMode(mode, &o.images);
      } else if (flag == "--image-base-dir")
        o.imageBaseDir = value();
      else if (flag == "--render")
        o.render = true;
      else if (flag == "--preview")
        o.preview = true;
      else if (flag == "--preview-columns")
        o.previewColumns = static_cast<int>(number(1, 32, true));
      else if (flag == "--scale") {
        o.scale = number(0, 10000, false);
        Require(o.scale > 0, "--scale must be positive");
      } else if (flag == "--no-resolve")
        o.resolve = false;
      else if (flag == "--overwrite")
        o.overwrite = true;
      else if (flag == "--dry-run")
        o.dryRun = true;
      else if (flag == "--verbose")
        o.verbose = true;
      else if (flag == "--embed-fonts")
        o.embedFonts = true;
      else if (flag.rfind("--svg-", 0) == 0) {
        svgFlags = true;
        if (flag == "--svg-no-expand-use") o.formatOptions.svgExpandUse = false;
        else if (flag == "--svg-flatten-transforms")
          o.formatOptions.svgFlattenTransforms = true;
        else if (flag == "--svg-preserve-unknown")
          o.formatOptions.svgPreserveUnknown = true;
        else
          throw std::runtime_error("unknown option: " + flag);
      } else if (flag.rfind("--html-", 0) == 0) {
        htmlFlags = true;
        if (flag == "--html-source") o.htmlSource = value();
        else if (flag == "--html-scenario")
          o.scenario = value();
        else if (flag == "--html-checkpoint")
          o.checkpoint = value();
        else if (flag == "--html-browser-engine")
          o.browserEngine = value();
        else if (flag == "--html-keep-snapshot")
          o.keepSnapshot = true;
        else if (flag == "--html-capture-animations")
          o.captureAnimations = true;
        else if (flag == "--html-scroll-reveal")
          o.scrollReveal = true;
        else if (flag == "--html-no-inline-icon-fonts")
          o.inlineIconFonts = false;
        else if (flag == "--html-download-fonts")
          o.downloadFonts = true;
        else if (flag == "--html-cookie")
          o.cookies.push_back(value());
        else if (flag == "--html-header")
          o.headers.push_back(value());
        else if (flag == "--html-selector")
          o.browserOptions["selector"] = value();
        else if (flag == "--html-viewport-width")
          o.browserOptions["viewport-width"] =
              std::to_string(static_cast<int>(number(1, 16384, true)));
        else if (flag == "--html-viewport-height")
          o.browserOptions["viewport-height"] =
              std::to_string(static_cast<int>(number(1, 16384, true)));
        else if (flag == "--html-wait-ms")
          o.browserOptions["wait-ms"] = std::to_string(static_cast<int>(number(0, 300000, true)));
        else
          throw std::runtime_error("unknown option: " + flag);
      } else {
        static const std::set<std::string> renamedHTMLOptions = {
            "--scenario",           "--checkpoint",      "--browser-engine",
            "--viewport-width",     "--viewport-height", "--wait-ms",
            "--selector",           "--cookie",          "--header",
            "--capture-animations", "--scroll-reveal",   "--no-inline-icon-fonts",
            "--download-fonts",     "--keep-snapshot"};
        if (renamedHTMLOptions.count(flag))
          throw std::runtime_error(flag + " has been renamed to --html-" + flag.substr(2));
        throw std::runtime_error("unknown option: " + flag);
      }
    }
    Require(o.input.empty() != o.batch.empty(), "specify one input or --batch <directory>");
    Require(o.output.empty() || o.outputDir.empty(),
            "--output and --output-dir are mutually exclusive");
    Require(o.format.empty() || o.format == "html" || o.format == "svg",
            "--format expects html or svg");
    Require(o.htmlSource == "browser" || o.htmlSource == "snapshot",
            "--html-source expects browser or snapshot");
    Require(o.browserEngine == "puppeteer" || o.browserEngine == "playwright",
            "unsupported --html-browser-engine");
    Require(
        o.outputName.empty() || (!o.outputDir.empty() && o.batch.empty() && SafeName(o.outputName)),
        "--output-name requires --output-dir, a single input and a safe basename");
    Require(o.checkpoint.empty() || !o.scenario.empty(),
            "--html-checkpoint requires --html-scenario");
    if (!o.batch.empty()) {
      Require(!o.outputDir.empty(), "--batch requires --output-dir");
      Require(o.scenario.empty(), "--batch cannot be combined with --html-scenario");
      Require(o.format.empty(),
              "--format is only valid with a single input; batch infers each file's format");
    }
    if (!o.scenario.empty()) {
      Require(!o.outputDir.empty() || (!o.output.empty() && !o.checkpoint.empty()),
              "--html-scenario requires --output-dir, or --html-checkpoint with --output");
      Require(!o.captureAnimations && !o.scrollReveal && o.browserOptions.empty(),
              "--html-scenario owns viewport, waiting and capture selection; it cannot be combined "
              "with --html-capture-animations, --html-scroll-reveal, --html-viewport-width, "
              "--html-viewport-height, --html-wait-ms or --html-selector");
      Require(o.htmlSource == "browser", "--html-scenario requires --html-source browser");
    }
    o.render = o.render || o.preview;
    Require(o.resolve || (!o.render && !o.embedFonts),
            "--no-resolve conflicts with --render, --preview and --embed-fonts");
    Require(!o.previewColumns || o.preview, "--preview-columns requires --preview");
    Require(!seen.count("--scale") || o.render, "--scale requires --render or --preview");
    if (!o.input.empty()) {
      auto format = Format(o.input, o.format);
      Require(format == "html" || !htmlFlags, "HTML options require HTML input");
      Require(format == "svg" || !svgFlags, "SVG options require SVG input");
    }
    if (o.htmlSource == "snapshot") {
      Require(!IsImportURL(o.input), "snapshot input must be a local file");
      Require(!o.captureAnimations && !o.scrollReveal && !o.downloadFonts && o.inlineIconFonts &&
                  o.cookies.empty() && o.headers.empty() && o.browserOptions.empty() &&
                  !seen.count("--html-browser-engine"),
              "browser options cannot be used with --html-source snapshot");
    }
    Require((o.cookies.empty() && o.headers.empty()) || IsImportURL(o.input),
            "--html-cookie and --html-header require a URL input");
    if (IsImportURL(o.input)) {
      Require(o.imageBaseDir.empty(), "--image-base-dir requires a local input");
      Require(!o.output.empty() || (!o.outputDir.empty() && !o.outputName.empty()),
              "URL input requires --output, or --output-dir with --output-name");
    }
    o.hasHTMLOptions = htmlFlags;
    o.hasSVGOptions = svgFlags;
    return true;
  } catch (const std::exception& e) {
    *error = e.what();
    return false;
  }
}

bool BuildImportPlan(const ImportRequest& o, const std::vector<std::string>& names,
                     ImportPlan* plan, std::string* error) {
  *plan = {};
  error->clear();
  try {
    std::vector<std::string> inputs;
    if (o.batch.empty()) {
      Require(IsImportURL(o.input) || fs::is_regular_file(fs::u8path(o.input)),
              "input not found: " + o.input);
      inputs.push_back(IsImportURL(o.input) ? o.input : Absolute(o.input));
    } else {
      Require(fs::is_directory(fs::u8path(o.batch)), "batch directory not found: " + o.batch);
      auto root = fs::u8path(Absolute(o.batch));
      auto out = fs::u8path(Absolute(o.outputDir));
      Require(root != out, "batch output directory must differ from the input directory");
      for (auto it = fs::recursive_directory_iterator(root);
           it != fs::recursive_directory_iterator(); ++it) {
        if (it->is_directory() && fs::weakly_canonical(it->path()) == out) {
          it.disable_recursion_pending();
          continue;
        }
        if (!it->is_regular_file()) continue;
        auto ext = Lower(it->path().extension().u8string());
        auto stem = Lower(it->path().stem().u8string());
        if (stem.size() >= 7 && stem.substr(stem.size() - 7) == ".subset") continue;
        if (ext == ".html" || ext == ".htm" || ext == ".svg")
          inputs.push_back(it->path().u8string());
      }
      std::sort(inputs.begin(), inputs.end());
      Require(!inputs.empty(), "no supported inputs found in " + o.batch);
    }
    std::vector<std::string> checkpoints = {""};
    if (!o.scenario.empty()) {
      Require(!names.empty(), "scenario has no capture checkpoints");
      checkpoints = o.checkpoint.empty() ? names : std::vector<std::string>{o.checkpoint};
      if (!o.checkpoint.empty())
        Require(std::find(names.begin(), names.end(), o.checkpoint) != names.end(),
                "Unknown checkpoint '" + o.checkpoint + "'");
      for (const auto& name : checkpoints)
        Require(SafeName(name), "invalid checkpoint name: " + name);
    }
    std::set<std::string> targets, sources;
    for (const auto& input : inputs)
      if (!IsImportURL(input)) sources.insert(PathKey(input));
    if (!o.scenario.empty()) sources.insert(PathKey(o.scenario));
    auto target = [&](const std::string& file, bool directory = false) {
      auto key = PathKey(file);
      Require(!sources.count(key), "output would replace an input: " + file);
      Require(targets.insert(key).second, "output collision: " + file);
      auto path = fs::u8path(file);
      if (fs::exists(path)) {
        Require(directory ? fs::is_directory(path) : fs::is_regular_file(path),
                "invalid output type: " + file);
        Require(o.overwrite, "output already exists (use --overwrite): " + file);
      }
      for (auto parent = path.parent_path(); !parent.empty() && parent != parent.parent_path();
           parent = parent.parent_path())
        Require(!fs::exists(parent) || fs::is_directory(parent),
                "output parent is not a directory: " + parent.u8string());
    };
    for (const auto& input : inputs) {
      auto format = Format(input, o.format);
      Require(format == "html" || !o.hasHTMLOptions,
              "HTML options cannot be applied to batch SVG input: " + input);
      Require(format == "svg" || !o.hasSVGOptions,
              "SVG options cannot be applied to batch HTML input: " + input);
      auto base = o.outputName.empty() ? fs::u8path(input).stem().u8string() : o.outputName;
      Require(!o.output.empty() || SafeName(base),
              "input needs a portable --output-name or explicit --output: " + input);
      auto dir =
          o.outputDir.empty() ? fs::u8path(input).parent_path() : fs::u8path(Absolute(o.outputDir));
      if (!o.batch.empty())
        dir /= fs::relative(fs::u8path(input), fs::u8path(Absolute(o.batch))).parent_path();
      for (const auto& checkpoint : checkpoints) {
        ImportItem item;
        item.input = input;
        item.format = format;
        item.checkpoint = checkpoint;
        auto stem = base + (checkpoint.empty() ? "" : "." + checkpoint);
        item.output =
            o.output.empty() ? (dir / fs::u8path(stem + ".pagx")).u8string() : Absolute(o.output);
        target(item.output);
        auto outputStem = fs::u8path(item.output);
        outputStem.replace_extension();
        if (o.images == ImageStorageMode::External) {
          item.assets = outputStem.u8string() + ".assets";
          target(item.assets, true);
        }
        if (o.downloadFonts) {
          item.fonts = outputStem.u8string() + ".fonts";
          target(item.fonts, true);
        }
        if (o.render) {
          item.png = outputStem.u8string() + ".png";
          target(item.png);
        }
        if (o.keepSnapshot) {
          item.snapshot = outputStem.u8string() + ".subset.html";
          target(item.snapshot);
        }
        plan->items.push_back(std::move(item));
      }
    }
    if (!o.outputDir.empty()) {
      plan->manifest = (fs::u8path(Absolute(o.outputDir)) / "manifest.json").u8string();
      target(plan->manifest);
    }
    if (o.preview) {
      auto stem = fs::u8path(plan->items[0].output);
      stem.replace_extension();
      plan->previewDir = o.outputDir.empty()
                             ? stem.u8string() + ".preview"
                             : (fs::u8path(Absolute(o.outputDir)) / "preview").u8string();
      target(plan->previewDir, true);
    }
    // A resource/preview directory owns its subtree; other outputs cannot be nested under it.
    for (const auto& key : targets) {
      for (auto parent = fs::u8path(key).parent_path();
           !parent.empty() && parent != parent.parent_path(); parent = parent.parent_path())
        Require(!targets.count(parent.u8string()), "overlapping output paths: " + key);
    }
    return true;
  } catch (const std::exception& e) {
    *error = e.what();
    return false;
  }
}

std::string ImportUsage() {
  return R"(Usage: pagx import <file|url> [options]
       pagx import --batch <directory> --output-dir <directory> [options]

Import SVG, HTML or web interaction states as PAGX.

  -o, --output <file>           One PAGX main file (default: <input-stem>.pagx)
  --output-dir <directory>      Result set, including manifest.json; exclusive with -o
  --output-name <name>          Basename in directory mode; required for URL input
  --input <file|url>            Alternative to the positional input
  --format html|svg             Force a single input's format
  --batch <directory>           Recursively import HTML/SVG, preserving relative directories
  --render                      Also render PNG files (default: PAGX only)
  --preview                     Render PNGs and an ordered contact sheet
  --preview-columns <1..32>     Contact sheet columns (default: automatic)
  --scale <number>              PNG render scale; requires --render or --preview
  --no-resolve                  Keep import directives; conflicts with rendering/font embedding
  --images external|embed       Final image storage (default: external)
  --image-base-dir <directory>  Override local relative image lookup
  --embed-fonts                 Embed shaped fonts; acquire web fonts for HTML
  --overwrite                   Replace planned outputs; never clear the output directory
  --dry-run                     Print the output plan without capturing or writing outputs
  --verbose, -v                 Print conversion warnings
  --help, -h                    Show this help without loading Node or a browser

HTML options:
  --html-source browser|snapshot   Browser document (default) or prepared local subset
  --html-scenario <file.json>      Capture all named states; requires --output-dir unless
                                   --html-checkpoint selects one state with --output
  --html-checkpoint <name>         Select one state from --html-scenario
  --html-browser-engine <engine>   puppeteer (default) or playwright
  --html-viewport-width <pixels>   Initial viewport width (1..16384)
  --html-viewport-height <pixels>  Initial viewport height (1..16384)
  --html-wait-ms <milliseconds>    Additional settling time (0..300000)
  --html-selector <css>            Wait for a selector before capture
  --html-cookie <name=value>       URL cookie (repeatable)
  --html-header <Key: Value>       URL request header (repeatable)
  --html-download-fonts            Acquire and preserve web fonts in <output-stem>.fonts/
  --html-capture-animations        Capture supported animations; exclusive with --html-scenario
  --html-scroll-reveal             Trigger scroll-based content before capture
  --html-no-inline-icon-fonts      Disable webfont icon conversion
  --html-keep-snapshot             Preserve captured or prepared .subset.html files
  Scenario files own their viewport, waits and capture selection.
  Browser capture options require --html-source browser.

SVG options:
  --svg-no-expand-use
  --svg-flatten-transforms
  --svg-preserve-unknown
)";
}
}  // namespace pagx::cli
