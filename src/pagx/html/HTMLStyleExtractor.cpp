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
#include <unordered_map>
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
  int classIndex = -1;
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
// Extract
//==============================================================================

std::string HTMLStyleExtractor::Extract(const std::string& html) {
  if (html.empty()) return html;
  auto tags = Tokenize(html);
  // Pass 1: collect unique style values and assign class indices.
  std::vector<std::string> orderedStyles;
  std::unordered_map<std::string, int> styleToIndex;
  for (auto& tag : tags) {
    if (!tag.hasStyle) continue;
    if (tag.tagName == "body") continue;
    auto value = html.substr(tag.styleValueStart, tag.styleValueEnd - tag.styleValueStart);
    if (value.empty()) continue;
    auto it = styleToIndex.find(value);
    if (it != styleToIndex.end()) {
      tag.classIndex = it->second;
    } else {
      tag.classIndex = static_cast<int>(orderedStyles.size());
      styleToIndex[value] = tag.classIndex;
      orderedStyles.push_back(value);
    }
  }
  if (orderedStyles.empty()) return html;
  // Pass 2: rebuild output. For each tag that needs rewriting, build its new opening tag by
  // iterating through the original attributes, dropping style and inserting/updating class.
  std::string output;
  output.reserve(html.size());
  size_t prev = 0;
  for (auto& tag : tags) {
    if (!tag.hasStyle || tag.tagName == "body" || tag.classIndex < 0) continue;
    auto value = html.substr(tag.styleValueStart, tag.styleValueEnd - tag.styleValueStart);
    if (value.empty()) continue;
    // Copy everything from prev to the start of this tag.
    output.append(html, prev, tag.tagStart - prev);
    std::string className = "ps" + std::to_string(tag.classIndex);
    // Find the end of the tag name.
    size_t nameEnd = tag.tagStart + 1;
    while (nameEnd < html.size() && html[nameEnd] != ' ' && html[nameEnd] != '>' &&
           html[nameEnd] != '/' && html[nameEnd] != '\t' && html[nameEnd] != '\n') {
      nameEnd++;
    }
    // Copy '<tagName'.
    output.append(html, tag.tagStart, nameEnd - tag.tagStart);
    // Emit class attribute.
    if (tag.hasClass) {
      std::string existingClass =
          html.substr(tag.classValueStart, tag.classValueEnd - tag.classValueStart);
      output += " class=\"";
      if (existingClass.empty()) {
        output += className;
      } else {
        output += existingClass;
        output += " ";
        output += className;
      }
      output += "\"";
    } else {
      output += " class=\"";
      output += className;
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
      // Copy this attribute (including any leading whitespace we already skipped).
      // Go back to include the leading whitespace.
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
  // Pass 3: insert <style> block before </head>.
  std::string styleBlock = "<style>\n";
  for (size_t i = 0; i < orderedStyles.size(); i++) {
    styleBlock += ".ps" + std::to_string(i) + "{" + DecodeHTMLEntities(orderedStyles[i]) + "}\n";
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
