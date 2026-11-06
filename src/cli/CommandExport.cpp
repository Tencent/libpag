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

#include "cli/CommandExport.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "cli/VideoEncoder.h"
#include "pagx/FontConfig.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PPTExporter.h"
#include "pagx/SVGExporter.h"
#include "pagx/VideoExporter.h"
#include "pagx/types/Color.h"

namespace pagx::cli {

struct ExportOptions {
  std::vector<std::string> inputFiles = {};
  std::string outputFile = {};
  std::string format = {};
  int svgIndent = 2;
  bool svgNoXmlDeclaration = false;
  bool textToPath = false;
  bool pptBakeUnsupported = true;
  bool pptIgnoreGlyphRuns = false;
  // Sampling options shared with the render command. Zero frame rate means "use the document
  // animation frame rate"; a non-positive duration means "the longest animation".
  int frameRate = 0;
  float duration = -1.0f;
  float scale = 1.0f;
  // Video containers carry no alpha, so the background is drawn by default and must be opaque.
  float bgRed = 1.0f;
  float bgGreen = 1.0f;
  float bgBlue = 1.0f;
  float bgAlpha = 1.0f;
  // Negative crf and an empty preset omit the corresponding ffmpeg option, which encoders like
  // h264_videotoolbox do not accept.
  int crf = 20;
  std::string preset = "medium";
  std::string encoder = {};
  std::vector<std::string> encoderArgs = {};
  std::string ffmpegPath = {};
  bool dryRun = false;
  std::vector<std::string> fontFiles = {};
  std::vector<std::string> fallbacks = {};
  // First option that only a video format understands, so a mismatch can be reported instead of
  // silently ignored.
  std::string firstVideoOnlyOption = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx export [options]\n"
      << "\n"
      << "Export a PAGX file to another format.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file>              Input PAGX file (required; repeat to add more slides,\n"
      << "                              pptx only). Each --input becomes one slide in the deck.\n"
      << "  --output <file>             Output file (default: <first input>.<format>)\n"
      << "  --format <format>           Output format (svg, pptx, html, mp4; inferred from "
         "--output extension)\n"
      << "  --text-to-path              Convert text to path geometry (default: native text)\n"
      << "  --font <path>               Register a font file (repeatable; used by mp4 rendering)\n"
      << "  --fallback <path|name>      Add a fallback font file or system font name (repeatable;\n"
      << "                              used by mp4 rendering)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-indent <n>            Indentation spaces (default: 2, valid range: 0-16)\n"
      << "  --svg-no-xml-declaration    Omit the <?xml ...?> declaration\n"
      << "\n"
      << "PPT options:\n"
      << "  --ppt-no-bake-unsupported\n"
      << "                              Do not bake layers that use features OOXML cannot\n"
      << "                              represent natively (masks, scrollRect clipping, blend\n"
      << "                              modes outside of Normal/Multiply/Screen/Darken/Lighten,\n"
      << "                              wide-gamut color, and BackgroundBlurStyle) into PNG\n"
      << "                              patches. By default these layers are baked so the slide\n"
      << "                              matches the tgfx renderer; for unsupported blend modes\n"
      << "                              and BackgroundBlurStyle the backdrop beneath the layer\n"
      << "                              is baked too so the composite (or frosted-glass blur)\n"
      << "                              renders correctly, at the cost of turning native content\n"
      << "                              under the patch into pixels. Pass this flag to silently\n"
      << "                              drop those features and emit the layer as editable\n"
      << "                              shapes instead (mask ignored, scrollRect dropped, blend\n"
      << "                              falls back to Normal, wide-gamut clamped to sRGB).\n"
      << "  --ppt-ignore-glyphruns\n"
      << "                              Ignore the GlyphRun geometry carried by Text nodes and\n"
      << "                              emit native, editable PowerPoint text derived from the\n"
      << "                              Text's text attribute instead. By default a Text with\n"
      << "                              GlyphRun data is rendered as custom glyph paths for exact\n"
      << "                              fidelity; pass this flag to trade that fidelity for\n"
      << "                              editable text. Text nodes without GlyphRun data are\n"
      << "                              unaffected. Opposite of --text-to-path; if both are\n"
      << "                              given, --text-to-path wins.\n"
      << "\n"
      << "MP4 options (requires an external ffmpeg executable: --ffmpeg, $FFMPEG_BIN, or PATH):\n"
      << "  --fps <n>                   Sampling frame rate (default: the document animation "
         "frame\n"
      << "                              rate, falling back to 60); valid range: 1-120\n"
      << "  --duration <seconds>        Sequence length (default: the longest animation)\n"
      << "  --scale <float>             Output scale applied to the document size (default: 1.0)\n"
      << "  --background <color>        Opaque background color as #RRGGBB (default: #FFFFFF);\n"
      << "                              mp4 cannot carry alpha\n"
      << "  --crf <0-51>                Constant rate factor (default: 20, lower is better)\n"
      << "  --no-crf                    Do not pass -crf, for encoders without CRF support\n"
      << "  --preset <name>             Encoder preset (default: medium)\n"
      << "  --no-preset                 Do not pass -preset, for encoders without preset support\n"
      << "  --encoder <name>            Encoder (default: libx264; h264_videotoolbox on macOS)\n"
      << "  --encoder-args \"<args>\"      Extra ffmpeg arguments appended after the generated "
         "ones\n"
      << "  --ffmpeg <path>             ffmpeg executable (default: $FFMPEG_BIN, then PATH)\n"
      << "  --dry-run                   Print the ffmpeg command line instead of encoding\n"
      << "\n"
      << "Examples:\n"
      << "  pagx export --input icon.pagx                    # PAGX to icon.svg\n"
      << "  pagx export --input icon.pagx --output out.svg   # PAGX to out.svg\n"
      << "  pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx\n"
      << "  pagx export --format svg --input icon.pagx       # force SVG output format\n"
      << "  pagx export --format pptx --input icon.pagx      # force PPTX output format\n"
      << "  pagx export --input a.pagx --input b.pagx --output deck.pptx\n"
      << "                                                   # multi-slide deck (one slide per "
         "input)\n"
      << "  pagx export --input icon.pagx --svg-indent 4     # 4-space indent\n"
      << "  pagx export --input icon.pagx --text-to-path     # convert text to paths\n"
      << "  pagx export --input icon.pagx --output out.pptx --ppt-no-bake-unsupported\n"
      << "                                                   # keep unsupported features "
         "editable\n"
      << "  pagx export --input icon.pagx --output out.pptx --ppt-ignore-glyphruns\n"
      << "                                                   # emit editable text, ignore "
         "GlyphRuns\n"
      << "  pagx export --input icon.pagx --output icon.html # PAGX to HTML\n"
      << "  pagx export --input motion.pagx --output out.mp4 --fps 60 --crf 20\n"
      << "                                                   # animated PAGX to MP4\n"
      << "  pagx export --input motion.pagx --output out.mp4 --dry-run\n"
      << "                                                   # print the ffmpeg command line\n";
}

static int ParseOptions(int argc, char* argv[], ExportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFiles.emplace_back(argv[++i]);
    } else if (arg == "--output" && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--svg-indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 16) {
        std::cerr << "pagx export: error: invalid indent '" << argv[i] << "' (must be 0-16)\n";
        return 1;
      }
      options->svgIndent = static_cast<int>(value);
    } else if (arg == "--svg-no-xml-declaration") {
      options->svgNoXmlDeclaration = true;
    } else if (arg == "--text-to-path") {
      options->textToPath = true;
    } else if (arg == "--ppt-no-bake-unsupported") {
      options->pptBakeUnsupported = false;
    } else if (arg == "--ppt-ignore-glyphruns") {
      options->pptIgnoreGlyphRuns = true;
    } else if (arg == "--fps" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 1 || value > 120) {
        std::cerr << "pagx export: error: invalid fps '" << argv[i] << "', must be 1-120\n";
        return 1;
      }
      options->frameRate = static_cast<int>(value);
      options->firstVideoOnlyOption = "--fps";
    } else if (arg == "--duration" && i + 1 < argc) {
      char* endPtr = nullptr;
      options->duration = strtof(argv[++i], &endPtr);
      if (endPtr == argv[i] || *endPtr != '\0' || !std::isfinite(options->duration) ||
          options->duration <= 0.0f) {
        std::cerr << "pagx export: error: invalid duration '" << argv[i]
                  << "', must be a positive number of seconds\n";
        return 1;
      }
      options->firstVideoOnlyOption = "--duration";
    } else if (arg == "--scale" && i + 1 < argc) {
      char* endPtr = nullptr;
      options->scale = strtof(argv[++i], &endPtr);
      if (endPtr == argv[i] || *endPtr != '\0' || !std::isfinite(options->scale) ||
          options->scale <= 0.0f) {
        std::cerr << "pagx export: error: invalid scale '" << argv[i] << "'\n";
        return 1;
      }
      options->firstVideoOnlyOption = "--scale";
    } else if (arg == "--background" && i + 1 < argc) {
      if (!ParseHexColor(argv[++i], &options->bgRed, &options->bgGreen, &options->bgBlue,
                         &options->bgAlpha)) {
        std::cerr << "pagx export: error: invalid color format, expected #RRGGBB or #RRGGBBAA\n";
        return 1;
      }
      options->firstVideoOnlyOption = "--background";
    } else if (arg == "--crf" && i + 1 < argc) {
      char* endPtr = nullptr;
      errno = 0;
      long value = strtol(argv[++i], &endPtr, 10);
      if (errno != 0 || endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 51) {
        std::cerr << "pagx export: error: invalid crf '" << argv[i] << "', must be 0-51\n";
        return 1;
      }
      options->crf = static_cast<int>(value);
      options->firstVideoOnlyOption = "--crf";
    } else if (arg == "--no-crf") {
      options->crf = -1;
      options->firstVideoOnlyOption = "--no-crf";
    } else if (arg == "--preset" && i + 1 < argc) {
      options->preset = argv[++i];
      options->firstVideoOnlyOption = "--preset";
    } else if (arg == "--no-preset") {
      options->preset.clear();
      options->firstVideoOnlyOption = "--no-preset";
    } else if (arg == "--encoder" && i + 1 < argc) {
      options->encoder = argv[++i];
      options->firstVideoOnlyOption = "--encoder";
    } else if (arg == "--encoder-args" && i + 1 < argc) {
      auto extraArgs = SplitArguments(argv[++i]);
      options->encoderArgs.insert(options->encoderArgs.end(), extraArgs.begin(), extraArgs.end());
      options->firstVideoOnlyOption = "--encoder-args";
    } else if (arg == "--ffmpeg" && i + 1 < argc) {
      options->ffmpegPath = argv[++i];
      options->firstVideoOnlyOption = "--ffmpeg";
    } else if (arg == "--dry-run") {
      options->dryRun = true;
      options->firstVideoOnlyOption = "--dry-run";
    } else if (arg == "--font" && i + 1 < argc) {
      options->fontFiles.emplace_back(argv[++i]);
    } else if (arg == "--fallback" && i + 1 < argc) {
      options->fallbacks.emplace_back(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx export: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx export: error: unexpected argument '" << arg
                << "', use --input to specify the input file\n";
      return 1;
    }
    i++;
  }

  if (options->inputFiles.empty()) {
    std::cerr << "pagx export: error: missing --input file\n";
    return 1;
  }

  if (options->format.empty() && !options->outputFile.empty()) {
    options->format = GetFileExtension(options->outputFile);
  }
  if (options->format.empty()) {
    std::cerr << "pagx export: error: cannot infer output format, use --format or specify an "
                 "output file with a known extension\n";
    return 1;
  }

  if (options->format != "pptx" && options->inputFiles.size() > 1) {
    std::cerr << "pagx export: error: multiple --input files are only supported for pptx output\n";
    return 1;
  }

  auto* videoFormat = FindVideoFormat(options->format);
  if (videoFormat == nullptr && !options->firstVideoOnlyOption.empty()) {
    std::cerr << "pagx export: error: " << options->firstVideoOnlyOption
              << " is only supported for video formats (mp4)\n";
    return 1;
  }
  if (videoFormat != nullptr && options->bgAlpha < 1.0f) {
    std::cerr << "pagx export: error: " << videoFormat->name
              << " output cannot carry alpha, --background must be opaque\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    options->outputFile = ReplaceExtension(options->inputFiles.front(), options->format);
  }

  return 0;
}

