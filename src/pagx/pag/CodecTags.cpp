// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/CodecTags.h"
#include <cmath>
#include <cstring>
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/ValueCodec.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

// Adapts DecodeContext to the DiagnosticCollectorGuard ValueCodec expects.
// We can't pass the DecodeContext directly because the guard wants a
// DiagnosticCollector* and DecodeContext's pushWarning is protected — but
// since DecodeContext IS-A DiagnosticCollector, an upcast satisfies the
// guard's friend access.
DiagnosticCollectorGuard MakeGuard(DecodeContext* ctx) {
  DiagnosticCollectorGuard g;
  g.collector = static_cast<DiagnosticCollector*>(ctx);
  return g;
}

// Computes one-past-the-end stream position from a base + length using
// uint64_t to defeat the 32-bit overflow attack from §D.3. Returns false
// (and pushes MalformedTag fatal) when the resulting end exceeds stream
// size; on success writes the absolute end into `*outEnd`.
bool SafeTagEnd(::pag::DecodeStream* stream, uint64_t bodyStart, uint32_t bodyLength,
                DecodeContext* ctx, uint64_t* outEnd) {
  uint64_t end = bodyStart + static_cast<uint64_t>(bodyLength);
  if (end > stream->length()) {
    ctx->error(ErrorCode::MalformedTag, "tag length overflows stream");
    return false;
  }
  *outEnd = end;
  return true;
}

}  // namespace

// =============================================================================
// FileHeader (TagCode = 1)
// =============================================================================

void WriteFileHeader(::pag::EncodeStream* stream, const FileHeader& header,
                     EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeFloat(header.width);
  body.writeFloat(header.height);
  WriteColor(&body, header.backgroundColor);
  body.writeInt32(header.frameRate.numerator);
  body.writeUint32(header.frameRate.denominator);
  body.writeUint32(header.duration);
  WriteTag(stream, TagCode::FileHeader, &body);
}

FileHeader ReadFileHeader(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd) {
  FileHeader header;

  // §D.5 P0 canvas-size guard: reject NaN / Inf / non-positive / oversized
  // values BEFORE caller wires them into a tgfx::Surface (where w*h*4 may
  // overflow into a small allocation paired with a large write).
  float w = stream->readFloat();
  float h = stream->readFloat();
  if (!std::isfinite(w) || !std::isfinite(h) || w <= 0.0f || h <= 0.0f ||
      w > static_cast<float>(limits::MAX_CANVAS_DIM) ||
      h > static_cast<float>(limits::MAX_CANVAS_DIM)) {
    ctx->error(ErrorCode::MalformedTag, "FileHeader canvas size out of range");
    return header;  // default-constructed
  }
  header.width = w;
  header.height = h;

  header.backgroundColor = ReadColor(stream);
  header.frameRate.numerator = stream->readInt32();
  header.frameRate.denominator = stream->readUint32();
  // Defend against /0 in Ratio::value(): a zero denominator would crash any
  // downstream FPS calculation. Decoder downgrades to {24, 1} default.
  if (header.frameRate.denominator == 0) {
    ctx->warn(ErrorCode::MalformedTag, "FileHeader frameRate denominator == 0; reset to {24,1}");
    header.frameRate = {24, 1};
  }
  header.duration = stream->readUint32();

  // The body has no trailing bytes today; tagEnd alignment is enforced by
  // the dispatcher's setPosition(tagEnd) on return.
  (void)tagEnd;
  return header;
}

// =============================================================================
// Composition (TagCode = 5)
// =============================================================================

void WriteComposition(::pag::EncodeStream* stream, const Composition& comp,
                      EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  WriteUtf8String(&body, comp.id);
  body.writeUint32(comp.width);
  body.writeUint32(comp.height);
  body.writeEncodedUint32(static_cast<uint32_t>(comp.layers.size()));
  for (const auto& layer : comp.layers) {
    WriteLayerBlock(&body, *layer, session);
  }
  WriteTag(stream, TagCode::Composition, &body);
}

