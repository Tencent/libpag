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

#include "pagx/HTMLImporter.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include "pagx/html/HTMLParserContext.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

//==================================================================================================
// detail namespace: shared CSS lexical helpers (non-inline definitions live here so that
// the four split translation units can all link against a single copy).
//==================================================================================================

namespace detail {

std::vector<std::string> SplitTopLevelCommas(const std::string& s) {
  std::vector<std::string> out;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '(') {
      depth++;
    } else if (c == ')') {
      if (depth > 0) depth--;
    } else if (c == ',' && depth == 0) {
      out.push_back(Trim(s.substr(start, i - start)));
      start = i + 1;
    }
  }
  out.push_back(Trim(s.substr(start)));
  return out;
}

std::vector<std::string> SplitTopLevelWhitespace(const std::string& s) {
  std::vector<std::string> out;
  int depth = 0;
  std::string cur;
  for (char c : s) {
    if (c == '(') {
      depth++;
      cur.push_back(c);
    } else if (c == ')') {
      if (depth > 0) depth--;
      cur.push_back(c);
    } else if (depth == 0 && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

void ParseStyleString(const std::string& styleStr,
                      std::unordered_map<std::string, std::string>& out) {
  size_t pos = 0;
  while (pos < styleStr.size()) {
    while (pos < styleStr.size() && std::isspace(static_cast<unsigned char>(styleStr[pos]))) {
      ++pos;
    }
    if (pos + 1 < styleStr.size() && styleStr[pos] == '/' && styleStr[pos + 1] == '*') {
      auto commentEnd = styleStr.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? styleStr.size() : commentEnd + 2;
      continue;
    }
    size_t colonPos = styleStr.find(':', pos);
    if (colonPos == std::string::npos) {
      break;
    }
    std::string propName = ToLower(Trim(styleStr.substr(pos, colonPos - pos)));
    size_t searchStart = colonPos + 1;
    size_t semicolonPos = std::string::npos;
    int parenDepth = 0;
    for (size_t i = searchStart; i < styleStr.size(); i++) {
      if (styleStr[i] == '(') {
        parenDepth++;
      } else if (styleStr[i] == ')') {
        if (parenDepth > 0) parenDepth--;
      } else if (styleStr[i] == ';' && parenDepth == 0) {
        semicolonPos = i;
        break;
      }
    }
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }
    std::string propValue = Trim(styleStr.substr(colonPos + 1, semicolonPos - colonPos - 1));
    if (!propName.empty() && !propValue.empty()) {
      out[propName] = propValue;
    }
    pos = semicolonPos + 1;
  }
}

const std::unordered_map<std::string, uint32_t>& NamedColors() {
  // clang-format off
  static const std::unordered_map<std::string, uint32_t> table = {
      {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7}, {"aqua", 0x00FFFF},
      {"aquamarine", 0x7FFFD4}, {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC},
      {"bisque", 0xFFE4C4}, {"black", 0x000000}, {"blanchedalmond", 0xFFEBCD},
      {"blue", 0x0000FF}, {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
      {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0}, {"chartreuse", 0x7FFF00},
      {"chocolate", 0xD2691E}, {"coral", 0xFF7F50}, {"cornflowerblue", 0x6495ED},
      {"cornsilk", 0xFFF8DC}, {"crimson", 0xDC143C}, {"cyan", 0x00FFFF},
      {"darkblue", 0x00008B}, {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
      {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400}, {"darkgrey", 0xA9A9A9},
      {"darkkhaki", 0xBDB76B}, {"darkmagenta", 0x8B008B}, {"darkolivegreen", 0x556B2F},
      {"darkorange", 0xFF8C00}, {"darkorchid", 0x9932CC}, {"darkred", 0x8B0000},
      {"darksalmon", 0xE9967A}, {"darkseagreen", 0x8FBC8F}, {"darkslateblue", 0x483D8B},
      {"darkslategray", 0x2F4F4F}, {"darkslategrey", 0x2F4F4F}, {"darkturquoise", 0x00CED1},
      {"darkviolet", 0x9400D3}, {"deeppink", 0xFF1493}, {"deepskyblue", 0x00BFFF},
      {"dimgray", 0x696969}, {"dimgrey", 0x696969}, {"dodgerblue", 0x1E90FF},
      {"firebrick", 0xB22222}, {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22},
      {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC}, {"ghostwhite", 0xF8F8FF},
      {"gold", 0xFFD700}, {"goldenrod", 0xDAA520}, {"gray", 0x808080},
      {"green", 0x008000}, {"greenyellow", 0xADFF2F}, {"grey", 0x808080},
      {"honeydew", 0xF0FFF0}, {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C},
      {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0}, {"khaki", 0xF0E68C},
      {"lavender", 0xE6E6FA}, {"lavenderblush", 0xFFF0F5}, {"lawngreen", 0x7CFC00},
      {"lemonchiffon", 0xFFFACD}, {"lightblue", 0xADD8E6}, {"lightcoral", 0xF08080},
      {"lightcyan", 0xE0FFFF}, {"lightgoldenrodyellow", 0xFAFAD2}, {"lightgray", 0xD3D3D3},
      {"lightgreen", 0x90EE90}, {"lightgrey", 0xD3D3D3}, {"lightpink", 0xFFB6C1},
      {"lightsalmon", 0xFFA07A}, {"lightseagreen", 0x20B2AA}, {"lightskyblue", 0x87CEFA},
      {"lightslategray", 0x778899}, {"lightslategrey", 0x778899}, {"lightsteelblue", 0xB0C4DE},
      {"lightyellow", 0xFFFFE0}, {"lime", 0x00FF00}, {"limegreen", 0x32CD32},
      {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF}, {"maroon", 0x800000},
      {"mediumaquamarine", 0x66CDAA}, {"mediumblue", 0x0000CD}, {"mediumorchid", 0xBA55D3},
      {"mediumpurple", 0x9370DB}, {"mediumseagreen", 0x3CB371}, {"mediumslateblue", 0x7B68EE},
      {"mediumspringgreen", 0x00FA9A}, {"mediumturquoise", 0x48D1CC}, {"mediumvioletred", 0xC71585},
      {"midnightblue", 0x191970}, {"mintcream", 0xF5FFFA}, {"mistyrose", 0xFFE4E1},
      {"moccasin", 0xFFE4B5}, {"navajowhite", 0xFFDEAD}, {"navy", 0x000080},
      {"oldlace", 0xFDF5E6}, {"olive", 0x808000}, {"olivedrab", 0x6B8E23},
      {"orange", 0xFFA500}, {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
      {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98}, {"paleturquoise", 0xAFEEEE},
      {"palevioletred", 0xDB7093}, {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9},
      {"peru", 0xCD853F}, {"pink", 0xFFC0CB}, {"plum", 0xDDA0DD},
      {"powderblue", 0xB0E0E6}, {"purple", 0x800080}, {"rebeccapurple", 0x663399},
      {"red", 0xFF0000}, {"rosybrown", 0xBC8F8F}, {"royalblue", 0x4169E1},
      {"saddlebrown", 0x8B4513}, {"salmon", 0xFA8072}, {"sandybrown", 0xF4A460},
      {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE}, {"sienna", 0xA0522D},
      {"silver", 0xC0C0C0}, {"skyblue", 0x87CEEB}, {"slateblue", 0x6A5ACD},
      {"slategray", 0x708090}, {"slategrey", 0x708090}, {"snow", 0xFFFAFA},
      {"springgreen", 0x00FF7F}, {"steelblue", 0x4682B4}, {"tan", 0xD2B48C},
      {"teal", 0x008080}, {"thistle", 0xD8BFD8}, {"tomato", 0xFF6347},
      {"transparent", 0x000000}, {"turquoise", 0x40E0D0}, {"violet", 0xEE82EE},
      {"wheat", 0xF5DEB3}, {"white", 0xFFFFFF}, {"whitesmoke", 0xF5F5F5},
      {"yellow", 0xFFFF00}, {"yellowgreen", 0x9ACD32},
  };
  // clang-format on
  return table;
}

const std::unordered_map<std::string, std::string>& ElementDefaults() {
  // clang-format off
  static const std::unordered_map<std::string, std::string> table = {
      {"body", "font-family: Arial; font-size: 14px; color: #1E293B;"},
      {"h1", "font-size: 32px; font-weight: bold;"},
      {"h2", "font-size: 24px; font-weight: bold;"},
      {"h3", "font-size: 20px; font-weight: bold;"},
      {"h4", "font-size: 18px; font-weight: bold;"},
      {"h5", "font-size: 16px; font-weight: bold;"},
      {"h6", "font-size: 14px; font-weight: bold;"},
      {"a", "color: #2563EB; text-decoration: underline;"},
  };
  // clang-format on
  return table;
}

void LowercaseTagsInPlace(const std::shared_ptr<DOMNode>& node) {
  if (!node) return;
  if (node->type == DOMNodeType::Element) {
    node->name = ToLower(std::move(node->name));
    // Lower-case attribute names while we're here (CSS keys must be lower-case anyway).
    for (auto& attr : node->attributes) {
      attr.name = ToLower(std::move(attr.name));
    }
  }
  auto child = node->firstChild;
  while (child) {
    LowercaseTagsInPlace(child);
    child = child->nextSibling;
  }
}

std::string EscapeXml(const std::string& text, bool isAttribute) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += isAttribute ? "&quot;" : "\"";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

float ParseAngle(const std::string& raw) {
  std::string s = Trim(raw);
  if (s.empty()) return 0.0f;
  char* end = nullptr;
  float v = std::strtof(s.c_str(), &end);
  if (end == s.c_str()) return 0.0f;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "deg") return v;
  if (suffix == "rad") return v * 180.0f / 3.14159265358979323846f;
  if (suffix == "turn") return v * 360.0f;
  return v;
}

float CssDirectionToAngle(const std::string& kw) {
  std::string s = ToLower(Trim(kw));
  if (s.compare(0, 3, "to ") != 0) return 180.0f;
  s = Trim(s.substr(3));
  if (s == "top") return 0.0f;
  if (s == "right") return 90.0f;
  if (s == "bottom") return 180.0f;
  if (s == "left") return 270.0f;
  if (s == "top right" || s == "right top") return 45.0f;
  if (s == "bottom right" || s == "right bottom") return 135.0f;
  if (s == "bottom left" || s == "left bottom") return 225.0f;
  if (s == "top left" || s == "left top") return 315.0f;
  return 180.0f;
}

std::string ExtractParenArgs(const std::string& value) {
  auto open = value.find('(');
  auto close = value.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open) return {};
  return value.substr(open + 1, close - open - 1);
}

std::string DirectoryOf(const std::string& filePath) {
  auto lastSlash = filePath.find_last_of("/\\");
  if (lastSlash == std::string::npos) {
    return {};
  }
  return filePath.substr(0, lastSlash + 1);
}

bool LooksAbsolutePath(const std::string& src) {
  if (src.empty()) return false;
  if (src[0] == '/' || src[0] == '\\') return true;
  if (src.find("://") != std::string::npos) return true;
  if (src.compare(0, 5, "data:") == 0) return true;
  // Windows drive letter ("C:/")
  if (src.size() > 2 && std::isalpha(static_cast<unsigned char>(src[0])) && src[1] == ':' &&
      (src[2] == '/' || src[2] == '\\')) {
    return true;
  }
  return false;
}

Color HexToColor(uint32_t hex, bool hasAlpha) {
  Color color = {};
  color.colorSpace = ColorSpace::SRGB;
  if (hasAlpha) {
    color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
  } else {
    color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
    color.alpha = 1.0f;
  }
  return color;
}

Padding BuildPaddingShorthand(const std::vector<float>& nums) {
  Padding p = {};
  if (nums.size() == 1) {
    p.top = p.right = p.bottom = p.left = nums[0];
  } else if (nums.size() == 2) {
    p.top = p.bottom = nums[0];
    p.left = p.right = nums[1];
  } else if (nums.size() == 3) {
    p.top = nums[0];
    p.left = p.right = nums[1];
    p.bottom = nums[2];
  } else if (nums.size() >= 4) {
    p.top = nums[0];
    p.right = nums[1];
    p.bottom = nums[2];
    p.left = nums[3];
  }
  return p;
}

bool ParseSizingDimension(const std::string& raw, float& outPx, float& outPct) {
  std::string value = Trim(raw);
  if (value.empty()) return false;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return false;
  std::string suffix = Trim(end);
  if (suffix == "%") {
    outPct = num;
    return true;
  }
  if (suffix.empty() || suffix == "px") {
    outPx = num;
    return true;
  }
  if (suffix == "em" || suffix == "rem") {
    outPx = num * 16.0f;
    return true;
  }
  outPx = num;
  return true;
}

std::string CollapseHTMLWhitespace(const std::string& raw) {
  std::string normalized;
  normalized.reserve(raw.size());
  bool prevSpace = false;
  for (char c : raw) {
    if (c == '\n') {
      if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
      }
      normalized.push_back('\n');
      prevSpace = false;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      if (!prevSpace && !normalized.empty()) {
        normalized.push_back(' ');
        prevSpace = true;
      }
    } else {
      normalized.push_back(c);
      prevSpace = false;
    }
  }
  while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\n')) {
    normalized.pop_back();
  }
  size_t start = 0;
  while (start < normalized.size() && normalized[start] == ' ') start++;
  if (start > 0) normalized.erase(0, start);
  return normalized;
}

}  // namespace detail

