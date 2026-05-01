// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 EncodeSession — 2-pointer stack aggregate passed down through every
// Write<TagName>() function. Encode never produces fatal errors (only
// warnings), so we don't need a full Context — just a DiagnosticCollector to
// receive warn() and the v1 StreamContext required by EncodeStream.
//
// Authoritative design: §8.5 (3 Context + 1 EncodeSession architecture).
#pragma once

#include "codec/utils/StreamContext.h"
#include "pagx/pag/DiagnosticCollector.h"

namespace pagx::pag {

struct EncodeSession {
  DiagnosticCollector* diag = nullptr;
  ::pag::StreamContext* sc = nullptr;
};

}  // namespace pagx::pag
