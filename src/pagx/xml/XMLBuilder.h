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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace pagx {

/**
 * Unified XML string builder used by PAGX, PPT, and SVG exporters.
 *
 * Supports two output styles controlled by the constructor:
 *  - Compact mode (prettyPrint=false): no indentation or newlines, suitable for OOXML.
 *  - Pretty-print mode (prettyPrint=true): indented output with newlines, suitable for
 *    human-readable XML (PAGX, SVG).
 *
 * All mutating methods return *this to allow chaining.
 */
class XMLBuilder {
 public:
  explicit XMLBuilder(bool prettyPrint = false, int indentSpaces = 2, int initialIndentLevel = 0,
                      size_t reserve = 4096)
      : _prettyPrint(prettyPrint), _indentSpaces(indentSpaces), _indent(initialIndentLevel) {
    _buf.reserve(reserve);
    _tags.reserve(32);
  }

  //--- XML Declaration -------------------------------------------------------

  XMLBuilder& decl(bool standalone = false) {
    _buf += "<?xml version=\"1.0\" encoding=\"UTF-8\"";
    if (standalone) {
      _buf += " standalone=\"yes\"";
    }
    _buf += "?>";
    if (_prettyPrint) {
      _buf += '\n';
    }
    return *this;
  }

  XMLBuilder& appendDeclaration() {
    return decl();
  }

  //--- Open Element ----------------------------------------------------------

  XMLBuilder& open(const char* tag) {
    if (_prettyPrint) {
      writeIndent();
    }
    _buf += '<';
    _buf += tag;
    _tags.push_back(tag);
    return *this;
  }

  XMLBuilder& openElement(const char* tag) {
    return open(tag);
  }

  //--- Always-write Attributes (PPT style) -----------------------------------

  XMLBuilder& attr(const char* name, const char* val) {
    _buf += ' ';
    _buf += name;
    _buf += "=\"";
    escapeAttrTo(_buf, val);
    _buf += '"';
    return *this;
  }

  XMLBuilder& attr(const char* name, const std::string& val) {
    return attr(name, val.c_str());
  }

  XMLBuilder& attr(const char* name, int64_t val) {
    return attr(name, std::to_string(val).c_str());
  }

  //--- Required Attributes (always written) ----------------------------------

  XMLBuilder& addRequiredAttribute(const char* name, const std::string& value) {
    return attr(name, value);
  }

  XMLBuilder& addRequiredAttribute(const char* name, float value) {
    _buf += ' ';
    _buf += name;
    _buf += "=\"";
    _buf += formatFloat(value);
    _buf += '"';
    return *this;
  }

  //--- Conditional Attributes (skip when value == default) -------------------

  XMLBuilder& addAttribute(const char* name, const char* value) {
    if (value && value[0] != '\0') {
      return attr(name, value);
    }
    return *this;
  }

  XMLBuilder& addAttribute(const char* name, const std::string& value) {
    if (!value.empty()) {
      return attr(name, value);
    }
    return *this;
  }

  XMLBuilder& addAttribute(const char* name, float value, float defaultValue = 0) {
    if (value != defaultValue) {
      return addRequiredAttribute(name, value);
    }
    return *this;
  }

  XMLBuilder& addAttribute(const char* name, int value, int defaultValue = 0) {
    if (value != defaultValue) {
      return attr(name, static_cast<int64_t>(value));
    }
    return *this;
  }

  XMLBuilder& addAttribute(const char* name, bool value, bool defaultValue = false) {
    if (value != defaultValue) {
      _buf += ' ';
      _buf += name;
      _buf += "=\"";
      _buf += (value ? "true" : "false");
      _buf += '"';
    }
    return *this;
  }

  XMLBuilder& addAttributeIfNonZero(const char* name, float value,
                                    float tolerance = 1.0f / 4096.0f) {
    if (std::fabs(value) > tolerance) {
      return addRequiredAttribute(name, value);
    }
    return *this;
  }

  //--- Close Start Tag -------------------------------------------------------

  XMLBuilder& gt() {
    _buf += '>';
    if (_prettyPrint) {
      _buf += '\n';
      _indent++;
    }
    return *this;
  }

  XMLBuilder& closeElementStart() {
    return gt();
  }

  //--- Self-Closing ----------------------------------------------------------

  XMLBuilder& sc() {
    _buf += "/>";
    if (_prettyPrint) {
      _buf += '\n';
    }
    _tags.pop_back();
    return *this;
  }

  XMLBuilder& closeElementSelfClosing() {
    return sc();
  }

  //--- Close Element ---------------------------------------------------------

  XMLBuilder& end() {
    if (_prettyPrint) {
      _indent--;
      writeIndent();
    }
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    if (_prettyPrint) {
      _buf += '\n';
    }
    _tags.pop_back();
    return *this;
  }

  XMLBuilder& closeElement() {
    return end();
  }

  //--- Inline Text Element: >text</tag>\n ------------------------------------

  XMLBuilder& closeElementWithText(const std::string& text) {
    _buf += '>';
    escapeTextTo(_buf, text);
    _buf += "</";
    _buf += _tags.back();
    _buf += '>';
    if (_prettyPrint) {
      _buf += '\n';
    }
    _tags.pop_back();
    return *this;
  }

  //--- Text / Raw Content ----------------------------------------------------

  XMLBuilder& text(const std::string& t) {
    escapeTextTo(_buf, t);
    return *this;
  }

  XMLBuilder& addTextContent(const std::string& t) {
    return text(t);
  }

  XMLBuilder& raw(const std::string& s) {
    _buf += s;
    return *this;
  }

  XMLBuilder& addRawContent(const std::string& s) {
    return raw(s);
  }

  //--- Result ----------------------------------------------------------------

  std::string release() {
    return std::move(_buf);
  }

 private:
  std::string _buf;
  std::vector<const char*> _tags;
  bool _prettyPrint;
  int _indentSpaces;
  int _indent = 0;

  void writeIndent() {
    _buf.append(static_cast<size_t>(_indent * _indentSpaces), ' ');
  }

  static void escapeAttrTo(std::string& buf, const char* val) {
    for (const char* p = val; *p; ++p) {
      switch (*p) {
        case '"':
          buf += "&quot;";
          break;
        case '&':
          buf += "&amp;";
          break;
        case '<':
          buf += "&lt;";
          break;
        case '\'':
          buf += "&apos;";
          break;
        case '\n':
          buf += "&#10;";
          break;
        case '\r':
          buf += "&#13;";
          break;
        case '\t':
          buf += "&#9;";
          break;
        default:
          buf += *p;
          break;
      }
    }
  }

  static void escapeTextTo(std::string& buf, const std::string& text) {
    for (char c : text) {
      switch (c) {
        case '&':
          buf += "&amp;";
          break;
        case '<':
          buf += "&lt;";
          break;
        case '>':
          buf += "&gt;";
          break;
        default:
          buf += c;
          break;
      }
    }
  }

  static std::string formatFloat(float value) {
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%g", value);
    return buf;
  }
};

}  // namespace pagx
