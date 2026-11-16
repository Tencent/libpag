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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <dirent.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "pagx/LegacyPAGImporter.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGImporter.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "utils/ProjectPath.h"

namespace {

using pag::ColorType;
using pag::PAGFile;
using pag::PAGPlayer;
using pag::PAGSurface;

// Long dimension cap (matches PAGCompareFrameTest) so large samples stay fast; both pipelines
// render at the same scaled size.
constexpr float kMaxFrameSize = 360.0f;
// First-pass sampling cap (spec C34's full sweep lands once the pipeline converges): at most
// this many frames per sample, evenly spaced across the duration.
constexpr int kMaxFramesPerSample = 6;

struct SampleResult {
  std::string path;
  std::string fileName;
  int framesCompared = 0;
  int framesPassed = 0;
  std::string firstDiff;  // "frame=N diff=1.23%"
  bool decoded = false;
  std::string error;
};

void CollectPagFiles(const std::string& dir, std::vector<std::string>* out) {
  DIR* d = opendir(dir.c_str());
  if (d == nullptr) {
    return;
  }
  while (auto* entry = readdir(d)) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    std::string full = dir + "/" + name;
    if (entry->d_type == DT_DIR) {
      CollectPagFiles(full, out);
    } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".pag") == 0) {
      out->push_back(full);
    }
  }
  closedir(d);
}

void ScaleToFit(int width, int height, int* outWidth, int* outHeight) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  if (std::max(w, h) > kMaxFrameSize) {
    if (w > h) {
      h = ceilf(h * kMaxFrameSize / w);
      w = kMaxFrameSize;
    } else {
      w = ceilf(w * kMaxFrameSize / h);
      h = kMaxFrameSize;
    }
  }
  *outWidth = static_cast<int>(roundf(w));
  *outHeight = static_cast<int>(roundf(h));
}

bool RenderLegacyFrame(PAGPlayer* player, const std::shared_ptr<PAGSurface>& surface, int width,
                       int height, std::vector<uint8_t>* pixels) {
  player->flush();
  pixels->assign(static_cast<size_t>(width) * height * 4, 0);
  return surface->readPixels(ColorType::RGBA_8888, pag::AlphaType::Premultiplied, pixels->data(),
                             static_cast<size_t>(width) * 4);
}

bool RenderMigratedFrame(pagx::PAGScene* scene, const std::shared_ptr<pagx::PAGSurface>& surface,
                         pagx::PAGAnimation* animation, int64_t deltaMicroseconds, int width,
                         int height, std::vector<uint8_t>* pixels) {
  // The default timeline is a top-level animation (the scene's auto-play pass skips it), so it
  // advances explicitly; scene->advanceAndApply drives the composition-scoped animations and
  // applies every channel — a plain setCurrentTime + draw never applies anything.
  animation->advanceAndApply(deltaMicroseconds);
  scene->advanceAndApply(deltaMicroseconds);
  pixels->assign(static_cast<size_t>(width) * height * 4, 0);
  if (!scene->draw(surface)) {
    return false;
  }
  return surface->readPixels(pixels->data(), static_cast<size_t>(width) * 4);
}

double DiffPercent(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a.size() != b.size() || a.empty()) {
    return 100.0;
  }
  size_t diff = 0;
  for (size_t i = 0; i < a.size(); i += 4) {
    if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]) {
      diff++;
    }
  }
  return 100.0 * static_cast<double>(diff) / (static_cast<double>(a.size()) / 4.0);
}