using namespace pagx::detail;

//==================================================================================================
// HTMLImporter public entry points
//==================================================================================================

std::shared_ptr<PAGXDocument> HTMLImporter::Parse(const std::string& filePath,
                                                  const Options& options) {
  HTMLParserContext parser(options);
  return parser.parseFile(filePath);
}

std::shared_ptr<PAGXDocument> HTMLImporter::Parse(const uint8_t* data, size_t length,
                                                  const Options& options) {
  HTMLParserContext parser(options);
  return parser.parse(data, length);
}

std::shared_ptr<PAGXDocument> HTMLImporter::ParseString(const std::string& htmlContent,
                                                        const Options& options) {
  return Parse(reinterpret_cast<const uint8_t*>(htmlContent.data()), htmlContent.size(), options);
}

//==================================================================================================
// HTMLParserContext: top-level traversal
//==================================================================================================

HTMLParserContext::HTMLParserContext(const HTMLImporter::Options& options) : _options(options) {
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parseFile(const std::string& filePath) {
  auto dom = XMLDOM::MakeFromFile(filePath);
  if (!dom) {
    return nullptr;
  }
  _basePath = DirectoryOf(filePath);
  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }
  auto dom = XMLDOM::Make(data, length);
  if (!dom) {
    return nullptr;
  }
  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parseDOM(const std::shared_ptr<XMLDOM>& dom) {
  auto root = dom->getRootNode();
  if (!root) {
    return nullptr;
  }
  LowercaseTagsInPlace(root);
  if (root->name != "html") {
    return nullptr;
  }

  // Find <head> and <body>.
  std::shared_ptr<DOMNode> head = nullptr;
  std::shared_ptr<DOMNode> body = nullptr;
  auto child = root->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "head" && !head) {
        head = child;
      } else if (child->name == "body" && !body) {
        body = child;
      }
    }
    child = child->getNextSibling();
  }
  if (!body) {
    return nullptr;
  }

  // Collect <style> rules from <head>.
  if (head) {
    collectStyles(head);
  }

  // Determine canvas size.
  float canvasW = 0;
  float canvasH = 0;
  if (!resolveCanvasSize(body, canvasW, canvasH)) {
    return nullptr;
  }

  _canvasWidth = canvasW;
  _canvasHeight = canvasH;
  _document = PAGXDocument::Make(canvasW, canvasH);

  // Title -> data-title on the document (PAGX has no top-level title node; the
  // exporter writes data-* on the root <pagx>).
  if (head) {
    auto titleNode = head->getFirstChild("title");
    if (titleNode) {
      auto textNode = titleNode->getFirstChild();
      if (textNode && textNode->type == DOMNodeType::Text) {
        _document->customData["title"] = Trim(textNode->name);
      }
    }
  }

  collectAllIds(root);

  Layer* bodyLayer = convertBody(body, canvasW, canvasH);
  if (_hadHardError) {
    return nullptr;
  }
  if (bodyLayer) {
    _document->layers.push_back(bodyLayer);
  }
  return _document;
}

