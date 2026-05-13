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
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "pagx/html/HTMLParserContext.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

//==================================================================================================
// Static helpers (HTML/CSS lexical primitives)
//==================================================================================================

namespace {

// Lower-case ASCII in place.
std::string ToLower(std::string s) {
  for (auto& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// Trim ASCII whitespace from both ends.
std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Split a CSS value list on top-level commas (commas inside parentheses are not split).
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

// Split a CSS value on whitespace runs while respecting parenthesised groups (so
// "0 2px 6px rgba(0,0,0,0.2)" yields four tokens).
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

// Parse a CSS style string into a property map. Later properties override earlier ones.
// Mirrors the SVG importer's ParseStyleString but kept private so the two modules stay
// independent. Comments and parenthesised values are respected.
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

// CSS named color table (CSS Color 3 + rebeccapurple). Duplicated to keep HTML importer
// independent of the SVG module.
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

// Default style sheet by element tag. Returns the declarations to apply; concatenated
// before any class rules so user styles always win.
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

bool IsContainerTag(const std::string& tag) {
  return tag == "div" || tag == "section" || tag == "header" || tag == "footer" || tag == "main" ||
         tag == "aside" || tag == "nav" || tag == "article" || tag == "body";
}

bool IsTextLeafTag(const std::string& tag) {
  return tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" ||
         tag == "h6" || tag == "span" || tag == "a";
}

bool IsInlineRunTag(const std::string& tag) {
  return tag == "span" || tag == "a" || tag == "br";
}

// Lower-case the tag name of every element node in place. Avoids needing to test every
// tag with both cases later on. Text nodes keep their content unchanged.
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

// Escape XML text/attribute values for round-tripping inline SVG content.
std::string EscapeXml(const std::string& text, bool isAttribute) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '&': out += "&amp;"; break;
      case '"': out += isAttribute ? "&quot;" : "\""; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

// Try to parse a CSS angle in `deg`, `rad`, `turn`, or unitless (deg). Returns 0 on
// failure.
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

// Returns the converted PAGX gradient angle (degrees, 0° = +X axis) for a CSS angle
// (degrees, 0° = top, clockwise).
float CssToPagxAngle(float cssDeg) {
  return cssDeg - 90.0f;
}

// Strip trailing slashes so file path resolution doesn't pick up wrong directories.
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

}  // namespace

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
// Style sheet collection
//==================================================================================================

void HTMLParserContext::collectStyles(const std::shared_ptr<DOMNode>& head) {
  auto child = head->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element && child->name == "style") {
      parseStyleBlock(child);
    }
    child = child->getNextSibling();
  }
}

void HTMLParserContext::parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode) {
  // The text content is stored as a child Text node whose `name` field carries the text.
  auto textChild = styleNode->getFirstChild();
  if (!textChild || textChild->type != DOMNodeType::Text) {
    return;
  }
  const std::string& css = textChild->name;

  size_t pos = 0;
  while (pos < css.size()) {
    while (pos < css.size() && std::isspace(static_cast<unsigned char>(css[pos]))) {
      pos++;
    }
    if (pos >= css.size()) break;
    if (pos + 1 < css.size() && css[pos] == '/' && css[pos + 1] == '*') {
      auto commentEnd = css.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? css.size() : commentEnd + 2;
      continue;
    }
    size_t bracePos = css.find('{', pos);
    if (bracePos == std::string::npos) break;
    std::string selectorStr = Trim(css.substr(pos, bracePos - pos));
    size_t closePos = css.find('}', bracePos);
    if (closePos == std::string::npos) break;
    std::string body = Trim(css.substr(bracePos + 1, closePos - bracePos - 1));
    if (selectorStr.empty() || body.empty()) {
      pos = closePos + 1;
      continue;
    }

    auto selectors = SplitTopLevelCommas(selectorStr);
    for (auto& sel : selectors) {
      sel = Trim(sel);
      if (sel.empty()) continue;
      if (sel[0] == '.') {
        std::string cls = sel.substr(1);
        if (cls.find_first_of(" \t.>+~:[#") == std::string::npos) {
          auto& slot = _cssClassRules[cls];
          slot = slot.empty() ? body : (slot + ";" + body);
          continue;
        }
      }
      // Element selector (lower-case ascii letters / digits only, e.g. body, h1, p).
      bool allowed = !sel.empty();
      for (char c : sel) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
          allowed = false;
          break;
        }
      }
      if (allowed) {
        std::string lowered = ToLower(sel);
        auto& slot = _cssElementRules[lowered];
        slot = slot.empty() ? body : (slot + ";" + body);
        continue;
      }
      warn("html: unsupported selector '" + sel + "' in <style>; declarations dropped");
    }
    pos = closePos + 1;
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
  // Apply class rules to read width/height; we do not cache here since this is one-shot.
  auto* classAttr = body->findAttribute("class");
  if (classAttr) {
    size_t p = 0;
    const auto& s = *classAttr;
    while (p < s.size()) {
      while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
      if (p >= s.size()) break;
      size_t e = p;
      while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) e++;
      auto it = _cssClassRules.find(s.substr(p, e - p));
      if (it != _cssClassRules.end()) {
        ParseStyleString(it->second, props);
      }
      p = e;
    }
  }
  auto findVal = [&](const std::string& k) -> std::string {
    auto it = props.find(k);
    return it == props.end() ? std::string{} : it->second;
  };
  float bodyW = parsePxLength(findVal("width"));
  float bodyH = parsePxLength(findVal("height"));
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

//==================================================================================================
// Body / element conversion (foundation – minimal stub; expanded in subsequent milestones)
//==================================================================================================

