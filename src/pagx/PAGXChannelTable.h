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

#include <cstdint>
#include <vector>
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Node.h"

namespace pagx {

// Independent properties of a node channel, combined as a bitmask. The two axes are orthogonal: a
// channel may be neither, either, or both (e.g. a shape's size.width is animated in place AND, when
// edited on the document, must re-run layout before the edit shows up).
//   - Animatable: the channel has a lightweight runtime writer, so an animation channel can drive
//     it in place without rebuilding the layer.
//   - RequiresLayout: editing the channel on the document only takes effect after a layout pass,
//     because its value reaches the rendered layer through a layout-derived accessor (renderSize /
//     renderPosition / renderScale) or because it is an auto-layout input (size, constraints,
//     padding, fonts, text, container layout).
enum class ChannelFlags : uint32_t {
  None = 0,
  Animatable = 1u << 0,
  RequiresLayout = 1u << 1,
};

inline constexpr ChannelFlags operator|(ChannelFlags a, ChannelFlags b) {
  return static_cast<ChannelFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool HasFlag(ChannelFlags value, ChannelFlags flag) {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

// Function that reads or writes one node channel. Exactly one of getOut / setIn is non-null: getOut
// for a read (copies the field into *getOut), setIn for a write (validates and copies into the
// field). Returns false on a type mismatch or an invalid enum string.
using ChannelAccessor = bool (*)(Node* node, KeyValue* getOut, const KeyValue* setIn);

// One reflective channel of a node type, addressed by channel name.
struct ChannelDef {
  const char* channel;
  ChannelFlags flags;
  ChannelAccessor access;
};

// Returns the channel table for the given node type, or an empty table if the type has no
// reflectable scalar channels. The table is the single source of truth for channel names and their
// flags, backing the public read/write/query API in PAGXNodeChannel.h.
const std::vector<ChannelDef>& ChannelsFor(NodeType type);

}  // namespace pagx
