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

#include "ConfigUtils.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "tinyxml2.h"

namespace exporter {

float SafeStringToFloat(std::string_view str, float defaultValue) {
  if (str.empty()) {
    return defaultValue;
  }
  try {
    return std::stof(std::string(str));
  } catch (const std::invalid_argument&) {
    return defaultValue;
  } catch (const std::out_of_range&) {
    return defaultValue;
  }
}

bool SafeStringEqual(const char* str, const char* target) {
  if (!str || !target) {
    return false;
  }
  return std::string_view(str) == std::string_view(target);
}

std::string FormatFloat(float value, int precision) {
  std::ostringstream oss;
  if (precision == 0 && value == static_cast<int>(value)) {
    oss << static_cast<int>(value);
  } else {
    oss << std::fixed << std::setprecision(precision) << value;
  }
  return oss.str();
}

void AddElement(tinyxml2::XMLElement* parent, const std::string& name, const std::string& value) {
  tinyxml2::XMLDocument* doc = parent->GetDocument();
  tinyxml2::XMLElement* element = doc->NewElement(name.c_str());
  element->SetText(value.c_str());
  parent->InsertEndChild(element);
}

const char* GetChildElementText(tinyxml2::XMLElement* fatherElement, const char* childName) {
  tinyxml2::XMLElement* childElement = fatherElement->FirstChildElement(childName);
  if (childElement == nullptr) {
    return nullptr;
  }
  return childElement->GetText();
}

}  // namespace exporter
