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
//  license is distributed on "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the details.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/HTMLStyleExtractor.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pagx {

//==============================================================================
// HTML entity decoding
//==============================================================================

static std::string DecodeHTMLEntities(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  size_t i = 0;
  while (i < str.size()) {
    if (str[i] != '&') {
      result += str[i++];
      continue;
    }
    // Named entities.
    if (str.compare(i, 5, "&amp;") == 0) {
      result += '&';
      i += 5;
    } else if (str.compare(i, 4, "&lt;") == 0) {
      result += '<';
      i += 4;
    } else if (str.compare(i, 4, "&gt;") == 0) {
      result += '>';
      i += 4;
    } else if (str.compare(i, 6, "&quot;") == 0) {
      result += '"';
      i += 6;
    } else if (str.compare(i, 6, "&apos;") == 0) {
      result += '\'';
      i += 6;
    } else if (i + 2 < str.size() && str[i + 1] == '#') {
      // Numeric character reference: &#NNN; or &#xHH;
      size_t end = str.find(';', i);
      if (end == std::string::npos) {
        result += str[i++];
        continue;
      }
      bool hex = (i + 2 < str.size() && (str[i + 2] == 'x' || str[i + 2] == 'X'));
      size_t numStart = hex ? i + 3 : i + 2;
      if (numStart >= end) {
        result += str[i++];
        continue;
      }
      unsigned long codePoint = 0;
      bool valid = true;
      if (hex) {
        for (size_t j = numStart; j < end; j++) {
          char c = str[j];
          if (c >= '0' && c <= '9') {
            codePoint = codePoint * 16 + static_cast<unsigned long>(c - '0');
          } else if (c >= 'a' && c <= 'f') {
            codePoint = codePoint * 16 + static_cast<unsigned long>(c - 'a' + 10);
          } else if (c >= 'A' && c <= 'F') {
            codePoint = codePoint * 16 + static_cast<unsigned long>(c - 'A' + 10);
          } else {
            valid = false;
            break;
          }
        }
      } else {
        for (size_t j = numStart; j < end; j++) {
          char c = str[j];
          if (c >= '0' && c <= '9') {
            codePoint = codePoint * 10 + static_cast<unsigned long>(c - '0');
          } else {
            valid = false;
            break;
          }
        }
      }
      if (!valid || codePoint > 0x10FFFF) {
        result += str[i++];
        continue;
      }
      // Encode codePoint as UTF-8.
      if (codePoint <= 0x7F) {
        result += static_cast<char>(codePoint);
      } else if (codePoint <= 0x7FF) {
        result += static_cast<char>(0xC0 | (codePoint >> 6));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
      } else if (codePoint <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (codePoint >> 12));
        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
      } else {
        result += static_cast<char>(0xF0 | (codePoint >> 18));
        result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codePoint & 0x3F));
      }
      i = end + 1;
    } else {
      result += str[i++];
    }
  }
  return result;
}

// Re-encode only the entity subset that can break a double-quoted HTML attribute value.
// The inline style values produced by Pass 1.5 come from entity-decoded declarations
// (via DecodeHTMLEntities during parse), so any raw " or & must be escaped before being
// embedded back into a style="..." attribute. Other characters are safe inside a
// double-quoted attribute per the HTML5 parser.
static std::string EncodeAttrValue(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '&') {
      out += "&amp;";
    } else if (c == '"') {
      out += "&quot;";
    } else {
      out += c;
    }
  }
  return out;
}

//==============================================================================
// Tokenizer
//==============================================================================

struct StartTagInfo {
  size_t tagStart = 0;
  size_t tagEnd = 0;
  std::string tagName = {};
  bool hasStyle = false;
  size_t styleAttrStart = 0;  // position of 's' in 'style="..."'
  size_t styleAttrEnd = 0;    // position after closing '"'
  size_t styleValueStart = 0;
  size_t styleValueEnd = 0;
  bool hasClass = false;
  size_t classAttrStart = 0;  // position of 'c' in 'class="..."'
  size_t classAttrEnd = 0;    // position after closing '"'
  size_t classValueStart = 0;
  size_t classValueEnd = 0;
};

static bool IsAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static std::string ToLower(const std::string& s) {
  std::string result = s;
  for (auto& c : result) {
    if (c >= 'A' && c <= 'Z') c += 32;
  }
  return result;
}

