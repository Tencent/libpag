// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 LayerBlock + LayerTransform + CompositionRefPayload encoders /
// decoders. Phase 4b focuses on the framework that other Bakers in
// Phase 5-8 will plug payload Tags into.
//
// Authoritative byte layouts: §D.8 (LayerBlock body + sub-Tag conventions)
// and §D.9 (LayerTransform=15, the visible/alpha/blendMode/matrix/matrix3D
// /scrollRect carrier introduced in v2.19 P0-R2).
//
// Phase 4b deliberately omits LayerMaskRef=12, LayerFilters=13,
// LayerStyles=14, and most payload Tags — those land in Phase 5/7/8 where
// the Baker side has matching producers. The byte layout already reserves
// their TagCode values, so adding readers later is purely additive.
#include "pagx/pag/CodecTags.h"
#include "pagx/pag/ElementTags.h"
#include "pagx/pag/PropertyEncoding.h"
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/ValueCodec.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

DiagnosticCollectorGuard MakeGuard(DecodeContext* ctx) {
  DiagnosticCollectorGuard g;
  g.collector = static_cast<DiagnosticCollector*>(ctx);
  return g;
}

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

// layerFlags bit positions — keep in sync with §D.8 and PAGDocument's
// non-animatable bool flags. Locally scoped because Phase 4b is the only
// site touching these bits today.
namespace flags {
constexpr uint8_t HasScrollRect = 1u << 0;
constexpr uint8_t Preserve3D = 1u << 1;
constexpr uint8_t PassThroughBackground = 1u << 2;
constexpr uint8_t AllowsEdgeAntialiasing = 1u << 3;
constexpr uint8_t AllowsGroupOpacity = 1u << 4;
}  // namespace flags

uint8_t PackLayerFlags(const Layer& layer) {
  uint8_t f = 0;
  if (layer.hasScrollRect) f |= flags::HasScrollRect;
  if (layer.preserve3D) f |= flags::Preserve3D;
  if (layer.passThroughBackground) f |= flags::PassThroughBackground;
  if (layer.allowsEdgeAntialiasing) f |= flags::AllowsEdgeAntialiasing;
  if (layer.allowsGroupOpacity) f |= flags::AllowsGroupOpacity;
  return f;
}

void UnpackLayerFlags(uint8_t f, Layer* layer) {
  layer->hasScrollRect = (f & flags::HasScrollRect) != 0;
  layer->preserve3D = (f & flags::Preserve3D) != 0;
  layer->passThroughBackground = (f & flags::PassThroughBackground) != 0;
  layer->allowsEdgeAntialiasing = (f & flags::AllowsEdgeAntialiasing) != 0;
  layer->allowsGroupOpacity = (f & flags::AllowsGroupOpacity) != 0;
  // bits 5-7 reserved — explicitly ignored so future writers can light them
  // up without breaking this Reader (§D.8 forward-compat).
}

// =============================================================================
// LayerTransform sub-Tag (TagCode = 15)
// =============================================================================

// Property defaults match the ones declared in PAGDocument::Layer so that
// the Encoder's "is default" bit and the Decoder's fallback both line up.
const tgfx::Matrix kIdentityMatrix2D = tgfx::Matrix::I();
const tgfx::Matrix3D kIdentityMatrix3D = tgfx::Matrix3D::I();

void WriteLayerTransform(::pag::EncodeStream* stream, const Layer& layer, EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  WriteProperty<bool>(&body, layer.visible, /*defaultValue=*/true);
  WriteProperty<float>(&body, layer.alpha, /*defaultValue=*/1.0f);
  WriteProperty<tgfx::BlendMode>(&body, layer.blendMode, /*defaultValue=*/tgfx::BlendMode::SrcOver);
  WriteProperty<tgfx::Matrix>(&body, layer.matrix, /*defaultValue=*/kIdentityMatrix2D);
  WriteProperty<tgfx::Matrix3D>(&body, layer.matrix3D, /*defaultValue=*/kIdentityMatrix3D);
  if (layer.hasScrollRect) {
    WriteProperty<tgfx::Rect>(&body, layer.scrollRect,
                              /*defaultValue=*/tgfx::Rect::MakeEmpty());
  }
  WriteTag(stream, TagCode::LayerTransform, &body);
  (void)session;
}