Layer* HTMLParserContext::convertBody(const std::shared_ptr<DOMNode>& body, float canvasW,
                                      float canvasH) {
  HTMLInheritedStyle inherited = computeInherited(body, HTMLInheritedStyle{});
  HTMLBoxAttributes box = resolveBox(body, inherited);

  auto layer = _document->makeNode<Layer>();
  layer->width = canvasW;
  layer->height = canvasH;
  layer->percentWidth = NAN;
  layer->percentHeight = NAN;

  // body acts as the outer canvas Layer. If it has background visuals, emit those first.
  bool hasBackgroundVisuals =
      box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
      box.borderSet || !box.boxShadow.empty() || !box.backdropFilter.empty();

  if (hasBackgroundVisuals) {
    applyBackgroundVisuals(layer, box, /*addRectangle=*/true);
  }

  applyLayerAttributes(layer, body, box);

  // Layout/children container: when the body itself has layout/padding, use the layer
  // directly as the layout container; otherwise create children Layers as siblings.
  Layer* contentHost = layer;
  bool needsInnerHost = hasBackgroundVisuals && (box.paddingSet || box.displayFlex);
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

  // body id propagation.
  std::string bodyId = consumeId(body);
  if (!bodyId.empty()) {
    layer->id = bodyId;
  }
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
    // <br/> only meaningful inside text leaves; emit a synthetic Layer with Text "\n".
    auto layer = _document->makeNode<Layer>();
    auto text = _document->makeNode<Text>();
    text->text = "\n";
    text->fontFamily = inherited.fontFamily.empty() ? "Arial" : inherited.fontFamily;
    text->fontSize = HTML_DEFAULT_FONT_SIZE;
    layer->contents.push_back(text);
    return layer;
  }
  if (tag == "svg") {
    HTMLBoxAttributes box = resolveBox(element, inherited);
    return convertInlineSvg(element, box);
  }
  if (tag == "img") {
    HTMLBoxAttributes box = resolveBox(element, inherited);
    return convertImage(element, box);
  }

  HTMLInheritedStyle childInherited = computeInherited(element, inherited);
  HTMLBoxAttributes box = resolveBox(element, childInherited);

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

//==================================================================================================
// Container / text / image / svg conversion (m3+ milestones extend these)
//==================================================================================================

Layer* HTMLParserContext::convertContainer(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box,
                                           const HTMLInheritedStyle& inherited, int depth) {
  bool hasBgVisuals = box.backgroundColorSet || !box.backgroundImage.empty() ||
                      box.borderRadiusSet || box.borderSet || !box.boxShadow.empty() ||
                      !box.backdropFilter.empty();
  bool needsInner = hasBgVisuals && (box.paddingSet || box.displayFlex || box.gapSet);

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
      // Text directly inside a container that is not a text leaf: rare in our subset.
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
  std::string id = consumeId(element);
  if (!id.empty()) layer->id = id;
  return layer;
}

// Collapse HTML whitespace in a single fragment: convert tabs/CR to spaces, collapse
// adjacent ASCII whitespace, and trim leading/trailing whitespace at the fragment level.
static std::string CollapseHTMLWhitespace(const std::string& raw) {
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
  while (!normalized.empty() &&
         (normalized.back() == ' ' || normalized.back() == '\n')) {
    normalized.pop_back();
  }
  size_t start = 0;
  while (start < normalized.size() && normalized[start] == ' ') start++;
  if (start > 0) normalized.erase(0, start);
  return normalized;
}

