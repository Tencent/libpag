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

#pragma once

#include <cstring>
#include <string>
#include <vector>

namespace pagx {

class HTMLBuilder {
 public:
  explicit HTMLBuilder(int indentSpaces, int initialLevel = 0, size_t reserve = 4096)
      : _level(initialLevel), _spaces(indentSpaces) {
    _buf.reserve(reserve);
  }

  void openTag(const char* tag) {
    indent();
    _buf += '<';
    _buf += tag;
    _tags.push_back(tag);
  }

  void addAttr(const char* name, const std::string& value) {
    if (value.empty()) {
      return;
    }
    _buf += ' ';
    _buf += name;
    _buf += "=\"";
    _buf += EscapeAttr(value);
    _buf += '"';
  }

  void closeTagStart() {
    _buf += '>';
    newline();
    _level++;
  }

  void closeTagSelfClosing() {
    if (_tags.empty()) {
      return;
    }
    auto& tag = _tags.back();
    if (std::strcmp(tag, "div") == 0 || std::strcmp(tag, "span") == 0) {
      _buf += "></";
      _buf += tag;
      _buf += '>';
    } else {
      _buf += "/>";
    }
    newline();
    _tags.pop_back();
  }

  void closeTag() {
    if (_tags.empty()) {
      return;
    }
    _level--;
    indent();
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    newline();
    _tags.pop_back();
  }

  void closeTagWithText(const std::string& text) {
    if (_tags.empty()) {
      return;
    }
    _buf += '>';
    _buf += EscapeHTML(text);
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    newline();
    _tags.pop_back();
  }

  // Like closeTagWithText but converts U+000A (newline, from &#10; in PAGX source) to <br>
  // elements. Use this for TextBox spans where white-space is not `pre`; without the
  // conversion the browser folds newlines into spaces under the default white-space:normal.
  //
  // Both leading and trailing newlines are hoisted OUTSIDE the <span>...</span> so that
  // the <br> elements inherit the container's strut rather than the span's font-size.
  // Returns the number of leading newlines so the caller can emit them before openTag.
  // Usage:
  //   size_t leading = HTMLBuilder::CountLeadingBreaks(text);
  //   out.emitBreaks(leading);
  //   out.openTag("span"); out.addAttr(...);
  //   out.closeTagWithTextBreaks(text);
  static size_t CountLeadingBreaks(const std::string& text) {
    size_t n = 0;
    while (n < text.size() && text[n] == '\n') ++n;
    return n;
  }

  void emitBreaks(size_t count) {
    for (size_t i = 0; i < count; ++i) _buf += "<br>";
  }

  // Counts the trailing newline characters (U+000A) in the text. Mirrors
  // CountLeadingBreaks; used by HTMLWriterLayer to track the previous span's trailing
  // breaks so it can wrap subsequent empty-line `<br>`s in the previous span's font-size.
  static size_t CountTrailingBreaks(const std::string& text) {
    size_t n = 0;
    while (n < text.size() && text[text.size() - 1 - n] == '\n') ++n;
    return n;
  }

  // Appends a raw HTML fragment to the buffer. Used when the caller pre-formats a
  // wrapped element (e.g. an empty-line `<br>` that needs a specific font-size).
  void emitRaw(const std::string& raw) {
    _buf += raw;
  }

  // Closes the current open tag with caller-supplied pre-formatted HTML (raw — not escaped).
  // The caller is responsible for producing well-formed HTML inside content.
  void closeTagWithRawContent(const std::string& rawContent) {
    if (_tags.empty()) {
      return;
    }
    _buf += '>';
    _buf += rawContent;
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    newline();
    _tags.pop_back();
  }

  void closeTagWithTextBreaks(const std::string& text) {
    if (_tags.empty()) {
      return;
    }
    size_t len = text.size();
    // Skip leading newlines — caller must have already emitted them via emitBreaks().
    size_t innerStart = 0;
    while (innerStart < len && text[innerStart] == '\n') ++innerStart;
    // Skip trailing newlines — caller is expected to emit them between spans, where it
    // can wrap empty-line `<br>`s with the appropriate owner font-size (PAGX uses the
    // fontLineHeight of the `\n` that produced each empty line; bare `<br>` inherits the
    // container strut, which can be too small in mixed-size TextBoxes).
    size_t trailingBreaks = 0;
    while (trailingBreaks < len - innerStart && text[len - 1 - trailingBreaks] == '\n') {
      ++trailingBreaks;
    }
    size_t innerLen = len - innerStart - trailingBreaks;
    _buf += '>';
    for (size_t i = 0; i < innerLen; ++i) {
      char c = text[innerStart + i];
      switch (c) {
        case '&':
          _buf += "&amp;";
          break;
        case '<':
          _buf += "&lt;";
          break;
        case '>':
          _buf += "&gt;";
          break;
        case '\n':
          _buf += "<br>";
          break;
        default:
          _buf += c;
          break;
      }
    }
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    newline();
    _tags.pop_back();
  }

  void addRawContent(const std::string& c) {
    _buf += c;
  }

  void addTextContent(const std::string& t) {
    _buf += EscapeHTML(t);
  }

  int level() const {
    return _level;
  }

  std::string release() {
    return std::move(_buf);
  }

 private:
  std::string _buf = {};
  std::vector<const char*> _tags = {};
  int _level = 0;
  int _spaces = 2;

  void indent() {
    if (_level > 0) {
      _buf.append(static_cast<size_t>(_level * _spaces), ' ');
    }
  }

  void newline() {
    _buf += '\n';
  }

  static std::string EscapeAttr(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '&':
          r += "&amp;";
          break;
        case '"':
          r += "&quot;";
          break;
        case '\'':
          r += "&#39;";
          break;
        case '<':
          r += "&lt;";
          break;
        case '>':
          r += "&gt;";
          break;
        default:
          r += c;
      }
    }
    return r;
  }

  static std::string EscapeHTML(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '&':
          r += "&amp;";
          break;
        case '<':
          r += "&lt;";
          break;
        case '>':
          r += "&gt;";
          break;
        default:
          r += c;
      }
    }
    return r;
  }
};

}  // namespace pagx
