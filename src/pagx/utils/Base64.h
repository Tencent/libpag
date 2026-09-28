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

#include <memory>
#include <string>
#include "pagx/types/Data.h"

namespace pagx {

std::shared_ptr<Data> Base64Decode(const std::string& encodedString);

/**
 * Decodes the base64 payload of a `data:` URI. Returns nullptr when `dataURI` is not a
 * base64-encoded data URI. The `data:` scheme is matched case-insensitively, as RFC 3986 requires
 * of URI schemes.
 */
std::shared_ptr<Data> DecodeBase64DataURI(const std::string& dataURI);

/**
 * Decodes the first `maxBytes` bytes of a `data:` URI's base64 payload, and no more. Same
 * requirements and nullptr contract as DecodeBase64DataURI(). Callers that inspect only the
 * payload's leading bytes — a format sniffer reading magic bytes, say — use this to avoid
 * materializing a multi-megabyte inlined image.
 */
std::shared_ptr<Data> DecodeBase64DataURIPrefix(const std::string& dataURI, size_t maxBytes);

std::string Base64Encode(const uint8_t* data, size_t length);

}  // namespace pagx
