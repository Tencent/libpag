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

#include "PAGFileSerializer.h"
#include <PAGExtraRttr.hpp>
#include <PAGRttr.hpp>
#include <QDebug>

namespace pag::FileSerializer {

auto getWrappedInstance(const rttr::instance& item) {
  return item.get_type().get_raw_type().is_wrapper() ? item.get_wrapped_instance() : item;
}

auto getWrappedType(const rttr::type& type) {
  return type.is_wrapper() ? type.get_wrapped_type() : type;
}

void Serialize(std::shared_ptr<File> file, PAGTreeNode* node) {
  RegisterExtraTypes();
  rttr::instance item = file;
  SerializeInstance(item, node);
}

void SerializeInstance(const rttr::instance& item, PAGTreeNode* node) {
  rttr::instance object = getWrappedInstance(item);
  auto derivedType = object.get_derived_type();
  node->setValue(derivedType.get_name().data());

  auto properties = derivedType.get_properties();
  for (const auto& property : properties) {
    if (property.get_metadata("NO_SERIALIZE")) {
      continue;
    }
    if (property.get_name() == "fileAttributes") {
      continue;
    }

    rttr::variant variant = property.get_value(object);
    auto childNode = std::make_unique<PAGTreeNode>(node);
    childNode->setName(property.get_name().data());
    SerializeVariant(variant, childNode.get());
    node->appendChild(std::move(childNode));
  }
}

void SerializeVariant(const rttr::variant& value, PAGTreeNode* node) {
  auto wrappedType = getWrappedType(value.get_type());
  bool isWrapped = wrappedType != value.get_type();
  auto realValue = isWrapped ? value.extract_wrapped_value() : value;

  if (value.can_convert<std::nullptr_t>()) {
    node->setValue("<null>");
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
    node->setValue(wrappedType.get_name().to_string().c_str());
    SerializeSequentialContainer(value.create_sequential_view(), node);
    return;
  }
  if (value.is_associative_container()) {
    node->setValue(wrappedType.get_name().to_string().c_str());
    SerializeAssociativeContainer(value.create_associative_view(), node);
    return;
  }

  auto properties = wrappedType.get_properties();
  if (properties.empty()) {
    bool result = false;
    std::string str = value.to_string(&result);
    if (!result) {
      str = "{}";
    }
    node->setValue(str.c_str());
    return;
  }
  node->setValue(wrappedType.get_name().to_string().c_str());
  SerializeInstance(value, node);
}

void SerializeSequentialContainer(const rttr::variant_sequential_view& view, PAGTreeNode* node) {
  int index = 0;
  auto wrappedType = getWrappedType(view.get_value_type().get_raw_type());
  node->setValue(QString(wrappedType.get_name().data()) + "[" + QString::number(view.get_size()) +
                 "]");
  for (auto& item : view) {
    auto childNode = std::make_unique<PAGTreeNode>(node);
    auto indexStr = QString::number(index);
    childNode->setName(indexStr);
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

void SerializeAssociativeContainer(const rttr::variant_associative_view& view, PAGTreeNode* node) {
  static const std::string_view key_name = "key";
  static const std::string_view value_name = "value";

  if (view.is_key_only_type()) {
    for (auto& item : view) {
      auto childNode = std::make_unique<PAGTreeNode>(node);
      childNode->setName(key_name.data());
      SerializeVariant(item, childNode.get());
      node->appendChild(std::move(childNode));
    }
  } else {
    for (auto& item : view) {
      auto keyNode = std::make_unique<PAGTreeNode>(node);
      keyNode->setName(key_name.data());
      SerializeVariant(item.first, keyNode.get());
      node->appendChild(std::move(keyNode));

      auto valueNode = std::make_unique<PAGTreeNode>(node);
      valueNode->setName(value_name.data());
      SerializeVariant(item.second, valueNode.get());
      node->appendChild(std::move(valueNode));
    }
  }
}

QString TransformNumberToQString(const rttr::type& type, const rttr::variant& value) {
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
    return QString::number(value.to_float());
  }
  if (type == rttr::type::get<double>()) {
    return QString::number(value.to_double());
  }

  return "";
}

QString TransformEnumToQString(const rttr::variant& value) {
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

}  // namespace pag::FileSerializer