void HTMLParserContext::collectTextFragments(const std::shared_ptr<DOMNode>& element,
                                             const HTMLInheritedStyle& inherited,
                                             std::vector<TextFragment>& out) {
  // Build a fragment template for this element's inherited style.
  auto makeFragment = [&]() {
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
  };

  auto appendText = [&](std::string text) {
    if (text.empty()) return;
    if (!out.empty()) {
      TextFragment& last = out.back();
      // Merge into previous fragment when style fingerprint matches.
      TextFragment proto = makeFragment();
      if (last.fontFamily == proto.fontFamily && last.fontStyleName == proto.fontStyleName &&
          last.fontSize == proto.fontSize && last.letterSpacing == proto.letterSpacing &&
          last.color == proto.color && last.textDecoration == proto.textDecoration) {
        last.text += text;
        return;
      }
    }
    TextFragment frag = makeFragment();
    frag.text = std::move(text);
    out.push_back(frag);
  };

  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      appendText(child->name);
    } else if (child->type == DOMNodeType::Element) {
      if (child->name == "br") {
        appendText("\n");
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

  // Normalize whitespace fragment-by-fragment, then trim global leading/trailing.
  for (auto& f : fragments) {
    f.text = CollapseHTMLWhitespace(f.text);
  }
  // Drop empty fragments while preserving structure.
  fragments.erase(std::remove_if(fragments.begin(), fragments.end(),
                                 [](const TextFragment& f) { return f.text.empty(); }),
                  fragments.end());
  if (fragments.empty()) {
    return nullptr;
  }

  bool hasBgVisuals = box.backgroundColorSet || !box.backgroundImage.empty() ||
                      box.borderRadiusSet || box.borderSet || !box.boxShadow.empty() ||
                      !box.backdropFilter.empty();

  // Decide if we need TextBox.
  bool hasMultipleFragments = fragments.size() > 1;
  bool needsTextBox = hasMultipleFragments || !inherited.textAlign.empty() ||
                      !inherited.lineHeight.empty() || box.clipOverflow ||
                      (!inherited.whiteSpace.empty() &&
                       ToLower(Trim(inherited.whiteSpace)) == "nowrap");

  // Construct outer Layer that holds either the Text/TextBox directly or the bg+inner
  // pattern when visuals are present.
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

  auto buildText = [&](const TextFragment& f) -> Text* {
    auto t = _document->makeNode<Text>();
    t->text = f.text;
    t->fontFamily = f.fontFamily;
    t->fontStyle = f.fontStyleName;
    t->fontSize = f.fontSize;
    t->letterSpacing = f.letterSpacing;
    return t;
  };
  auto buildFill = [&](const Color& c) -> Fill* {
    auto fill = _document->makeNode<Fill>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = c;
    fill->color = solid;
    return fill;
  };

  if (!needsTextBox) {
    const auto& f = fragments.front();
    textHost->contents.push_back(buildText(f));
    textHost->contents.push_back(buildFill(f.color));
  } else {
    auto box_ = _document->makeNode<TextBox>();
    // text-align mapping.
    std::string ta = ToLower(Trim(inherited.textAlign));
    if (!ta.empty()) {
      if (ta == "left" || ta == "start") box_->textAlign = TextAlign::Start;
      else if (ta == "center") box_->textAlign = TextAlign::Center;
      else if (ta == "right" || ta == "end") box_->textAlign = TextAlign::End;
      else if (ta == "justify") box_->textAlign = TextAlign::Justify;
      else warn("html: unsupported text-align '" + ta + "'");
    }
    if (!inherited.lineHeight.empty()) {
      float lh = parsePxLength(inherited.lineHeight);
      if (!std::isnan(lh) && lh > 0) box_->lineHeight = lh;
    }
    if (!inherited.whiteSpace.empty() &&
        ToLower(Trim(inherited.whiteSpace)) == "nowrap") {
      box_->wordWrap = false;
    }
    if (box.clipOverflow) {
      box_->overflow = Overflow::Hidden;
      // also keep Layer.clipToBounds (set by applyLayerAttributes) to match HTML semantics
    }
    // Push fragments: first inline, subsequent wrapped in a Group with its own Fill.
    for (size_t i = 0; i < fragments.size(); i++) {
      const auto& f = fragments[i];
      if (i == 0) {
        box_->elements.push_back(buildText(f));
        box_->elements.push_back(buildFill(f.color));
      } else {
        auto group = _document->makeNode<Group>();
        group->elements.push_back(buildText(f));
        group->elements.push_back(buildFill(f.color));
        box_->elements.push_back(group);
      }
    }
    textHost->contents.push_back(box_);
  }

  // Text decoration overlay.
  std::string deco = ToLower(Trim(inherited.textDecoration));
  if (!deco.empty() && deco != "none") {
    auto addOverlay = [&](float yAnchor, bool atBottom) {
      auto rect = _document->makeNode<Rectangle>();
      rect->percentWidth = 100.0f;
      rect->height = 1.0f;
      if (atBottom) {
        rect->bottom = 0.0f;
      } else {
        rect->centerY = yAnchor;
      }
      textHost->contents.push_back(rect);
      Color c = fragments.front().color;
      textHost->contents.push_back(buildFill(c));
    };
    if (deco.find("underline") != std::string::npos) {
      addOverlay(0.0f, /*atBottom=*/true);
    }
    if (deco.find("line-through") != std::string::npos) {
      addOverlay(0.0f, /*atBottom=*/false);
    }
    if (deco.find("overline") != std::string::npos) {
      warn("html: text-decoration overline not supported");
    }
  }

  std::string id = consumeId(element);
  if (!id.empty()) outer->id = id;
  return outer;
}

Layer* HTMLParserContext::convertImage(const std::shared_ptr<DOMNode>& element,
                                       const HTMLBoxAttributes& box) {
  auto* srcAttr = element->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) {
    warn("html: <img> missing src; skipped");
    return nullptr;
  }
  std::string src = *srcAttr;
  if (src.size() > 4 && ToLower(src.substr(src.size() - 4)) == ".svg" &&
      src.compare(0, 5, "data:") != 0) {
    // External SVG -> use PAGX import directive on a Layer.
    auto layer = _document->makeNode<Layer>();
    HTMLBoxAttributes filledBox = box;
    applySizeAndPosition(layer, filledBox);
    applyLayerAttributes(layer, element, filledBox);
    layer->importDirective.source = LooksAbsolutePath(src) ? src : (_basePath + src);
    layer->importDirective.format = "svg";
    std::string id = consumeId(element);
    if (!id.empty()) layer->id = id;
    return layer;
  }

  std::string resolved = LooksAbsolutePath(src) ? src : (_basePath + src);
  auto* imageNode = registerImageResource(resolved);
  if (!imageNode) return nullptr;

  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  auto rect = _document->makeNode<Rectangle>();
  rect->percentWidth = 100.0f;
  rect->percentHeight = 100.0f;
  layer->contents.push_back(rect);

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  fill->color = pattern;
  layer->contents.push_back(fill);
  std::string id = consumeId(element);
  if (!id.empty()) layer->id = id;
  return layer;
}

Layer* HTMLParserContext::convertInlineSvg(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box) {
  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);
  layer->importDirective.content = serializeSvg(element);
  layer->importDirective.format = "svg";
  std::string id = consumeId(element);
  if (!id.empty()) layer->id = id;
  return layer;
}

//==================================================================================================
// Style resolution
//==================================================================================================

const std::unordered_map<std::string, std::string>& HTMLParserContext::getResolvedStyle(
    const std::shared_ptr<DOMNode>& node) {
  auto it = _stylePropertiesCache.find(node.get());
  if (it != _stylePropertiesCache.end()) {
    return it->second;
  }
  auto& slot = _stylePropertiesCache[node.get()];

  // Priority: element defaults -> element rules from <style> -> class rules -> inline.
  auto& tagDefaults = ElementDefaults();
  auto tagIt = tagDefaults.find(node->name);
  if (tagIt != tagDefaults.end()) {
    ParseStyleString(tagIt->second, slot);
  }
  auto elemRuleIt = _cssElementRules.find(node->name);
  if (elemRuleIt != _cssElementRules.end()) {
    ParseStyleString(elemRuleIt->second, slot);
  }
  auto* classAttr = node->findAttribute("class");
  if (classAttr) {
    const auto& s = *classAttr;
    size_t p = 0;
    while (p < s.size()) {
      while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
      if (p >= s.size()) break;
      size_t e = p;
      while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) e++;
      auto crIt = _cssClassRules.find(s.substr(p, e - p));
      if (crIt != _cssClassRules.end()) {
        ParseStyleString(crIt->second, slot);
      }
      p = e;
    }
  }
  auto* styleAttr = node->findAttribute("style");
  if (styleAttr) {
    ParseStyleString(*styleAttr, slot);
  }
  return slot;
}

std::string HTMLParserContext::getStyleProperty(const std::shared_ptr<DOMNode>& node,
                                                const std::string& property,
                                                const std::string& fallback) {
  const auto& props = getResolvedStyle(node);
  auto it = props.find(property);
  if (it != props.end()) return it->second;
  return fallback;
}

