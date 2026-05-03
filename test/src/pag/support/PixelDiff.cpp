// Copyright (C) 2026 Tencent. All rights reserved.
//
// See PixelDiff.h for contract.

#include "PixelDiff.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"
#include "utils/ProjectPath.h"

namespace pagx::test {

std::vector<uint8_t> ReadSurfacePixels(const std::shared_ptr<tgfx::Surface>& surface) {
  if (surface == nullptr) {
    return {};
  }
  tgfx::Bitmap bitmap(surface->width(), surface->height());
  if (bitmap.isEmpty()) {
    return {};
  }
  tgfx::Pixmap pixmap(bitmap);
  pixmap.clear();
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return {};
  }
  const size_t byteCount = pixmap.info().byteSize();
  std::vector<uint8_t> out(byteCount);
  std::memcpy(out.data(), pixmap.pixels(), byteCount);
  return out;
}

double ComputePSNR(const std::vector<uint8_t>& pixelsA, const std::vector<uint8_t>& pixelsB,
                   int width, int height) {
  if (width <= 0 || height <= 0) {
    return -std::numeric_limits<double>::infinity();
  }
  const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  if (pixelsA.size() != expected || pixelsB.size() != expected) {
    return -std::numeric_limits<double>::infinity();
  }

  // Per-channel squared error summation in double to avoid uint32 overflow
  // on large canvases (e.g. 4096×4096 × 4 channels × 255² already ≈ 4e9).
  double sumSq = 0.0;
  for (size_t i = 0; i < expected; ++i) {
    const int delta = static_cast<int>(pixelsA[i]) - static_cast<int>(pixelsB[i]);
    sumSq += static_cast<double>(delta) * static_cast<double>(delta);
  }
  if (sumSq == 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  const double mse = sumSq / static_cast<double>(expected);
  // PSNR = 20 * log10(MAX / sqrt(MSE)); MAX = 255 for u8 pixels.
  return 20.0 * std::log10(255.0 / std::sqrt(mse));
}

namespace {

// Histogram buckets by |delta|: [0], [1-3], [4-10], [11-50], [>50].
struct ChannelBuckets {
  size_t exact = 0;
  size_t small = 0;
  size_t medium = 0;
  size_t large = 0;
  size_t huge = 0;
};

void BucketDelta(ChannelBuckets& b, int delta) {
  const int abs = delta < 0 ? -delta : delta;
  if (abs == 0) {
    ++b.exact;
  } else if (abs <= 3) {
    ++b.small;
  } else if (abs <= 10) {
    ++b.medium;
  } else if (abs <= 50) {
    ++b.large;
  } else {
    ++b.huge;
  }
}

void WriteChannel(std::ostream& os, const char* name, const ChannelBuckets& b) {
  os << "channel " << name << ": "
     << "0=" << b.exact << "  "
     << "1-3=" << b.small << "  "
     << "4-10=" << b.medium << "  "
     << "11-50=" << b.large << "  "
     << ">50=" << b.huge << "\n";
}

}  // namespace

void DumpPixelDiffHistogram(const std::vector<uint8_t>& pixelsA,
                            const std::vector<uint8_t>& pixelsB, const std::string& sampleName) {
  if (pixelsA.size() != pixelsB.size() || pixelsA.empty()) {
    return;
  }
  ChannelBuckets r{};
  ChannelBuckets g{};
  ChannelBuckets b{};
  ChannelBuckets a{};
  // Assume native tgfx ColorType is 4-channel (RGBA_8888 or BGRA_8888).
  // Histogram presentation is channel-order-agnostic — whichever channel
  // is first gets labelled R, etc. Good enough for an advisory dump.
  for (size_t i = 0; i + 3 < pixelsA.size(); i += 4) {
    BucketDelta(r, static_cast<int>(pixelsA[i]) - static_cast<int>(pixelsB[i]));
    BucketDelta(g, static_cast<int>(pixelsA[i + 1]) - static_cast<int>(pixelsB[i + 1]));
    BucketDelta(b, static_cast<int>(pixelsA[i + 2]) - static_cast<int>(pixelsB[i + 2]));
    BucketDelta(a, static_cast<int>(pixelsA[i + 3]) - static_cast<int>(pixelsB[i + 3]));
  }

  const auto outDir = pag::ProjectPath::Absolute("test/out/PixelDiff");
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    return;
  }
  const auto outPath = std::filesystem::path(outDir) / (sampleName + "_histogram.txt");
  std::ofstream os(outPath);
  if (!os.is_open()) {
    return;
  }

  os << "=== " << sampleName << " diff histogram (4 channels × 5 buckets) ===\n";
  WriteChannel(os, "R", r);
  WriteChannel(os, "G", g);
  WriteChannel(os, "B", b);
  WriteChannel(os, "A", a);
}

}  // namespace pagx::test