//==================================================================================================
// Diagnostics
//==================================================================================================

void HTMLParserContext::warn(const std::string& message) {
  if (_options.strict) {
    hardError(message);
    return;
  }
  if (_document) {
    _document->errors.push_back(message);
  }
}

void HTMLParserContext::hardError(const std::string& message) {
  _hadHardError = true;
  if (_document) {
    _document->errors.push_back(message);
  }
}

//==================================================================================================
// Canvas size resolution
//==================================================================================================

bool HTMLParserContext::resolveCanvasSize(const std::shared_ptr<DOMNode>& body, float& outW,
                                          float& outH) {
  bool haveTarget = !std::isnan(_options.targetWidth) && !std::isnan(_options.targetHeight);
  std::unordered_map<std::string, std::string> props;
  auto* styleAttr = body->findAttribute("style");
  if (styleAttr) {
    ParseStyleString(*styleAttr, props);
  }
  auto* classAttr = body->findAttribute("class");
  if (classAttr) {
    mergeClassRules(*classAttr, props);
  }
  float bodyW = parsePxLength(LookupProperty(props, "width"));
  float bodyH = parsePxLength(LookupProperty(props, "height"));
  bool haveBody = !std::isnan(bodyW) && !std::isnan(bodyH) && bodyW > 0 && bodyH > 0;

  if (haveTarget && (!haveBody || !_options.preferBodySize)) {
    outW = _options.targetWidth;
    outH = _options.targetHeight;
    return true;
  }
  if (haveBody) {
    outW = bodyW;
    outH = bodyH;
    return true;
  }
  return false;
}

