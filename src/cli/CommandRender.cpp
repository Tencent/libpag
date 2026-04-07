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

#include "cli/CommandRender.h"
#include <libxml/parser.h>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "cli/XPathQuery.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Node.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx::cli {

struct RenderOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = "png";
  float scale = 1.0f;
  bool hasCrop = false;
  float cropX = 0.0f;
  float cropY = 0.0f;
  float cropWidth = 0.0f;
  float cropHeight = 0.0f;
  int quality = 100;
  bool hasBackground = false;
  float bgRed = 0.0f;
  float bgGreen = 0.0f;
  float bgBlue = 0.0f;
  float bgAlpha = 0.0f;
  std::string id = {};
  std::string xpath = {};
  std::vector<std::string> fontFiles = {};
  std::vector<std::string> fallbacks = {};
};

static void PrintRenderUsage() {
  std::cout
      << "Usage: pagx render [options] <file.pagx>\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <path>       Output file path (default: input path with format "
         "extension)\n"
      << "  --format png|webp|jpg     Output format (default: png)\n"
      << "  --scale <float>           Scale factor (default: 1.0)\n"
      << "  --crop <x,y,w,h>          Crop region in document coordinates\n"
      << "  --id <id>                 Render only the Layer with the specified id\n"
      << "  --xpath <expr>            Render only the Layer matched by XPath expression\n"
      << "  --quality <0-100>         Encoding quality (default: 100)\n"
      << "  --background <color>      Background color (hex: #RRGGBB or #RRGGBBAA)\n"
      << "  --font <path>             Register a font file (can be specified multiple times)\n"
      << "  --fallback <path|name>    Add a fallback font file or system font name (can be\n"
         "                            specified multiple times)\n";
}

static bool ParseHexColor(const std::string& hex, float* red, float* green, float* blue,
                          float* alpha) {
  std::string color = hex;
  if (!color.empty() && color[0] == '#') {
    color = color.substr(1);
  }
  if (color.size() != 6 && color.size() != 8) {
    return false;
  }
  char* endPtr = nullptr;
  unsigned long value = strtoul(color.c_str(), &endPtr, 16);
  if (endPtr != color.c_str() + color.size()) {
    return false;
  }
  if (color.size() == 8) {
    *red = static_cast<float>((value >> 24) & 0xFF) / 255.0f;
    *green = static_cast<float>((value >> 16) & 0xFF) / 255.0f;
    *blue = static_cast<float>((value >> 8) & 0xFF) / 255.0f;
    *alpha = static_cast<float>(value & 0xFF) / 255.0f;
  } else {
    *red = static_cast<float>((value >> 16) & 0xFF) / 255.0f;
    *green = static_cast<float>((value >> 8) & 0xFF) / 255.0f;
    *blue = static_cast<float>(value & 0xFF) / 255.0f;
    *alpha = 1.0f;
  }
  return true;
}