HTMLInheritedStyle HTMLParserContext::computeInherited(const std::shared_ptr<DOMNode>& element,
                                                       const HTMLInheritedStyle& parent) {
  HTMLInheritedStyle out = parent;
  const auto& props = getResolvedStyle(element);
  auto take = [&](const std::string& key, std::string& slot) {
    auto it = props.find(key);
    if (it != props.end()) slot = it->second;
  };
  take("color", out.color);
  take("font-family", out.fontFamily);
  take("font-size", out.fontSize);
  take("font-weight", out.fontWeight);
  take("font-style", out.fontStyle);
  take("letter-spacing", out.letterSpacing);
  take("line-height", out.lineHeight);
  take("text-align", out.textAlign);
  take("text-decoration", out.textDecoration);
  take("white-space", out.whiteSpace);
  // Compute combined font-style label used by PAGX Text.
  bool isBold = false;
  if (!out.fontWeight.empty()) {
    std::string w = ToLower(Trim(out.fontWeight));
    if (w == "bold" || w == "bolder") isBold = true;
    if (!isBold) {
      char* end = nullptr;
      long n = std::strtol(w.c_str(), &end, 10);
      if (end != w.c_str() && n >= 600) isBold = true;
    }
  }
  bool isItalic = false;
  if (!out.fontStyle.empty()) {
    std::string fs = ToLower(Trim(out.fontStyle));
    if (fs == "italic" || fs == "oblique") isItalic = true;
  }
  out.fontStyleName.clear();
  if (isBold && isItalic) {
    out.fontStyleName = "Bold Italic";
  } else if (isBold) {
    out.fontStyleName = "Bold";
  } else if (isItalic) {
    out.fontStyleName = "Italic";
  }
  return out;
}

HTMLBoxAttributes HTMLParserContext::resolveBox(const std::shared_ptr<DOMNode>& element,
                                                const HTMLInheritedStyle& inherited) {
  (void)inherited;
  HTMLBoxAttributes box = {};
  const auto& props = getResolvedStyle(element);
  auto getVal = [&](const std::string& k) -> std::string {
    auto it = props.find(k);
    return it == props.end() ? std::string{} : it->second;
  };

  // Sizing
  {
    std::string w = getVal("width");
    if (!w.empty()) {
      bool pct = false;
      float v = parseLength(w, 0, &pct);
      if (pct) {
        box.widthPct = v;  // parseLength already multiplied by containerSize=0 if pct; redo
        // Re-parse to get the raw percent number.
        char* end = nullptr;
        float raw = std::strtof(w.c_str(), &end);
        box.widthPct = raw;
      } else {
        box.widthPx = v;
      }
    }
    std::string h = getVal("height");
    if (!h.empty()) {
      bool pct = false;
      float v = parseLength(h, 0, &pct);
      if (pct) {
        char* end = nullptr;
        float raw = std::strtof(h.c_str(), &end);
        box.heightPct = raw;
      } else {
        box.heightPx = v;
      }
    }
  }

  // Positioning
  {
    std::string pos = ToLower(Trim(getVal("position")));
    if (pos == "absolute") {
      box.absolute = true;
    } else if (!pos.empty() && pos != "static") {
      warn("html: position: " + pos + " not supported; downgraded to absolute");
      box.absolute = true;
    }
    if (box.absolute) {
      auto fill = [&](const std::string& key, float& slot) {
        std::string v = getVal(key);
        if (!v.empty()) slot = parsePxLength(v);
      };
      fill("left", box.leftPx);
      fill("right", box.rightPx);
      fill("top", box.topPx);
      fill("bottom", box.bottomPx);
    }
  }

  // Layout
  {
    std::string disp = ToLower(Trim(getVal("display")));
    if (disp == "flex") {
      box.displayFlex = true;
    } else if (!disp.empty() && disp != "block" && disp != "inline" && disp != "inline-block") {
      warn("html: display: " + disp + " not supported; ignored");
    } else if (disp == "inline-block") {
      warn("html: display: inline-block not supported; treated as block");
    }
    std::string fd = ToLower(Trim(getVal("flex-direction")));
    if (fd == "column" || fd == "column-reverse") {
      box.flexRow = false;
      if (fd == "column-reverse") warn("html: flex-direction: column-reverse not supported");
    } else if (fd == "row-reverse") {
      warn("html: flex-direction: row-reverse not supported");
    }
    std::string gap = getVal("gap");
    if (!gap.empty()) {
      box.gapPx = parsePxLength(gap);
      box.gapSet = !std::isnan(box.gapPx);
      if (!box.gapSet) box.gapPx = 0;
    }
    std::string padding = getVal("padding");
    if (!padding.empty()) {
      // Parse 1-4 px tokens, mirroring CSS shorthand.
      auto tokens = SplitTopLevelWhitespace(padding);
      std::vector<float> nums;
      for (auto& t : tokens) {
        float v = parsePxLength(t);
        if (std::isnan(v)) {
          warn("html: invalid padding token '" + t + "'");
          continue;
        }
        nums.push_back(v);
      }
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
      box.padding = p;
      box.paddingSet = nums.size() > 0;
    }
    std::string ai = ToLower(Trim(getVal("align-items")));
    if (!ai.empty()) box.alignItems = ai;
    std::string jc = ToLower(Trim(getVal("justify-content")));
    if (!jc.empty()) box.justifyContent = jc;
    std::string flex = getVal("flex");
    if (!flex.empty()) {
      char* end = nullptr;
      float v = std::strtof(flex.c_str(), &end);
      if (end != flex.c_str()) {
        box.flexGrow = v;
        box.flexGrowSet = true;
      } else {
        warn("html: flex shorthand '" + flex + "' not supported beyond 'flex: N'");
      }
    }
    if (!getVal("flex-wrap").empty()) warn("html: flex-wrap not supported; ignored");
    if (!getVal("margin").empty()) warn("html: margin not supported; use padding/gap/flex");
    if (!getVal("margin-top").empty()) warn("html: margin-top not supported");
    if (!getVal("margin-right").empty()) warn("html: margin-right not supported");
    if (!getVal("margin-bottom").empty()) warn("html: margin-bottom not supported");
    if (!getVal("margin-left").empty()) warn("html: margin-left not supported");
    if (!getVal("grid-template-columns").empty()) warn("html: grid layout not supported");
    if (!getVal("transform").empty()) warn("html: transform not supported");
  }

  // Visuals
  {
    std::string bgColor = getVal("background-color");
    if (bgColor.empty()) {
      bgColor = getVal("background");  // accept shorthand if it's color-only
    }
    if (!bgColor.empty() && bgColor.find('(') == std::string::npos &&
        bgColor.find("gradient") == std::string::npos && bgColor.find("url") == std::string::npos) {
      box.backgroundColor = parseColor(bgColor);
      box.backgroundColorSet = true;
    }
    std::string bgImage = getVal("background-image");
    if (bgImage.empty()) {
      std::string sh = getVal("background");
      if (!sh.empty() && sh.find("gradient") != std::string::npos) {
        bgImage = sh;
      }
    }
    if (!bgImage.empty()) {
      box.backgroundImage = bgImage;
    }

    std::string br = getVal("border-radius");
    if (!br.empty()) {
      float v = parsePxLength(br);
      if (!std::isnan(v)) {
        box.borderRadiusPx = v;
        box.borderRadiusSet = true;
      }
    }

    std::string border = getVal("border");
    if (!border.empty()) {
      auto tokens = SplitTopLevelWhitespace(border);
      for (auto& t : tokens) {
        float w = parsePxLength(t);
        if (!std::isnan(w)) {
          box.borderWidthPx = w;
          continue;
        }
        std::string lt = ToLower(t);
        if (lt == "solid" || lt == "none") continue;
        if (lt == "dashed" || lt == "dotted" || lt == "double" || lt == "groove" ||
            lt == "ridge" || lt == "inset" || lt == "outset") {
          warn("html: border style '" + lt + "' not supported; treated as solid");
          continue;
        }
        box.borderColor = parseColor(t);
      }
      box.borderSet = box.borderWidthPx > 0;
    }

    box.boxShadow = getVal("box-shadow");
    box.filter = getVal("filter");
    box.backdropFilter = getVal("backdrop-filter");

    std::string op = getVal("opacity");
    if (!op.empty()) {
      char* end = nullptr;
      float v = std::strtof(op.c_str(), &end);
      if (end != op.c_str()) {
        box.opacity = std::max(0.0f, std::min(1.0f, v));
        box.opacitySet = true;
      }
    }
    box.mixBlendMode = ToLower(Trim(getVal("mix-blend-mode")));

    std::string overflow = ToLower(Trim(getVal("overflow")));
    if (overflow == "hidden") {
      box.clipOverflow = true;
    } else if (!overflow.empty() && overflow != "visible") {
      warn("html: overflow: " + overflow + " not fully supported");
    }
  }
  return box;
}

