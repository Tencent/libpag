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

#include "pagx/PAGXNode.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace pagx {

const char* PAGXNodeTypeName(PAGXNodeType type) {
  switch (type) {
    case PAGXNodeType::Document:
      return "Document";
    case PAGXNodeType::Resources:
      return "Resources";
    case PAGXNodeType::Layer:
      return "Layer";
    case PAGXNodeType::Group:
      return "Group";
    case PAGXNodeType::Rectangle:
      return "Rectangle";
    case PAGXNodeType::Ellipse:
      return "Ellipse";
    case PAGXNodeType::Polystar:
      return "Polystar";
    case PAGXNodeType::Path:
      return "Path";
    case PAGXNodeType::Text:
      return "Text";
    case PAGXNodeType::Fill:
      return "Fill";
    case PAGXNodeType::Stroke:
      return "Stroke";
    case PAGXNodeType::TrimPath:
      return "TrimPath";
    case PAGXNodeType::RoundCorner:
      return "RoundCorner";
    case PAGXNodeType::MergePath:
      return "MergePath";
    case PAGXNodeType::Repeater:
      return "Repeater";
    case PAGXNodeType::SolidColor:
      return "SolidColor";
    case PAGXNodeType::LinearGradient:
      return "LinearGradient";
    case PAGXNodeType::RadialGradient:
      return "RadialGradient";
    case PAGXNodeType::ConicGradient:
      return "ConicGradient";
    case PAGXNodeType::DiamondGradient:
      return "DiamondGradient";
    case PAGXNodeType::ImagePattern:
      return "ImagePattern";
    case PAGXNodeType::BlurFilter:
      return "BlurFilter";
    case PAGXNodeType::DropShadowFilter:
      return "DropShadowFilter";
    case PAGXNodeType::DropShadowStyle:
      return "DropShadowStyle";
    case PAGXNodeType::InnerShadowStyle:
      return "InnerShadowStyle";
    case PAGXNodeType::TextSpan:
      return "TextSpan";
    case PAGXNodeType::Mask:
      return "Mask";
    case PAGXNodeType::ClipPath:
      return "ClipPath";
    case PAGXNodeType::Image:
      return "Image";
    case PAGXNodeType::Unknown:
    default:
      return "Unknown";
  }
}

PAGXNodeType PAGXNodeTypeFromName(const std::string& name) {
  if (name == "Document")
    return PAGXNodeType::Document;
  if (name == "Resources")
    return PAGXNodeType::Resources;
  if (name == "Layer")
    return PAGXNodeType::Layer;
  if (name == "Group")
    return PAGXNodeType::Group;
  if (name == "Rectangle")
    return PAGXNodeType::Rectangle;
  if (name == "Ellipse")
    return PAGXNodeType::Ellipse;
  if (name == "Polystar")
    return PAGXNodeType::Polystar;
  if (name == "Path")
    return PAGXNodeType::Path;
  if (name == "Text")
    return PAGXNodeType::Text;
  if (name == "Fill")
    return PAGXNodeType::Fill;
  if (name == "Stroke")
    return PAGXNodeType::Stroke;
  if (name == "TrimPath")
    return PAGXNodeType::TrimPath;
  if (name == "RoundCorner")
    return PAGXNodeType::RoundCorner;
  if (name == "MergePath")
    return PAGXNodeType::MergePath;
  if (name == "Repeater")
    return PAGXNodeType::Repeater;
  if (name == "SolidColor")
    return PAGXNodeType::SolidColor;
  if (name == "LinearGradient")
    return PAGXNodeType::LinearGradient;
  if (name == "RadialGradient")
    return PAGXNodeType::RadialGradient;
  if (name == "ConicGradient")
    return PAGXNodeType::ConicGradient;
  if (name == "DiamondGradient")
    return PAGXNodeType::DiamondGradient;
  if (name == "ImagePattern")
    return PAGXNodeType::ImagePattern;
  if (name == "BlurFilter")
    return PAGXNodeType::BlurFilter;
  if (name == "DropShadowFilter")
    return PAGXNodeType::DropShadowFilter;
  if (name == "DropShadowStyle")
    return PAGXNodeType::DropShadowStyle;
  if (name == "InnerShadowStyle")
    return PAGXNodeType::InnerShadowStyle;
  if (name == "TextSpan")
    return PAGXNodeType::TextSpan;
  if (name == "Mask")
    return PAGXNodeType::Mask;
  if (name == "ClipPath")
    return PAGXNodeType::ClipPath;
  if (name == "Image")
    return PAGXNodeType::Image;
  return PAGXNodeType::Unknown;
}

std::unique_ptr<PAGXNode> PAGXNode::Make(PAGXNodeType type) {
  return std::unique_ptr<PAGXNode>(new PAGXNode(type));
}

bool PAGXNode::hasAttribute(const std::string& name) const {
  return _attributes.find(name) != _attributes.end();
}

std::string PAGXNode::getAttribute(const std::string& name, const std::string& defaultValue) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    return it->second;
  }
  return defaultValue;
}

float PAGXNode::getFloatAttribute(const std::string& name, float defaultValue) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    return std::stof(it->second);
  }
  return defaultValue;
}