static bool ParseCrop(const std::string& cropStr, float* x, float* y, float* w, float* h) {
  // Expected format: x,y,w,h
  char* end = nullptr;
  const char* str = cropStr.c_str();
  *x = strtof(str, &end);
  if (end == str || *end != ',') {
    return false;
  }
  str = end + 1;
  *y = strtof(str, &end);
  if (end == str || *end != ',') {
    return false;
  }
  str = end + 1;
  *w = strtof(str, &end);
  if (end == str || *end != ',') {
    return false;
  }
  str = end + 1;
  *h = strtof(str, &end);
  if (end == str || (*end != '\0' && *end != ' ')) {
    return false;
  }
  return *w > 0 && *h > 0;
}

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseRenderOptions(int argc, char* argv[], RenderOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
      if (options->format != "png" && options->format != "webp" && options->format != "jpg") {
        std::cerr << "pagx render: unsupported format '" << options->format << "'\n";
        return 1;
      }
    } else if (arg == "--scale" && i + 1 < argc) {
      char* endPtr = nullptr;
      options->scale = strtof(argv[++i], &endPtr);
      if (endPtr == argv[i] || *endPtr != '\0' || !std::isfinite(options->scale) ||
          options->scale <= 0.0f) {
        std::cerr << "pagx render: invalid scale '" << argv[i] << "'\n";
        return 1;
      }
    } else if (arg == "--crop" && i + 1 < argc) {
      options->hasCrop = true;
      if (!ParseCrop(argv[++i], &options->cropX, &options->cropY, &options->cropWidth,
                     &options->cropHeight)) {
        std::cerr << "pagx render: invalid crop format, expected x,y,w,h\n";
        return 1;
      }
    } else if (arg == "--quality" && i + 1 < argc) {
      char* endPtr = nullptr;
      errno = 0;
      long val = strtol(argv[++i], &endPtr, 10);
      if (errno != 0 || endPtr == argv[i] || *endPtr != '\0' || val < 0 || val > 100) {
        std::cerr << "pagx render: invalid quality '" << argv[i] << "', must be 0-100\n";
        return 1;
      }
      options->quality = static_cast<int>(val);
    } else if (arg == "--background" && i + 1 < argc) {
      options->hasBackground = true;
      if (!ParseHexColor(argv[++i], &options->bgRed, &options->bgGreen, &options->bgBlue,
                         &options->bgAlpha)) {
        std::cerr << "pagx render: invalid color format, expected #RRGGBB or #RRGGBBAA\n";
        return 1;
      }
    } else if (arg == "--id" && i + 1 < argc) {
      options->id = argv[++i];
    } else if (arg == "--xpath" && i + 1 < argc) {
      options->xpath = argv[++i];
    } else if (arg == "--font" && i + 1 < argc) {
      options->fontFiles.push_back(argv[++i]);
    } else if (arg == "--fallback" && i + 1 < argc) {
      options->fallbacks.push_back(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      PrintRenderUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx render: unknown option '" << arg << "'\n";
      return 1;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (!options->id.empty() && !options->xpath.empty()) {
    std::cerr << "pagx render: --id and --xpath are mutually exclusive\n";
    return 1;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx render: missing input file\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    auto dot = options->inputFile.rfind('.');
    auto base = dot != std::string::npos ? options->inputFile.substr(0, dot) : options->inputFile;
    options->outputFile = base + "." + options->format;
  }
  return 0;
}

static tgfx::EncodedFormat GetEncodedFormat(const std::string& format) {
  if (format == "webp") {
    return tgfx::EncodedFormat::WEBP;
  }
  if (format == "jpg") {
    return tgfx::EncodedFormat::JPEG;
  }
  return tgfx::EncodedFormat::PNG;
}

static bool WriteDataToFile(const std::string& filePath, const std::shared_ptr<tgfx::Data>& data) {
  FILE* file = fopen(filePath.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  auto written = fwrite(data->data(), 1, data->size(), file);
  fclose(file);
  return written == data->size();
}

static tgfx::Bitmap RenderCore(const RenderOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx render: failed to load '" << options.inputFile << "'\n";
    return {};
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx render: warning: " << error << "\n";
    }
  }

  FontConfig fontConfig = {};
  for (const auto& fontFile : options.fontFiles) {
    auto typeface = tgfx::Typeface::MakeFromPath(fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx render: failed to load font '" << fontFile << "'\n";
      return {};
    }
    fontConfig.registerTypeface(typeface);
  }
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};
  for (const auto& fallbackStr : options.fallbacks) {
    auto typeface = ResolveFallbackTypeface(fallbackStr);
    if (typeface == nullptr) {
      std::cerr << "pagx render: fallback font '" << fallbackStr << "' not found\n";
      return {};
    }
    fontConfig.registerTypeface(typeface);
    fallbackTypefaces.push_back(typeface);
  }
  if (!fallbackTypefaces.empty()) {
    fontConfig.addFallbackTypefaces(std::move(fallbackTypefaces));
  }
  document->applyLayout(&fontConfig);
  bool hasTarget = !options.id.empty() || !options.xpath.empty();
  std::shared_ptr<tgfx::Layer> rootLayer = nullptr;
  std::shared_ptr<tgfx::Layer> targetTgfxLayer = nullptr;
  LayerBuildResult buildResult = {};

  if (hasTarget) {
    buildResult = LayerBuilder::BuildWithMap(document.get());
    rootLayer = buildResult.root;
  } else {
    rootLayer = LayerBuilder::Build(document.get());
  }
  if (rootLayer == nullptr) {
    std::cerr << "pagx render: failed to build layer tree\n";
    return {};
  }

  if (hasTarget) {
    const Layer* targetPagxLayer = nullptr;
    if (!options.id.empty()) {
      auto* node = document->findNode(options.id);
      if (node == nullptr) {
        std::cerr << "pagx render: no node found with id '" << options.id << "'\n";
        return {};
      }
      if (node->nodeType() != NodeType::Layer) {
        std::cerr << "pagx render: node '" << options.id << "' is not a Layer\n";
        return {};
      }
      targetPagxLayer = static_cast<const Layer*>(node);
    } else {
      xmlDocPtr xmlDoc = xmlReadFile(options.inputFile.c_str(), nullptr, XML_PARSE_NONET);
      if (xmlDoc == nullptr) {
        std::cerr << "pagx render: failed to parse XML from '" << options.inputFile << "'\n";
        return {};
      }
      targetPagxLayer = EvaluateSingleXPath(xmlDoc, options.xpath, document.get(), "render");
      xmlFreeDoc(xmlDoc);
      if (targetPagxLayer == nullptr) {
        std::cerr << "pagx render: no matching Layer found\n";
        return {};
      }
    }
    auto it = buildResult.layerMap.find(targetPagxLayer);
    if (it == buildResult.layerMap.end()) {
      std::cerr << "pagx render: target Layer has no rendered layer\n";
      return {};
    }
    targetTgfxLayer = it->second;
  }

  float sourceWidth = 0;
  float sourceHeight = 0;
  float offsetX = 0;
  float offsetY = 0;

  tgfx::DisplayList displayList = {};
  displayList.setRenderMode(tgfx::RenderMode::Direct);
  displayList.root()->addChild(rootLayer);

  if (targetTgfxLayer != nullptr) {
    auto globalBounds = targetTgfxLayer->getBounds(rootLayer.get(), true);
    if (options.hasCrop) {
      sourceWidth = options.cropWidth;
      sourceHeight = options.cropHeight;
      offsetX = globalBounds.left + options.cropX;
      offsetY = globalBounds.top + options.cropY;
    } else {
      sourceWidth = globalBounds.width();
      sourceHeight = globalBounds.height();
      offsetX = globalBounds.left;
      offsetY = globalBounds.top;
    }
  } else {
    sourceWidth = options.hasCrop ? options.cropWidth : document->width;
    sourceHeight = options.hasCrop ? options.cropHeight : document->height;
    if (options.hasCrop) {
      offsetX = -options.cropX;
      offsetY = -options.cropY;
    }
  }

  int outputWidth = static_cast<int>(ceilf(sourceWidth * options.scale));
  int outputHeight = static_cast<int>(ceilf(sourceHeight * options.scale));
  if (outputWidth <= 0 || outputHeight <= 0) {
    std::cerr << "pagx render: output dimensions are zero\n";
    return {};
  }

  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    std::cerr << "pagx render: failed to create GL device\n";
    return {};
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    std::cerr << "pagx render: failed to lock GL context\n";
    return {};
  }

  if (options.hasBackground) {
    displayList.setBackgroundColor(
        tgfx::Color(options.bgRed, options.bgGreen, options.bgBlue, options.bgAlpha));
  }

  auto surface = tgfx::Surface::Make(context, outputWidth, outputHeight);
  if (surface == nullptr) {
    device->unlock();
    std::cerr << "pagx render: failed to create surface (" << outputWidth << "x" << outputHeight
              << ")\n";
    return {};
  }

  if (targetTgfxLayer != nullptr) {
    auto canvas = surface->getCanvas();
    canvas->scale(options.scale, options.scale);
    canvas->translate(-offsetX, -offsetY);
    canvas->concat(targetTgfxLayer->getRelativeMatrix(rootLayer.get()));
    targetTgfxLayer->draw(canvas);
  } else {
    displayList.setZoomScale(options.scale);
    if (offsetX != 0.0f || offsetY != 0.0f) {
      displayList.setContentOffset(offsetX * options.scale, offsetY * options.scale);
    }
    displayList.render(surface.get());
  }

  tgfx::Bitmap bitmap(outputWidth, outputHeight, false, false);
  if (bitmap.isEmpty()) {
    device->unlock();
    std::cerr << "pagx render: failed to allocate bitmap\n";
    return {};
  }
  tgfx::Pixmap pixmap(bitmap);
  bool readSuccess = surface->readPixels(pixmap.info(), pixmap.writablePixels());
  device->unlock();
  if (!readSuccess) {
    std::cerr << "pagx render: failed to read pixels from surface\n";
    return {};
  }
  return bitmap;
}

tgfx::Bitmap RenderToBitmap(int argc, char* argv[]) {
  RenderOptions options = {};
  auto parseResult = ParseRenderOptions(argc, argv, &options);
  if (parseResult != 0) {
    return {};
  }
  return RenderCore(options);
}

int RunRender(int argc, char* argv[]) {
  RenderOptions options = {};
  auto parseResult = ParseRenderOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }
  auto bitmap = RenderCore(options);
  if (bitmap.isEmpty()) {
    return 1;
  }
  tgfx::Pixmap pixmap(bitmap);
  auto encodedData =
      tgfx::ImageCodec::Encode(pixmap, GetEncodedFormat(options.format), options.quality);
  if (encodedData == nullptr) {
    std::cerr << "pagx render: failed to encode image\n";
    return 1;
  }
  if (!WriteDataToFile(options.outputFile, encodedData)) {
    std::cerr << "pagx render: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  std::cout << "pagx render: wrote " << options.outputFile << " (" << bitmap.width() << "x"
            << bitmap.height() << ")\n";
  return 0;
}

}  // namespace pagx::cli
