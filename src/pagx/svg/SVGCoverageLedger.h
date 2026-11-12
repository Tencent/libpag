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
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * Implementation status of one animatable channel with respect to the SVG exporter. This is an
 * implementation ledger, not the N/G/B/U capability classification of the design document:
 * "classified" only means the channel has been considered, never that it is exported today.
 */
enum class SVGChannelStatus : uint8_t {
  /**
   * The exporter emits an SMIL track for this channel today.
   */
  Supported,
  /**
   * Expressing this channel is planned but not implemented; the exporter reports a warning.
   */
  Planned,
  /**
   * SVG has no representation of this channel, so it will never be exported.
   */
  NotExpressible,
};

/**
 * One row of the ledger: the node type, the channel name, its status and a short reason.
 */
struct SVGChannelEntry {
  NodeType type;
  const char* channel;
  SVGChannelStatus status;
  const char* note;
};

/**
 * Every animatable channel the PAGX reflection table knows about, in the order of
 * PAGXNodeChannel.cpp. The list is pinned by SVGCoverageLedgerIsComplete, so adding an Animatable
 * channel without recording it here fails the test suite.
 */
const std::vector<SVGChannelEntry>& SVGChannelLedger();

/**
 * Returns the ledger row for (type, channel), or nullptr when the pair is not recorded.
 */
const SVGChannelEntry* FindSVGChannelEntry(NodeType type, const std::string& channel);

/**
 * Number of node types that own at least one animatable channel.
 */
size_t SVGClassifiedNodeTypeCount();

}  // namespace pagx