std::unique_ptr<Composition> ReadComposition(::pag::DecodeStream* stream, DecodeContext* ctx,
                                             uint64_t tagEnd, size_t totalCompositionCount) {
  auto comp = std::make_unique<Composition>();
  auto guard = MakeGuard(ctx);

  comp->id = ReadUtf8String(stream, &guard, limits::MAX_NAME_STRING_BYTES);
  if (ctx->hasError()) {
    return nullptr;
  }

  uint32_t w = stream->readUint32();
  uint32_t h = stream->readUint32();
  // §D.7 P2-11: reject 0 — a 0×0 root surface triggers tgfx UB downstream.
  // Warn + clamp to 1 so the decode can continue (the rest of the body may
  // still hold valuable diagnostics for the caller).
  if (w == 0) {
    ctx->warn(ErrorCode::MalformedTag, "Composition width == 0; reset to 1");
    w = 1;
  }
  if (h == 0) {
    ctx->warn(ErrorCode::MalformedTag, "Composition height == 0; reset to 1");
    h = 1;
  }
  comp->width = w;
  comp->height = h;

  uint32_t layerCount = ReadEncodedUint32Safe(stream, &guard);
  if (layerCount > limits::MAX_LAYERS_PER_COMPOSITION) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "Composition layerCount exceeds MAX_LAYERS_PER_COMPOSITION");
    return nullptr;
  }

  for (uint32_t i = 0; i < layerCount; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "Composition truncated before all LayerBlock tags read");
      return nullptr;
    }
    TagHeader header = ReadTagHeader(stream);
    if (header.code != TagCode::LayerBlock) {
      ctx->error(ErrorCode::MalformedTag, "Composition expected nested LayerBlock tag");
      return nullptr;
    }
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return nullptr;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "LayerBlock tag extends past Composition body");
      return nullptr;
    }
    auto layer = ReadLayerBlock(stream, ctx, childEnd, totalCompositionCount);
    if (ctx->hasError()) {
      return nullptr;
    }
    if (layer != nullptr) {
      comp->layers.push_back(std::move(layer));
    }
    SeekTo(stream, static_cast<uint32_t>(childEnd));
  }

  return comp;
}

// =============================================================================
// CompositionList (TagCode = 4)
// =============================================================================

void WriteCompositionList(::pag::EncodeStream* stream,
                          const std::vector<std::unique_ptr<Composition>>& compositions,
                          EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(compositions.size()));
  for (const auto& comp : compositions) {
    WriteComposition(&body, *comp, session);
  }
  WriteTag(stream, TagCode::CompositionList, &body);
}

void ReadCompositionList(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                         std::vector<std::unique_ptr<Composition>>* out) {
  auto guard = MakeGuard(ctx);
  uint32_t count = ReadEncodedUint32Safe(stream, &guard);
  if (count > limits::MAX_COMPOSITIONS) {
    ctx->error(ErrorCode::StructureLimitExceeded, "CompositionList count exceeds MAX_COMPOSITIONS");
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData,
                 "CompositionList truncated before all compositions read");
      return;
    }
    uint64_t childStart = stream->position();
    TagHeader header = ReadTagHeader(stream);
    if (header.code != TagCode::Composition) {
      ctx->error(ErrorCode::MalformedTag, "CompositionList expected nested Composition tag");
      return;
    }
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "Composition tag extends past CompositionList body");
      return;
    }
    // §D.6 forward-reference support: CompositionRef payloads may target any
    // composition in the already-known total count (Encode writes it up
    // front in the CompositionList header), including compositions that
    // haven't been decoded yet. Pass the total rather than the running
    // `out->size()` so forward refs (e.g. root composition referring to a
    // sibling that serialises later) resolve cleanly. Phase 11.5 fix — prior
    // to this, every multi-composition document fatal-ed at decode time
    // with `InvalidCompositionIndex=306`.
    auto comp = ReadComposition(stream, ctx, childEnd, count);
    if (ctx->hasError()) {
      return;
    }
    if (comp != nullptr) {
      out->push_back(std::move(comp));
    }
    // Force-align to childEnd in case ReadComposition left bytes behind
    // (Phase 4a always does, since we don't decode layers yet).
    SeekTo(stream, static_cast<uint32_t>(childEnd));
    (void)childStart;
  }
}

}  // namespace pagx::pag
