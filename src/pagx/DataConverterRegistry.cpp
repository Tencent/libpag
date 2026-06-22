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

#include "pagx/DataConverterRegistry.h"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include "pagx/nodes/DataConverter.h"

namespace pagx {

DataConverterRegistry::DataConverterRegistry() {
  // ---- secondsToFrames ----
  // Converts seconds (float) to frames (float) using frameRate parameter.
  registerConverter("secondsToFrames", [](const KeyValue& input,
                                          const std::unordered_map<std::string, std::string>& params) -> KeyValue {
    if (!std::holds_alternative<float>(input)) {
      return input;
    }
    float seconds = std::get<float>(input);
    float frameRate = 30.0f;
    auto it = params.find("frameRate");
    if (it != params.end()) {
      frameRate = std::strtof(it->second.c_str(), nullptr);
    }
    return KeyValue{seconds * frameRate};
  });

  // ---- priceFormat ----
  // Formats a number (float) to a string with optional prefix/suffix/decimals.
  // Output is a string value (e.g., "$5999.00").
  registerConverter("priceFormat", [](const KeyValue& input,
                                       const std::unordered_map<std::string, std::string>& params) -> KeyValue {
    float value = 0.0f;
    if (std::holds_alternative<float>(input)) {
      value = std::get<float>(input);
    } else if (std::holds_alternative<int>(input)) {
      value = static_cast<float>(std::get<int>(input));
    } else {
      return input;
    }
    std::string prefix = "";
    std::string suffix = "";
    int decimals = 0;
    auto it = params.find("prefix");
    if (it != params.end()) prefix = it->second;
    it = params.find("suffix");
    if (it != params.end()) suffix = it->second;
    it = params.find("decimals");
    if (it != params.end()) decimals = std::atoi(it->second.c_str());

    std::ostringstream oss;
    oss << prefix;
    oss << std::fixed << std::setprecision(decimals) << value;
    oss << suffix;
    return KeyValue{oss.str()};
  });

  // ---- Inverse converters (syncBack direction) ----

  registerInverseConverter("secondsToFrames",
      [](const KeyValue& input,
         const std::unordered_map<std::string, std::string>& params) -> KeyValue {
        if (!std::holds_alternative<float>(input)) return input;
        float frames = std::get<float>(input);
        float frameRate = 30.0f;
        auto it = params.find("frameRate");
        if (it != params.end()) frameRate = std::strtof(it->second.c_str(), nullptr);
        return KeyValue{frames / frameRate};
      });

  registerInverseConverter("priceFormat",
      [](const KeyValue& input,
         const std::unordered_map<std::string, std::string>& params) -> KeyValue {
        std::string str;
        if (std::holds_alternative<std::string>(input)) {
          str = std::get<std::string>(input);
        } else if (std::holds_alternative<float>(input)) {
          return input;
        } else {
          return input;
        }
        // Strip known prefix and suffix, parse remainder as float.
        auto it = params.find("prefix");
        if (it != params.end() && str.find(it->second) == 0)
          str = str.substr(it->second.size());
        it = params.find("suffix");
        if (it != params.end() && str.size() >= it->second.size() &&
            str.compare(str.size() - it->second.size(), it->second.size(), it->second) == 0)
          str = str.substr(0, str.size() - it->second.size());
        return KeyValue{std::strtof(str.c_str(), nullptr)};
      });
}

DataConverterRegistry& DataConverterRegistry::instance() {
  static DataConverterRegistry registry;
  return registry;
}

void DataConverterRegistry::registerConverter(const std::string& typeName, ConverterFn fn) {
  converters[typeName] = std::move(fn);
}

void DataConverterRegistry::registerInverseConverter(const std::string& typeName, ConverterFn fn) {
  inverseConverters[typeName] = std::move(fn);
}

KeyValue DataConverterRegistry::apply(const DataConverter* converter,
                                      const KeyValue& input) const {
  if (converter == nullptr || converter->converterType.empty()) {
    return input;
  }
  auto it = converters.find(converter->converterType);
  if (it == converters.end()) {
    return input;
  }
  return it->second(input, converter->params);
}

KeyValue DataConverterRegistry::applyInverse(const DataConverter* converter,
                                             const KeyValue& input) const {
  if (converter == nullptr || converter->converterType.empty()) {
    return input;
  }
  auto it = inverseConverters.find(converter->converterType);
  if (it == inverseConverters.end()) {
    return input;
  }
  return it->second(input, converter->params);
}

}  // namespace pagx