//==================================================================================================
// ID handling
//==================================================================================================

void HTMLParserContext::collectAllIds(const std::shared_ptr<DOMNode>& node) {
  if (!node) return;
  if (node->type == DOMNodeType::Element) {
    auto* idAttr = node->findAttribute("id");
    if (idAttr && !idAttr->empty()) {
      _existingIds.insert(*idAttr);
    }
  }
  auto child = node->firstChild;
  while (child) {
    collectAllIds(child);
    child = child->nextSibling;
  }
}

std::string HTMLParserContext::generateUniqueId(const std::string& prefix) {
  std::string id;
  do {
    id = prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0 || (_document && _document->findNode(id) != nullptr));
  _existingIds.insert(id);
  return id;
}

std::string HTMLParserContext::consumeId(const std::shared_ptr<DOMNode>& element) {
  auto* idAttr = element->findAttribute("id");
  if (!idAttr || idAttr->empty()) return {};
  std::string id = *idAttr;
  // The id must be unique inside the PAGX document. Since IDs are author-controlled here,
  // we trust the source unless it would collide with an already-bound node.
  if (_document && _document->findNode(id) != nullptr) {
    warn("html: duplicate id '" + id + "' on element; renaming");
    id = generateUniqueId(id + "_");
  }
  return id;
}