void ReadLayerTransform(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                        Layer* layer) {
  uint32_t te = static_cast<uint32_t>(tagEnd);
  layer->visible = ReadProperty<bool>(stream, /*default=*/true, te);
  if (stream->position() >= tagEnd) return;
  layer->alpha = ReadProperty<float>(stream, /*default=*/1.0f, te);
  if (stream->position() >= tagEnd) return;
  layer->blendMode =
      ReadProperty<tgfx::BlendMode>(stream, /*default=*/tgfx::BlendMode::SrcOver, te);
  if (stream->position() >= tagEnd) return;
  layer->matrix = ReadProperty<tgfx::Matrix>(stream, /*default=*/kIdentityMatrix2D, te);
  if (stream->position() >= tagEnd) return;
  layer->matrix3D = ReadProperty<tgfx::Matrix3D>(stream, /*default=*/kIdentityMatrix3D, te);
  if (layer->hasScrollRect && stream->position() < tagEnd) {
    layer->scrollRect = ReadProperty<tgfx::Rect>(stream, /*default=*/tgfx::Rect::MakeEmpty(), te);
  }
  // Phase 4b leaves any trailing bytes (future AnimationData sub-Tag, §D.14)
  // for the dispatcher's SeekTo(tagEnd) to consume.
  (void)ctx;
}

// =============================================================================
// CompositionRefPayload (TagCode = 26)
// =============================================================================

void WriteCompositionRefPayload(::pag::EncodeStream* stream, uint32_t compositionIndex,
                                EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeUint32(compositionIndex);
  WriteTag(stream, TagCode::CompositionRefPayload, &body);
  (void)session;
}

bool ReadCompositionRefPayload(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                               size_t existingCompositionCount, uint32_t* outIndex) {
  uint32_t idx = stream->readUint32();
  // §D.10 P0: refuse UINT32_MAX sentinel and any out-of-range index, so the
  // Inflater can never deref a stale slot. Phase 4b only resolves refs to
  // already-decoded compositions (forward references would need a two-pass
  // pass in Phase 9).
  if (idx == UINT32_MAX || idx >= existingCompositionCount) {
    ctx->error(ErrorCode::InvalidCompositionIndex, "CompositionRef.compositionIndex out of range");
    return false;
  }
  *outIndex = idx;
  (void)tagEnd;
  return true;
}

}  // namespace

// =============================================================================
// LayerBlock (TagCode = 10)
// =============================================================================

void WriteLayerBlock(::pag::EncodeStream* stream, const Layer& layer, EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeUint8(static_cast<uint8_t>(layer.type));
  WriteUtf8String(&body, layer.name);
  body.writeUint32(layer.startTime);
  body.writeUint32(layer.duration);
  body.writeInt32(layer.stretch.numerator);
  body.writeUint32(layer.stretch.denominator);
  body.writeUint8(PackLayerFlags(layer));

  // LayerTransform — strictly required (§D.9 P0-R2). Always emitted so the
  // Reader can rely on its presence as an alignment anchor for §4.4 rule 1.
  WriteLayerTransform(&body, layer, session);

  // Phase 5a/4b emit one payload Tag for non-Layer types. Phase 5a adds
  // VectorPayload; Phase 4b shipped CompositionRefPayload. Other types get
  // empty bodies (no payload Tag) — Phase 5-8 will fill them in.
  if (layer.type == LayerType::CompositionRef) {
    WriteCompositionRefPayload(&body, layer.compositionIndex, session);
  } else if (layer.type == LayerType::Vector && layer.vector != nullptr) {
    WriteVectorPayload(&body, *layer.vector, session);
  }

  // Children (recursive).
  body.writeEncodedUint32(static_cast<uint32_t>(layer.children.size()));
  for (const auto& child : layer.children) {
    WriteLayerBlock(&body, *child, session);
  }

  WriteTag(stream, TagCode::LayerBlock, &body);
}

