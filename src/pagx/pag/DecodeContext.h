// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 DecodeContext — Decoder-side state aggregator. Inherits the shared
// DiagnosticCollector base (Phase 0) for warnings cap + message truncation,
// adds errors[] / depth counters / DoS accounting + RAII guard for the
// nested-stream pointer.
//
// Authoritative design: §8.5 (Diagnostic / Context architecture) + §G.2
// (ErrorCode segments) + §H.1 (DoS limits) + §D.3 (TagHeader length overflow
// guard which also drives DecodeContext.totalAllocatedBytes accounting).
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/StreamContext.h"
#include "pagx/Diagnostic.h"
#include "pagx/pag/DiagnosticCollector.h"
#include "pagx/pag/ErrorCode.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

struct DecodeContext : DiagnosticCollector {
  // v1 StreamContext — owned here; passed into every DecodeStream so v1's
  // PAGThrowError() machinery routes errors back to syncFromStreamContext().
  ::pag::StreamContext streamContext;

  // Weak pointer to the stream currently being dispatched. Set via
  // CurrentStreamScope (RAII) only — direct assignment is forbidden because
  // sub-streams are stack objects whose lifetime ends at `}` boundary.
  ::pag::DecodeStream* currentStream = nullptr;

  // Fatal diagnostics; non-empty == hasError() == abort decode.
  std::vector<Diagnostic> errors;

  // Depth counters cross-checked in LayerBlock / VectorGroup readers
  // (Phase 5+). Maintained by the readers' own enter/exit pairs.
  uint32_t currentLayerDepth = 0;
  uint32_t currentVectorElementDepth = 0;

  // Cumulative bytes allocated for resource buffers (image/font payloads,
  // string tables, vector<T> properties). Decoder readers MUST consult
  // `MAX_TOTAL_BODY_BYTES - totalAllocatedBytes` BEFORE alloc — see §H.1
  // safe-subtraction pattern.
  size_t totalAllocatedBytes = 0;

  uint32_t currentOffset() const {
    return currentStream != nullptr ? static_cast<uint32_t>(currentStream->position()) : 0;
  }

  // Public 3-arg API. Subclass owns its own error/warn signature so callers
  // never see C++ name hiding from the protected base helpers.
  void error(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) {
      return;
    }
    if (msg.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      msg.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back({code, std::move(msg), currentOffset(), contextIndex});
  }

  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    Diagnostic d{code, std::move(msg), currentOffset(), contextIndex};
    pushWarning(std::move(d));
  }

  bool hasError() const {
    return !errors.empty();
  }

  // Drain any errors v1 EncodeStream/DecodeStream pushed into our
  // streamContext (out-of-bound reads, premature EOF, etc.) into our v2
  // errors[] so byteOffset reflects the tag boundary at sync time.
  // NOTE: ::pag::StreamContext::hasException() is not const-qualified, so
  // syncFromStreamContext() can't be either.
  void syncFromStreamContext() {
    if (!streamContext.hasException()) {
      return;
    }
    uint32_t offset = currentOffset();
    for (const auto& msg : streamContext.errorMessages) {
      if (errors.size() >= limits::MAX_DIAGNOSTICS) {
        break;
      }
      errors.push_back({ErrorCode::TruncatedData, msg, offset, UINT32_MAX});
    }
    streamContext.errorMessages.clear();
  }

 protected:
  // Override pushError so the base helpers also funnel into errors[].
  void pushError(Diagnostic d) override {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) {
      return;
    }
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back(std::move(d));
  }
};

// RAII guard for currentStream — entry swaps the pointer, exit restores the
// previous value (allowing nested SubStream dispatch without dangling
// pointers when the inner sub-stream's stack frame unwinds).
//
// Usage:
//   {
//     CurrentStreamScope scope(ctx, &subStream);
//     ReadFooBody(&subStream, ctx);
//   }   // currentStream restored to whatever it was on entry
struct CurrentStreamScope {
  DecodeContext* ctx;
  ::pag::DecodeStream* prev;

  CurrentStreamScope(DecodeContext* c, ::pag::DecodeStream* s) : ctx(c), prev(c->currentStream) {
    c->currentStream = s;
  }

  ~CurrentStreamScope() {
    ctx->currentStream = prev;
  }

  CurrentStreamScope(const CurrentStreamScope&) = delete;
  CurrentStreamScope& operator=(const CurrentStreamScope&) = delete;
};

// Forward-only seek to an absolute stream position. v1 DecodeStream::setPosition
// has an EOF-check bug — it interprets its argument as a "bytesToRead" delta
// (DecodeStream.cpp:23-27), so passing an absolute target re-trips EOF.
// SeekTo skips the difference instead, which uses the same underlying delta
// path correctly. Backward seeks are not supported (Phase 4 decoder only
// aligns forward to tag boundaries); callers requiring rewind must extend.
inline void SeekTo(::pag::DecodeStream* stream, uint32_t absolutePos) {
  uint32_t cur = stream->position();
  if (absolutePos > cur) {
    stream->skip(absolutePos - cur);
  }
}

}  // namespace pagx::pag
