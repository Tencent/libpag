// Copyright (C) 2026 Tencent. All rights reserved.
//
// One-shot helper test that renders every spec/samples/*.pagx straight
// through the PAGX-native path (PAGXImporter -> applyLayout ->
// LayerBuilder -> tgfx Surface) and writes the result to
// test/out/pagx_native/{name}.webp. Paired with the existing
// test/out/PAGRenderTest_Render/{name}.webp (PAGX -> PAG -> Inflater
// output produced by PAGRenderEquivalenceTest.Render_Baseline), it gives
// a side-by-side visual-audit data set for Phase 15 baseline approval.
//
// Not a regression gate: the test succeeds as soon as the 48 files are
// written. Uses the same font fallback + system-font registration logic
// as PAGXTest so text-bearing samples render identically to the
// PAGXTest.SpecSamples baseline (which is gated by MD5 drift elsewhere).

#include <filesystem>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

namespace {

std::vector<std::shared_ptr<tgfx::Typeface>> CreateFallbackTypefaces() {
  std::vector<std::shared_ptr<tgfx::Typeface>> result = {};
  auto regularTypeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  if (regularTypeface) {
    result.push_back(regularTypeface);
  }
  auto emojiTypeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"));
  if (emojiTypeface) {
    result.push_back(emojiTypeface);
  }
  auto hebrewTypeface = tgfx::Typeface::MakeFromPath(
      ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"));
  if (hebrewTypeface) {
    result.push_back(hebrewTypeface);
  }
  return result;
}

void RegisterSystemFonts(pagx::FontConfig& fontConfig) {
#ifdef __APPLE__
  const char* arialPaths[] = {
      "/System/Library/Fonts/Supplemental/Arial.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
      "/System/Library/Fonts/Supplemental/Arial Italic.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf",
  };
  for (const char* path : arialPaths) {
    auto typeface = tgfx::Typeface::MakeFromPath(path);
    if (typeface) {
      fontConfig.registerTypeface(std::move(typeface));
    }
  }
#else
  (void)fontConfig;
#endif
}

}  // namespace

class PAGXNativeReferenceTest : public PAGXTest {};

TEST_F(PAGXNativeReferenceTest, RenderSpecSamplesToPAGXNativeDir) {
  const auto samplesDir = ProjectPath::Absolute("spec/samples");
  ASSERT_TRUE(std::filesystem::exists(samplesDir)) << "spec/samples missing: " << samplesDir;

  std::vector<std::string> files = {};
  for (const auto& entry : std::filesystem::directory_iterator(samplesDir)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_FALSE(files.empty()) << "no .pagx fixtures under " << samplesDir;

  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(CreateFallbackTypefaces());
  RegisterSystemFonts(fontConfig);

  size_t written = 0;
  for (const auto& filePath : files) {
    const auto name = std::filesystem::path(filePath).stem().string();

    auto doc = pagx::PAGXImporter::FromFile(filePath);
    if (!doc) {
      ADD_FAILURE() << "PAGXImporter::FromFile failed: " << name;
      continue;
    }
    doc->applyLayout(&fontConfig);

    auto layer = pagx::LayerBuilder::Build(doc.get());
    if (!layer) {
      ADD_FAILURE() << "LayerBuilder::Build returned null: " << name;
      continue;
    }

    const int canvasW = static_cast<int>(doc->width);
    const int canvasH = static_cast<int>(doc->height);
    if (canvasW <= 0 || canvasH <= 0) {
      ADD_FAILURE() << "invalid canvas size for " << name << ": " << canvasW << "x" << canvasH;
      continue;
    }

    auto surface = tgfx::Surface::Make(context, canvasW, canvasH);
    if (!surface) {
      ADD_FAILURE() << "Surface::Make failed for " << name;
      continue;
    }
    tgfx::DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);

    tgfx::Bitmap bitmap(canvasW, canvasH, false, false);
    tgfx::Pixmap pixmap(bitmap);
    if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
      ADD_FAILURE() << "readPixels failed for " << name;
      continue;
    }

    SaveImage(pixmap, "pagx_native/" + name);
    ++written;
  }

  EXPECT_EQ(written, files.size())
      << "expected " << files.size() << " PAGX-native references, wrote " << written;
}

}  // namespace pag
