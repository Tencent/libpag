/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "PAGTest.h"
#include <vector>
#include "pagx/FontConfig.h"
#include "pagx/pag/FontProvider.h"
#include "tgfx/core/Typeface.h"
#include "utils/ProjectPath.h"

namespace pag {
static bool hasFailure = false;

bool PAGTest::HasFailure() {
  return hasFailure;
}

void PAGTest::SetUp() {
}

void PAGTest::TearDown() {
  hasFailure = hasFailure || ::testing::Test::HasFailure();
}

namespace {

// Standard fallback fonts used by PAGXTest fixtures: Noto Sans SC covers
// CJK, Noto Color Emoji covers emoji, Noto Sans Hebrew covers Hebrew. Order
// matters — the LayoutContext walks this list in sequence when shaping
// codepoints that the primary font can't render.
void AddFallbackFonts(pagx::FontConfig& fontConfig) {
  const char* paths[] = {
      "resources/font/NotoSansSC-Regular.otf",
      "resources/font/NotoColorEmoji.ttf",
      "resources/font/NotoSansHebrew-Regular.ttf",
  };
  for (const char* relPath : paths) {
    fontConfig.addFallbackFont(ProjectPath::Absolute(relPath), 0);
  }
}

// macOS-only: register the four most common Arial faces so PAGX samples that
// reference Arial by name (every spec/samples/text*.pagx) resolve to the
// actual system font rather than falling through to Noto. Linux / CI Android
// containers rarely ship Arial; we silently skip on those platforms. The Web
// build has its own font registration path outside of PAGXTest.
void RegisterSystemFonts(pagx::FontConfig& fontConfig) {
#ifdef __APPLE__
  const char* arialPaths[] = {
      "/System/Library/Fonts/Supplemental/Arial.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
      "/System/Library/Fonts/Supplemental/Arial Italic.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf",
  };
  for (const char* path : arialPaths) {
    fontConfig.registerFont(path, 0);
  }
#else
  (void)fontConfig;
#endif
}

}  // namespace

void PAGXTest::SetUp() {
  PAGTest::SetUp();
  device = tgfx::GLDevice::Make();
  ASSERT_TRUE(device != nullptr);
  context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  sharedFontConfig = std::make_shared<pagx::FontConfig>();
  AddFallbackFonts(*sharedFontConfig);
  RegisterSystemFonts(*sharedFontConfig);
  sharedFontProvider = pagx::pag::MakeFontProviderFromConfig(sharedFontConfig);
}

void PAGXTest::TearDown() {
  if (device) {
    device->unlock();
  }
  sharedFontProvider.reset();
  sharedFontConfig.reset();
  PAGTest::TearDown();
}

pagx::FontConfig& PAGXTest::fontConfig() {
  return *sharedFontConfig;
}

std::shared_ptr<pagx::pag::FontProvider> PAGXTest::fontProvider() {
  return sharedFontProvider;
}

}  // namespace pag
