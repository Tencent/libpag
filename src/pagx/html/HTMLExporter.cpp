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

#include "pagx/HTMLExporter.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include "pagx/html/HTMLBuilder.h"
#include "pagx/html/HTMLStyleExtractor.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

//==============================================================================
// Font-face rule generation
//==============================================================================

static const char* DetectFontFormat(const std::string& uri) {
  auto dot = uri.rfind('.');
  if (dot == std::string::npos) {
    return nullptr;
  }
  auto ext = uri.substr(dot);
  for (auto& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (ext == ".otf") return "opentype";
  if (ext == ".ttf" || ext == ".ttc") return "truetype";
  if (ext == ".woff") return "woff";
  if (ext == ".woff2") return "woff2";
  return nullptr;
}

static const char* DetectFontMime(const std::string& uri) {
  auto dot = uri.rfind('.');
  if (dot == std::string::npos) {
    return "font/ttf";
  }
  auto ext = uri.substr(dot);
  for (auto& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (ext == ".otf") return "font/otf";
  if (ext == ".ttf" || ext == ".ttc") return "font/ttf";
  if (ext == ".woff") return "font/woff";
  if (ext == ".woff2") return "font/woff2";
  return "font/ttf";
}

static std::string EncodeBase64(const std::vector<uint8_t>& data) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);
  size_t i = 0;
  while (i + 2 < data.size()) {
    uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                 (static_cast<uint32_t>(data[i + 1]) << 8) | data[i + 2];
    out += table[(v >> 18) & 0x3F];
    out += table[(v >> 12) & 0x3F];
    out += table[(v >> 6) & 0x3F];
    out += table[v & 0x3F];
    i += 3;
  }
  if (i + 1 == data.size()) {
    uint32_t v = static_cast<uint32_t>(data[i]) << 16;
    out += table[(v >> 18) & 0x3F];
    out += table[(v >> 12) & 0x3F];
    out += '=';
    out += '=';
  } else if (i + 2 == data.size()) {
    uint32_t v = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8);
    out += table[(v >> 18) & 0x3F];
    out += table[(v >> 12) & 0x3F];
    out += table[(v >> 6) & 0x3F];
    out += '=';
  }
  return out;
}

static std::string ReadFileAsDataURI(const std::string& path, const char* mime) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }
  auto size = file.tellg();
  file.seekg(0);
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  file.read(reinterpret_cast<char*>(bytes.data()), size);
  return std::string("data:") + mime + ";base64," + EncodeBase64(bytes);
}

static std::string BuildFontFaceCSS(const std::vector<FontFaceRule>& rules, bool minify) {
  if (rules.empty()) {
    return {};
  }
  std::string css;
  for (const auto& rule : rules) {
    if (rule.fontFamily.empty() || rule.uri.empty()) {
      continue;
    }
    std::string srcUrl;
    switch (rule.mode) {
      case FontEmbedMode::URL:
        srcUrl = rule.uri;
        break;
      case FontEmbedMode::Base64: {
        auto dataUri = ReadFileAsDataURI(rule.uri, DetectFontMime(rule.uri));
        if (dataUri.empty()) {
          continue;
        }
        srcUrl = dataUri;
        break;
      }
      case FontEmbedMode::FilePath:
        srcUrl = "file://" + rule.uri;
        break;
    }
    auto format = DetectFontFormat(rule.uri);
    std::string srcValue = "url('" + srcUrl + "')";
    if (format) {
      srcValue += std::string(" format('") + format + "')";
    }
    if (minify) {
      css += "@font-face{font-family:'" + rule.fontFamily + "';src:" + srcValue;
      if (!rule.fontWeight.empty()) {
        css += ";font-weight:" + rule.fontWeight;
      }
      if (!rule.fontStyle.empty()) {
        css += ";font-style:" + rule.fontStyle;
      }
      css += ";font-display:block}";
    } else {
      css += "@font-face {\n";
      css += "  font-family: '" + rule.fontFamily + "';\n";
      css += "  src: " + srcValue + ";\n";
      if (!rule.fontWeight.empty()) {
        css += "  font-weight: " + rule.fontWeight + ";\n";
      }
      if (!rule.fontStyle.empty()) {
        css += "  font-style: " + rule.fontStyle + ";\n";
      }
      css += "  font-display: block;\n";
      css += "}\n";
    }
  }
  return css;
}