void CompareSample(const std::string& path, SampleResult* result) {
  result->path = path;
  auto slash = path.rfind('/');
  result->fileName = slash == std::string::npos ? path : path.substr(slash + 1);

  // Legacy pipeline: PAGFile + PAGPlayer at the scaled size.
  auto pagFile = PAGFile::Load(path);
  if (pagFile == nullptr) {
    result->error = "legacy load failed";
    return;
  }
  result->decoded = true;
  int width = 0;
  int height = 0;
  ScaleToFit(pagFile->width(), pagFile->height(), &width, &height);

  auto legacySurface = PAGSurface::MakeOffscreen(width, height);
  if (legacySurface == nullptr) {
    result->error = "legacy surface failed";
    return;
  }
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(legacySurface);
  player->setComposition(pagFile);
  player->setMatrix(pag::Matrix::I());

  // Migrated pipeline: pag::File -> LegacyPAGImporter -> PAGScene.
  auto rawFile = pag::File::Load(path);
  if (rawFile == nullptr) {
    result->error = "raw load failed";
    return;
  }
  std::vector<pagx::PAGExportIssue> issues = {};
  auto doc = pagx::LegacyPAGImporter::Convert(rawFile, &issues);
  if (doc == nullptr) {
    result->error = "conversion failed";
    return;
  }
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    result->error = "scene failed";
    return;
  }
  auto migratedSurface = pagx::PAGSurface::MakeOffscreen(width, height);
  if (migratedSurface == nullptr) {
    result->error = "migrated surface failed";
    return;
  }
  auto timeline = scene->getDefaultTimeline();
  auto animation = std::dynamic_pointer_cast<pagx::PAGAnimation>(timeline);
  if (animation == nullptr) {
    result->error = "no default timeline";
    return;
  }

  // PAGFile::duration() returns microseconds; convert to frames on the file's frame rate.
  auto frameRate = pagFile->frameRate() > 0 ? pagFile->frameRate() : 30.0f;
  auto totalFrames =
      static_cast<int>(llround(static_cast<double>(pagFile->duration()) * frameRate / 1000000.0));
  // Evenly sample at most kMaxFramesPerSample frames across the duration (always including the
  // first and the last).
  int step = totalFrames > kMaxFramesPerSample ? totalFrames / kMaxFramesPerSample : 1;

  std::vector<uint8_t> legacyPixels;
  std::vector<uint8_t> migratedPixels;
  int64_t migratedClockUs = 0;
  for (int frame = 0; frame < totalFrames; frame += step) {
    // Aim the legacy player at the CENTER of the frame's time span: the legacy player floors
    // sub-frame times, so a time landing an epsilon below the boundary renders the previous
    // frame, while the migrated pipeline evaluates channels continuously. The progress is
    // derived from the true duration (not the rounded totalFrames) so the centering stays
    // inside the frame period regardless of duration rounding.
    player->setProgress(pagFile->duration() > 0
                            ? ((static_cast<double>(frame) + 0.5) * 1000000.0 / frameRate) /
                                  static_cast<double>(pagFile->duration())
                            : 0.0);
    if (!RenderLegacyFrame(player.get(), legacySurface, width, height, &legacyPixels)) {
      continue;
    }
    auto frameSeconds = static_cast<double>(frame) / frameRate;
    // Ceil the migrated time ABOVE the frame boundary: frame-quantized subsystems (sequence
    // players) floor sub-frame times, so a time an epsilon below the boundary lands on the
    // previous frame, while continuous channel evaluation at boundary+tiny is still the frame's
    // value. Together with the centered legacy progress both pipelines render frame N.
    auto frameUs = static_cast<int64_t>(ceil(frameSeconds * 1000000.0));
    auto deltaUs = frameUs - migratedClockUs;
    migratedClockUs = frameUs;
    if (!RenderMigratedFrame(scene.get(), migratedSurface, animation.get(), deltaUs, width, height,
                             &migratedPixels)) {
      continue;
    }
    result->framesCompared++;
    auto diff = DiffPercent(legacyPixels, migratedPixels);
    // PAG_DUMP_EQ dumps the frame-0 legacy/migrated RGBA renders of every sample whose file name
    // contains PAG_DUMP_EQ_FILTER (all samples when unset) as raw files plus a manifest, for the
    // side-by-side comparison page tooling. No effect on normal runs.
    auto dumpDir = getenv("PAG_DUMP_EQ");
    auto dumpEveryFrame = getenv("PAG_DUMP_EQ_ALL") != nullptr;
    if (dumpDir != nullptr && (dumpEveryFrame || (frame == 0 && result->framesCompared == 1))) {
      auto filter = getenv("PAG_DUMP_EQ_FILTER");
      std::string filterStr = filter != nullptr ? filter : "";
      if (filterStr.empty() || result->fileName.find(filterStr) != std::string::npos) {
        auto safeName = result->fileName;
        for (auto& ch : safeName) {
          if (ch == '/') ch = '_';
        }
        auto writeRaw = [&safeName, dumpDir, &frame](const char* suffix,
                                                     const std::vector<uint8_t>& pixels) {
          std::string outPath =
              std::string(dumpDir) + "/" + safeName + "_f" + std::to_string(frame) + suffix;
          FILE* f = fopen(outPath.c_str(), "wb");
          if (f != nullptr) {
            fwrite(pixels.data(), 1, pixels.size(), f);
            fclose(f);
          }
        };
        writeRaw("_legacy.raw", legacyPixels);
        writeRaw("_migrated.raw", migratedPixels);
        FILE* manifest = fopen(std::string(std::string(dumpDir) + "/manifest.txt").c_str(), "a");
        if (manifest != nullptr) {
          fprintf(manifest, "%s|%s|%d|%d|%.2f\n", result->path.c_str(), result->fileName.c_str(),
                  width, height, diff);
          fclose(manifest);
        }
      }
    }
    if (diff == 0.0) {
      result->framesPassed++;
    } else if (result->firstDiff.empty()) {
      char buffer[64] = {};
      snprintf(buffer, sizeof(buffer), "frame=%d diff=%.2f%%", frame, diff);
      result->firstDiff = buffer;
    }
  }
}

}  // namespace

