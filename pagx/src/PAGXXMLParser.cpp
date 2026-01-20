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

#include "PAGXXMLParser.h"
#include <cctype>
#include <cstring>
#include <stack>

namespace pagx {

// Simple XML tokenizer and parser (no external dependencies)
class XMLTokenizer {
 public:
  XMLTokenizer(const char* data, size_t length) : _data(data), _length(length), _pos(0) {
  }

  bool isEnd() const {
    return _pos >= _length;
  }

  void skipWhitespace() {
    while (_pos < _length && std::isspace(_data[_pos])) {
      ++_pos;
    }
  }

  bool match(char c) {
    if (_pos < _length && _data[_pos] == c) {
      ++_pos;
      return true;
    }
    return false;
  }

  bool matchString(const char* str) {
    size_t len = std::strlen(str);
    if (_pos + len <= _length && std::strncmp(_data + _pos, str, len) == 0) {
      _pos += len;
      return true;
    }
    return false;
  }

  std::string readUntil(char delimiter) {
    std::string result;
    while (_pos < _length && _data[_pos] != delimiter) {
      result += _data[_pos++];
    }
    return result;
  }

  std::string readName() {
    std::string result;
    while (_pos < _length &&
           (std::isalnum(_data[_pos]) || _data[_pos] == '_' || _data[_pos] == '-' ||
            _data[_pos] == ':' || _data[_pos] == '.')) {
      result += _data[_pos++];
    }
    return result;
  }

  std::string readQuotedString() {
    char quote = _data[_pos++];
    std::string result;
    while (_pos < _length && _data[_pos] != quote) {
      if (_data[_pos] == '&') {
        result += readEscapeSequence();
      } else {
        result += _data[_pos++];
      }
    }
    if (_pos < _length) {
      ++_pos;  // Skip closing quote
    }
    return result;
  }

  std::string readEscapeSequence() {
    std::string seq;
    ++_pos;  // Skip '&'
    while (_pos < _length && _data[_pos] != ';') {
      seq += _data[_pos++];
    }
    if (_pos < _length) {
      ++_pos;  // Skip ';'
    }
    if (seq == "lt") {
      return "<";
    }
    if (seq == "gt") {
      return ">";
    }
    if (seq == "amp") {
      return "&";
    }
    if (seq == "quot") {
      return "\"";
    }
    if (seq == "apos") {
      return "'";
    }
    if (!seq.empty() && seq[0] == '#') {
      int code = 0;
      if (seq.length() > 1 && seq[1] == 'x') {
        code = std::stoi(seq.substr(2), nullptr, 16);
      } else {
        code = std::stoi(seq.substr(1));
      }
      if (code > 0 && code < 128) {
        return std::string(1, static_cast<char>(code));
      }
    }
    return "&" + seq + ";";
  }

  void skipComment() {
    while (_pos + 2 < _length) {
      if (_data[_pos] == '-' && _data[_pos + 1] == '-' && _data[_pos + 2] == '>') {
        _pos += 3;
        return;
      }
      ++_pos;
    }
  }

  void skipCDATA() {
    while (_pos + 2 < _length) {
      if (_data[_pos] == ']' && _data[_pos + 1] == ']' && _data[_pos + 2] == '>') {
        _pos += 3;
        return;
      }
      ++_pos;
    }
  }

  void skipProcessingInstruction() {
    while (_pos + 1 < _length) {
      if (_data[_pos] == '?' && _data[_pos + 1] == '>') {
        _pos += 2;
        return;
      }
      ++_pos;
    }
  }

 private:
  const char* _data = nullptr;
  size_t _length = 0;
  size_t _pos = 0;
};

static std::unique_ptr<PAGXNode> ParseElement(XMLTokenizer& tokenizer,
                                              std::shared_ptr<PAGXDocument>& doc);

static void ParseAttributes(XMLTokenizer& tokenizer, PAGXNode* node) {
  while (true) {
    tokenizer.skipWhitespace();
    if (tokenizer.isEnd()) {
      break;
    }

    std::string name = tokenizer.readName();
    if (name.empty()) {
      break;
    }

    tokenizer.skipWhitespace();
    if (!tokenizer.match('=')) {
      break;
    }

    tokenizer.skipWhitespace();
    std::string value = tokenizer.readQuotedString();

    if (name == "id") {
      node->setId(value);
    } else {
      node->setAttribute(name, value);
    }
  }
}

static std::unique_ptr<PAGXNode> ParseElement(XMLTokenizer& tokenizer,
                                              std::shared_ptr<PAGXDocument>& doc) {
  tokenizer.skipWhitespace();

  if (tokenizer.isEnd() || !tokenizer.match('<')) {
    return nullptr;
  }

  // Skip comments, CDATA, and processing instructions
  while (true) {
    if (tokenizer.matchString("!--")) {
      tokenizer.skipComment();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("![CDATA[")) {
      tokenizer.skipCDATA();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("?")) {
      tokenizer.skipProcessingInstruction();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("!DOCTYPE")) {
      tokenizer.readUntil('>');
      tokenizer.match('>');
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else {
      break;
    }
  }

  // Check for closing tag
  if (tokenizer.match('/')) {
    return nullptr;
  }

  std::string tagName = tokenizer.readName();
  if (tagName.empty()) {
    return nullptr;
  }

  auto nodeType = PAGXNodeTypeFromName(tagName);
  auto node = PAGXNode::Make(nodeType);

  ParseAttributes(tokenizer, node.get());

  tokenizer.skipWhitespace();

  // Self-closing tag
  if (tokenizer.match('/')) {
    tokenizer.match('>');
    return node;
  }

  if (!tokenizer.match('>')) {
    return node;
  }

  // Parse children
  while (true) {
    tokenizer.skipWhitespace();
    if (tokenizer.isEnd()) {
      break;
    }

    // Check for closing tag
    size_t savedPos = 0;  // Would need to save position for proper lookahead
    auto child = ParseElement(tokenizer, doc);
    if (!child) {
      // Might be a closing tag, try to consume it
      tokenizer.skipWhitespace();
      std::string closeTag = tokenizer.readUntil('>');
      tokenizer.match('>');
      break;
    }

    // Handle Resources node specially
    if (child->type() == PAGXNodeType::Resources && doc) {
      for (size_t i = 0; i < child->childCount(); ++i) {
        auto resourceChild = child->removeChild(0);
        if (resourceChild) {
          doc->addResource(std::move(resourceChild));
        }
      }
    } else {
      node->appendChild(std::move(child));
    }
  }

  return node;
}

std::shared_ptr<PAGXDocument> PAGXXMLParser::Parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }

  XMLTokenizer tokenizer(reinterpret_cast<const char*>(data), length);

  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());

  auto root = ParseElement(tokenizer, doc);
  if (!root) {
    return nullptr;
  }

  // Extract dimensions from root
  doc->_width = root->getFloatAttribute("width", 0);
  doc->_height = root->getFloatAttribute("height", 0);
  doc->_version = root->getAttribute("version", "1.0");

  doc->setRoot(std::move(root));
  return doc;
}

}  // namespace pagx