static std::vector<StartTagInfo> Tokenize(const std::string& html) {
  std::vector<StartTagInfo> tags;
  size_t pos = 0;
  while (pos < html.size()) {
    if (html[pos] != '<') {
      pos++;
      continue;
    }
    if (pos + 1 >= html.size()) break;
    char next = html[pos + 1];
    // Comment: <!-- ... -->
    if (next == '!' && pos + 3 < html.size() && html[pos + 2] == '-' && html[pos + 3] == '-') {
      size_t end = html.find("-->", pos + 4);
      pos = (end == std::string::npos) ? html.size() : end + 3;
      continue;
    }
    // DOCTYPE or CDATA: <! ... >
    if (next == '!') {
      size_t end = html.find('>', pos + 2);
      pos = (end == std::string::npos) ? html.size() : end + 1;
      continue;
    }
    // Processing instruction: <? ... ?>
    if (next == '?') {
      size_t end = html.find("?>", pos + 2);
      pos = (end == std::string::npos) ? html.size() : end + 2;
      continue;
    }
    // End tag: skip.
    if (next == '/') {
      size_t end = html.find('>', pos + 2);
      pos = (end == std::string::npos) ? html.size() : end + 1;
      continue;
    }
    // Start tag.
    if (!IsAlpha(next)) {
      pos++;
      continue;
    }
    StartTagInfo tag = {};
    tag.tagStart = pos;
    // Read tag name.
    size_t nameStart = pos + 1;
    size_t nameEnd = nameStart;
    while (nameEnd < html.size() && html[nameEnd] != ' ' && html[nameEnd] != '>' &&
           html[nameEnd] != '/' && html[nameEnd] != '\t' && html[nameEnd] != '\n') {
      nameEnd++;
    }
    tag.tagName = ToLower(html.substr(nameStart, nameEnd - nameStart));
    // Scan attributes.
    size_t attrPos = nameEnd;
    while (attrPos < html.size()) {
      while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t' ||
                                       html[attrPos] == '\n' || html[attrPos] == '\r')) {
        attrPos++;
      }
      if (attrPos >= html.size() || html[attrPos] == '>' || html[attrPos] == '/') break;
      // Read attribute name.
      size_t attrNameStart = attrPos;
      while (attrPos < html.size() && html[attrPos] != '=' && html[attrPos] != '>' &&
             html[attrPos] != ' ' && html[attrPos] != '/' && html[attrPos] != '\t') {
        attrPos++;
      }
      std::string attrName = ToLower(html.substr(attrNameStart, attrPos - attrNameStart));
      while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
        attrPos++;
      }
      if (attrPos >= html.size() || html[attrPos] != '=') continue;
      attrPos++;  // skip '='
      while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
        attrPos++;
      }
      if (attrPos >= html.size()) break;
      // Read attribute value.
      if (html[attrPos] == '"') {
        size_t valueStart = attrPos + 1;
        size_t valueEnd = html.find('"', valueStart);
        if (valueEnd == std::string::npos) {
          attrPos = html.size();
          break;
        }
        if (attrName == "style") {
          tag.hasStyle = true;
          tag.styleAttrStart = attrNameStart;
          tag.styleAttrEnd = valueEnd + 1;
          tag.styleValueStart = valueStart;
          tag.styleValueEnd = valueEnd;
        } else if (attrName == "class") {
          tag.hasClass = true;
          tag.classAttrStart = attrNameStart;
          tag.classAttrEnd = valueEnd + 1;
          tag.classValueStart = valueStart;
          tag.classValueEnd = valueEnd;
        }
        attrPos = valueEnd + 1;
      } else {
        while (attrPos < html.size() && html[attrPos] != ' ' && html[attrPos] != '>' &&
               html[attrPos] != '\t') {
          attrPos++;
        }
      }
    }
    // Find tag end.
    while (attrPos < html.size() && html[attrPos] != '>') {
      attrPos++;
    }
    if (attrPos < html.size()) attrPos++;  // skip '>'
    tag.tagEnd = attrPos;
    tags.push_back(tag);
    // Raw text elements: <style> and <script>.
    if (tag.tagName == "style" || tag.tagName == "script") {
      std::string endTag = "</" + tag.tagName + ">";
      size_t rawEnd = html.find(endTag, attrPos);
      if (rawEnd == std::string::npos) {
        pos = html.size();
      } else {
        size_t endClose = html.find('>', rawEnd);
        pos = (endClose == std::string::npos) ? html.size() : endClose + 1;
      }
    } else {
      pos = attrPos;
    }
  }
  return tags;
}

//==============================================================================
// CSS property parsing and classification
//==============================================================================

struct CSSProperty {
  std::string name;
  std::string value;
};

struct StyleEntry {
  int tagIndex = -1;
  std::vector<CSSProperty> properties;
  std::string signature;
  std::string decodedStyle;
};

struct ClassRule {
  std::string className;
  std::string declarations;
  bool isPaired = false;  // emitted by the base+modifier split branch (either role)
  bool isRoot = false;    // document root class; never eligible for inline collapse
};

