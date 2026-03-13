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

#include "PAGXDocumentSerializer.h"
#include <PAGXRttr.hpp>
#include <QString>
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"

namespace pag::PAGXDocumentSerializer {

static rttr::type GetRttrTypeForNodeType(pagx::NodeType nodeType) {
  switch (nodeType) {
    case pagx::NodeType::PathData:
      return rttr::type::get<pagx::PathData>();
    case pagx::NodeType::Image:
      return rttr::type::get<pagx::Image>();
    case pagx::NodeType::Composition:
      return rttr::type::get<pagx::Composition>();
    case pagx::NodeType::SolidColor:
      return rttr::type::get<pagx::SolidColor>();
    case pagx::NodeType::LinearGradient:
      return rttr::type::get<pagx::LinearGradient>();
    case pagx::NodeType::RadialGradient:
      return rttr::type::get<pagx::RadialGradient>();
    case pagx::NodeType::ConicGradient:
      return rttr::type::get<pagx::ConicGradient>();
    case pagx::NodeType::DiamondGradient:
      return rttr::type::get<pagx::DiamondGradient>();
    case pagx::NodeType::ImagePattern:
      return rttr::type::get<pagx::ImagePattern>();
    case pagx::NodeType::ColorStop:
      return rttr::type::get<pagx::ColorStop>();
    case pagx::NodeType::Font:
      return rttr::type::get<pagx::Font>();
    case pagx::NodeType::Glyph:
      return rttr::type::get<pagx::GlyphRun>();
    case pagx::NodeType::Layer:
      return rttr::type::get<pagx::Layer>();
    case pagx::NodeType::DropShadowStyle:
      return rttr::type::get<pagx::DropShadowStyle>();
    case pagx::NodeType::InnerShadowStyle:
      return rttr::type::get<pagx::InnerShadowStyle>();
    case pagx::NodeType::BackgroundBlurStyle:
      return rttr::type::get<pagx::BackgroundBlurStyle>();
    case pagx::NodeType::BlurFilter:
      return rttr::type::get<pagx::BlurFilter>();
    case pagx::NodeType::DropShadowFilter:
      return rttr::type::get<pagx::DropShadowFilter>();
    case pagx::NodeType::InnerShadowFilter:
      return rttr::type::get<pagx::InnerShadowFilter>();
    case pagx::NodeType::BlendFilter:
      return rttr::type::get<pagx::BlendFilter>();
    case pagx::NodeType::ColorMatrixFilter:
      return rttr::type::get<pagx::ColorMatrixFilter>();
    case pagx::NodeType::Rectangle:
      return rttr::type::get<pagx::Rectangle>();
    case pagx::NodeType::Ellipse:
      return rttr::type::get<pagx::Ellipse>();
    case pagx::NodeType::Polystar:
      return rttr::type::get<pagx::Polystar>();
    case pagx::NodeType::Path:
      return rttr::type::get<pagx::Path>();
    case pagx::NodeType::Text:
      return rttr::type::get<pagx::Text>();
    case pagx::NodeType::Fill:
      return rttr::type::get<pagx::Fill>();
    case pagx::NodeType::Stroke:
      return rttr::type::get<pagx::Stroke>();
    case pagx::NodeType::TrimPath:
      return rttr::type::get<pagx::TrimPath>();
    case pagx::NodeType::RoundCorner:
      return rttr::type::get<pagx::RoundCorner>();
    case pagx::NodeType::MergePath:
      return rttr::type::get<pagx::MergePath>();
    case pagx::NodeType::TextModifier:
      return rttr::type::get<pagx::TextModifier>();
    case pagx::NodeType::TextPath:
      return rttr::type::get<pagx::TextPath>();
    case pagx::NodeType::TextBox:
      return rttr::type::get<pagx::TextBox>();
    case pagx::NodeType::Group:
      return rttr::type::get<pagx::Group>();
    case pagx::NodeType::Repeater:
      return rttr::type::get<pagx::Repeater>();
    case pagx::NodeType::RangeSelector:
      return rttr::type::get<pagx::RangeSelector>();
    case pagx::NodeType::GlyphRun:
      return rttr::type::get<pagx::GlyphRun>();
    default:
      return rttr::type::get<pagx::Node>();
  }
}

static rttr::instance GetWrappedInstance(const rttr::instance& item) {
  return item.get_type().get_raw_type().is_wrapper() ? item.get_wrapped_instance() : item;
}

static rttr::type GetWrappedType(const rttr::type& type) {
  return type.is_wrapper() ? type.get_wrapped_type() : type;
}

static void SerializeInstance(const rttr::instance& item, PAGTreeNode* node);
static void SerializeVariant(const rttr::variant& value, PAGTreeNode* node);
static void SerializeSequentialContainer(const rttr::variant_sequential_view& view,
                                         PAGTreeNode* node);

static QString TransformNumberToQString(const rttr::type& type, const rttr::variant& value) {
  if (type == rttr::type::get<bool>()) {
    return value.to_bool() ? "true" : "false";
  }
  if (type == rttr::type::get<char>()) {
    return QString::number(value.to_int8());
  }
  if (type == rttr::type::get<int8_t>()) {
    return QString::number(value.to_int8());
  }
  if (type == rttr::type::get<int16_t>()) {
    return QString::number(value.to_int16());
  }
  if (type == rttr::type::get<int32_t>()) {
    return QString::number(value.to_int32());
  }
  if (type == rttr::type::get<int64_t>()) {
    return QString::number(value.to_int64());
  }
  if (type == rttr::type::get<uint8_t>()) {
    return QString::number(value.to_uint8());
  }
  if (type == rttr::type::get<uint16_t>()) {
    return QString::number(value.to_uint16());
  }
  if (type == rttr::type::get<uint32_t>()) {
    return QString::number(value.to_uint32());
  }
  if (type == rttr::type::get<uint64_t>()) {
    return QString::number(value.to_uint64());
  }
  if (type == rttr::type::get<float>()) {
    return QString::number(static_cast<double>(value.to_float()));
  }
  if (type == rttr::type::get<double>()) {
    return QString::number(value.to_double());
  }
  if (type == rttr::type::get<size_t>()) {
    return QString::number(value.to_uint64());
  }
  return "";
}

static QString TransformEnumToQString(const rttr::variant& value) {
  bool result = false;
  std::string str = value.to_string(&result);
  if (result) {
    return QString::fromStdString(str);
  }

  result = false;
  uint64_t num = value.to_uint64(&result);
  if (result) {
    return QString::fromStdString(std::to_string(num));
  }

  return "<null>";
}

static void SerializeInstance(const rttr::instance& item, PAGTreeNode* node) {
  rttr::instance object = GetWrappedInstance(item);
  auto derivedType = object.get_derived_type();
  node->setValue(derivedType.get_name().data());

  auto properties = derivedType.get_properties();
  for (const auto& property : properties) {
    if (property.get_metadata("NO_SERIALIZE")) {
      continue;
    }

    rttr::variant variant = property.get_value(object);
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(property.get_name().data());
    SerializeVariant(variant, childNode.get());
    node->appendChild(std::move(childNode));
  }
}

static void SerializeVariant(const rttr::variant& value, PAGTreeNode* node) {
  auto wrappedType = GetWrappedType(value.get_type());
  bool isWrapped = wrappedType != value.get_type();
  auto realValue = isWrapped ? value.extract_wrapped_value() : value;

  // Check for nullptr in various ways
  if (value.can_convert<std::nullptr_t>()) {
    node->setValue("<null>");
    return;
  }

  // Check if it's a pointer type and the pointer is null
  if (wrappedType.is_pointer()) {
    void* ptr = nullptr;
    if (value.convert(ptr) || realValue.convert(ptr)) {
      if (ptr == nullptr) {
        node->setValue("<null>");
        return;
      }
    }
  }

  if (wrappedType.is_arithmetic()) {
    node->setValue(TransformNumberToQString(wrappedType, realValue));
    return;
  }
  if (wrappedType.is_enumeration()) {
    node->setValue(TransformEnumToQString(realValue));
    return;
  }
  if (wrappedType == rttr::type::get<std::string>()) {
    node->setValue(realValue.to_string().c_str());
    return;
  }
  if (value.is_sequential_container()) {
    SerializeSequentialContainer(value.create_sequential_view(), node);
    return;
  }

  auto properties = wrappedType.get_properties();
  if (properties.empty()) {
    bool result = false;
    std::string str = value.to_string(&result);
    if (!result) {
      // Try to provide a more meaningful representation for unknown types
      str = wrappedType.get_name().to_string();
      if (str.empty()) {
        str = "<unknown>";
      }
    }
    node->setValue(str.c_str());
    return;
  }
  node->setValue(wrappedType.get_name().to_string().c_str());
  SerializeInstance(value, node);
}

static void SerializeSequentialContainer(const rttr::variant_sequential_view& view,
                                         PAGTreeNode* node) {
  auto wrappedType = GetWrappedType(view.get_value_type().get_raw_type());
  node->setValue(QString(wrappedType.get_name().data()) + "[" + QString::number(view.get_size()) +
                 "]");
  int index = 0;
  for (auto& item : view) {
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(QString::number(index));
    if (item.is_sequential_container()) {
      childNode->setValue("[]");
      SerializeSequentialContainer(item.create_sequential_view(), childNode.get());
    } else {
      auto wrappedValue = item.extract_wrapped_value();
      auto type = wrappedValue.get_type();
      if (type.is_arithmetic()) {
        childNode->setValue(TransformNumberToQString(type, wrappedValue));
      } else if (type.is_enumeration()) {
        childNode->setValue(TransformEnumToQString(wrappedValue));
      } else if (type == rttr::type::get<std::string>()) {
        childNode->setValue(wrappedValue.to_string().c_str());
      } else {
        SerializeInstance(wrappedValue, childNode.get());
      }
    }
    index++;
    node->appendChild(std::move(childNode));
  }
}

static void SerializeNode(pagx::Node* nodePtr, PAGTreeNode* treeNode) {
  if (nodePtr == nullptr) {
    treeNode->setValue("<null>");
    return;
  }
  // Get the derived type by using nodeType(), avoiding name conflicts with pag:: types
  auto type = GetRttrTypeForNodeType(nodePtr->nodeType());
  treeNode->setValue(type.get_name().data());

  // Serialize id from base Node class
  if (!nodePtr->id.empty()) {
    auto idNode = std::make_unique<PAGTreeNode>(treeNode);
    idNode->setName("id");
    idNode->setValue(QString::fromStdString(nodePtr->id));
    treeNode->appendChild(std::move(idNode));
  }

  // Get properties of the derived type
  auto properties = type.get_properties();
  for (const auto& property : properties) {
    if (property.get_metadata("NO_SERIALIZE")) {
      continue;
    }
    auto propertyName = property.get_name().to_string();
    if (propertyName == "id") {
      continue;  // Already handled above
    }

    // Create rttr::instance from the raw pointer
    auto variant = property.get_value(nodePtr);
    auto childNode = std::make_unique<PAGTreeNode>(treeNode);
    childNode->setName(propertyName.c_str());
    SerializeVariant(variant, childNode.get());
    treeNode->appendChild(std::move(childNode));
  }
}

static void SerializeNodes(const std::vector<std::unique_ptr<pagx::Node>>& nodes,
                           PAGTreeNode* node) {
  node->setValue(QString("Node[%1]").arg(static_cast<int>(nodes.size())));
  int index = 0;
  for (const auto& nodePtr : nodes) {
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(QString::number(index));
    SerializeNode(nodePtr.get(), childNode.get());
    node->appendChild(std::move(childNode));
    index++;
  }
}

static void SerializeLayers(const std::vector<pagx::Layer*>& layers, PAGTreeNode* node) {
  node->setValue(QString("Layer[%1]").arg(static_cast<int>(layers.size())));
  int index = 0;
  for (auto* layer : layers) {
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(QString::number(index));
    SerializeNode(layer, childNode.get());
    node->appendChild(std::move(childNode));
    index++;
  }
}

static void AddProperty(PAGTreeNode* parent, const QString& name, const QString& value) {
  auto childNode = std::make_unique<PAGTreeNode>(parent);
  childNode->setName(name);
  childNode->setValue(value);
  parent->appendChild(std::move(childNode));
}

void Serialize(const std::shared_ptr<pagx::PAGXDocument>& document, PAGTreeNode* node) {
  if (document == nullptr) {
    node->setValue("<null>");
    return;
  }

  node->setValue("PAGXDocument");

  // Manually serialize PAGXDocument properties to avoid RTTR variant issues
  AddProperty(node, "version", QString::fromStdString(document->version));
  AddProperty(node, "width", QString::number(static_cast<double>(document->width)));
  AddProperty(node, "height", QString::number(static_cast<double>(document->height)));

  // Serialize layers
  auto layersNode = std::make_unique<PAGTreeNode>(node);
  layersNode->setName("layers");
  SerializeLayers(document->layers, layersNode.get());
  node->appendChild(std::move(layersNode));

  // Serialize nodes (unique_ptr vector)
  auto nodesNode = std::make_unique<PAGTreeNode>(node);
  nodesNode->setName("nodes");
  SerializeNodes(document->nodes, nodesNode.get());
  node->appendChild(std::move(nodesNode));

  // Serialize errors
  if (!document->errors.empty()) {
    auto errorsNode = std::make_unique<PAGTreeNode>(node);
    errorsNode->setName("errors");
    errorsNode->setValue(QString("string[%1]").arg(static_cast<int>(document->errors.size())));
    int index = 0;
    for (const auto& error : document->errors) {
      auto childNode = std::make_unique<PAGTreeNode>(errorsNode.get());
      childNode->setName(QString::number(index));
      childNode->setValue(QString::fromStdString(error));
      errorsNode->appendChild(std::move(childNode));
      index++;
    }
    node->appendChild(std::move(errorsNode));
  }
}

}  // namespace pag::PAGXDocumentSerializer