std::unique_ptr<Layer> ReadLayerBlock(::pag::DecodeStream* stream, DecodeContext* ctx,
                                      uint64_t tagEnd, size_t existingCompositionCount) {
  // §H.2 LayerDepthLimitExceeded check — DecodeContext mirrors the Baker's
  // depth gate so a malicious stream can't recurse past MAX_LAYER_DEPTH.
  if (ctx->currentLayerDepth >= limits::MAX_LAYER_DEPTH) {
    ctx->error(ErrorCode::LayerDepthLimitExceeded, "LayerBlock nesting exceeds MAX_LAYER_DEPTH");
    return nullptr;
  }
  ++ctx->currentLayerDepth;

  auto layer = std::make_unique<Layer>();
  auto guard = MakeGuard(ctx);

  uint8_t typeByte = stream->readUint8();
  // Range-check via the highest known LayerType. Out-of-range values
  // degrade to LayerType::Layer with a warn (§G.5 ReadEnum convention) so
  // the layer tree still threads through.
  if (typeByte > static_cast<uint8_t>(LayerType::CompositionRef)) {
    ctx->warn(ErrorCode::InvalidEnumValue, "LayerBlock.type out of range; using Layer");
    layer->type = LayerType::Layer;
  } else {
    layer->type = static_cast<LayerType>(typeByte);
  }

  layer->name = ReadUtf8String(stream, &guard, limits::MAX_NAME_STRING_BYTES);
  if (ctx->hasError()) {
    --ctx->currentLayerDepth;
    return nullptr;
  }

  layer->startTime = stream->readUint32();
  layer->duration = stream->readUint32();
  layer->stretch.numerator = stream->readInt32();
  layer->stretch.denominator = stream->readUint32();
  if (layer->stretch.denominator == 0) {
    ctx->warn(ErrorCode::MalformedTag, "Layer.stretch denominator == 0; reset to {1,1}");
    layer->stretch = {1, 1};
  }

  uint8_t layerFlags = stream->readUint8();
  UnpackLayerFlags(layerFlags, layer.get());

  // Sub-Tag region: Phase 4b lays out LayerBlock body in a fixed order so we
  // don't need to peek bytes to disambiguate "next sub-Tag" vs
  // "childCount varU32". Order is:
  //
  //   1. LayerTransform (TagCode = 15) — required (§D.9 P0-R2)
  //   2. payload Tag    (TagCode 20-26) — present iff type != Layer
  //   3. childCount varU32 + children
  //
  // LayerMaskRef / LayerFilters / LayerStyles slots stay reserved between
  // LayerTransform and the payload — Phase 5+ inserts them by widening this
  // dispatch when the corresponding Bakers come online.

  // ---- 1. LayerTransform (required) ----
  if (stream->position() >= tagEnd) {
    ctx->error(ErrorCode::MalformedTag, "LayerBlock missing required LayerTransform sub-Tag");
    --ctx->currentLayerDepth;
    return nullptr;
  }
  TagHeader xformHeader = ReadTagHeader(stream);
  if (xformHeader.code != TagCode::LayerTransform) {
    ctx->error(ErrorCode::MalformedTag, "LayerBlock first sub-Tag must be LayerTransform=15");
    --ctx->currentLayerDepth;
    return nullptr;
  }
  uint64_t xformBodyStart = stream->position();
  uint64_t xformEnd = 0;
  if (!SafeTagEnd(stream, xformBodyStart, xformHeader.length, ctx, &xformEnd) ||
      xformEnd > tagEnd) {
    if (!ctx->hasError()) {
      ctx->error(ErrorCode::MalformedTag, "LayerTransform extends past LayerBlock body");
    }
    --ctx->currentLayerDepth;
    return nullptr;
  }
  ReadLayerTransform(stream, ctx, xformEnd, layer.get());
  SeekTo(stream, static_cast<uint32_t>(xformEnd));

  // ---- 2. Payload Tag (present iff type != Layer) ----
  if (layer->type != LayerType::Layer) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "LayerBlock missing payload Tag for non-Layer type");
      --ctx->currentLayerDepth;
      return nullptr;
    }
    TagHeader payloadHeader = ReadTagHeader(stream);
    uint64_t payloadBodyStart = stream->position();
    uint64_t payloadEnd = 0;
    if (!SafeTagEnd(stream, payloadBodyStart, payloadHeader.length, ctx, &payloadEnd) ||
        payloadEnd > tagEnd) {
      if (!ctx->hasError()) {
        ctx->error(ErrorCode::MalformedTag, "payload Tag extends past LayerBlock body");
      }
      --ctx->currentLayerDepth;
      return nullptr;
    }
    switch (payloadHeader.code) {
      case TagCode::CompositionRefPayload: {
        uint32_t idx = UINT32_MAX;
        if (!ReadCompositionRefPayload(stream, ctx, payloadEnd, existingCompositionCount, &idx)) {
          --ctx->currentLayerDepth;
          return nullptr;
        }
        layer->compositionIndex = idx;
        break;
      }
      case TagCode::VectorPayload: {
        layer->vector = ReadVectorPayload(stream, ctx, payloadEnd);
        if (ctx->hasError()) {
          --ctx->currentLayerDepth;
          return nullptr;
        }
        break;
      }
      // Phase 6-8 will plug Shape / Text / Image / Solid / Mesh payload
      // readers in here. Until then, length-skip with a warn so forward-
      // compat upgrades don't break the stream.
      case TagCode::ShapePayload:
      case TagCode::TextPayload:
      case TagCode::ImagePayload:
      case TagCode::SolidPayload:
      case TagCode::MeshPayload: {
        ctx->warn(ErrorCode::UnknownTagCode, "payload TagCode not yet decoded; skipped");
        break;
      }
      default: {
        ctx->error(ErrorCode::MalformedTag, "LayerBlock payload TagCode does not match LayerType");
        --ctx->currentLayerDepth;
        return nullptr;
      }
    }
    SeekTo(stream, static_cast<uint32_t>(payloadEnd));
  }

  // ---- 3. childCount + children ----
  if (stream->position() >= tagEnd) {
    // No room left → zero children.
    --ctx->currentLayerDepth;
    return layer;
  }
  uint32_t childCount = ReadEncodedUint32Safe(stream, &guard);
  if (childCount > limits::MAX_CHILDREN_PER_LAYER) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "LayerBlock childCount exceeds MAX_CHILDREN_PER_LAYER");
    --ctx->currentLayerDepth;
    return nullptr;
  }

  for (uint32_t i = 0; i < childCount; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "LayerBlock truncated before all children read");
      --ctx->currentLayerDepth;
      return nullptr;
    }
    TagHeader header = ReadTagHeader(stream);
    if (header.code != TagCode::LayerBlock) {
      ctx->error(ErrorCode::MalformedTag, "LayerBlock expected nested LayerBlock for child");
      --ctx->currentLayerDepth;
      return nullptr;
    }
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      --ctx->currentLayerDepth;
      return nullptr;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "child LayerBlock extends past parent body");
      --ctx->currentLayerDepth;
      return nullptr;
    }
    auto child = ReadLayerBlock(stream, ctx, childEnd, existingCompositionCount);
    if (ctx->hasError()) {
      --ctx->currentLayerDepth;
      return nullptr;
    }
    if (child != nullptr) {
      layer->children.push_back(std::move(child));
    }
    SeekTo(stream, static_cast<uint32_t>(childEnd));
  }

  --ctx->currentLayerDepth;
  return layer;
}

}  // namespace pagx::pag
