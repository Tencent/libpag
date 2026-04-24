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
  explicit HTMLBuilder(int indentSpaces, int initialLevel = 0, size_t reserve = 4096,
                       bool minify = false)
      : _level(initialLevel), _spaces(indentSpaces), _minify(minify) {
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
    _buf += escapeAttr(value);
    _buf += '"';
  }

  void closeTagStart() {
    _buf += '>';
    newline();
    _level++;
  }

  void closeTagSelfClosing() {
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
    _level--;
    indent();
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    newline();
    _tags.pop_back();
  }

  void closeTagWithText(const std::string& text) {
    _buf += '>';
    _buf += escapeHTML(text);
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
    _buf += escapeHTML(t);
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
  bool _minify = false;

  void indent() {
    if (_minify) {
      return;
    }
    if (_level > 0) {
      _buf.append(static_cast<size_t>(_level * _spaces), ' ');
    }
  }

  void newline() {
    if (!_minify) {
      _buf += '\n';
    }
  }

  static std::string escapeAttr(const std::string& s) {
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

  static std::string escapeHTML(const std::string& s) {
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
