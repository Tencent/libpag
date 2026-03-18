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
  if (type == rttr::type::get<char>() || type == rttr::type::get<int8_t>()) {
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
  if (type == rttr::type::get<uint64_t>() || type == rttr::type::get<size_t>()) {
    return QString::number(value.to_uint64());
  }
  if (type == rttr::type::get<float>()) {
    return QString::number(static_cast<double>(value.to_float()));
  }
  if (type == rttr::type::get<double>()) {
    return QString::number(value.to_double());
  }
  return "";
}

static QString TransformEnumToQString(const rttr::variant& value) {
  bool result = false;
  std::string str = value.to_string(&result);
  if (result) {
    return QString::fromStdString(str);
  }
  uint64_t num = value.to_uint64(&result);
  if (result) {
    return QString::number(num);
  }
  return "null";
}

static void SerializeInstance(const rttr::instance& item, PAGTreeNode* node) {
  rttr::instance object = GetWrappedInstance(item);
  auto derivedType = object.get_derived_type();
  node->setValue(derivedType.get_name().data());

  for (const auto& property : derivedType.get_properties()) {
    if (property.get_metadata("NO_SERIALIZE")) {
      continue;
    }
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(property.get_name().data());
    SerializeVariant(property.get_value(object), childNode.get());
    node->appendChild(std::move(childNode));
  }
}

static void SerializeVariant(const rttr::variant& value, PAGTreeNode* node) {
  auto wrappedType = GetWrappedType(value.get_type());
  auto realValue = wrappedType != value.get_type() ? value.extract_wrapped_value() : value;

  if (value.can_convert<std::nullptr_t>()) {
    node->setValue("nullptr");
    return;
  }

  if (wrappedType.is_pointer()) {
    void* ptr = nullptr;
    if (realValue.convert(ptr)) {
      if (ptr == nullptr) {
        node->setValue("nullptr");
        return;
      }
    }
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
    node->setValue(result ? str.c_str() : wrappedType.get_name().to_string().c_str());
    return;
  }
  node->setValue(wrappedType.get_name().to_string().c_str());
  SerializeInstance(value, node);
}

static void SerializeSequentialContainer(const rttr::variant_sequential_view& view,
                                         PAGTreeNode* node) {
  auto wrappedType = GetWrappedType(view.get_value_type().get_raw_type());
  node->setValue(QString("%1[%2]").arg(wrappedType.get_name().data()).arg(view.get_size()));

  int index = 0;
  for (const auto& item : view) {
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(QString::number(index++));

    if (item.is_sequential_container()) {
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
        void* ptr = nullptr;
        if (wrappedValue.convert(ptr) && ptr != nullptr) {
          SerializeInstance(wrappedValue, childNode.get());
        } else {
          childNode->setValue("nullptr");
        }
      } else {
        SerializeInstance(wrappedValue, childNode.get());
      }
    }
    node->appendChild(std::move(childNode));
  }
}

void Serialize(const std::shared_ptr<pagx::PAGXDocument>& document, PAGTreeNode* node) {
  if (document == nullptr) {
    node->setValue("nullptr");
    return;
  }
  rttr::instance item = document;
  SerializeInstance(item, node);
}

}  // namespace pag::PAGXDocumentSerializer
