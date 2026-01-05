/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "PAGTreeNode.h"
#include <QJsonObject>
#include <QVariant>

namespace pag {

PAGTreeNode::PAGTreeNode(PAGTreeNode* parent) : parent(parent) {
}

PAGTreeNode::~PAGTreeNode() {
  clear();
}

PAGTreeNode* PAGTreeNode::getChild(int row) const {
  if (row < 0 || static_cast<size_t>(row) >= children.size()) {
    return nullptr;
  }
  return children[row].get();
}

PAGTreeNode* PAGTreeNode::getParent() const {
  return parent;
}

int PAGTreeNode::getRow() const {
  if (parent != nullptr) {
    for (size_t i = 0; i < parent->children.size(); ++i) {
      if (parent->children[i].get() == this) {
        return i;
      }
    }
  }
  return 0;
}

int PAGTreeNode::getChildrenCount() const {
  return static_cast<int>(children.size());
}

int PAGTreeNode::getColumnCount() const {
  return 1;
}

QVariant PAGTreeNode::getData(int column) const {
  if (column == 0) {
    QJsonObject json;
    json["name"] = name;
    auto valueStr = value;
    valueStr = valueStr.replace("pag::", "");
    valueStr = valueStr.replace("<", "&lt;").replace(">", "&gt;");
    json["value"] = valueStr;

    if (parent == nullptr) {
      return {json};
    }

    for (int i = 0; i < parent->getChildrenCount(); i++) {
      auto child = parent->getChild(i);
      if (child->name == "id") {
        auto layerID = child->value;
        json["LayerIdKey"] = layerID;
      }
    }

    json["MarkerIndexKey"] = -1;
    if (parent->value == "pag::Marker") {
      auto markerIndex = parent->name;
      json["MarkerIndexKey"] = markerIndex;

      if (parent->parent != nullptr && parent->parent->parent != nullptr) {
        for (int i = 0; i < parent->parent->parent->getChildrenCount(); i++) {
          auto child = parent->parent->parent->getChild(i);
          if (child->name == "id") {
            auto layerID = child->value;
            json["LayerIdKey"] = layerID;
          }
        }
      }
    }

    json["IsEditableKey"] = false;
    if (name == "timeStretchMode") {
      json["IsEditableKey"] = true;
    }

    if (name == "name") {
      if (parent->value.endsWith("Layer")) {
        json["IsEditableKey"] = true;
      }
    }

    if (parent->value == "pag::Marker") {
      if (name == "startTime" || name == "duration" || name == "comment") {
        json["IsEditableKey"] = true;
      }
    }

    return {json};
  }

  return {value};
}

QString PAGTreeNode::getName() const {
  return name;
}

QString PAGTreeNode::getValue() const {
  return value;
}

void PAGTreeNode::setName(QString name) {
  this->name = name;
}

void PAGTreeNode::setValue(QVariant value) {
  this->value = value.toString();
}

void PAGTreeNode::setValue(QString value) {
  this->value = value;
}

void PAGTreeNode::setParent(PAGTreeNode* parent) {
  this->parent = parent;
}

void PAGTreeNode::appendChild(std::unique_ptr<PAGTreeNode> child) {
  children.push_back(std::move(child));
}

void PAGTreeNode::clear() {
  children.clear();
}

}  //  namespace pag