//==================================================================================================
// Layer attribute application (m3 / m5 milestones extend these)
//==================================================================================================

void HTMLParserContext::applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box) {
  if (!std::isnan(box.widthPx)) layer->width = box.widthPx;
  if (!std::isnan(box.heightPx)) layer->height = box.heightPx;
  if (!std::isnan(box.widthPct)) layer->percentWidth = box.widthPct;
  if (!std::isnan(box.heightPct)) layer->percentHeight = box.heightPct;
  if (box.absolute) {
    layer->includeInLayout = false;
    if (!std::isnan(box.leftPx)) layer->left = box.leftPx;
    if (!std::isnan(box.rightPx)) layer->right = box.rightPx;
    if (!std::isnan(box.topPx)) layer->top = box.topPx;
    if (!std::isnan(box.bottomPx)) layer->bottom = box.bottomPx;
  }
  // `flex` is an "as-a-child" property; apply it on the outermost Layer regardless of
  // whether this Layer is a container, leaf, or image.
  if (box.flexGrowSet) {
    layer->flex = box.flexGrow;
  }
}

void HTMLParserContext::applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box) {
  if (box.displayFlex) {
    layer->layout = box.flexRow ? LayoutMode::Horizontal : LayoutMode::Vertical;
  }
  if (box.gapSet) layer->gap = box.gapPx;
  if (box.paddingSet) layer->padding = box.padding;
  if (!box.alignItems.empty()) {
    const std::string& a = box.alignItems;
    if (a == "stretch") layer->alignment = Alignment::Stretch;
    else if (a == "center") layer->alignment = Alignment::Center;
    else if (a == "flex-start" || a == "start") layer->alignment = Alignment::Start;
    else if (a == "flex-end" || a == "end") layer->alignment = Alignment::End;
    else warn("html: unsupported align-items '" + a + "'");
  }
  if (!box.justifyContent.empty()) {
    const std::string& j = box.justifyContent;
    if (j == "flex-start" || j == "start") layer->arrangement = Arrangement::Start;
    else if (j == "center") layer->arrangement = Arrangement::Center;
    else if (j == "flex-end" || j == "end") layer->arrangement = Arrangement::End;
    else if (j == "space-between") layer->arrangement = Arrangement::SpaceBetween;
    else if (j == "space-evenly") layer->arrangement = Arrangement::SpaceEvenly;
    else if (j == "space-around") layer->arrangement = Arrangement::SpaceAround;
    else warn("html: unsupported justify-content '" + j + "'");
  }
}