struct PropertyClassification {
  std::vector<CSSProperty> sharedProps;
  std::vector<std::string> varyingPropNames;
};

static std::string TrimString(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
    start++;
  }
  size_t end = s.size();
  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
    end--;
  }
  return s.substr(start, end - start);
}

static std::vector<CSSProperty> ParseStyleProperties(const std::string& decodedStyle) {
  std::vector<CSSProperty> props;
  int parenDepth = 0;
  size_t segStart = 0;
  for (size_t i = 0; i <= decodedStyle.size(); i++) {
    char c = (i < decodedStyle.size()) ? decodedStyle[i] : ';';
    if (c == '(') {
      parenDepth++;
    } else if (c == ')') {
      if (parenDepth > 0) parenDepth--;
    } else if (c == ';' && parenDepth == 0) {
      auto segment = TrimString(decodedStyle.substr(segStart, i - segStart));
      segStart = i + 1;
      if (segment.empty()) continue;
      size_t colonPos = segment.find(':');
      if (colonPos == std::string::npos) continue;
      CSSProperty prop;
      prop.name = TrimString(segment.substr(0, colonPos));
      prop.value = TrimString(segment.substr(colonPos + 1));
      if (prop.name.empty() || prop.value.empty()) continue;
      props.push_back(prop);
    }
  }
  return props;
}

static std::string BuildPropertySignature(const std::vector<CSSProperty>& props) {
  std::vector<std::string> names;
  names.reserve(props.size());
  for (const auto& p : props) {
    names.push_back(p.name);
  }
  std::sort(names.begin(), names.end());
  std::string sig;
  for (size_t i = 0; i < names.size(); i++) {
    if (i > 0) sig += ';';
    sig += names[i];
  }
  return sig;
}

static PropertyClassification ClassifyGroupProperties(const std::vector<StyleEntry*>& members) {
  PropertyClassification result;
  if (members.empty()) return result;
  const auto& firstProps = members[0]->properties;
  // Build name→value map for each member.
  std::vector<std::unordered_map<std::string, std::string>> memberMaps;
  memberMaps.reserve(members.size());
  for (const auto* m : members) {
    std::unordered_map<std::string, std::string> map;
    for (const auto& p : m->properties) {
      map[p.name] = p.value;
    }
    memberMaps.push_back(map);
  }
  // Classify each property using the first member's property order.
  for (const auto& prop : firstProps) {
    bool allSame = true;
    for (size_t m = 1; m < members.size(); m++) {
      auto it = memberMaps[m].find(prop.name);
      if (it == memberMaps[m].end() || it->second != prop.value) {
        allSame = false;
        break;
      }
    }
    if (allSame) {
      result.sharedProps.push_back(prop);
    } else {
      result.varyingPropNames.push_back(prop.name);
    }
  }
  return result;
}

static bool HasPropNamed(const std::vector<CSSProperty>& props, const std::string& name) {
  for (const auto& p : props) {
    if (p.name == name) return true;
  }
  return false;
}

static bool HasPropValue(const std::vector<CSSProperty>& props, const std::string& name,
                         const std::string& value) {
  for (const auto& p : props) {
    if (p.name == name && p.value == value) return true;
  }
  return false;
}

static bool VectorContains(const std::vector<std::string>& vec, const std::string& value) {
  for (const auto& v : vec) {
    if (v == value) return true;
  }
  return false;
}

// Recognizes the document root div by its style signature emitted by HTMLExporter:
// "position:relative;width:{W}px;height:{H}px;overflow:hidden". The root is unique per document
// and always the first tokenized div with all four properties.
static bool IsRootStyle(const std::vector<CSSProperty>& props) {
  return HasPropValue(props, "position", "relative") && HasPropNamed(props, "width") &&
         HasPropNamed(props, "height") && HasPropValue(props, "overflow", "hidden");
}