void HTMLParserContext::assignElementId(Layer* layer, const std::shared_ptr<DOMNode>& element) {
  std::string id = consumeId(element);
  if (!id.empty()) layer->id = id;
}

bool HTMLParserContext::hasBackgroundVisuals(const HTMLBoxAttributes& box) {
  return box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
         box.borderSet || !box.boxShadow.empty() || !box.backdropFilter.empty();
}

bool HTMLParserContext::requiresInnerHost(const HTMLBoxAttributes& box) {
  return box.paddingSet || box.displayFlex || box.gapSet;
}

//==================================================================================================
// Body / element conversion
//==================================================================================================

Layer* HTMLParserContext::convertBody(const std::shared_ptr<DOMNode>& body, float canvasW,
                                      float canvasH) {
  HTMLInheritedStyle inherited = computeInherited(body, HTMLInheritedStyle{});
  HTMLBoxAttributes box = resolveBox(body);

  auto layer = _document->makeNode<Layer>();
  layer->width = canvasW;
  layer->height = canvasH;
  layer->percentWidth = NAN;
  layer->percentHeight = NAN;

  bool hasBgVisuals = hasBackgroundVisuals(box);
  if (hasBgVisuals) {
    applyBackgroundVisuals(layer, box, /*addRectangle=*/true);
  }
  applyLayerAttributes(layer, body, box);

  Layer* contentHost = layer;
  bool needsInnerHost = hasBgVisuals && (box.paddingSet || box.displayFlex);
  if (needsInnerHost) {
    auto inner = _document->makeNode<Layer>();
    inner->percentWidth = 100.0f;
    inner->percentHeight = 100.0f;
    applyLayoutAttributes(inner, box);
    layer->children.push_back(inner);
    contentHost = inner;
  } else {
    applyLayoutAttributes(layer, box);
  }

  auto child = body->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto* childLayer = convertElement(child, inherited, 1);
      if (childLayer) {
        contentHost->children.push_back(childLayer);
      }
    }
    child = child->getNextSibling();
  }
  assignElementId(layer, body);
  return layer;
}

