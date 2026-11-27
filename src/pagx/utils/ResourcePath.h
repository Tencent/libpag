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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

namespace pagx {

/**
 * Returns the RFC 3986 URI scheme (without ':') when `path` starts with one, or an empty string
 * for local filesystem references. A colon that belongs to a Windows drive letter, appears after
 * a path separator, or is not followed by a valid scheme character sequence is not a scheme.
 * Callers that need case-insensitive comparisons should normalize with `ToLowerAscii`.
 */
std::string GetUriScheme(const std::string& path);

/** Returns true when `path` starts with a URI scheme such as `http:`, `file:` or `data:`. */
bool HasUriScheme(const std::string& path);

/**
 * Returns true for paths that can be joined to a base directory: non-empty, no URI scheme, not
 * rooted (POSIX, UNC or Windows drive), and without a Windows drive prefix.
 */
bool IsRelativeFilePath(const std::string& path);

/**
 * Returns true for local filesystem references: non-empty and without a URI scheme. Windows drive
 * letters, extended `\\?\` paths, UNC paths and relative paths are local.
 */
bool IsLocalFilePath(const std::string& path);

}  // namespace pagx