// Recognizes layer divs emitted by HTMLWriterLayer. Layer divs carry layout/transform/filter
// styles that fill divs never have. Fill divs use inset:0 to size themselves inside a layer
// parent; presence of inset excludes a div from being a layer.
static bool IsLayerStyle(const std::vector<CSSProperty>& baseProps,
                         const std::vector<CSSProperty>& sampleProps,
                         const std::vector<std::string>& varyingPropNames) {
  bool hasInset = HasPropNamed(baseProps, "inset") || HasPropNamed(sampleProps, "inset");
  // Strong positive signals: properties emitted only on layer divs.
  if (HasPropValue(baseProps, "display", "flex") || HasPropValue(sampleProps, "display", "flex")) {
    return true;
  }
  if (VectorContains(varyingPropNames, "mix-blend-mode")) return true;
  if (HasPropNamed(baseProps, "mix-blend-mode") || HasPropNamed(sampleProps, "mix-blend-mode")) {
    return true;
  }
  if (HasPropNamed(baseProps, "filter") || HasPropNamed(sampleProps, "filter")) return true;
  if (HasPropNamed(baseProps, "opacity") || HasPropNamed(sampleProps, "opacity")) return true;
  if (HasPropNamed(baseProps, "transform-origin") ||
      HasPropNamed(sampleProps, "transform-origin")) {
    return true;
  }
  if (HasPropNamed(baseProps, "flex") || HasPropNamed(sampleProps, "flex")) return true;
  // Positional+size pattern (without inset) marks a layer wrapper.
  bool hasAbsolute = HasPropValue(baseProps, "position", "absolute") ||
                     HasPropValue(sampleProps, "position", "absolute");
  bool hasRelative = HasPropValue(baseProps, "position", "relative") ||
                     HasPropValue(sampleProps, "position", "relative");
  bool hasSize = HasPropNamed(baseProps, "width") || HasPropNamed(sampleProps, "width") ||
                 HasPropNamed(baseProps, "height") || HasPropNamed(sampleProps, "height");
  if (!hasInset && hasSize && (hasAbsolute || hasRelative)) return true;
  return false;
}

static std::string InferSemanticPrefix(const std::string& tagName,
                                       const std::vector<CSSProperty>& baseProps,
                                       const std::vector<CSSProperty>& sampleProps,
                                       const std::vector<std::string>& varyingPropNames) {
  if (tagName == "span") return "text";
  if (tagName == "svg") return "svg";
  if (tagName == "div" && IsLayerStyle(baseProps, sampleProps, varyingPropNames)) {
    if (VectorContains(varyingPropNames, "mix-blend-mode")) return "blend";
    if (VectorContains(varyingPropNames, "opacity")) return "alpha";
    if (VectorContains(varyingPropNames, "background-color")) return "bg";
    if (HasPropValue(baseProps, "display", "flex") ||
        HasPropValue(sampleProps, "display", "flex")) {
      return "flex";
    }
    return "layer";
  }
  return "div";
}

static std::string InferStandalonePrefix(const std::string& tagName,
                                         const std::vector<CSSProperty>& props) {
  if (tagName == "span") return "text";
  if (tagName == "svg") return "svg";
  std::vector<std::string> emptyVarying;
  if (tagName == "div" && IsLayerStyle(props, props, emptyVarying)) return "layer";
  return "div";
}

// Maps a CSS property name to a sort key grouping it with semantically related properties.
// Professional front-end convention orders declarations by role: position → layout → box →
// transform → visual → background → border → clipping → typography. Unknown names receive 999
// so they're appended at the end in source order (via stable_sort). Vendor-prefixed variants
// sit next to their standard counterpart (e.g., backdrop-filter 55 then -webkit-backdrop-filter 56).
static int CSSPropertyOrder(const std::string& name) {
  static const std::unordered_map<std::string, int> ORDER = {
      // Position & flow
      {"position", 10},
      {"inset", 11},
      {"top", 12},
      {"right", 13},
      {"bottom", 14},
      {"left", 15},
      {"z-index", 16},
      // Display & layout
      {"display", 20},
      {"flex", 21},
      {"flex-direction", 22},
      {"flex-wrap", 23},
      {"justify-content", 24},
      {"align-items", 25},
      {"align-content", 26},
      {"align-self", 27},
      {"gap", 28},
      {"order", 29},
      // Box model
      {"box-sizing", 30},
      {"width", 31},
      {"height", 32},
      {"min-width", 33},
      {"max-width", 34},
      {"min-height", 35},
      {"max-height", 36},
      {"padding", 37},
      {"margin", 38},
      // Transform
      {"transform", 40},
      {"transform-origin", 41},
      {"transform-style", 42},
      {"perspective", 43},
      // Visual effects
      {"opacity", 50},
      {"visibility", 51},
      {"mix-blend-mode", 52},
      {"isolation", 53},
      {"filter", 54},
      {"backdrop-filter", 55},
      {"-webkit-backdrop-filter", 56},
      // Background
      {"background", 60},
      {"background-color", 61},
      {"background-image", 62},
      {"background-position", 63},
      {"background-repeat", 64},
      {"background-clip", 65},
      {"-webkit-background-clip", 66},
      // Border & shadow
      {"border-radius", 70},
      {"box-shadow", 71},
      // Clipping & masking
      {"overflow", 80},
      {"clip-path", 81},
      {"mask-image", 82},
      {"mask-mode", 83},
      {"mask-repeat", 84},
      {"-webkit-mask-image", 85},
      {"-webkit-mask-mode", 86},
      {"-webkit-mask-repeat", 87},
      // Typography
      {"color", 90},
      {"font-family", 91},
      {"font-size", 92},
      {"font-style", 93},
      {"font-weight", 94},
      {"line-height", 95},
      {"text-align", 96},
      {"letter-spacing", 97},
      {"white-space", 98},
      {"word-wrap", 99},
      {"writing-mode", 100},
      {"text-stroke", 101},
      {"-webkit-text-stroke", 102},
      {"-webkit-text-fill-color", 103},
      {"paint-order", 104},
      {"shape-rendering", 105},
      {"image-rendering", 106},
  };
  auto it = ORDER.find(name);
  return it != ORDER.end() ? it->second : 999;
}

