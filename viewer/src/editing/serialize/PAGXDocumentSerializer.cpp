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
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
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

  return "null";
}

// Check if the variant is a Node pointer type (regardless of whether the pointer is null)
static bool IsNodePointerType(const rttr::variant& value) {
  auto type = value.get_type();
  auto wrappedType = GetWrappedType(type);

  if (!wrappedType.is_pointer()) {
    return false;
  }

  auto rawType = wrappedType.get_raw_type();

  // Check if the type is Node* or derived from Node
  // With RTTR_ENABLE() macros in place, is_derived_from works correctly
  return rawType == rttr::type::get<pagx::Node>() || rawType.is_derived_from<pagx::Node>();
}

// Try to extract a Node* from a variant
// Returns: pair<bool, Node*> where bool indicates if it's a Node pointer type
//          - {false, nullptr}: not a Node pointer type
//          - {true, nullptr}: is a Node pointer type, but the pointer value is nullptr
//          - {true, ptr}: is a Node pointer type with valid pointer
static std::pair<bool, pagx::Node*> TryExtractNodePointer(const rttr::variant& value) {
  if (!IsNodePointerType(value)) {
    return {false, nullptr};
  }

  auto realValue = value.extract_wrapped_value();
  void* ptr = nullptr;
  if (realValue.convert(ptr) && ptr != nullptr) {
    return {true, static_cast<pagx::Node*>(ptr)};
  }

  return {true, nullptr};
}