Layer* HTMLParserContext::convertElement(const std::shared_ptr<DOMNode>& element,
                                         const HTMLInheritedStyle& inherited, int depth) {
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    warn("html: maximum recursion depth reached; element skipped");
    return nullptr;
  }
  if (element->type != DOMNodeType::Element) {
    return nullptr;
  }
  const std::string& tag = element->name;

  if (tag == "br") {
    auto layer = _document->makeNode<Layer>();
    auto text = _document->makeNode<Text>();
    text->text = "\n";
    text->fontFamily = inherited.fontFamily.empty() ? "Arial" : inherited.fontFamily;
    text->fontSize = HTML_DEFAULT_FONT_SIZE;
    layer->contents.push_back(text);
    return layer;
  }
  if (tag == "svg") {
    HTMLBoxAttributes box = resolveBox(element);
    return convertInlineSvg(element, box);
  }
  if (tag == "img") {
    HTMLBoxAttributes box = resolveBox(element);
    return convertImage(element, box);
  }

  HTMLInheritedStyle childInherited = computeInherited(element, inherited);
  HTMLBoxAttributes box = resolveBox(element);

  if (IsContainerTag(tag)) {
    return convertContainer(element, box, childInherited, depth);
  }
  if (IsTextLeafTag(tag)) {
    return convertTextLeaf(element, box, childInherited);
  }
  if (_options.preserveUnknownElements) {
    warn("html: unknown element '" + tag + "' preserved as placeholder");
    auto layer = _document->makeNode<Layer>();
    layer->customData["html-unknown"] = tag;
    return layer;
  }
  warn("html: element '" + tag + "' not supported; skipped");
  return nullptr;
}