// Encodes the document's animations into a video file through an external ffmpeg executable. The
// frames come from VideoExporter one at a time and go straight into ffmpeg's stdin, so the whole
// sequence is never held in memory and no codec is linked into pagx.
static int ExportToMP4(const ExportOptions& options, const VideoFormatSpec& format) {
  auto document = LoadDocument(options.inputFiles.front(), "pagx export");
  if (document == nullptr) {
    return 1;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  // Video is really rendered, unlike the vector exporters that carry the document's own geometry,
  // so the fonts have to be registered before the scene applies layout.
  FontConfig fontConfig = {};
  if (!LoadFontConfig(&fontConfig, options.fontFiles, options.fallbacks, "pagx export")) {
    return 1;
  }
  document->applyLayout(&fontConfig);

  auto location = FindFFmpeg(options.ffmpegPath);
  if (location.path.empty()) {
    std::cerr << "pagx export: error: " << location.error << "\n"
              << "pagx export: install ffmpeg (for example 'brew install ffmpeg'), pass "
                 "--ffmpeg <path>, or set FFMPEG_BIN. To write an image sequence instead, use "
                 "'pagx render --frames-dir <dir>'.\n";
    return 1;
  }

  VideoExportOptions videoOptions = {};
  videoOptions.frameRate = options.frameRate;
  videoOptions.duration = options.duration;
  videoOptions.scale = options.scale;
  videoOptions.hasBackground = true;
  videoOptions.background =
      Color{options.bgRed, options.bgGreen, options.bgBlue, options.bgAlpha, ColorSpace::SRGB};
  // 4:2:0 chroma subsampling cannot represent an odd width or height.
  videoOptions.alignToEvenSize = true;

  std::string errorMsg = {};
  auto exporter = VideoExporter::Make(document, videoOptions, &errorMsg);
  if (exporter == nullptr) {
    std::cerr << "pagx export: error: " << errorMsg << "\n";
    return 1;
  }

  auto width = exporter->width();
  auto height = exporter->height();
  auto requestedWidth =
      static_cast<int>(std::ceil(static_cast<double>(document->width) * options.scale));
  auto requestedHeight =
      static_cast<int>(std::ceil(static_cast<double>(document->height) * options.scale));
  if (requestedWidth != width || requestedHeight != height) {
    std::cerr << "pagx export: " << requestedWidth << "x" << requestedHeight << " rounded up to "
              << width << "x" << height << " for the " << format.pixelFormat << " pixel format\n";
  }

  VideoEncodeOptions encodeOptions = {};
  encodeOptions.width = width;
  encodeOptions.height = height;
  encodeOptions.frameRate = exporter->frameRate();
  encodeOptions.crf = options.crf;
  encodeOptions.preset = options.preset;
  encodeOptions.encoder = options.encoder;
  encodeOptions.extraArgs = options.encoderArgs;
  encodeOptions.outputFile = options.outputFile;

  std::vector<std::string> arguments = {};
  if (!BuildFFmpegCommandLine(location.path, format, encodeOptions, &arguments, &errorMsg)) {
    std::cerr << "pagx export: error: " << errorMsg << "\n";
    return 1;
  }
  if (options.dryRun) {
    std::cout << FormatCommandLine(arguments) << "\n";
    return 0;
  }

  auto encoder = VideoEncoder::Start(location.path, format, encodeOptions, &errorMsg);
  if (encoder == nullptr) {
    std::cerr << "pagx export: error: " << errorMsg << "\n";
    return 1;
  }

  auto frameRate = exporter->frameRate();
  auto frameCount = exporter->frameCount();
  int64_t progressStep = std::max<int64_t>(1, frameCount / 10);
  while (exporter->nextFrame()) {
    auto frame = exporter->currentFrame() - 1;
    if (!encoder->writeFrame(exporter->pixels(), exporter->rowBytes(), &errorMsg)) {
      std::cerr << "pagx export: error: " << errorMsg << "\n";
      return 1;
    }
    if (frame % progressStep == 0) {
      std::cout << "pagx export: " << (frame * 100 / frameCount) << "% (" << frame << "/"
                << frameCount << ")" << std::endl;
    }
  }
  if (exporter->currentFrame() < frameCount) {
    std::cerr << "pagx export: error: failed to render frame " << exporter->currentFrame() << "\n";
    return 1;
  }
  if (!encoder->finish(&errorMsg)) {
    std::cerr << "pagx export: error: " << errorMsg << "\n";
    return 1;
  }

  auto encoderName = encodeOptions.encoder.empty() ? format.defaultEncoder : encodeOptions.encoder;
  auto durationSeconds = static_cast<float>(static_cast<double>(frameCount) / frameRate);
  std::cout << "pagx export: wrote " << options.outputFile << " (" << width << "x" << height << ", "
            << frameRate << " fps, " << durationSeconds << "s, " << frameCount << " frames, "
            << encoderName << ")\n";
  return 0;
}

static int ExportToSVG(const ExportOptions& options) {
  auto document = LoadDocument(options.inputFiles.front(), "pagx export");
  if (document == nullptr) {
    return 1;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  SVGExporter::Options svgOptions = {};
  svgOptions.indent = options.svgIndent;
  svgOptions.xmlDeclaration = !options.svgNoXmlDeclaration;
  svgOptions.convertTextToPath = options.textToPath;

  if (!SVGExporter::ToFile(*document, options.outputFile, svgOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

static int ExportToHTML(const ExportOptions& options) {
  const auto& inputFile = options.inputFiles.front();
  auto document = PAGXImporter::FromFile(inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx export: warning: " << error << "\n";
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  std::string errorMsg;
  if (!HTMLExporter::ToFile(*document, options.outputFile, {}, &errorMsg)) {
    std::cerr << "pagx export: error: " << (errorMsg.empty() ? "export failed" : errorMsg) << "\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

static int ExportToPPT(const ExportOptions& options) {
  std::vector<std::shared_ptr<PAGXDocument>> documents = {};
  documents.reserve(options.inputFiles.size());
  for (const auto& inputFile : options.inputFiles) {
    auto document = PAGXImporter::FromFile(inputFile);
    if (document == nullptr) {
      std::cerr << "pagx export: error: failed to load '" << inputFile << "'\n";
      return 1;
    }
    for (auto& error : document->errors) {
      std::cerr << "pagx export: warning: " << error << "\n";
    }
    if (document->hasUnresolvedImports()) {
      std::cerr << "pagx export: error: unresolved import directive in '" << inputFile
                << "', run 'pagx resolve' first\n";
      return 1;
    }
    documents.emplace_back(std::move(document));
  }

  std::vector<PAGXDocument*> documentPtrs = {};
  documentPtrs.reserve(documents.size());
  for (const auto& document : documents) {
    documentPtrs.emplace_back(document.get());
  }

  PPTExporter::Options pptOptions = {};
  pptOptions.convertTextToPath = options.textToPath;
  pptOptions.bakeUnsupported = options.pptBakeUnsupported;
  pptOptions.ignoreGlyphRuns = options.pptIgnoreGlyphRuns;

  if (!PPTExporter::ToFile(documentPtrs, options.outputFile, pptOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

int RunExport(int argc, char* argv[]) {
  ExportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.format == "svg") {
    return ExportToSVG(options);
  }
  if (options.format == "pptx") {
    return ExportToPPT(options);
  }

  if (options.format == "html") {
    return ExportToHTML(options);
  }

  auto* videoFormat = FindVideoFormat(options.format);
  if (videoFormat != nullptr) {
    return ExportToMP4(options, *videoFormat);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