//==============================================================================
// Coordinate rounding
//==============================================================================

// Rounds every <number>px in a CSS style fragment to at most two decimal places. Matching on
// the `px` suffix keeps transform matrix components, color channels, rotation angles (deg),
// scale factors, and SVG path data untouched.
static std::string RoundPxInStyle(const std::string& style) {
  std::string result;
  result.reserve(style.size());
  size_t i = 0;
  while (i < style.size()) {
    char c = style[i];
    bool isDigitStart = (c >= '0' && c <= '9') || c == '.';
    bool isSignedDigit = (c == '-' || c == '+') && i + 1 < style.size() &&
                         ((style[i + 1] >= '0' && style[i + 1] <= '9') || style[i + 1] == '.');
    if (!isDigitStart && !isSignedDigit) {
      result += c;
      i++;
      continue;
    }

    size_t numStart = i;
    size_t j = i;
    if (style[j] == '-' || style[j] == '+') {
      j++;
    }
    bool sawDigit = false;
    bool sawDot = false;
    while (j < style.size()) {
      char cj = style[j];
      if (cj >= '0' && cj <= '9') {
        sawDigit = true;
        j++;
      } else if (cj == '.' && !sawDot) {
        sawDot = true;
        j++;
      } else {
        break;
      }
    }

    if (!sawDigit || j + 1 >= style.size() || style[j] != 'p' || style[j + 1] != 'x') {
      result += style.substr(numStart, j - numStart);
      i = j;
      continue;
    }

    std::string numStr = style.substr(numStart, j - numStart);
    float value = static_cast<float>(std::atof(numStr.c_str()));
    auto rounded = CoordToString(value);
    result += rounded;
    if (rounded != "0") {
      result += "px";
    }
    i = j + 2;
  }
  return result;
}

// Walks the generated HTML, finds every `style="..."` attribute, and applies RoundPxInStyle
// to its contents. Leaves <script> bodies, SVG path data, transform matrix components, and
// every attribute other than style untouched.
static std::string RoundCoordinatesInHTML(const std::string& html) {
  std::string result;
  result.reserve(html.size());
  size_t pos = 0;
  while (pos < html.size()) {
    size_t stylePos = html.find("style=\"", pos);
    if (stylePos == std::string::npos) {
      result += html.substr(pos);
      break;
    }
    result += html.substr(pos, stylePos - pos);
    size_t valueStart = stylePos + 7;
    size_t valueEnd = html.find('"', valueStart);
    if (valueEnd == std::string::npos) {
      result += html.substr(stylePos);
      break;
    }
    std::string styleValue = html.substr(valueStart, valueEnd - valueStart);
    result += "style=\"";
    result += RoundPxInStyle(styleValue);
    result += "\"";
    pos = valueEnd + 1;
  }
  return result;
}

//==============================================================================
// ToHTML / ToFile
//==============================================================================

static constexpr const char* kGeneratedComment =
    "<!-- Generated by PAGX HTMLExporter. Do not edit. -->\n";

static HTMLStyleExtractor::Format ToExtractorFormat(HTMLFormat format) {
  switch (format) {
    case HTMLFormat::Pretty:
      return HTMLStyleExtractor::Format::Pretty;
    case HTMLFormat::Minify:
      return HTMLStyleExtractor::Format::Minify;
    case HTMLFormat::Compact:
      break;
  }
  return HTMLStyleExtractor::Format::Compact;
}