Layer* HTMLParserContext::convertContainer(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box,
                                           const HTMLInheritedStyle& inherited, int depth) {
  bool hasBgVisuals = hasBackgroundVisuals(box);
  bool needsInner = hasBgVisuals && requiresInnerHost(box);

  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  if (hasBgVisuals) {
    applyBackgroundVisuals(layer, box, /*addRectangle=*/true);
  }

  Layer* contentHost = layer;
  if (needsInner) {
    auto inner = _document->makeNode<Layer>();
    inner->percentWidth = 100.0f;
    inner->percentHeight = 100.0f;
    applyLayoutAttributes(inner, box);
    layer->children.push_back(inner);
    contentHost = inner;
  } else {
    applyLayoutAttributes(layer, box);
  }

  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto* childLayer = convertElement(child, inherited, depth + 1);
      if (childLayer) {
        contentHost->children.push_back(childLayer);
      }
    } else if (child->type == DOMNodeType::Text) {
      std::string trimmed = Trim(child->name);
      if (!trimmed.empty()) {
        warn("html: stray text inside non-text container; emitted as anonymous text leaf");
        HTMLBoxAttributes leafBox = HTMLBoxAttributes{};
        auto anon = std::make_shared<DOMNode>();
        anon->name = "span";
        anon->type = DOMNodeType::Element;
        auto textNode = std::make_shared<DOMNode>();
        textNode->name = trimmed;
        textNode->type = DOMNodeType::Text;
        anon->firstChild = textNode;
        Layer* leaf = convertTextLeaf(anon, leafBox, inherited);
        if (leaf) {
          contentHost->children.push_back(leaf);
        }
      }
    }
    child = child->getNextSibling();
  }
  assignElementId(layer, element);
  return layer;
}

//==================================================================================================
// Text fragment collection / text leaf conversion
//==================================================================================================

HTMLParserContext::TextFragment HTMLParserContext::makeTextFragment(
    const HTMLInheritedStyle& inherited) {
  TextFragment frag;
  frag.fontFamily = inherited.fontFamily.empty() ? "Arial" : inherited.fontFamily;
  frag.fontStyleName = inherited.fontStyleName;
  float fontSizePx =
      inherited.fontSize.empty() ? HTML_DEFAULT_FONT_SIZE : parsePxLength(inherited.fontSize);
  if (std::isnan(fontSizePx) || fontSizePx <= 0) fontSizePx = HTML_DEFAULT_FONT_SIZE;
  frag.fontSize = fontSizePx;
  if (!inherited.letterSpacing.empty()) {
    float ls = parsePxLength(inherited.letterSpacing);
    if (!std::isnan(ls)) frag.letterSpacing = ls;
  }
  std::string colorStr = inherited.color.empty() ? "#1E293B" : inherited.color;
  frag.color = parseColor(colorStr);
  frag.textDecoration = inherited.textDecoration;
  return frag;
}

bool HTMLParserContext::fragmentsShareStyle(const TextFragment& a, const TextFragment& b) {
  return a.fontFamily == b.fontFamily && a.fontStyleName == b.fontStyleName &&
         a.fontSize == b.fontSize && a.letterSpacing == b.letterSpacing && a.color == b.color &&
         a.textDecoration == b.textDecoration;
}

void HTMLParserContext::appendTextFragment(std::vector<TextFragment>& out,
                                           const HTMLInheritedStyle& inherited, std::string text) {
  if (text.empty()) return;
  TextFragment frag = makeTextFragment(inherited);
  if (!out.empty() && fragmentsShareStyle(out.back(), frag)) {
    out.back().text += text;
    return;
  }
  frag.text = std::move(text);
  out.push_back(std::move(frag));
}

void HTMLParserContext::collectTextFragments(const std::shared_ptr<DOMNode>& element,
                                             const HTMLInheritedStyle& inherited,
                                             std::vector<TextFragment>& out) {
  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      appendTextFragment(out, inherited, child->name);
    } else if (child->type == DOMNodeType::Element) {
      if (child->name == "br") {
        appendTextFragment(out, inherited, "\n");
      } else if (IsInlineRunTag(child->name)) {
        HTMLInheritedStyle childInherited = computeInherited(child, inherited);
        collectTextFragments(child, childInherited, out);
      } else {
        warn("html: '<" + child->name + ">' not supported inside text leaf; skipped");
      }
    }
    child = child->getNextSibling();
  }
}