static bool CompareCSSProperty(const CSSProperty& a, const CSSProperty& b) {
  return CSSPropertyOrder(a.name) < CSSPropertyOrder(b.name);
}

namespace {
struct GroupInfo {
  std::string signature;
  int firstEntryIndex;
  int firstTagIndex;
};
}  // namespace

static bool CompareGroupInfo(const GroupInfo& a, const GroupInfo& b) {
  return a.firstTagIndex < b.firstTagIndex;
}

static std::string BuildDeclarationsString(const std::vector<CSSProperty>& props) {
  std::vector<CSSProperty> sorted(props);
  std::stable_sort(sorted.begin(), sorted.end(), CompareCSSProperty);
  std::string result;
  for (size_t i = 0; i < sorted.size(); i++) {
    if (i > 0) result += ';';
    result += sorted[i].name + ':' + sorted[i].value;
  }
  return result;
}

// Returns true when `pos` falls inside any of the half-open [start,end) byte ranges. Used to
// filter out tags nested inside <foreignObject> elements during style extraction.
static bool IsPosInsideRange(size_t pos, const std::vector<std::pair<size_t, size_t>>& ranges) {
  for (const auto& r : ranges) {
    if (pos >= r.first && pos < r.second) return true;
  }
  return false;
}

//==============================================================================
// Extract
//==============================================================================

