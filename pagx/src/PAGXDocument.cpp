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

#include "pagx/PAGXDocument.h"
#include <fstream>
#include <sstream>
#include "PAGXXMLParser.h"
#include "PAGXXMLWriter.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXDocument::Make(float width, float height) {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->_width = width;
  doc->_height = height;
  doc->_root = PAGXNode::Make(PAGXNodeType::Document);
  doc->_root->setFloatAttribute("width", width);
  doc->_root->setFloatAttribute("height", height);
  doc->_resources = PAGXNode::Make(PAGXNodeType::Resources);
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromFile(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return nullptr;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  auto doc = FromXML(content);
  if (doc) {
    // Extract base path from file path
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      doc->_basePath = filePath.substr(0, lastSlash + 1);
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromXML(const std::string& xmlContent) {
  return FromXML(reinterpret_cast<const uint8_t*>(xmlContent.data()), xmlContent.size());
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromXML(const uint8_t* data, size_t length) {
  return PAGXXMLParser::Parse(data, length);
}

void PAGXDocument::setSize(float width, float height) {
  _width = width;
  _height = height;
  if (_root) {
    _root->setFloatAttribute("width", width);
    _root->setFloatAttribute("height", height);
  }
}

void PAGXDocument::setRoot(std::unique_ptr<PAGXNode> root) {
  _root = std::move(root);
}

std::unique_ptr<PAGXNode> PAGXDocument::createNode(PAGXNodeType type) {
  return PAGXNode::Make(type);
}

PAGXNode* PAGXDocument::getResourceById(const std::string& id) const {
  auto it = _resourceMap.find(id);
  if (it != _resourceMap.end()) {
    return it->second;
  }
  return nullptr;
}

void PAGXDocument::addResource(std::unique_ptr<PAGXNode> resource) {
  if (!resource || resource->id().empty()) {
    return;
  }
  auto id = resource->id();
  auto* rawPtr = resource.get();
  _resources->appendChild(std::move(resource));
  _resourceMap[id] = rawPtr;
}

std::vector<std::string> PAGXDocument::getResourceIds() const {
  std::vector<std::string> ids;
  ids.reserve(_resourceMap.size());
  for (const auto& pair : _resourceMap) {
    ids.push_back(pair.first);
  }
  return ids;
}

std::string PAGXDocument::toXML() const {
  return PAGXXMLWriter::Write(this);
}

bool PAGXDocument::saveToFile(const std::string& filePath) const {
  std::string xml = toXML();
  std::ofstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file.write(xml.data(), static_cast<std::streamsize>(xml.size()));
  return file.good();
}

}  // namespace pagx
