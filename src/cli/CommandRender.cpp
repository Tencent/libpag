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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "renderer/LayerBuilder.h"
#include "renderer/TextLayout.h"
#include "pagx/PAGXImporter.h"
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
};

static void PrintRenderUsage() {
  std::cerr << "Usage: pagx render [options] <file.pagx>\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>       Output file path (required)\n"
            << "  --format png|webp|jpg     Output format (default: png)\n"
            << "  --scale <float>           Scale factor (default: 1.0)\n"
            << "  --crop <x,y,w,h>          Crop region in document coordinates\n"
            << "  --quality <0-100>         Encoding quality (default: 100)\n"
            << "  --background <color>      Background color (hex: #RRGGBB or #RRGGBBAA)\n";
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

static bool ParseRenderOptions(int argc, char* argv[], RenderOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
      if (options->format != "png" && options->format != "webp" && options->format != "jpg") {
        std::cerr << "pagx render: unsupported format '" << options->format << "'\n";
        return false;
      }
    } else if (arg == "--scale" && i + 1 < argc) {
      options->scale = strtof(argv[++i], nullptr);
      if (options->scale <= 0.0f) {
        std::cerr << "pagx render: scale must be positive\n";
        return false;
      }
    } else if (arg == "--crop" && i + 1 < argc) {
      options->hasCrop = true;
      if (!ParseCrop(argv[++i], &options->cropX, &options->cropY, &options->cropWidth,
                     &options->cropHeight)) {
        std::cerr << "pagx render: invalid crop format, expected x,y,w,h\n";
        return false;
      }
    } else if (arg == "--quality" && i + 1 < argc) {
      options->quality = atoi(argv[++i]);
      if (options->quality < 0 || options->quality > 100) {
        std::cerr << "pagx render: quality must be between 0 and 100\n";
        return false;
      }
    } else if (arg == "--background" && i + 1 < argc) {
      options->hasBackground = true;
      if (!ParseHexColor(argv[++i], &options->bgRed, &options->bgGreen, &options->bgBlue,
                         &options->bgAlpha)) {
        std::cerr << "pagx render: invalid color format, expected #RRGGBB or #RRGGBBAA\n";
        return false;
      }
    } else if (arg == "--help" || arg == "-h") {
      PrintRenderUsage();
      return false;
    } else if (arg[0] == '-') {
      std::cerr << "pagx render: unknown option '" << arg << "'\n";
      return false;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx render: no input file specified\n";
    return false;
  }
  if (options->outputFile.empty()) {
    std::cerr << "pagx render: no output file specified (use -o)\n";
    return false;
  }
  return true;
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

int RunRender(int argc, char* argv[]) {
  RenderOptions options = {};
  if (!ParseRenderOptions(argc, argv, &options)) {
    return 1;
  }

  // Load the PAGX document.
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx render: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx render: warning: " << error << "\n";
    }
  }

  // Perform text layout and build the layer tree.
  TextLayout textLayout = {};
  textLayout.layout(document.get());
  auto rootLayer = LayerBuilder::Build(document.get(), &textLayout);
  if (rootLayer == nullptr) {
    std::cerr << "pagx render: failed to build layer tree\n";
    return 1;
  }

  // Calculate output dimensions.
  float sourceWidth = options.hasCrop ? options.cropWidth : document->width;
  float sourceHeight = options.hasCrop ? options.cropHeight : document->height;
  int outputWidth = static_cast<int>(ceilf(sourceWidth * options.scale));
  int outputHeight = static_cast<int>(ceilf(sourceHeight * options.scale));
  if (outputWidth <= 0 || outputHeight <= 0) {
    std::cerr << "pagx render: output dimensions are zero\n";
    return 1;
  }

  // Create GPU device and context.
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    std::cerr << "pagx render: failed to create GL device\n";
    return 1;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    std::cerr << "pagx render: failed to lock GL context\n";
    return 1;
  }

  // Create the rendering surface.
  auto surface = tgfx::Surface::Make(context, outputWidth, outputHeight);
  if (surface == nullptr) {
    device->unlock();
    std::cerr << "pagx render: failed to create surface (" << outputWidth << "x" << outputHeight
              << ")\n";
    return 1;
  }

  // Set up the display list with zoom and offset for crop+scale.
  tgfx::DisplayList displayList = {};
  displayList.setRenderMode(tgfx::RenderMode::Direct);
  if (options.hasBackground) {
    displayList.setBackgroundColor(
        tgfx::Color(options.bgRed, options.bgGreen, options.bgBlue, options.bgAlpha));
  }
  displayList.root()->addChild(rootLayer);
  displayList.setZoomScale(options.scale);
  if (options.hasCrop) {
    displayList.setContentOffset(-options.cropX * options.scale, -options.cropY * options.scale);
  }

  // Render the display list to the surface.
  displayList.render(surface.get());

  // Read pixels from the surface.
  tgfx::Bitmap bitmap(outputWidth, outputHeight, false, false);
  if (bitmap.isEmpty()) {
    device->unlock();
    std::cerr << "pagx render: failed to allocate bitmap\n";
    return 1;
  }
  tgfx::Pixmap pixmap(bitmap);
  bool readSuccess = surface->readPixels(pixmap.info(), pixmap.writablePixels());
  device->unlock();

  if (!readSuccess) {
    std::cerr << "pagx render: failed to read pixels from surface\n";
    return 1;
  }

  // Encode and write the output file.
  auto encodedFormat = GetEncodedFormat(options.format);
  auto encodedData = tgfx::ImageCodec::Encode(pixmap, encodedFormat, options.quality);
  if (encodedData == nullptr) {
    std::cerr << "pagx render: failed to encode image\n";
    return 1;
  }
  if (!WriteDataToFile(options.outputFile, encodedData)) {
    std::cerr << "pagx render: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx render: wrote " << options.outputFile << " (" << outputWidth << "x"
            << outputHeight << ")\n";
  return 0;
}

}  // namespace pagx::cli