bool HTMLParserContext::applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box,
                                               bool addRectangle) {
  bool emitted = false;
  Element* lastGeometry = nullptr;
  Rectangle* rect = nullptr;
  if (addRectangle && (box.backgroundColorSet || !box.backgroundImage.empty() ||
                       box.borderRadiusSet || box.borderSet)) {
    rect = _document->makeNode<Rectangle>();
    rect->percentWidth = 100.0f;
    rect->percentHeight = 100.0f;
    rect->roundness = box.borderRadiusSet ? box.borderRadiusPx : 0.0f;
    layer->contents.push_back(rect);
    lastGeometry = rect;
    emitted = true;
  }

  // Background colour / gradient.
  if (rect && box.backgroundColorSet && box.backgroundImage.empty()) {
    auto fill = _document->makeNode<Fill>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = box.backgroundColor;
    fill->color = solid;
    layer->contents.push_back(fill);
    emitted = true;
  } else if (rect && !box.backgroundImage.empty()) {
    auto fill = _document->makeNode<Fill>();
    std::string bg = Trim(box.backgroundImage);
    std::string lower = ToLower(bg);
    if (lower.compare(0, 16, "linear-gradient(") == 0) {
      auto* grad = parseLinearGradient(bg);
      if (grad) {
        fill->color = grad;
      } else if (box.backgroundColorSet) {
        auto solid = _document->makeNode<SolidColor>();
        solid->color = box.backgroundColor;
        fill->color = solid;
      }
    } else if (lower.compare(0, 16, "radial-gradient(") == 0) {
      auto* grad = parseRadialGradient(bg);
      if (grad) {
        fill->color = grad;
      } else if (box.backgroundColorSet) {
        auto solid = _document->makeNode<SolidColor>();
        solid->color = box.backgroundColor;
        fill->color = solid;
      }
    } else if (lower.compare(0, 15, "conic-gradient(") == 0) {
      auto* grad = parseConicGradient(bg);
      if (grad) {
        fill->color = grad;
      } else if (box.backgroundColorSet) {
        auto solid = _document->makeNode<SolidColor>();
        solid->color = box.backgroundColor;
        fill->color = solid;
      }
    } else if (lower.compare(0, 4, "url(") == 0) {
      warn("html: background-image: url(...) not supported; use <img>");
    } else {
      warn("html: background-image '" + bg + "' not supported");
    }
    if (fill->color) {
      layer->contents.push_back(fill);
      emitted = true;
    }
  }

  // Border.
  if (rect && box.borderSet) {
    auto stroke = _document->makeNode<Stroke>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = box.borderColor;
    stroke->color = solid;
    stroke->width = box.borderWidthPx;
    stroke->align = StrokeAlign::Inside;
    layer->contents.push_back(stroke);
    emitted = true;
  }

  // box-shadow → DropShadowStyle / InnerShadowStyle list.
  if (!box.boxShadow.empty()) {
    auto shadows = parseShadowList(box.boxShadow);
    for (auto& s : shadows) {
      if (s.inset) {
        auto inner = _document->makeNode<InnerShadowStyle>();
        inner->offsetX = s.offsetX;
        inner->offsetY = s.offsetY;
        inner->blurX = s.blur;
        inner->blurY = s.blur;
        inner->color = s.color;
        layer->styles.push_back(inner);
      } else {
        auto drop = _document->makeNode<DropShadowStyle>();
        drop->offsetX = s.offsetX;
        drop->offsetY = s.offsetY;
        drop->blurX = s.blur;
        drop->blurY = s.blur;
        drop->color = s.color;
        layer->styles.push_back(drop);
      }
    }
    if (!shadows.empty()) emitted = true;
  }

  // backdrop-filter: blur(...) → BackgroundBlurStyle.
  if (!box.backdropFilter.empty()) {
    auto steps = parseFilterChain(box.backdropFilter);
    for (auto& step : steps) {
      if (step.kind == FilterStep::Kind::Blur) {
        auto bg = _document->makeNode<BackgroundBlurStyle>();
        bg->blurX = step.blurX;
        bg->blurY = step.blurY;
        layer->styles.push_back(bg);
        emitted = true;
      } else {
        warn("html: backdrop-filter '" + step.raw + "' not supported");
      }
    }
  }

  (void)lastGeometry;
  return emitted;
}

void HTMLParserContext::applyLayerAttributes(Layer* layer,
                                             const std::shared_ptr<DOMNode>& element,
                                             const HTMLBoxAttributes& box) {
  if (box.opacitySet) layer->alpha = box.opacity;
  if (!box.mixBlendMode.empty()) {
    BlendMode mode = BlendModeFromString(box.mixBlendMode);
    layer->blendMode = mode;
  }
  if (box.clipOverflow) layer->clipToBounds = true;

  // filter chain (excluding backdrop-filter, which is handled as a Layer style).
  if (!box.filter.empty()) {
    auto steps = parseFilterChain(box.filter);
    for (auto& step : steps) {
      if (step.kind == FilterStep::Kind::Blur) {
        auto blur = _document->makeNode<BlurFilter>();
        blur->blurX = step.blurX;
        blur->blurY = step.blurY;
        layer->filters.push_back(blur);
      } else if (step.kind == FilterStep::Kind::DropShadow) {
        auto drop = _document->makeNode<DropShadowFilter>();
        drop->offsetX = step.shadow.offsetX;
        drop->offsetY = step.shadow.offsetY;
        drop->blurX = step.shadow.blur;
        drop->blurY = step.shadow.blur;
        drop->color = step.shadow.color;
        layer->filters.push_back(drop);
      } else {
        warn("html: filter '" + step.raw + "' not supported");
      }
    }
  }

  // data-* attributes -> customData
  for (const auto& attr : element->attributes) {
    if (attr.name.compare(0, 5, "data-") == 0) {
      std::string key = attr.name.substr(5);
      if (IsValidCustomDataKey(key)) {
        layer->customData[key] = attr.value;
      } else {
        warn("html: invalid data-* attribute name '" + attr.name + "'");
      }
    }
  }
  // href on <a>
  if (element->name == "a") {
    auto* href = element->findAttribute("href");
    if (href && !href->empty() && IsValidCustomDataKey("href")) {
      layer->customData["href"] = *href;
    }
  }
}

//==================================================================================================
// Value parsing
//==================================================================================================

