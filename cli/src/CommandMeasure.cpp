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

#include "CommandMeasure.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/core/UTF.h"

namespace pagx::cli {

struct MeasureOptions {
  std::string fontFamily = {};
  float fontSize = 0.0f;
  std::string fontStyle = {};
  std::string text = {};
  float letterSpacing = 0.0f;
  bool jsonOutput = false;
};

static void PrintMeasureUsage() {
  std::cerr << "Usage: pagx measure [options]\n"
            << "\n"
            << "Options:\n"
            << "  --font <family>              Font family name (required)\n"
            << "  --size <pt>                  Font size in points (required)\n"
            << "  --style <style>              Font style (e.g., Bold, Italic)\n"
            << "  --text <string>              Text to measure (required)\n"
            << "  --letter-spacing <float>     Additional spacing between characters\n"
            << "  --format json                Output in JSON format\n";
}

static bool ParseMeasureOptions(int argc, char* argv[], MeasureOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--font" && i + 1 < argc) {
      options->fontFamily = argv[++i];
    } else if (arg == "--size" && i + 1 < argc) {
      options->fontSize = strtof(argv[++i], nullptr);
      if (options->fontSize <= 0.0f) {
        std::cerr << "pagx measure: font size must be positive\n";
        return false;
      }
    } else if (arg == "--style" && i + 1 < argc) {
      options->fontStyle = argv[++i];
    } else if (arg == "--text" && i + 1 < argc) {
      options->text = argv[++i];
    } else if (arg == "--letter-spacing" && i + 1 < argc) {
      options->letterSpacing = strtof(argv[++i], nullptr);
    } else if (arg == "--format" && i + 1 < argc) {
      std::string fmt = argv[++i];
      if (fmt == "json") {
        options->jsonOutput = true;
      } else {
        std::cerr << "pagx measure: unsupported format '" << fmt << "'\n";
        return false;
      }
    } else if (arg == "--help" || arg == "-h") {
      PrintMeasureUsage();
      return false;
    } else if (arg[0] == '-') {
      std::cerr << "pagx measure: unknown option '" << arg << "'\n";
      return false;
    }
    i++;
  }
  if (options->fontFamily.empty()) {
    std::cerr << "pagx measure: --font is required\n";
    return false;
  }
  if (options->fontSize <= 0.0f) {
    std::cerr << "pagx measure: --size is required\n";
    return false;
  }
  if (options->text.empty()) {
    std::cerr << "pagx measure: --text is required\n";
    return false;
  }
  return true;
}

int RunMeasure(int argc, char* argv[]) {
  MeasureOptions options = {};
  if (!ParseMeasureOptions(argc, argv, &options)) {
    return 1;
  }

  // Create the typeface and font.
  auto typeface = tgfx::Typeface::MakeFromName(options.fontFamily, options.fontStyle);
  if (typeface == nullptr) {
    std::cerr << "pagx measure: font '" << options.fontFamily << "' not found\n";
    return 1;
  }
  tgfx::Font font(typeface, options.fontSize);

  // Get font metrics.
  auto metrics = font.getMetrics();

  // Measure text width by iterating Unicode code points.
  const char* ptr = options.text.c_str();
  const char* end = ptr + options.text.size();
  float totalWidth = 0.0f;
  int charCount = 0;
  while (ptr < end) {
    tgfx::Unichar codePoint = tgfx::UTF::NextUTF8(&ptr, end);
    if (codePoint < 0) {
      break;
    }
    auto glyphID = font.getGlyphID(codePoint);
    float advance = font.getAdvance(glyphID);
    totalWidth += advance;
    charCount++;
  }

  // Add letter spacing between characters (not after the last one).
  if (charCount > 1) {
    totalWidth += options.letterSpacing * static_cast<float>(charCount - 1);
  }

  if (options.jsonOutput) {
    std::cout << "{\"fontFamily\": \"" << options.fontFamily << "\""
              << ", \"fontSize\": " << options.fontSize << ", \"ascent\": " << metrics.ascent
              << ", \"descent\": " << metrics.descent << ", \"leading\": " << metrics.leading
              << ", \"capHeight\": " << metrics.capHeight << ", \"xHeight\": " << metrics.xHeight
              << ", \"width\": " << totalWidth << ", \"charCount\": " << charCount << "}\n";
  } else {
    std::cout << "Font: " << options.fontFamily;
    if (!options.fontStyle.empty()) {
      std::cout << " " << options.fontStyle;
    }
    std::cout << " @ " << options.fontSize << "pt\n";
    std::cout << "Metrics:\n";
    std::cout << "  ascent:    " << metrics.ascent << "\n";
    std::cout << "  descent:   " << metrics.descent << "\n";
    std::cout << "  leading:   " << metrics.leading << "\n";
    std::cout << "  capHeight: " << metrics.capHeight << "\n";
    std::cout << "  xHeight:   " << metrics.xHeight << "\n";
    std::cout << "Text: \"" << options.text << "\"\n";
    std::cout << "  width:     " << totalWidth << "\n";
    std::cout << "  chars:     " << charCount << "\n";
  }

  return 0;
}

}  // namespace pagx::cli