int PAGXNode::getIntAttribute(const std::string& name, int defaultValue) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    return std::stoi(it->second);
  }
  return defaultValue;
}

bool PAGXNode::getBoolAttribute(const std::string& name, bool defaultValue) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    const auto& value = it->second;
    return value == "true" || value == "1";
  }
  return defaultValue;
}

Color PAGXNode::getColorAttribute(const std::string& name, const Color& defaultValue) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    const auto& value = it->second;
    if (value.empty()) {
      return defaultValue;
    }
    // Parse hex color like "#RRGGBB" or "#AARRGGBB"
    if (value[0] == '#' && (value.length() == 7 || value.length() == 9)) {
      uint32_t hex = std::stoul(value.substr(1), nullptr, 16);
      if (value.length() == 7) {
        hex = 0xFF000000 | hex;  // Add full alpha
      }
      return Color::FromHex(hex);
    }
  }
  return defaultValue;
}

Matrix PAGXNode::getMatrixAttribute(const std::string& name) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    const auto& value = it->second;
    // Parse "scaleX skewX transX skewY scaleY transY"
    std::istringstream iss(value);
    Matrix matrix = Matrix::Identity();
    iss >> matrix.scaleX >> matrix.skewX >> matrix.transX >> matrix.skewY >> matrix.scaleY >>
        matrix.transY;
    return matrix;
  }
  return Matrix::Identity();
}

PathData PAGXNode::getPathAttribute(const std::string& name) const {
  auto it = _attributes.find(name);
  if (it != _attributes.end()) {
    return PathData::FromSVGString(it->second);
  }
  return PathData();
}

void PAGXNode::setAttribute(const std::string& name, const std::string& value) {
  _attributes[name] = value;
}

void PAGXNode::setFloatAttribute(const std::string& name, float value) {
  std::ostringstream oss;
  oss << value;
  _attributes[name] = oss.str();
}

void PAGXNode::setIntAttribute(const std::string& name, int value) {
  _attributes[name] = std::to_string(value);
}

void PAGXNode::setBoolAttribute(const std::string& name, bool value) {
  _attributes[name] = value ? "true" : "false";
}

void PAGXNode::setColorAttribute(const std::string& name, const Color& color) {
  uint32_t hex = color.toHex();
  std::ostringstream oss;
  oss << "#" << std::hex << std::uppercase;
  oss.width(8);
  oss.fill('0');
  oss << hex;
  _attributes[name] = oss.str();
}

void PAGXNode::setMatrixAttribute(const std::string& name, const Matrix& matrix) {
  std::ostringstream oss;
  oss << matrix.scaleX << " " << matrix.skewX << " " << matrix.transX << " " << matrix.skewY << " "
      << matrix.scaleY << " " << matrix.transY;
  _attributes[name] = oss.str();
}

void PAGXNode::setPathAttribute(const std::string& name, const PathData& path) {
  _attributes[name] = path.toSVGString();
}

void PAGXNode::removeAttribute(const std::string& name) {
  _attributes.erase(name);
}

std::vector<std::string> PAGXNode::getAttributeNames() const {
  std::vector<std::string> names;
  names.reserve(_attributes.size());
  for (const auto& pair : _attributes) {
    names.push_back(pair.first);
  }
  return names;
}

PAGXNode* PAGXNode::childAt(size_t index) const {
  if (index < _children.size()) {
    return _children[index].get();
  }
  return nullptr;
}

void PAGXNode::appendChild(std::unique_ptr<PAGXNode> child) {
  if (child) {
    child->_parent = this;
    _children.push_back(std::move(child));
  }
}

void PAGXNode::insertChild(size_t index, std::unique_ptr<PAGXNode> child) {
  if (child) {
    child->_parent = this;
    if (index >= _children.size()) {
      _children.push_back(std::move(child));
    } else {
      _children.insert(_children.begin() + static_cast<ptrdiff_t>(index), std::move(child));
    }
  }
}

std::unique_ptr<PAGXNode> PAGXNode::removeChild(size_t index) {
  if (index < _children.size()) {
    auto child = std::move(_children[index]);
    _children.erase(_children.begin() + static_cast<ptrdiff_t>(index));
    child->_parent = nullptr;
    return child;
  }
  return nullptr;
}

void PAGXNode::clearChildren() {
  for (auto& child : _children) {
    child->_parent = nullptr;
  }
  _children.clear();
}

PAGXNode* PAGXNode::findById(const std::string& id) {
  if (_id == id) {
    return this;
  }
  for (auto& child : _children) {
    auto found = child->findById(id);
    if (found) {
      return found;
    }
  }
  return nullptr;
}

std::vector<PAGXNode*> PAGXNode::findByType(PAGXNodeType type) {
  std::vector<PAGXNode*> result;
  if (_type == type) {
    result.push_back(this);
  }
  for (auto& child : _children) {
    auto childResults = child->findByType(type);
    result.insert(result.end(), childResults.begin(), childResults.end());
  }
  return result;
}

std::unique_ptr<PAGXNode> PAGXNode::clone() const {
  auto copy = PAGXNode::Make(_type);
  copy->_id = _id;
  copy->_attributes = _attributes;
  for (const auto& child : _children) {
    copy->appendChild(child->clone());
  }
  return copy;
}

}  // namespace pagx