Layer* HTMLParserContext::convertTextLeaf(const std::shared_ptr<DOMNode>& element,
                                          const HTMLBoxAttributes& box,
                                          const HTMLInheritedStyle& inherited) {
  std::vector<TextFragment> fragments;
  collectTextFragments(element, inherited, fragments);
  for (auto& f : fragments) {
    f.text = CollapseHTMLWhitespace(f.text);
  }
  fragments.erase(std::remove_if(fragments.begin(), fragments.end(),
                                 [](const TextFragment& f) { return f.text.empty(); }),
                  fragments.end());
  if (fragments.empty()) {
    return nullptr;
  }

  bool hasBgVisuals = hasBackgroundVisuals(box);
  bool hasMultipleFragments = fragments.size() > 1;
  bool hasNoWrap = !inherited.whiteSpace.empty() && ToLower(Trim(inherited.whiteSpace)) == "nowrap";
  bool needsTextBox = hasMultipleFragments || !inherited.textAlign.empty() ||
                      !inherited.lineHeight.empty() || box.clipOverflow || hasNoWrap;

  auto outer = _document->makeNode<Layer>();
  applySizeAndPosition(outer, box);
  applyLayerAttributes(outer, element, box);

  Layer* textHost = outer;
  if (hasBgVisuals) {
    applyBackgroundVisuals(outer, box, /*addRectangle=*/true);
    auto inner = _document->makeNode<Layer>();
    inner->percentWidth = 100.0f;
    inner->percentHeight = 100.0f;
    if (box.paddingSet) inner->padding = box.padding;
    outer->children.push_back(inner);
    textHost = inner;
  } else if (box.paddingSet) {
    outer->padding = box.padding;
  }

  if (!needsTextBox) {
    const auto& f = fragments.front();
    textHost->contents.push_back(buildTextElement(f));
    textHost->contents.push_back(buildSolidFill(f.color));
  } else {
    auto textBox = _document->makeNode<TextBox>();
    std::string ta = ToLower(Trim(inherited.textAlign));
    if (!ta.empty()) {
      if (ta == "left" || ta == "start") textBox->textAlign = TextAlign::Start;
      else if (ta == "center")
        textBox->textAlign = TextAlign::Center;
      else if (ta == "right" || ta == "end")
        textBox->textAlign = TextAlign::End;
      else if (ta == "justify")
        textBox->textAlign = TextAlign::Justify;
      else
        warn("html: unsupported text-align '" + ta + "'");
    }
    if (!inherited.lineHeight.empty()) {
      float lh = parsePxLength(inherited.lineHeight);
      if (!std::isnan(lh) && lh > 0) textBox->lineHeight = lh;
    }
    if (hasNoWrap) {
      textBox->wordWrap = false;
    }
    if (box.clipOverflow) {
      textBox->overflow = Overflow::Hidden;
    }
    for (size_t i = 0; i < fragments.size(); i++) {
      const auto& f = fragments[i];
      if (i == 0) {
        textBox->elements.push_back(buildTextElement(f));
        textBox->elements.push_back(buildSolidFill(f.color));
      } else {
        auto group = _document->makeNode<Group>();
        group->elements.push_back(buildTextElement(f));
        group->elements.push_back(buildSolidFill(f.color));
        textBox->elements.push_back(group);
      }
    }
    textHost->contents.push_back(textBox);
  }

  std::string deco = ToLower(Trim(inherited.textDecoration));
  if (!deco.empty() && deco != "none") {
    Color decorationColor = fragments.front().color;
    if (deco.find("underline") != std::string::npos) {
      auto rect = _document->makeNode<Rectangle>();
      rect->percentWidth = 100.0f;
      rect->height = 1.0f;
      rect->bottom = 0.0f;
      textHost->contents.push_back(rect);
      textHost->contents.push_back(buildSolidFill(decorationColor));
    }
    if (deco.find("line-through") != std::string::npos) {
      auto rect = _document->makeNode<Rectangle>();
      rect->percentWidth = 100.0f;
      rect->height = 1.0f;
      rect->centerY = 0.0f;
      textHost->contents.push_back(rect);
      textHost->contents.push_back(buildSolidFill(decorationColor));
    }
    if (deco.find("overline") != std::string::npos) {
      warn("html: text-decoration overline not supported");
    }
  }

  assignElementId(outer, element);
  return outer;
}

}  // namespace pagx
