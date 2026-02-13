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

#include "DOM.h"
#include <stack>
#include <utility>
#include "XMLParser.h"

namespace pagx {

/**
 * XML parser that builds a DOM tree from parsing events.
 */
class DOMParser : public XMLParser {
 public:
  DOMParser() = default;

  std::shared_ptr<DOMNode> getRoot() const {
    return _root;
  }

 protected:
  bool onStartElement(const char* element) override {
    startCommon(element, DOMNodeType::Element);
    return false;
  }

  bool onAddAttribute(const char* name, const char* value) override {
    _attributes.push_back({name, value});
    return false;
  }

  bool onEndElement(const char* /*element*/) override {
    if (_needToFlush) {
      flushAttributes();
    }
    _needToFlush = false;
    --_level;

    _parentStack.pop();
    return false;
  }

  bool onText(const std::string& text) override {
    // Ignore text if it is empty or contains only whitespace.
    if (text.find_first_not_of(" \n\r\t") != std::string::npos) {
      startCommon(text, DOMNodeType::Text);
      onEndElement(_elementName.c_str());
    }
    return false;
  }

 private:
  void flushAttributes() {
    auto node = std::make_shared<DOMNode>();
    node->name = _elementName;
    node->firstChild = nullptr;
    node->attributes.swap(_attributes);
    node->type = _elementType;

    if (_root == nullptr) {
      node->nextSibling = nullptr;
      _root = node;
    } else {
      // Append to the end of the parent's child list (tail insertion).
      auto& parent = _parentStack.top();
      node->nextSibling = nullptr;
      if (parent.lastChild != nullptr) {
        parent.lastChild->nextSibling = node;
      } else {
        parent.node->firstChild = node;
      }
      parent.lastChild = node;
    }
    _parentStack.push({node, nullptr});
    _attributes.clear();
  }

  void startCommon(std::string element, DOMNodeType type) {
    if (_level > 0 && _needToFlush) {
      flushAttributes();
    }
    _needToFlush = true;
    _elementName = std::move(element);
    _elementType = type;
    ++_level;
  }

  struct ParentEntry {
    std::shared_ptr<DOMNode> node = nullptr;
    std::shared_ptr<DOMNode> lastChild = nullptr;
  };

  std::stack<ParentEntry> _parentStack;
  std::shared_ptr<DOMNode> _root = nullptr;
  bool _needToFlush = true;

  std::vector<DOMAttribute> _attributes;
  std::string _elementName;
  DOMNodeType _elementType = DOMNodeType::Element;
  int _level = 0;
};

// ============== DOM ==============

DOM::DOM(std::shared_ptr<DOMNode> root) : _root(std::move(root)) {
}

DOM::~DOM() = default;

std::shared_ptr<DOM> DOM::Make(const uint8_t* data, size_t length) {
  DOMParser parser;
  if (!parser.parse(data, length)) {
    return nullptr;
  }
  auto root = parser.getRoot();
  if (!root) {
    return nullptr;
  }
  return std::shared_ptr<DOM>(new DOM(root));
}

std::shared_ptr<DOM> DOM::MakeFromFile(const std::string& filePath) {
  DOMParser parser;
  if (!parser.parseFile(filePath)) {
    return nullptr;
  }
  auto root = parser.getRoot();
  if (!root) {
    return nullptr;
  }
  return std::shared_ptr<DOM>(new DOM(root));
}

std::shared_ptr<DOMNode> DOM::getRootNode() const {
  return _root;
}

// ============== DOMNode ==============

DOMNode::~DOMNode() {
  // Avoid recursive destruction crash on huge node counts: iteratively unlink children/siblings.
  if (!firstChild && !nextSibling) {
    return;
  }

  std::vector<std::shared_ptr<DOMNode>> stack;
  if (firstChild) {
    stack.emplace_back(std::move(firstChild));
  }
  if (nextSibling) {
    stack.emplace_back(std::move(nextSibling));
  }

  while (!stack.empty()) {
    auto node = std::move(stack.back());
    stack.pop_back();
    if (!node) {
      continue;
    }
    if (node->firstChild) {
      stack.emplace_back(std::move(node->firstChild));
    }
    if (node->nextSibling) {
      stack.emplace_back(std::move(node->nextSibling));
    }
  }
}

std::shared_ptr<DOMNode> DOMNode::getFirstChild(const std::string& name) const {
  auto child = firstChild;
  if (!name.empty()) {
    while (child && child->name != name) {
      child = child->nextSibling;
    }
  }
  return child;
}

std::shared_ptr<DOMNode> DOMNode::getNextSibling(const std::string& name) const {
  auto sibling = nextSibling;
  if (!name.empty()) {
    while (sibling && sibling->name != name) {
      sibling = sibling->nextSibling;
    }
  }
  return sibling;
}

const std::string* DOMNode::findAttribute(const std::string& attrName) const {
  for (const auto& attr : attributes) {
    if (attr.name == attrName) {
      return &attr.value;
    }
  }
  return nullptr;
}

}  // namespace pagx
