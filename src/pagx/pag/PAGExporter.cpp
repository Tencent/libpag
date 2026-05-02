// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 10 — Public export API. Wires `Bake → Codec::Encode → Result` with
// strict-mode warning promotion and atomic ToFile writes. Keeps the header
// `include/pagx/PAGExporter.h` tgfx-render-free by doing all encode /
// filesystem work here (implementation file — free to include anything).

#include "pagx/PAGExporter.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <process.h>  // _getpid
#else
#include <unistd.h>  // getpid
#endif

#include "pag/types.h"
#include "pagx/PAGXDocument.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"

namespace pagx {

namespace {

// Promote Warning-severity diagnostics to errors in place. Used when
// Options::strict is true — both Baker (200-299) and Codec (400-499)
// warnings qualify. Inflater warnings (600-699) cannot appear on the
// export path, so no stage filtering is needed here.
void PromoteWarningsToErrors(std::vector<Diagnostic>* errors, std::vector<Diagnostic>* warnings) {
  if (warnings->empty()) {
    return;
  }
  errors->reserve(errors->size() + warnings->size());
  for (auto& w : *warnings) {
    errors->push_back(std::move(w));
  }
  warnings->clear();
}

// Build a per-call unique temporary path next to `filePath`. Pairs pid with
// a high-resolution timestamp so concurrent ToFile calls from different
// threads or processes don't collide on the same suffix. Returned path
// inherits the parent directory of `filePath`, so no extra permissions
// check is needed.
std::string MakeTempPath(const std::string& filePath) {
  std::filesystem::path p(filePath);
  const auto pid = static_cast<unsigned long long>(
#ifdef _WIN32
      ::_getpid());
#else
      ::getpid());
#endif
  const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
  const auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
  char suffix[96];
  std::snprintf(suffix, sizeof(suffix), ".tmp-%llu-%llx-%lld", pid,
                static_cast<unsigned long long>(tid), static_cast<long long>(ts));
  return (p.parent_path() / (p.filename().string() + suffix)).string();
}

// Writes `bytes` to `path` as a single contiguous blob. Returns false with
// `errOut` populated on any failure. `errOut` messages are human-readable;
// callers wrap them in a Diagnostic (ProducerFatal=106 is the closest
// 100-series code covering "couldn't produce the output file", the design
// doc doesn't define a dedicated "WriteFailed" at the Exporter boundary so
// we reuse it for export-time I/O failures).
bool WriteFileAtomically(const std::string& finalPath, const uint8_t* data, size_t length,
                         std::string* errOut) {
  std::error_code ec;
  // Verify parent directory exists before touching anything.
  auto parent = std::filesystem::path(finalPath).parent_path();
  if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
    *errOut = "parent directory does not exist: " + parent.string();
    return false;
  }

  const std::string tempPath = MakeTempPath(finalPath);
  {
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      *errOut = "failed to open temporary file for writing: " + tempPath;
      return false;
    }
    if (length > 0) {
      out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
      if (!out.good()) {
        out.close();
        std::filesystem::remove(tempPath, ec);
        *errOut = "write error on temporary file: " + tempPath;
        return false;
      }
    }
    out.close();
    if (out.fail()) {
      std::filesystem::remove(tempPath, ec);
      *errOut = "close error on temporary file: " + tempPath;
      return false;
    }
  }

  // Atomic rename. filesystem::rename is POSIX rename(2) on Unix and
  // MoveFileEx on Windows — both are atomic within a single filesystem.
  std::filesystem::rename(tempPath, finalPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    *errOut = "rename to final path failed: " + finalPath + " (" + ec.message() + ")";
    return false;
  }
  return true;
}

// Shared pipeline: Bake → Encode. Returns bytes (possibly empty on fatal)
// with errors/warnings populated per the aggregation rules. Strict
// promotion and bytes-clearing happen at the call site after this returns.
struct PipelineOutput {
  std::vector<uint8_t> bytes;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};

PipelineOutput RunPipeline(const PAGXDocument& document) {
  PipelineOutput out;

  auto bake = pag::Bake(document);
  for (auto& e : bake.errors) {
    out.errors.push_back(std::move(e));
  }
  for (auto& w : bake.warnings) {
    out.warnings.push_back(std::move(w));
  }
  if (bake.doc == nullptr) {
    // Baker already surfaced the fatal code — leave bytes empty.
    return out;
  }

  auto encode = pag::Codec::Encode(*bake.doc);
  for (auto& w : encode.warnings) {
    out.warnings.push_back(std::move(w));
  }
  if (encode.bytes != nullptr && encode.bytes->length() > 0) {
    out.bytes.assign(encode.bytes->data(), encode.bytes->data() + encode.bytes->length());
  }
  return out;
}

}  // namespace

// =============================================================================
// ToBytes
// =============================================================================

PAGExporter::Result PAGExporter::ToBytes(const PAGXDocument& document, const Options& options) {
  Result result;
  auto pipe = RunPipeline(document);
  result.bytes = std::move(pipe.bytes);
  result.errors = std::move(pipe.errors);
  result.warnings = std::move(pipe.warnings);

  if (options.strict) {
    PromoteWarningsToErrors(&result.errors, &result.warnings);
  }

  if (!result.errors.empty()) {
    // Strict mode or Baker fatal — byte output is discarded so callers see
    // a well-defined "no bytes on failure" contract.
    result.bytes.clear();
  }
  result.ok = result.errors.empty();
  return result;
}

// =============================================================================
// ToFile
// =============================================================================

PAGExporter::Result PAGExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                                        const Options& options) {
  Result result;
  auto pipe = RunPipeline(document);
  result.errors = std::move(pipe.errors);
  result.warnings = std::move(pipe.warnings);

  if (options.strict) {
    PromoteWarningsToErrors(&result.errors, &result.warnings);
  }

  if (!result.errors.empty()) {
    // Don't even attempt the write — callers see a clean "no file on
    // failure" contract (no partial-write to clean up).
    result.ok = false;
    return result;
  }

  // Pipeline succeeded; attempt the atomic write.
  std::string writeErr;
  if (!WriteFileAtomically(filePath, pipe.bytes.data(), pipe.bytes.size(), &writeErr)) {
    result.errors.push_back({DiagnosticCode::ProducerFatal, std::move(writeErr), /*byteOffset=*/0,
                             /*contextIndex=*/UINT32_MAX});
    result.ok = false;
    return result;
  }

  // ToFile intentionally leaves `bytes` empty per the public docstring.
  result.ok = true;
  return result;
}

}  // namespace pagx