Color HTMLParserContext::parseColor(const std::string& valueRaw) {
  std::string value = Trim(valueRaw);
  if (value.empty() || ToLower(value) == "none" || ToLower(value) == "transparent") {
    return {0, 0, 0, 0, ColorSpace::SRGB};
  }
  if (value[0] == '#') {
    Color color = {};
    color.colorSpace = ColorSpace::SRGB;
    uint32_t hex = 0;
    if (value.length() == 4) {
      char r = value[1], g = value[2], b = value[3];
      char expanded[7] = {r, r, g, g, b, b, '\0'};
      hex = std::strtoul(expanded, nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    }
    if (value.length() == 5) {
      char r = value[1], g = value[2], b = value[3], a = value[4];
      char expanded[9] = {r, r, g, g, b, b, a, a, '\0'};
      hex = std::strtoul(expanded, nullptr, 16);
      color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
      return color;
    }
    if (value.length() == 7) {
      hex = std::strtoul(value.c_str() + 1, nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    }
    if (value.length() == 9) {
      hex = std::strtoul(value.c_str() + 1, nullptr, 16);
      color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
      return color;
    }
  }
  if (value.compare(0, 3, "rgb") == 0) {
    auto open = value.find('(');
    auto close = value.find(')');
    if (open != std::string::npos && close != std::string::npos) {
      std::string inner = value.substr(open + 1, close - open - 1);
      auto comps = ParseFloatList(inner);
      Color color = {};
      color.colorSpace = ColorSpace::SRGB;
      float r = 0, g = 0, b = 0, a = 1.0f;
      if (comps.size() >= 3) {
        r = comps[0];
        g = comps[1];
        b = comps[2];
        if (comps.size() >= 4) a = comps[3];
      }
      color.red = r / 255.0f;
      color.green = g / 255.0f;
      color.blue = b / 255.0f;
      color.alpha = a;
      return color;
    }
  }
  // Named color
  std::string lowered = ToLower(value);
  const auto& named = NamedColors();
  auto it = named.find(lowered);
  if (it != named.end()) {
    uint32_t hex = it->second;
    Color color = {};
    color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
    color.alpha = 1.0f;
    color.colorSpace = ColorSpace::SRGB;
    return color;
  }
  return {0, 0, 0, 1, ColorSpace::SRGB};
}

float HTMLParserContext::parseLength(const std::string& valueRaw, float containerSize,
                                     bool* outIsPercent) {
  std::string value = Trim(valueRaw);
  if (outIsPercent) *outIsPercent = false;
  if (value.empty()) return 0.0f;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return 0.0f;
  std::string suffix = Trim(end);
  if (suffix.empty()) return num;
  if (suffix == "px") return num;
  if (suffix == "%") {
    if (outIsPercent) *outIsPercent = true;
    return containerSize == 0 ? num : (num / 100.0f * containerSize);
  }
  if (suffix == "em" || suffix == "rem") {
    warn("html: em/rem unit not supported; treated as 16px");
    return num * 16.0f;
  }
  warn("html: length unit '" + suffix + "' not supported; treated as px");
  return num;
}

float HTMLParserContext::parsePxLength(const std::string& valueRaw) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return NAN;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return NAN;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "px") return num;
  if (suffix == "%") {
    return NAN;  // percent not allowed for properties parsed via parsePxLength
  }
  if (suffix == "em" || suffix == "rem") {
    warn("html: em/rem unit not supported; treated as 16px");
    return num * 16.0f;
  }
  warn("html: length unit '" + suffix + "' not supported; treated as px");
  return num;
}

std::vector<HTMLParserContext::ShadowSpec> HTMLParserContext::parseShadowList(
    const std::string& value) {
  std::vector<ShadowSpec> out;
  if (value.empty()) return out;
  auto items = SplitTopLevelCommas(value);
  for (auto& item : items) {
    auto tokens = SplitTopLevelWhitespace(item);
    if (tokens.empty()) continue;
    ShadowSpec s;
    std::vector<float> lengths;
    std::vector<std::string> nonLengths;
    for (auto& t : tokens) {
      std::string lt = ToLower(t);
      if (lt == "inset") {
        s.inset = true;
        continue;
      }
      // try as length
      char* end = nullptr;
      float num = std::strtof(t.c_str(), &end);
      if (end != t.c_str()) {
        std::string suffix = Trim(end);
        if (suffix.empty() || suffix == "px") {
          lengths.push_back(num);
          continue;
        }
      }
      nonLengths.push_back(t);
    }
    if (lengths.size() >= 2) {
      s.offsetX = lengths[0];
      s.offsetY = lengths[1];
      if (lengths.size() >= 3) s.blur = lengths[2];
      if (lengths.size() >= 4) {
        s.spread = lengths[3];
        if (s.spread != 0) warn("html: box-shadow spread is not supported and was ignored");
      }
    } else {
      warn("html: malformed box-shadow '" + item + "'");
      continue;
    }
    if (!nonLengths.empty()) {
      // Join color tokens back (handles rgb(...) etc.).
      std::string colorStr;
      for (size_t i = 0; i < nonLengths.size(); i++) {
        if (i) colorStr.push_back(' ');
        colorStr += nonLengths[i];
      }
      s.color = parseColor(colorStr);
    } else {
      s.color = {0, 0, 0, 1.0f, ColorSpace::SRGB};
    }
    out.push_back(s);
  }
  return out;
}

std::vector<HTMLParserContext::FilterStep> HTMLParserContext::parseFilterChain(
    const std::string& value) {
  std::vector<FilterStep> out;
  if (value.empty()) return out;
  size_t pos = 0;
  while (pos < value.size()) {
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) pos++;
    if (pos >= value.size()) break;
    size_t start = pos;
    size_t paren = value.find('(', pos);
    if (paren == std::string::npos) break;
    std::string name = ToLower(Trim(value.substr(start, paren - start)));
    int depth = 1;
    size_t end = paren + 1;
    while (end < value.size() && depth > 0) {
      if (value[end] == '(') depth++;
      else if (value[end] == ')') depth--;
      if (depth > 0) end++;
    }
    if (end >= value.size()) break;
    std::string args = value.substr(paren + 1, end - paren - 1);
    FilterStep step;
    step.raw = value.substr(start, end - start + 1);
    if (name == "blur") {
      float b = parsePxLength(args);
      step.kind = FilterStep::Kind::Blur;
      step.blurX = std::isnan(b) ? 0 : b;
      step.blurY = step.blurX;
    } else if (name == "drop-shadow") {
      step.kind = FilterStep::Kind::DropShadow;
      auto shadows = parseShadowList(args);
      if (!shadows.empty()) {
        step.shadow = shadows.front();
      }
    } else {
      step.kind = FilterStep::Kind::Unsupported;
    }
    out.push_back(step);
    pos = end + 1;
  }
  return out;
}

namespace {

// Convert CSS keyword direction ("to bottom right", etc.) to a CSS angle in degrees.
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

// Pull the bracketed args of a function-like CSS value: "linear-gradient(a, b, c)" -> "a, b, c".
std::string ExtractParenArgs(const std::string& value) {
  auto open = value.find('(');
  auto close = value.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open) return {};
  return value.substr(open + 1, close - open - 1);
}

}  // namespace