std::string HTMLStyleExtractor::Extract(const std::string& html) {
  if (html.empty()) return html;
  auto tags = Tokenize(html);

  // Pre-build a set of positions that lie inside <foreignObject>…</foreignObject> blocks.
  // Elements nested inside foreignObject are HTML-in-SVG; they cannot access the document's
  // <style> block, so their inline styles must NOT be extracted into CSS classes.
  // We collect the [start,end) byte ranges of every foreignObject element and test each tag's
  // tagStart against those ranges.
  std::vector<std::pair<size_t, size_t>> foreignObjectRanges;
  {
    std::vector<size_t> foOpenStack;
    size_t pos = 0;
    while (pos < html.size()) {
      size_t lt = html.find('<', pos);
      if (lt == std::string::npos) break;
      if (lt + 1 < html.size() && html[lt + 1] == '/') {
        // Closing tag: check if it's </foreignObject>
        size_t nameStart = lt + 2;
        size_t nameEnd = nameStart;
        while (nameEnd < html.size() && html[nameEnd] != '>' && html[nameEnd] != ' ') {
          nameEnd++;
        }
        std::string closeName = html.substr(nameStart, nameEnd - nameStart);
        // case-insensitive compare
        for (auto& ch : closeName)
          ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (closeName == "foreignobject" && !foOpenStack.empty()) {
          size_t openPos = foOpenStack.back();
          foOpenStack.pop_back();
          size_t closeEnd = html.find('>', nameEnd);
          if (closeEnd != std::string::npos) {
            foreignObjectRanges.push_back({openPos, closeEnd + 1});
          }
        }
        pos = nameEnd + 1;
      } else if (lt + 14 <= html.size()) {
        // Opening tag: check for <foreignObject
        std::string prefix = html.substr(lt + 1, 13);
        for (auto& ch : prefix)
          ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (prefix == "foreignobject" && (lt + 14 >= html.size() || html[lt + 14] == ' ' ||
                                          html[lt + 14] == '>' || html[lt + 14] == '\n')) {
          foOpenStack.push_back(lt);
        }
        pos = lt + 1;
      } else {
        pos = lt + 1;
      }
    }
  }

  // Pass 1: parse style values into StyleEntry structures.
  std::vector<StyleEntry> entries;
  // Map from tag index to entry index (-1 means no entry).
  std::vector<int> tagToEntry(tags.size(), -1);
  for (size_t ti = 0; ti < tags.size(); ti++) {
    const auto& tag = tags[ti];
    if (!tag.hasStyle || tag.tagName == "body") continue;
    // Do not extract styles from elements inside <foreignObject>: those elements live in a
    // separate HTML document context where the outer <style> block is not accessible, so
    // replacing their inline style with a CSS class name would leave them unstyled.
    if (IsPosInsideRange(tag.tagStart, foreignObjectRanges)) continue;
    auto rawValue = html.substr(tag.styleValueStart, tag.styleValueEnd - tag.styleValueStart);
    if (rawValue.empty()) continue;
    StyleEntry entry;
    entry.tagIndex = static_cast<int>(ti);
    entry.decodedStyle = DecodeHTMLEntities(rawValue);
    entry.properties = ParseStyleProperties(entry.decodedStyle);
    entry.signature = BuildPropertySignature(entry.properties);
    tagToEntry[ti] = static_cast<int>(entries.size());
    entries.push_back(entry);
  }
  if (entries.empty()) return html;

  // Intermediate: group by signature, classify, and assign class names.
  std::unordered_map<std::string, std::vector<int>> sigGroups;
  for (size_t ei = 0; ei < entries.size(); ei++) {
    sigGroups[entries[ei].signature].push_back(static_cast<int>(ei));
  }

  // Sort groups by first entry's tag position to maintain first-seen order.
  std::vector<GroupInfo> sortedGroups;
  for (const auto& pair : sigGroups) {
    int firstEntry = pair.second[0];
    sortedGroups.push_back({pair.first, firstEntry, entries[firstEntry].tagIndex});
  }
  std::sort(sortedGroups.begin(), sortedGroups.end(), CompareGroupInfo);

  // Per-tag class name list (may be 1 or 2 names for base+modifier).
  std::vector<std::vector<std::string>> tagClassNames(tags.size());
  // Ordered CSS rules for the <style> block.
  std::vector<ClassRule> classRules;
  // Per-prefix sequential counter.
  std::unordered_map<std::string, int> prefixCounters;
  // Track whether the document root div has been assigned; only the first matching div wins.
  bool rootAssigned = false;

  for (const auto& group : sortedGroups) {
    const auto& memberIndices = sigGroups[group.signature];
    int groupSize = static_cast<int>(memberIndices.size());

    // Build member pointers for classification.
    std::vector<StyleEntry*> members;
    members.reserve(memberIndices.size());
    for (int ei : memberIndices) {
      members.push_back(&entries[ei]);
    }

    auto classification = ClassifyGroupProperties(members);
    // Split into base+modifier whenever at least two shared declarations can be hoisted out.
    // The previous upper bound of 2 varying properties was too tight for text outputs where
    // per-character spans typically vary by 3 properties (top/left/transform along a path).
    // For a group of N members with S shared and V varying decls, splitting saves (N-1)*S
    // declarations; once V <= 3 the per-modifier overhead never outweighs that gain on real
    // documents.
    bool shouldSplit = groupSize >= 2 && classification.sharedProps.size() >= 2 &&
                       classification.varyingPropNames.size() >= 1 &&
                       classification.varyingPropNames.size() <= 3;

    // Never split when `background` (shorthand) is a varying prop AND `background-clip`
    // is a shared prop. CSS background shorthand implicitly resets background-clip to its
    // initial value (border-box), which would silently undo the shared background-clip:text
    // set in the base class and make gradient text render as a solid rectangle.
    if (shouldSplit && VectorContains(classification.varyingPropNames, "background") &&
        HasPropValue(classification.sharedProps, "background-clip", "text")) {
      shouldSplit = false;
    }

    if (shouldSplit) {
      // Use first tag for semantic prefix inference.
      const auto& firstTag = tags[members[0]->tagIndex];
      auto prefix = InferSemanticPrefix(firstTag.tagName, classification.sharedProps,
                                        members[0]->properties, classification.varyingPropNames);
      // Emit base class.
      auto baseName = prefix + std::to_string(prefixCounters[prefix]++);
      classRules.push_back({baseName, BuildDeclarationsString(classification.sharedProps),
                            /*isPaired=*/true,
                            /*isRoot=*/false});

      // Emit modifier classes (dedup by varying-value key within this group).
      std::unordered_map<std::string, std::string> varyingKeyToModifierName;
      for (int mi = 0; mi < groupSize; mi++) {
        const auto& entry = *members[mi];
        // Build varying-value key.
        std::string varyingKey;
        for (size_t vi = 0; vi < classification.varyingPropNames.size(); vi++) {
          if (vi > 0) varyingKey += ';';
          for (const auto& p : entry.properties) {
            if (p.name == classification.varyingPropNames[vi]) {
              varyingKey += p.value;
              break;
            }
          }
        }
        auto it = varyingKeyToModifierName.find(varyingKey);
        std::string modName;
        if (it != varyingKeyToModifierName.end()) {
          modName = it->second;
        } else {
          // Collect the varying properties for this member.
          std::vector<CSSProperty> varyingProps;
          for (const auto& vname : classification.varyingPropNames) {
            for (const auto& p : entry.properties) {
              if (p.name == vname) {
                varyingProps.push_back(p);
                break;
              }
            }
          }
          modName = prefix + std::to_string(prefixCounters[prefix]++);
          classRules.push_back({modName, BuildDeclarationsString(varyingProps),
                                /*isPaired=*/true, /*isRoot=*/false});
          varyingKeyToModifierName[varyingKey] = modName;
        }
        tagClassNames[entry.tagIndex] = {baseName, modName};
      }
    } else {
      // Fallback: exact-string dedup within the group.
      std::unordered_map<std::string, std::string> styleToClassName;
      for (int mi = 0; mi < groupSize; mi++) {
        const auto& entry = *members[mi];
        auto it = styleToClassName.find(entry.decodedStyle);
        if (it != styleToClassName.end()) {
          tagClassNames[entry.tagIndex] = {it->second};
        } else {
          const auto& tag = tags[entry.tagIndex];
          std::string prefix;
          if (!rootAssigned && tag.tagName == "div" && IsRootStyle(entry.properties)) {
            prefix = "root";
            rootAssigned = true;
          } else {
            prefix = InferStandalonePrefix(tag.tagName, entry.properties);
          }
          auto className = prefix + std::to_string(prefixCounters[prefix]++);
          // Normalize declaration order for professional front-end readability.
          // dedup key (styleToClassName) continues to use decodedStyle: two tags with the
          // same source style trivially map to the same sorted output, so the cache works.
          classRules.push_back({className, BuildDeclarationsString(entry.properties),
                                /*isPaired=*/false, /*isRoot=*/(prefix == "root")});
          styleToClassName[entry.decodedStyle] = className;
          tagClassNames[entry.tagIndex] = {className};
        }
      }
    }
  }

  // Pass 1.5: inline single-use standalone classes back into style="..." attributes.
  // When a generated class is referenced by exactly one tag and is not part of a
  // base+modifier pair and is not the document root, the class form costs more bytes
  // (rule header + class="name") than the equivalent inline style="..."; restore the
  // latter. Keeping root and paired members preserves base-class sharing and the
  // externally-visible root selector.
  std::vector<std::string> inlineStyle(tags.size());
  std::vector<bool> hasInlineStyle(tags.size(), false);
  if (!classRules.empty()) {
    std::unordered_map<std::string, int> classRefCount;
    for (const auto& names : tagClassNames) {
      for (const auto& n : names) classRefCount[n]++;
    }
    std::unordered_map<std::string, size_t> classIndex;
    for (size_t ri = 0; ri < classRules.size(); ri++) {
      classIndex[classRules[ri].className] = ri;
    }
    std::unordered_set<std::string> droppedClasses;
    for (size_t ti = 0; ti < tags.size(); ti++) {
      if (tagClassNames[ti].size() != 1) continue;
      const auto& name = tagClassNames[ti][0];
      auto it = classIndex.find(name);
      if (it == classIndex.end()) continue;
      const auto& rule = classRules[it->second];
      if (rule.isRoot) continue;
      if (rule.isPaired) continue;
      if (classRefCount[name] != 1) continue;
      inlineStyle[ti] = rule.declarations;  // already in canonical property order
      hasInlineStyle[ti] = true;
      tagClassNames[ti].clear();
      droppedClasses.insert(name);
    }
    if (!droppedClasses.empty()) {
      std::vector<ClassRule> kept;
      kept.reserve(classRules.size() - droppedClasses.size());
      for (auto& r : classRules) {
        if (!droppedClasses.count(r.className)) kept.push_back(std::move(r));
      }
      classRules = std::move(kept);
    }
  }

  // Pass 2: rebuild output HTML with class attributes replacing style attributes.
  std::string output;
  output.reserve(html.size());
  size_t prev = 0;
  for (size_t ti = 0; ti < tags.size(); ti++) {
    const auto& tag = tags[ti];
    if (tagToEntry[ti] < 0) continue;
    const auto& classNames = tagClassNames[ti];
    bool hasInline = hasInlineStyle[ti];
    if (classNames.empty() && !hasInline) continue;

    // Copy everything from prev to the start of this tag.
    output.append(html, prev, tag.tagStart - prev);

    // Find the end of the tag name.
    size_t nameEnd = tag.tagStart + 1;
    while (nameEnd < html.size() && html[nameEnd] != ' ' && html[nameEnd] != '>' &&
           html[nameEnd] != '/' && html[nameEnd] != '\t' && html[nameEnd] != '\n') {
      nameEnd++;
    }
    // Copy '<tagName'.
    output.append(html, tag.tagStart, nameEnd - tag.tagStart);

    // Build the class value string. Pre-existing source class (e.g. class="pagx-group"
    // emitted by HTMLWriterText) is preserved verbatim; extractor-assigned class names
    // are appended after it. When Pass 1.5 inlined the tag's only extractor class,
    // classNames is empty — then we only keep whatever the source already had.
    std::string classValue;
    if (tag.hasClass) {
      classValue = html.substr(tag.classValueStart, tag.classValueEnd - tag.classValueStart);
    }
    for (const auto& cn : classNames) {
      if (!classValue.empty()) classValue += " ";
      classValue += cn;
    }
    if (!classValue.empty()) {
      output += " class=\"";
      output += classValue;
      output += "\"";
    }
    if (hasInline) {
      output += " style=\"";
      output += EncodeAttrValue(inlineStyle[ti]);
      output += "\"";
    }

    // Re-emit all other attributes (skip class and style).
    size_t attrPos = nameEnd;
    while (attrPos < html.size()) {
      while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t' ||
                                       html[attrPos] == '\n' || html[attrPos] == '\r')) {
        attrPos++;
      }
      if (attrPos >= html.size() || html[attrPos] == '>' || html[attrPos] == '/') break;
      size_t attrNameStart = attrPos;
      while (attrPos < html.size() && html[attrPos] != '=' && html[attrPos] != '>' &&
             html[attrPos] != ' ' && html[attrPos] != '/' && html[attrPos] != '\t') {
        attrPos++;
      }
      std::string attrName = ToLower(html.substr(attrNameStart, attrPos - attrNameStart));
      // Skip class and style — we already emitted class, and style is being extracted.
      if (attrName == "class" || attrName == "style") {
        // Skip to end of attribute value.
        while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
          attrPos++;
        }
        if (attrPos < html.size() && html[attrPos] == '=') {
          attrPos++;
          while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
            attrPos++;
          }
          if (attrPos < html.size() && html[attrPos] == '"') {
            size_t closeQuote = html.find('"', attrPos + 1);
            attrPos = (closeQuote == std::string::npos) ? html.size() : closeQuote + 1;
          } else {
            while (attrPos < html.size() && html[attrPos] != ' ' && html[attrPos] != '>' &&
                   html[attrPos] != '\t') {
              attrPos++;
            }
          }
        }
        continue;
      }
      // Copy this attribute including leading whitespace.
      size_t attrWithWsStart = attrNameStart;
      while (attrWithWsStart > nameEnd &&
             (html[attrWithWsStart - 1] == ' ' || html[attrWithWsStart - 1] == '\t')) {
        attrWithWsStart--;
      }
      // Read the full attribute including value.
      while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
        attrPos++;
      }
      if (attrPos < html.size() && html[attrPos] == '=') {
        attrPos++;
        while (attrPos < html.size() && (html[attrPos] == ' ' || html[attrPos] == '\t')) {
          attrPos++;
        }
        if (attrPos < html.size() && html[attrPos] == '"') {
          size_t closeQuote = html.find('"', attrPos + 1);
          attrPos = (closeQuote == std::string::npos) ? html.size() : closeQuote + 1;
        } else {
          while (attrPos < html.size() && html[attrPos] != ' ' && html[attrPos] != '>' &&
                 html[attrPos] != '\t') {
            attrPos++;
          }
        }
      }
      output.append(html, attrWithWsStart, attrPos - attrWithWsStart);
    }
    // Copy the tag closing (>, />, or whatever remains).
    output.append(html, attrPos, tag.tagEnd - attrPos);
    prev = tag.tagEnd;
  }
  output.append(html, prev, html.size() - prev);

  // Pass 3: insert <style> block before </head>. Skipped entirely when every class
  // was inlined in Pass 1.5, so we never emit an empty <style></style>.
  if (classRules.empty()) {
    return output;
  }
  std::string styleBlock = "<style>\n";
  for (const auto& rule : classRules) {
    styleBlock += "." + rule.className + " {\n";
    auto props = ParseStyleProperties(rule.declarations);
    for (const auto& p : props) {
      styleBlock += "  " + p.name + ": " + p.value + ";\n";
    }
    styleBlock += "}\n";
  }
  styleBlock += "</style>\n";
  size_t headEndPos = output.find("</head>");
  if (headEndPos != std::string::npos) {
    output.insert(headEndPos, styleBlock);
  } else {
    size_t bodyPos = output.find("<body");
    if (bodyPos != std::string::npos) {
      output.insert(bodyPos, styleBlock);
    } else {
      output = styleBlock + output;
    }
  }
  return output;
}

}  // namespace pagx
