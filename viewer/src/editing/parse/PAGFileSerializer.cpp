/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <QDebug>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wdtor-name"
#include "editing/rttr/PAGRttr.hpp"
#pragma clang diagnostic pop

namespace pag {

auto getWrappedInstance(const rttr::instance& item) {
  return item.get_type().get_raw_type().is_wrapper() ? item.get_wrapped_instance() : item;
}

auto getWrappedType(const rttr::type& type) {
  return type.is_wrapper() ? type.get_wrapped_type() : type;
}

void PAGFileSerializer::serialize(std::shared_ptr<File>& file, PAGTreeNode* node) {
  rttr::instance item = file;
  serializeInstance(item, node);
}

void PAGFileSerializer::serializeInstance(const rttr::instance& item, PAGTreeNode* node) {
  rttr::instance object = getWrappedInstance(item);
  auto derivedType = object.get_derived_type();
  node->setValue(derivedType.get_name().data());

  auto properties = derivedType.get_properties();
  for (auto property : properties) {
    if (property.get_metadata("NO_SERIALIZE")) {
      continue;
    }
    if (property.get_name() == "fileAttributes") {
      continue;
    }

    rttr::variant variant = property.get_value(object);
    if (variant == nullptr) {
      continue;
    }

    auto childNode = new PAGTreeNode(node);
    childNode->setName(property.get_name().data());
    node->appendChild(childNode);
    serializeVariant(variant, childNode);
  }
}

bool PAGFileSerializer::serializeVariant(const rttr::variant& value, PAGTreeNode* node) {
  auto wrappedType = getWrappedType(value.get_type());
  bool isWrapped = wrappedType != value.get_type();
  auto realValue = isWrapped ? value.extract_wrapped_value() : value;

  if (value.can_convert<std::nullptr_t>()) {
    node->setValue("<null>");
    return true;
  }

  if (wrappedType.is_arithmetic()) {
    node->setValue(transformNumberToQString(wrappedType, realValue));
    return true;
  }
  if (wrappedType.is_enumeration()) {
    node->setValue(transformEnumToQString(wrappedType, realValue));
    return true;
  }
  if (wrappedType == rttr::type::get<std::string>()) {
    node->setValue(realValue.to_string().c_str());
    return true;
  }
  if (value.is_sequential_container()) {
    node->setValue(wrappedType.get_name().to_string().c_str());
    serializeSequentialContainer(value.create_sequential_view(), node);
    return true;
  }
  if (value.is_associative_container()) {
    node->setValue(wrappedType.get_name().to_string().c_str());
    serializeAssociativeContainer(value.create_associative_view(), node);
    return true;
  }

  auto properties = wrappedType.get_properties();
  if (properties.empty()) {
    bool result = false;
    std::string str = value.to_string(&result);
    if (!result) {
      str = "{}";
    }
    node->setValue(str.c_str());
    return true;
  }
  node->setValue(wrappedType.get_name().to_string().c_str());
  serializeInstance(value, node);
  return true;
}

void PAGFileSerializer::serializeSequentialContainer(const rttr::variant_sequential_view& view,
                                                     PAGTreeNode* node) {
  int index = 0;
  auto wrappedType = getWrappedType(view.get_value_type().get_raw_type());
  node->setValue(QString(wrappedType.get_name().data()) + "[" + QString::number(view.get_size()) +
                 "]");
  for (auto& item : view) {
    auto childNode = new PAGTreeNode(node);
    auto indexStr = QString::number(index);
    childNode->setName(indexStr);
    node->appendChild(childNode);
    if (item.is_sequential_container()) {
      childNode->setValue("[]");
      serializeSequentialContainer(item.create_sequential_view(), childNode);
    } else {
      auto wrappedValue = item.extract_wrapped_value();
      auto type = wrappedValue.get_type();
      if (type.is_arithmetic()) {
        childNode->setValue(transformNumberToQString(type, wrappedValue));
      } else if (type.is_enumeration()) {
        childNode->setValue(transformEnumToQString(type, wrappedValue));
      } else if (type == rttr::type::get<std::string>()) {
        childNode->setValue(wrappedValue.to_string().c_str());
      } else {
        serializeInstance(wrappedValue, childNode);
      }
    }
    index++;
  }
}

void PAGFileSerializer::serializeAssociativeContainer(const rttr::variant_associative_view& view,
                                                      PAGTreeNode* node) {
  static const std::string_view key_name = "key";
  static const std::string_view value_name = "value";

  if (view.is_key_only_type()) {
    for (auto& item : view) {
      auto childNode = new PAGTreeNode(node);
      childNode->setName(key_name.data());
      node->appendChild(childNode);
      serializeVariant(item, childNode);
    }
  } else {
    for (auto& item : view) {
      auto keyNode = new PAGTreeNode(node);
      keyNode->setName(key_name.data());
      node->appendChild(keyNode);
      serializeVariant(item.first, keyNode);

      auto valueNode = new PAGTreeNode(node);
      valueNode->setName(value_name.data());
      node->appendChild(valueNode);
      serializeVariant(item.second, valueNode);
    }
  }
}

QString PAGFileSerializer::transformNumberToQString(const rttr::type& type,
                                                    const rttr::variant& value) {
  std::string result;
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

  return "0";
}

QString PAGFileSerializer::transformEnumToQString([[maybe_unused]] const rttr::type& type,
                                                  const rttr::variant& value) {
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

}  //  namespace pag