LinearGradient* HTMLParserContext::parseLinearGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  float cssAngle = 180.0f;  // CSS default: to bottom
  size_t stopStart = 0;
  std::string first = Trim(parts[0]);
  std::string firstLower = ToLower(first);
  if (firstLower.compare(0, 3, "to ") == 0) {
    cssAngle = CssDirectionToAngle(firstLower);
    stopStart = 1;
  } else if (firstLower.find("deg") != std::string::npos ||
             firstLower.find("rad") != std::string::npos ||
             firstLower.find("turn") != std::string::npos) {
    cssAngle = ParseAngle(first);
    stopStart = 1;
  }
  auto grad = _document->makeNode<LinearGradient>();
  float angle = CssToPagxAngle(cssAngle) * 3.14159265358979323846f / 180.0f;
  float cx = 0.5f, cy = 0.5f;
  float half = 0.5f;
  grad->startPoint = {cx - std::cos(angle) * half, cy - std::sin(angle) * half};
  grad->endPoint = {cx + std::cos(angle) * half, cy + std::sin(angle) * half};

  std::vector<std::pair<float, Color>> stops;
  std::vector<size_t> missingOffsetIdx;
  for (size_t i = stopStart; i < parts.size(); i++) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    float offset = NAN;
    if (tokens.size() >= 2) {
      const std::string& off = tokens[1];
      if (!off.empty() && off.back() == '%') {
        offset = std::strtof(off.c_str(), nullptr) / 100.0f;
      } else {
        float v = parsePxLength(off);
        if (!std::isnan(v)) offset = v;
      }
    }
    if (std::isnan(offset)) missingOffsetIdx.push_back(stops.size());
    stops.emplace_back(offset, color);
  }
  if (stops.empty()) return nullptr;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  for (size_t i = 1; i + 1 < stops.size(); i++) {
    if (std::isnan(stops[i].first)) {
      // linear interpolation between bracketing known stops.
      size_t prev = i - 1;
      size_t next = i + 1;
      while (next < stops.size() && std::isnan(stops[next].first)) next++;
      if (next < stops.size()) {
        float span = stops[next].first - stops[prev].first;
        float steps = static_cast<float>(next - prev);
        stops[i].first = stops[prev].first + span * 1.0f / steps;
      } else {
        stops[i].first = 1.0f;
      }
    }
  }
  for (auto& [offset, color] : stops) {
    auto stop = _document->makeNode<ColorStop>();
    stop->offset = offset;
    stop->color = color;
    grad->colorStops.push_back(stop);
  }
  return grad;
}

RadialGradient* HTMLParserContext::parseRadialGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  size_t stopStart = 0;
  // Allow leading shape descriptor like "circle at center", "ellipse 50% 50%", etc.
  std::string first = ToLower(Trim(parts[0]));
  if (first.find("circle") != std::string::npos || first.find("ellipse") != std::string::npos ||
      first.find("at") != std::string::npos) {
    stopStart = 1;
  }
  auto grad = _document->makeNode<RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  std::vector<std::pair<float, Color>> stops;
  for (size_t i = stopStart; i < parts.size(); i++) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    float offset = NAN;
    if (tokens.size() >= 2) {
      const std::string& off = tokens[1];
      if (!off.empty() && off.back() == '%') {
        offset = std::strtof(off.c_str(), nullptr) / 100.0f;
      }
    }
    stops.emplace_back(offset, color);
  }
  if (stops.empty()) return nullptr;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  for (size_t i = 1; i + 1 < stops.size(); i++) {
    if (std::isnan(stops[i].first)) {
      stops[i].first = static_cast<float>(i) / static_cast<float>(stops.size() - 1);
    }
  }
  for (auto& [offset, color] : stops) {
    auto stop = _document->makeNode<ColorStop>();
    stop->offset = offset;
    stop->color = color;
    grad->colorStops.push_back(stop);
  }
  return grad;
}

ConicGradient* HTMLParserContext::parseConicGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  size_t stopStart = 0;
  float cssAngle = 0.0f;
  std::string first = ToLower(Trim(parts[0]));
  if (first.compare(0, 5, "from ") == 0) {
    cssAngle = ParseAngle(first.substr(5));
    stopStart = 1;
  }
  auto grad = _document->makeNode<ConicGradient>();
  grad->center = {0.5f, 0.5f};
  grad->startAngle = CssToPagxAngle(cssAngle);
  grad->endAngle = grad->startAngle + 360.0f;
  std::vector<std::pair<float, Color>> stops;
  for (size_t i = stopStart; i < parts.size(); i++) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    float offset = NAN;
    if (tokens.size() >= 2) {
      const std::string& off = tokens[1];
      if (!off.empty() && off.back() == '%') {
        offset = std::strtof(off.c_str(), nullptr) / 100.0f;
      } else if (!off.empty() && off.find("deg") != std::string::npos) {
        offset = ParseAngle(off) / 360.0f;
      }
    }
    stops.emplace_back(offset, color);
  }
  if (stops.empty()) return nullptr;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  for (size_t i = 1; i + 1 < stops.size(); i++) {
    if (std::isnan(stops[i].first)) {
      stops[i].first = static_cast<float>(i) / static_cast<float>(stops.size() - 1);
    }
  }
  for (auto& [offset, color] : stops) {
    auto stop = _document->makeNode<ColorStop>();
    stop->offset = offset;
    stop->color = color;
    grad->colorStops.push_back(stop);
  }
  return grad;
}

//==================================================================================================
// Image resource registration
//==================================================================================================

Image* HTMLParserContext::registerImageResource(const std::string& imageSource) {
  if (imageSource.empty()) return nullptr;
  auto it = _imageSourceToId.find(imageSource);
  if (it != _imageSourceToId.end()) return it->second;
  auto imageNode = _document->makeNode<Image>();
  imageNode->id = generateUniqueId("image");
  imageNode->filePath = imageSource;
  _imageSourceToId[imageSource] = imageNode;
  return imageNode;
}

//==================================================================================================
// Inline SVG serialisation
//==================================================================================================

namespace {

void SerializeNode(std::string& out, const std::shared_ptr<DOMNode>& node) {
  if (!node) return;
  if (node->type == DOMNodeType::Text) {
    out += EscapeXml(node->name, /*isAttribute=*/false);
    return;
  }
  out.push_back('<');
  out += node->name;
  for (const auto& attr : node->attributes) {
    out.push_back(' ');
    out += attr.name;
    out += "=\"";
    out += EscapeXml(attr.value, /*isAttribute=*/true);
    out.push_back('"');
  }
  if (!node->firstChild) {
    out += "/>";
    return;
  }
  out.push_back('>');
  auto child = node->firstChild;
  while (child) {
    SerializeNode(out, child);
    child = child->nextSibling;
  }
  out += "</";
  out += node->name;
  out.push_back('>');
}

}  // namespace

std::string HTMLParserContext::serializeSvg(const std::shared_ptr<DOMNode>& svgNode) {
  std::string out;
  SerializeNode(out, svgNode);
  return out;
}

}  // namespace pagx