// Serialize a Node pointer by casting to its actual derived type based on nodeType()
static void SerializeNodeAsDerived(pagx::Node* nodePtr, PAGTreeNode* treeNode) {
  if (nodePtr == nullptr) {
    treeNode->setValue("nullptr");
    return;
  }

#define SERIALIZE_NODE_CASE(EnumValue, ClassName)                         \
  case pagx::NodeType::EnumValue:                                         \
    SerializeInstance(*static_cast<pagx::ClassName*>(nodePtr), treeNode); \
    return;

  switch (nodePtr->nodeType()) {
    SERIALIZE_NODE_CASE(PathData, PathData)
    SERIALIZE_NODE_CASE(Image, Image)
    SERIALIZE_NODE_CASE(Composition, Composition)
    SERIALIZE_NODE_CASE(SolidColor, SolidColor)
    SERIALIZE_NODE_CASE(LinearGradient, LinearGradient)
    SERIALIZE_NODE_CASE(RadialGradient, RadialGradient)
    SERIALIZE_NODE_CASE(ConicGradient, ConicGradient)
    SERIALIZE_NODE_CASE(DiamondGradient, DiamondGradient)
    SERIALIZE_NODE_CASE(ImagePattern, ImagePattern)
    SERIALIZE_NODE_CASE(ColorStop, ColorStop)
    SERIALIZE_NODE_CASE(Font, Font)
    SERIALIZE_NODE_CASE(Glyph, Glyph)
    SERIALIZE_NODE_CASE(Layer, Layer)
    SERIALIZE_NODE_CASE(DropShadowStyle, DropShadowStyle)
    SERIALIZE_NODE_CASE(InnerShadowStyle, InnerShadowStyle)
    SERIALIZE_NODE_CASE(BackgroundBlurStyle, BackgroundBlurStyle)
    SERIALIZE_NODE_CASE(BlurFilter, BlurFilter)
    SERIALIZE_NODE_CASE(DropShadowFilter, DropShadowFilter)
    SERIALIZE_NODE_CASE(InnerShadowFilter, InnerShadowFilter)
    SERIALIZE_NODE_CASE(BlendFilter, BlendFilter)
    SERIALIZE_NODE_CASE(ColorMatrixFilter, ColorMatrixFilter)
    SERIALIZE_NODE_CASE(Rectangle, Rectangle)
    SERIALIZE_NODE_CASE(Ellipse, Ellipse)
    SERIALIZE_NODE_CASE(Polystar, Polystar)
    SERIALIZE_NODE_CASE(Path, Path)
    SERIALIZE_NODE_CASE(Text, Text)
    SERIALIZE_NODE_CASE(Fill, Fill)
    SERIALIZE_NODE_CASE(Stroke, Stroke)
    SERIALIZE_NODE_CASE(TrimPath, TrimPath)
    SERIALIZE_NODE_CASE(RoundCorner, RoundCorner)
    SERIALIZE_NODE_CASE(MergePath, MergePath)
    SERIALIZE_NODE_CASE(TextModifier, TextModifier)
    SERIALIZE_NODE_CASE(TextPath, TextPath)
    SERIALIZE_NODE_CASE(TextBox, TextBox)
    SERIALIZE_NODE_CASE(Group, Group)
    SERIALIZE_NODE_CASE(Repeater, Repeater)
    SERIALIZE_NODE_CASE(RangeSelector, RangeSelector)
    SERIALIZE_NODE_CASE(GlyphRun, GlyphRun)
  }

#undef SERIALIZE_NODE_CASE

  // Fallback to base Node if unknown type
  SerializeInstance(*nodePtr, treeNode);
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
    node->setValue("nullptr");
    return;
  }

  // Check if it's a pointer type
  if (wrappedType.is_pointer()) {
    // For Node pointers, use SerializeNodeAsDerived to properly detect derived type
    auto [isNodePtr, nodePtr] = TryExtractNodePointer(value);
    if (isNodePtr) {
      SerializeNodeAsDerived(nodePtr, node);
      return;
    }
    // For other pointer types, check if null
    void* ptr = nullptr;
    if (value.convert(ptr) || realValue.convert(ptr)) {
      if (ptr == nullptr) {
        node->setValue("nullptr");
        return;
      }
    }
    // For other pointer types, dereference and serialize the actual object
    SerializeInstance(realValue, node);
    return;
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
        str = "(unknown)";
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
      } else if (type.is_pointer()) {
        // For Node pointers, use SerializeNodeAsDerived to properly detect derived type
        auto [isNodePtr, nodePtr] = TryExtractNodePointer(wrappedValue);
        if (isNodePtr) {
          SerializeNodeAsDerived(nodePtr, childNode.get());
        } else {
          // For other pointer types, check if null
          void* ptr = nullptr;
          if (wrappedValue.convert(ptr) && ptr != nullptr) {
            SerializeInstance(wrappedValue, childNode.get());
          } else {
            childNode->setValue("nullptr");
          }
        }
      } else {
        SerializeInstance(wrappedValue, childNode.get());
      }
    }
    index++;
    node->appendChild(std::move(childNode));
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
    node->setValue("nullptr");
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
  layersNode->setValue(QString("Layer[%1]").arg(static_cast<int>(document->layers.size())));
  int index = 0;
  for (auto* layer : document->layers) {
    auto childNode = std::make_unique<PAGTreeNode>(layersNode.get());
    childNode->setName(QString::number(index));
    if (layer != nullptr) {
      SerializeInstance(*layer, childNode.get());
    } else {
      childNode->setValue("nullptr");
    }
    layersNode->appendChild(std::move(childNode));
    index++;
  }
  node->appendChild(std::move(layersNode));

  // Serialize nodes (unique_ptr vector)
  auto nodesNode = std::make_unique<PAGTreeNode>(node);
  nodesNode->setName("nodes");
  nodesNode->setValue(QString("Node[%1]").arg(static_cast<int>(document->nodes.size())));
  index = 0;
  for (const auto& nodePtr : document->nodes) {
    auto childNode = std::make_unique<PAGTreeNode>(nodesNode.get());
    childNode->setName(QString::number(index));
    SerializeNodeAsDerived(nodePtr.get(), childNode.get());
    nodesNode->appendChild(std::move(childNode));
    index++;
  }
  node->appendChild(std::move(nodesNode));

  // Serialize errors
  if (!document->errors.empty()) {
    auto errorsNode = std::make_unique<PAGTreeNode>(node);
    errorsNode->setName("errors");
    errorsNode->setValue(QString("string[%1]").arg(static_cast<int>(document->errors.size())));
    index = 0;
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