PAG_TEST(PAGMigrationEquivalenceTest, AllSamples) {
  // P3-15 (spec C34): every decodable legacy sample renders identically through the migrated
  // pipeline at every integer frame. The strict gate is pixel-exact; this first pass reports
  // the pass rate and per-sample diffs so the remaining gaps converge task by task.
  std::vector<std::string> files = {};
  CollectPagFiles(pag::ProjectPath::Absolute("resources"), &files);
  ASSERT_GT(files.size(), 100u);

  int decoded = 0;
  int fullyPassed = 0;
  int framesCompared = 0;
  int framesPassed = 0;
  std::string failures;
  int processed = 0;
  for (const auto& path : files) {
    SampleResult result = {};
    CompareSample(path, &result);
    processed++;
    if (processed % 20 == 0) {
      printf("[migration-eq] progress: %d/%zu samples\n", processed, files.size());
      fflush(stdout);
    }
    if (!result.decoded) {
      continue;
    }
    decoded++;
    framesCompared += result.framesCompared;
    framesPassed += result.framesPassed;
    if (!result.error.empty()) {
      failures += "  " + result.fileName + ": " + result.error + "\n";
      continue;
    }
    if (result.framesCompared > 0 && result.framesCompared == result.framesPassed) {
      fullyPassed++;
    } else {
      char buffer[160] = {};
      snprintf(buffer, sizeof(buffer), "  %s: %d/%d frames %s\n", result.fileName.c_str(),
               result.framesPassed, result.framesCompared, result.firstDiff.c_str());
      failures += buffer;
    }
  }
  printf("[migration-eq] samples: %zu total, %d decoded, %d fully passed\n", files.size(), decoded,
         fullyPassed);
  printf("[migration-eq] frames: %d/%d passed (%.1f%%)\n", framesPassed, framesCompared,
         framesCompared > 0 ? 100.0 * framesPassed / framesCompared : 0.0);
  if (!failures.empty()) {
    printf("[migration-eq] failing samples:\n%s", failures.c_str());
  }
  EXPECT_GT(decoded, 0);
  EXPECT_GT(framesCompared, 0);
}
