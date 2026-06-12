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

#include <string>
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * Reflective, by-name read/write access to a PAGX node's scalar properties. Channels use the same
 * attribute names as PAGX XML (e.g. "alpha", "position.x", "blendMode"). This is a document-level
 * facility: it reads and writes fields on PAGXDocument nodes and does not touch any live PAGScene.
 * After mutating, refresh live scenes via PAGXDocument::notifyChange (use RequiresLayout to decide
 * the layoutChanged flag). Typical use: editor property panels and data-driven CLI edits, where the
 * caller holds a string channel name rather than a compile-time field reference.
 *
 * Value encoding (KeyValue alternatives): scalars map directly (float/bool/int/string/Color);
 * enums are passed as their string name (e.g. blendMode = "multiply"); Point/Size fields are
 * addressed component-wise via suffixed channels ("position.x", "size.width"). Multi-component
 * fields without a component channel are not exposed. The set of channels available for each node
 * type is documented in the PAGX schema reference rather than enumerated through this API.
 */

/**
 * Reads the value of the given channel on the node into out.
 * @param node  the node to read from; must not be null.
 * @param channel  the channel name (see the encoding notes above).
 * @param out  receives the value on success; must not be null.
 * @return true on success; false if node/out is null, the channel is unknown for the node type, the
 * field type cannot be represented as a KeyValue, or an optional field is unset.
 */
bool GetNodeChannel(const Node* node, const std::string& channel, KeyValue* out);

/**
 * Writes value into the node field identified by channel. The node is the source of truth; callers
 * refresh any live scene separately via PAGXDocument::notifyChange.
 * @param node  the node to write to; must not be null.
 * @param channel  the channel name (see the encoding notes above).
 * @param value  the value to write; its KeyValue alternative must match the field's type.
 * @return true on success; false if node is null, the channel is unknown for the node type, the
 * KeyValue type does not match the field, or an enum string is invalid.
 */
bool SetNodeChannel(Node* node, const std::string& channel, const KeyValue& value);

/**
 * Resets the node field identified by channel to the default value of its node type, i.e. the value
 * a freshly created node of that type carries. This is the way to "clear" a previously edited
 * channel: for optional fields (e.g. a TextModifier's strokeWidth) the default is the unset state,
 * so resetting removes the value. Only the addressed component is reset for component-wise channels
 * ("position.x" resets x but leaves y). The node is the source of truth; callers refresh any live
 * scene separately via PAGXDocument::notifyChange.
 * @param node  the node to reset; must not be null.
 * @param channel  the channel name (see the encoding notes above).
 * @return true on success; false if node is null or the channel is unknown for the node type.
 */
bool ResetNodeChannel(Node* node, const std::string& channel);

/**
 * Returns true if the given channel exists on the node type and can be driven by an animation
 * channel, i.e. it has a lightweight runtime writer that updates the live layer in place. Returns
 * false for channels that only take effect through a layout/content rebuild and for unknown
 * channels. Note this is independent of RequiresLayout: a geometry channel such as a shape's
 * "size.width" is both animatable (in place, during playback) and layout-affecting (when edited on
 * the document).
 * @param type  the node type.
 * @param channel  the channel name.
 */
bool IsAnimatableChannel(NodeType type, const std::string& channel);

/**
 * Returns true if editing the given channel on the document requires a layout pass before the
 * change is visible in a live scene, i.e. its value reaches the rendered layer through a
 * layout-derived quantity (size/position/scale) or it is an auto-layout input (constraints,
 * padding, fonts, text, container layout). Callers that mutate a channel via SetNodeChannel should
 * pass this as the layoutChanged flag to PAGXDocument::notifyChange. Returns false for channels
 * that refresh without layout and for unknown channels.
 * @param type  the node type.
 * @param channel  the channel name.
 */
bool RequiresLayout(NodeType type, const std::string& channel);

}  // namespace pagx
