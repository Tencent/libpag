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
#include <vector>
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * Animation class of a field channel, controlling whether the channel can be driven by animation.
 *   - Animatable: a layout-output or pure render property that has a lightweight runtime writer; it
 *     can be both reflected (read/write on the document node) and animated.
 *   - LayoutInput: an auto-layout constraint (padding, alignment, container width/height, ...). It
 *     can be reflected but must NOT be animated, since changing it requires re-running layout.
 *   - Static: reflectable but neither a layout input nor animatable.
 */
enum class AnimClass { Animatable, LayoutInput, Static };

// Reads or writes one node field. Exactly one of getOut / setIn is non-null: getOut for a read
// (copy field into *getOut), setIn for a write (validate and copy into the field). Returns false on
// a type mismatch or invalid enum string.
using NodeAccessFn = bool (*)(Node* node, KeyValue* getOut, const KeyValue* setIn);

/**
 * One reflective field of a node type, addressed by channel name (the same attribute name used in
 * PAGX XML, e.g. "alpha", "position.x", "blendMode").
 */
struct NodeFieldDef {
  const char* channel;
  AnimClass animClass;
  NodeAccessFn access;
};

/**
 * Returns the reflective field table for the given node type, or an empty table if the type has no
 * reflectable scalar fields. The table is the single source of truth for channel names and their
 * animation class, consumed by node reflection (GetNodeChannel/SetNodeChannel), animation validity
 * checks (IsAnimatableChannel), and — indirectly, via a consistency test — the renderer writer
 * table.
 */
const std::vector<NodeFieldDef>& NodeFieldsFor(NodeType type);

/**
 * Reads the value of the given channel on the node into out. Returns true on success, false if the
 * channel is unknown for the node type, the field type cannot be represented as a KeyValue, or an
 * optional field is unset.
 */
bool GetNodeChannel(const Node* node, const std::string& channel, KeyValue* out);

/**
 * Writes value into the node field identified by channel. Returns true on success, false if the
 * channel is unknown for the node type, the KeyValue type does not match the field, or an enum
 * string is invalid. The node is the source of truth; callers refresh any live scene separately.
 */
bool SetNodeChannel(Node* node, const std::string& channel, const KeyValue& value);

/**
 * Returns true if the given channel exists on the node type and is classified as Animatable, i.e.
 * it is valid for an animation channel to drive it.
 */
bool IsAnimatableChannel(NodeType type, const std::string& channel);

}  // namespace pagx
