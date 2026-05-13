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
#include <iostream>
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

// Strips characters that would let a caller-supplied FontFaceRule value escape the `@font-face`
// declaration it is being concatenated into. Unlike EscapeCssFontFamily (which operates on a
// quoted string context), this filter is meant for unquoted CSS keyword/list values such as
// font-weight, font-style and unicode-range. Control characters, quote marks, backslashes and
// the declaration/block terminators ';', '{', '}' are all dropped; '<' and '>' are dropped too
// so the emitted CSS cannot break out of the surrounding <style> element. Every other visible
// ASCII byte — including the ',' '+' '-' 'U' needed by unicode-range — flows through unchanged,
// so legitimate values are preserved exactly.
static std::string SanitizeFontFaceValue(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    auto uc = static_cast<unsigned char>(c);
    if (uc < 0x20 || c == ';' || c == '{' || c == '}' || c == '<' || c == '>' || c == '\\' ||
        c == '"' || c == '\'') {
      continue;
    }
    out += c;
  }
  return out;
}

static std::string BuildFontFaceCSS(const std::vector<FontFaceRule>& rules) {
  if (rules.empty()) {
    return {};
  }
  std::string css;
  for (const auto& rule : rules) {
    if (rule.fontFamily.empty() || rule.sources.empty()) {
      continue;
    }
    // Assemble the CSS `src:` value by iterating every source in order. Browsers try each
    // url() entry left-to-right and fall back to the next one when the current source fails to
    // load; this makes "bundled local font first, CDN as safety net" expressible in a single
    // @font-face rule. A rule with all sources failing to resolve (e.g. every Base64 file
    // missing) is skipped entirely so we never emit a rule with no valid src.
    std::string srcValue;
    for (const auto& source : rule.sources) {
      if (source.uri.empty()) {
        continue;
      }
      std::string srcUrl;
      switch (source.mode) {
        case FontEmbedMode::URL:
          srcUrl = source.uri;
          break;
        case FontEmbedMode::Base64: {
          auto dataUri = ReadFileAsDataURI(source.uri, DetectFontMime(source.uri));
          if (dataUri.empty()) {
            continue;
          }
          srcUrl = dataUri;
          break;
        }
        case FontEmbedMode::FilePath:
          srcUrl = "file://" + source.uri;
          break;
      }
      if (srcUrl.empty()) {
        continue;
      }
      if (!srcValue.empty()) {
        srcValue += ",\n       ";
      }
      // Escape the URL value for the single-quoted CSS string context of url('...'). Base64
      // data URIs only contain characters the CSS parser accepts unescaped, so this is a
      // no-op for Base64 mode; for URL and FilePath mode it blocks a caller-supplied uri
      // containing a raw ' or control byte from breaking out of the declaration.
      srcValue += "url('" + EscapeCssFontFamily(srcUrl) + "')";
      auto format = DetectFontFormat(source.uri);
      if (format) {
        srcValue += std::string(" format('") + format + "')";
      }
    }
    if (srcValue.empty()) {
      continue;
    }
    css += "@font-face {\n";
    css += "  font-family: '" + EscapeCssFontFamily(rule.fontFamily) + "';\n";
    css += "  src: " + srcValue + ";\n";
    if (!rule.fontWeight.empty()) {
      css += "  font-weight: " + SanitizeFontFaceValue(rule.fontWeight) + ";\n";
    }
    if (!rule.fontStyle.empty()) {
      css += "  font-style: " + SanitizeFontFaceValue(rule.fontStyle) + ";\n";
    }
    if (!rule.unicodeRange.empty()) {
      css += "  unicode-range: " + SanitizeFontFaceValue(rule.unicodeRange) + ";\n";
    }
    css += "  font-display: block;\n";
    css += "}\n";
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

static constexpr const char* GENERATED_COMMENT =
    "<!-- Generated by PAGX HTMLExporter. Do not edit. -->\n";

std::string HTMLExporter::ToHTML(const PAGXDocument& doc, const std::string& resourceDir,
                                 const Options& options) {
  if (resourceDir.empty()) {
    std::cerr << "HTMLExporter::ToHTML: resourceDir must not be empty. "
                 "Use HTMLExporter::ToFile for automatic resource-directory derivation."
              << std::endl;
    return {};
  }
  // Derive the URL prefix used by <img src=...> from the resource directory's basename. This
  // assumes the caller will write the returned HTML string to a file located in resourceDir's
  // parent directory (ToFile enforces this layout automatically).
  auto resourceDirPath = std::filesystem::path(resourceDir);
  std::string urlPrefix = resourceDirPath.filename().string();
  if (!urlPrefix.empty()) {
    urlPrefix += '/';
  }

  HTMLBuilder html(0, 4096);
  HTMLBuilder defs(2, 4096);
  HTMLWriterContext ctx;
  ctx.docWidth = doc.width;
  ctx.docHeight = doc.height;
  ctx.staticImgDir = resourceDir;
  ctx.staticImgUrlPrefix = urlPrefix;
  ctx.rasterScale = options.rasterScale;

  // Pre-pass: for every compatible PlusDarker Layer, render a cropped backdrop PNG with the layer
  // temporarily hidden. The resulting base64 data URLs are consumed by writeLayer below to emit an
  // SVG filter (feImage + feComposite arithmetic) that matches tgfx PlusDarker pixel-for-pixel.
  HTMLPlusDarkerRenderer::RenderAll(doc, resourceDir, urlPrefix, options.rasterScale,
                                    ctx.plusDarkerBackdrops);

  HTMLWriter writer(&defs, &ctx);

  // Root div
  std::string rootStyle = "position:relative;width:" + CssFloatToString(doc.width) +
                          "px;height:" + CssFloatToString(doc.height) + "px;overflow:hidden";
  html.openTag("div");
  html.addAttr("data-pagx-version", "1.0");
  html.addAttr("style", rootStyle);
  html.closeTagStart();

  // Render layers into body content
  HTMLBuilder body(1, 4096);
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
    nativeHTML = HTMLStyleExtractor::Extract(nativeHTML);
  }

  // Inject @font-face rules into the <style> block (or create a standalone <style> block).
  std::string fontFaceCSS = BuildFontFaceCSS(options.fontFaceRules);
  if (!fontFaceCSS.empty()) {
    auto stylePos = nativeHTML.find("<style>");
    if (stylePos != std::string::npos) {
      nativeHTML.insert(stylePos + 7, "\n" + fontFaceCSS);
    } else {
      nativeHTML = "<style>\n" + fontFaceCSS + "</style>\n" + nativeHTML;
    }
  }

  return std::string(GENERATED_COMMENT) + nativeHTML;
}

bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                          const Options& options) {
  // Derive the resource directory as a sibling of the HTML file named after its stem:
  // "/tmp/abc.html" → resourceDir = "/tmp/abc".
  auto htmlPath = std::filesystem::path(filePath);
  auto stem = htmlPath.stem().string();
  if (stem.empty()) {
    std::cerr << "HTMLExporter::ToFile: filePath must have a non-empty stem: " << filePath
              << std::endl;
    return false;
  }
  auto parentDir = htmlPath.parent_path();
  auto resourceDir = (parentDir.empty() ? std::filesystem::path(".") : parentDir) / stem;

  auto html = ToHTML(document, resourceDir.string(), options);
  if (html.empty()) {
    return false;
  }
  if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
    std::error_code ec = {};
    std::filesystem::create_directories(parentDir, ec);
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
