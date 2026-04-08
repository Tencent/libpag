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

#include <libxml/tree.h>
#include <string>

namespace pagx::cli {

/**
 * Reorders attributes of a single XML element node to match the canonical attribute order defined
 * by the PAGX format. Attributes not in the canonical order retain their original relative order
 * and are appended at the end.
 */
void ReorderAttributes(xmlNodePtr node);

/**
 * Recursively reorders attributes on all element nodes in the subtree.
 */
void ReorderAttributesRecursive(xmlNodePtr node);

/**
 * Escapes special XML characters in a string for use in attribute values.
 */
std::string EscapeXmlAttr(const std::string& s);

/**
 * Serializes an XML subtree to a formatted string with consistent indentation and attribute
 * escaping.
 */
void SerializeNode(std::string& output, xmlNodePtr node, int indentLevel, int indentSpaces);

}  // namespace pagx::cli
