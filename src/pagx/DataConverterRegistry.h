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

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include "pagx/nodes/Channel.h"

namespace pagx {

class DataConverter;

/**
 * DataConverterRegistry manages named DataConverter implementations. Converters
 * transform KeyValue inputs into KeyValue outputs based on user-defined parameters.
 * Built-in converters are registered automatically.
 */
class DataConverterRegistry {
 public:
  using ConverterFn = std::function<KeyValue(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params)>;

  static DataConverterRegistry& instance();

  /**
   * Registers a converter function under the given type name.
   */
  void registerConverter(const std::string& typeName, ConverterFn fn);

  /**
   * Registers an inverse converter for the forward direction. Used during syncBack()
   * to convert layer values back to ViewModel domain values.
   */
  void registerInverseConverter(const std::string& typeName, ConverterFn fn);

  /**
   * Applies the forward converter. Returns input unchanged if no converter matches.
   */
  KeyValue apply(const DataConverter* converter, const KeyValue& input) const;

  /**
   * Applies the inverse converter. Used in syncBack() direction.
   * Returns input unchanged if no inverse converter is registered.
   */
  KeyValue applyInverse(const DataConverter* converter, const KeyValue& input) const;

 private:
  DataConverterRegistry();
  std::unordered_map<std::string, ConverterFn> converters = {};
  std::unordered_map<std::string, ConverterFn> inverseConverters = {};

  static KeyValue ConvertSecondsToFrames(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertPriceFormat(const KeyValue& input,
                                     const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertRangeMapper(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertDegsToRads(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertInverseSecondsToFrames(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertInversePriceFormat(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertInverseRangeMapper(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
  static KeyValue ConvertInverseDegsToRads(
      const KeyValue& input, const std::unordered_map<std::string, std::string>& params);
};

}  // namespace pagx
