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

#ifdef FILE_MOVIE

#include "MediaFormat.h"

namespace pag {
MediaFormat::~MediaFormat() {
  trackFormatMap.clear();
}

int MediaFormat::getInteger(const std::string& name) {
  auto iter = trackFormatMap.find(name);
  if (iter != trackFormatMap.end()) {
    return static_cast<int>(std::strtol(iter->second.data(), nullptr, 10));
  }
  return 0;
}

void MediaFormat::setInteger(std::string name, int value) {
  if (!name.empty()) {
    trackFormatMap[name] = std::to_string(value);
  }
}

int64_t MediaFormat::getLong(const std::string& name) {
  auto iter = trackFormatMap.find(name);
  if (iter != trackFormatMap.end()) {
    return strtoll(iter->second.data(), nullptr, 10);
  }
  return 0;
}

void MediaFormat::setLong(std::string name, int64_t value) {
  if (!name.empty()) {
    trackFormatMap[name] = std::to_string(value);
  }
}

float MediaFormat::getFloat(const std::string& name) {
  auto iter = trackFormatMap.find(name);
  if (iter != trackFormatMap.end()) {
    return static_cast<float>(std::strtod(iter->second.data(), nullptr));
  }
  return 0;
}

void MediaFormat::setFloat(std::string name, float value) {
  if (!name.empty()) {
    trackFormatMap[name] = std::to_string(value);
  }
}

std::string MediaFormat::getString(const std::string& name) {
  auto iter = trackFormatMap.find(name);
  if (iter != trackFormatMap.end()) {
    return iter->second;
  }
  return "";
}

void MediaFormat::setString(std::string name, std::string value) {
  if (name.empty() || value.empty()) {
    return;
  }
  trackFormatMap[name] = value;
}

std::vector<std::shared_ptr<ByteData>> MediaFormat::headers() {
  return _headers;
}

void MediaFormat::setHeaders(std::vector<std::shared_ptr<ByteData>> headers) {
  _headers = std::move(headers);
}
}  // namespace pag
#endif