std::string HTMLExporter::ToHTML(const PAGXDocument& doc, const Options& options) {
  bool minify = options.format == HTMLFormat::Minify;
  int indent = minify ? 0 : options.indent;
  HTMLBuilder html(indent, 0, 4096, minify);
  HTMLBuilder defs(indent, 2, 4096, minify);
  HTMLWriterContext ctx;
  ctx.docWidth = doc.width;
  ctx.docHeight = doc.height;
  ctx.staticImgDir = options.staticImgDir;
  ctx.staticImgUrlPrefix = options.staticImgUrlPrefix;
  ctx.staticImgNamePrefix = options.staticImgNamePrefix;
  ctx.staticImgPixelRatio = options.staticImgPixelRatio;
  ctx.fontSynthesisWeight = options.fontSynthesisWeight;
  ctx.fontSynthesisStyle = options.fontSynthesisStyle;

  // Pre-pass: for every compatible PlusDarker Layer, render a cropped backdrop PNG with the layer
  // temporarily hidden. The resulting base64 data URLs are consumed by writeLayer below to emit an
  // SVG filter (feImage + feComposite arithmetic) that matches tgfx PlusDarker pixel-for-pixel.
  // Gated on staticImgDir like the other rasterization paths (DiamondGradient, ImagePattern
  // mirror/clamp); when empty, writeLayer falls back to the existing mix-blend-mode:darken
  // approximation.
  if (!options.staticImgDir.empty()) {
    HTMLPlusDarkerRenderer::RenderAll(doc, options.staticImgDir, options.staticImgUrlPrefix,
                                      options.staticImgNamePrefix, options.staticImgPixelRatio,
                                      ctx.plusDarkerBackdrops);
  }

  HTMLWriter writer(&defs, &ctx);

  // Root div
  std::string rootStyle = "position:relative;width:" + FloatToString(doc.width) +
                          "px;height:" + FloatToString(doc.height) + "px;overflow:hidden";
  html.openTag("div");
  html.addAttr("data-pagx-version", "1.0");
  html.addAttr("style", rootStyle);
  html.closeTagStart();

  // Render layers into body content
  HTMLBuilder body(indent, 1, 4096, minify);
  for (auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }

  // Insert SVG defs if any
  std::string defsStr = defs.release();
  if (!defsStr.empty()) {
    html.openTag("svg");
    html.addAttr("style", "position:absolute;width:0;height:0;overflow:hidden");
    html.closeTagStart();
    html.openTag("defs");
    html.closeTagStart();
    html.addRawContent(defsStr);
    html.closeTag();  // </defs>
    html.closeTag();  // </svg>
  }

  html.addRawContent(body.release());

  html.closeTag();  // </div>

  std::string nativeHTML = RoundCoordinatesInHTML(html.release());

  if (options.extractStyleSheet) {
    nativeHTML = HTMLStyleExtractor::Extract(nativeHTML, ToExtractorFormat(options.format));
  }

  // Inject @font-face rules into the <style> block (or create a standalone <style> block).
  std::string fontFaceCSS = BuildFontFaceCSS(options.fontFaceRules, minify);
  if (!fontFaceCSS.empty()) {
    auto stylePos = nativeHTML.find("<style>");
    if (stylePos != std::string::npos) {
      nativeHTML.insert(stylePos + 7, minify ? fontFaceCSS : "\n" + fontFaceCSS);
    } else {
      std::string block =
          minify ? "<style>" + fontFaceCSS + "</style>" : "<style>\n" + fontFaceCSS + "</style>\n";
      nativeHTML = block + nativeHTML;
    }
  }

  const char* header = minify ? "" : kGeneratedComment;
  return std::string(header) + nativeHTML;
}

bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                          const Options& options) {
  auto html = ToHTML(document, options);
  if (html.empty()) {
    return false;
  }
  auto dirPath = std::filesystem::path(filePath).parent_path();
  if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
    std::error_code ec = {};
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
      return false;
    }
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  return file.good();
}

}  // namespace pagx
