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
#include <ostream>
#include <string>
#include "pagx/nodes/Node.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Padding.h"

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

const char* NodeTypeName(NodeType type);

/// Returns the string representation of a LayoutMode value, or nullptr for LayoutMode::None.
const char* LayoutModeName(LayoutMode mode);

/// Returns the string representation of an Alignment value, or nullptr for Alignment::Stretch.
const char* AlignmentName(Alignment alignment);

/// Returns the string representation of an Arrangement value, or nullptr for Arrangement::Start.
const char* ArrangementName(Arrangement arrangement);

/// Writes a padding attribute in shorthand form to the output stream. Writes nothing if padding is
/// zero. Uses 1-value, 2-value, or 4-value form depending on symmetry.
void WritePaddingAttr(std::ostream& os, const Padding& padding);

/// Writes layout-related attributes (layout, gap, flex, padding, alignment, arrangement,
/// includeInLayout, clipToBounds) to the output stream. Only writes non-default values.
void WriteLayoutAttrs(std::ostream& os, LayoutMode layout, float gap, float flex,
                      const Padding& padding, Alignment alignment, Arrangement arrangement,
                      bool includeInLayout, bool clipToBounds);

/// Writes a bounds attribute in "x,y,w,h" integer format to the output stream.
void WriteBoundsAttr(std::ostream& os, float x, float y, float width, float height);

}  // namespace pagx::cli
