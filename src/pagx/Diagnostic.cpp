// Copyright (C) 2026 Tencent. All rights reserved.
//
// Implementation for the public Diagnostic.h API. Authoritative spec:
// docs/pagx_to_pag_v2_design.md §G.3bis.
#include "pagx/Diagnostic.h"
#include <cstdio>
#include <string>

namespace pagx {

// Hand-written switch (NOT an X-macro) so the enum⇄string mapping is greppable.
// Returning nullptr from the default arm lets FormatDiagnostic emit the
// "Code(<num>)" forward-compatibility fallback for ABI-appended values that
// the running binary does not yet recognise.
static const char* CodeToString(DiagnosticCode c) {
  switch (c) {
    case DiagnosticCode::Ok:
      return "Ok";
    case DiagnosticCode::LayoutNotApplied:
      return "LayoutNotApplied";
    case DiagnosticCode::UnresolvedImports:
      return "UnresolvedImports";
    case DiagnosticCode::NullDocument:
      return "NullDocument";
    case DiagnosticCode::EmptyCompositions:
      return "EmptyCompositions";
    case DiagnosticCode::CompositionCycleDepth:
      return "CompositionCycleDepth";
    case DiagnosticCode::StructureLimitExceeded:
      return "StructureLimitExceeded";
    case DiagnosticCode::ProducerFatal:
      return "ProducerFatal";
    case DiagnosticCode::ImageSourceMissing:
      return "ImageSourceMissing";
    case DiagnosticCode::FontSourceMissing:
      return "FontSourceMissing";
    case DiagnosticCode::MaskTargetMissing:
      return "MaskTargetMissing";
    case DiagnosticCode::MaskSelfReference:
      return "MaskSelfReference";
    case DiagnosticCode::BlendModeUnmapped:
      return "BlendModeUnmapped";
    case DiagnosticCode::InverseMatrixNonInvertible:
      return "InverseMatrixNonInvertible";
    case DiagnosticCode::TextGlyphDataEmpty:
      return "TextGlyphDataEmpty";
    case DiagnosticCode::EmptyDocument:
      return "EmptyDocument";
    case DiagnosticCode::GlyphRunKindInferred:
      return "GlyphRunKindInferred";
    case DiagnosticCode::InvalidMagic:
      return "InvalidMagic";
    case DiagnosticCode::UnsupportedVersion:
      return "UnsupportedVersion";
    case DiagnosticCode::UnsupportedCompression:
      return "UnsupportedCompression";
    case DiagnosticCode::TruncatedData:
      return "TruncatedData";
    case DiagnosticCode::MalformedTag:
      return "MalformedTag";
    case DiagnosticCode::BodyLengthOutOfRange:
      return "BodyLengthOutOfRange";
    case DiagnosticCode::InvalidCompositionIndex:
      return "InvalidCompositionIndex";
    case DiagnosticCode::FileReadFailed:
      return "FileReadFailed";
    case DiagnosticCode::UnknownTagCode:
      return "UnknownTagCode";
    case DiagnosticCode::UnknownPropertyEncoding:
      return "UnknownPropertyEncoding";
    case DiagnosticCode::ImageResourceSizeExceeded:
      return "ImageResourceSizeExceeded";
    case DiagnosticCode::StringInvalidUtf8:
      return "StringInvalidUtf8";
    case DiagnosticCode::PathVerbLimitExceeded:
      return "PathVerbLimitExceeded";
    case DiagnosticCode::GlyphCountLimitExceeded:
      return "GlyphCountLimitExceeded";
    case DiagnosticCode::LayerDepthLimitExceeded:
      return "LayerDepthLimitExceeded";
    case DiagnosticCode::InvalidEnumValue:
      return "InvalidEnumValue";
    case DiagnosticCode::FontResourceSizeExceeded:
      return "FontResourceSizeExceeded";
    case DiagnosticCode::PrematureEndTag:
      return "PrematureEndTag";
    case DiagnosticCode::InflateImageDecodeFailed:
      return "InflateImageDecodeFailed";
    case DiagnosticCode::InflateFontCreateFailed:
      return "InflateFontCreateFailed";
    case DiagnosticCode::InflateGlyphRunBuildFailed:
      return "InflateGlyphRunBuildFailed";
    case DiagnosticCode::InflateMaskResolveFailed:
      return "InflateMaskResolveFailed";
    case DiagnosticCode::InflaterEmptyDocument:
      return "InflaterEmptyDocument";
    case DiagnosticCode::InflateCompositionCycle:
      return "InflateCompositionCycle";
    case DiagnosticCode::InflaterLayerBudgetExceeded:
      return "InflaterLayerBudgetExceeded";
    case DiagnosticCode::InflateMaskCycle:
      return "InflateMaskCycle";
  }
  return nullptr;
}

std::string FormatDiagnostic(const Diagnostic& d) {
  char buf[32];
  const char* name = CodeToString(d.code);
  std::string out = "[";
  if (name != nullptr) {
    out += name;
  } else {
    std::snprintf(buf, sizeof(buf), "Code(%u)", static_cast<unsigned>(d.code));
    out += buf;
  }
  out += "]";
  if (!d.message.empty()) {
    out += ' ';
    out += d.message;
  }
  if (d.byteOffset != 0) {
    std::snprintf(buf, sizeof(buf), " @0x%x", d.byteOffset);
    out += buf;
  }
  if (d.contextIndex != UINT32_MAX) {
    std::snprintf(buf, sizeof(buf), " #ctx=%u", d.contextIndex);
    out += buf;
  }
  return out;
}

DiagnosticSeverity SeverityOf(DiagnosticCode c) {
  uint16_t v = static_cast<uint16_t>(c);
  // Fatal segments: 100-199 / 300-399 / 500-599. Warning segments:
  // 200-299 / 400-499 / 600-699. Mapping is part of the public ABI.
  if ((v >= 100 && v < 200) || (v >= 300 && v < 400) || (v >= 500 && v < 600)) {
    return DiagnosticSeverity::Fatal;
  }
  return DiagnosticSeverity::Warning;
}

DiagnosticStage StageOf(DiagnosticCode c) {
  uint16_t v = static_cast<uint16_t>(c);
  if (v >= 100 && v < 300) {
    return DiagnosticStage::Baker;
  }
  if (v >= 300 && v < 500) {
    return DiagnosticStage::Codec;
  }
  return DiagnosticStage::Inflater;  // 500-699 plus the Ok=0 fallback
}

VersionInfo Version() {
  // gitSha and buildDate live in .rodata for the process lifetime. The build
  // system does not yet inject a git SHA — placeholder until the macro is
  // wired in; consumers should treat "unknown" as informational.
#ifdef PAGX_GIT_SHA
  static constexpr const char* kGitSha = PAGX_GIT_SHA;
#else
  static constexpr const char* kGitSha = "unknown";
#endif
  static constexpr const char* kBuildDate = __DATE__;
  return VersionInfo{0x0002, kGitSha, kBuildDate};
}

}  // namespace pagx
