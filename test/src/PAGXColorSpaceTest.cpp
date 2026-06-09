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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/types/Color.h"
#include "pagx/utils/ColorSpaceUtils.h"
#include "utils/TestUtils.h"

namespace pag {

/**
 * Test case: ConvertColorSpace returns a bit-identical copy when the source and target spaces match,
 * short-circuiting before any gamut math runs.
 */
PAGX_TEST(PAGXColorSpaceTest, ConvertSameSpacePassthrough) {
  pagx::Color input{0.3f, 0.6f, 0.9f, 0.8f, pagx::ColorSpace::SRGB};
  auto result = pagx::ConvertColorSpace(input, pagx::ColorSpace::SRGB);
  EXPECT_EQ(result.red, input.red);
  EXPECT_EQ(result.green, input.green);
  EXPECT_EQ(result.blue, input.blue);
  EXPECT_EQ(result.alpha, input.alpha);
  EXPECT_EQ(result.colorSpace, input.colorSpace);
}

/**
 * Test case: ConvertColorSpace preserves the alpha channel verbatim and stamps the target space onto
 * the result, since the gamut transform only touches the RGB components.
 */
PAGX_TEST(PAGXColorSpaceTest, ConvertPreservesAlphaAndStampsTarget) {
  pagx::Color input{0.5f, 0.5f, 0.5f, 0.5f, pagx::ColorSpace::SRGB};
  auto result = pagx::ConvertColorSpace(input, pagx::ColorSpace::DisplayP3);
  EXPECT_FLOAT_EQ(result.alpha, 0.5f);
  EXPECT_EQ(result.colorSpace, pagx::ColorSpace::DisplayP3);
}

/**
 * Test case: pure white and black sit on the achromatic axis, so the SRGB->DisplayP3 gamut transform
 * leaves them fixed. A naive encoded-space matrix multiply would not preserve them as cleanly.
 */
PAGX_TEST(PAGXColorSpaceTest, ConvertWhiteBlackGamutInvariant) {
  pagx::Color white{1.0f, 1.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  auto p3White = pagx::ConvertColorSpace(white, pagx::ColorSpace::DisplayP3);
  EXPECT_NEAR(p3White.red, 1.0f, 1e-3f);
  EXPECT_NEAR(p3White.green, 1.0f, 1e-3f);
  EXPECT_NEAR(p3White.blue, 1.0f, 1e-3f);

  pagx::Color black{0.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  auto p3Black = pagx::ConvertColorSpace(black, pagx::ColorSpace::DisplayP3);
  EXPECT_NEAR(p3Black.red, 0.0f, 1e-3f);
  EXPECT_NEAR(p3Black.green, 0.0f, 1e-3f);
  EXPECT_NEAR(p3Black.blue, 0.0f, 1e-3f);
}

/**
 * Test case: a mid-tone color survives an SRGB->DisplayP3->SRGB round-trip within tight tolerance.
 * The linear-light decode/transform/re-encode path is invertible, so the original channels are
 * recovered; a bare encoded-space multiply would drift on mid-tones.
 */
PAGX_TEST(PAGXColorSpaceTest, ConvertRoundTripStability) {
  pagx::Color original{0.4f, 0.6f, 0.2f, 1.0f, pagx::ColorSpace::SRGB};
  auto p3 = pagx::ConvertColorSpace(original, pagx::ColorSpace::DisplayP3);
  auto back = pagx::ConvertColorSpace(p3, pagx::ColorSpace::SRGB);
  EXPECT_NEAR(back.red, original.red, 1e-2f);
  EXPECT_NEAR(back.green, original.green, 1e-2f);
  EXPECT_NEAR(back.blue, original.blue, 1e-2f);
}

/**
 * Test case: LerpColor at t=0 returns the start color unchanged, including its color space.
 */
PAGX_TEST(PAGXColorSpaceTest, LerpAtZeroReturnsStart) {
  pagx::Color a{0.2f, 0.4f, 0.6f, 0.8f, pagx::ColorSpace::SRGB};
  pagx::Color b{0.9f, 0.1f, 0.5f, 0.3f, pagx::ColorSpace::SRGB};
  auto result = pagx::LerpColor(a, b, 0.0);
  EXPECT_FLOAT_EQ(result.red, a.red);
  EXPECT_FLOAT_EQ(result.green, a.green);
  EXPECT_FLOAT_EQ(result.blue, a.blue);
  EXPECT_FLOAT_EQ(result.alpha, a.alpha);
  EXPECT_EQ(result.colorSpace, a.colorSpace);
}

/**
 * Test case: LerpColor at t=1 with same-space endpoints converges onto b's channels and stays in
 * the shared color space.
 */
PAGX_TEST(PAGXColorSpaceTest, LerpAtOneReturnsEnd) {
  pagx::Color a{0.2f, 0.4f, 0.6f, 0.8f, pagx::ColorSpace::SRGB};
  pagx::Color b{0.9f, 0.1f, 0.5f, 0.3f, pagx::ColorSpace::SRGB};
  auto result = pagx::LerpColor(a, b, 1.0);
  EXPECT_NEAR(result.red, b.red, 1e-4f);
  EXPECT_NEAR(result.green, b.green, 1e-4f);
  EXPECT_NEAR(result.blue, b.blue, 1e-4f);
  EXPECT_NEAR(result.alpha, b.alpha, 1e-4f);
  EXPECT_EQ(result.colorSpace, pagx::ColorSpace::SRGB);
}

/**
 * Test case: LerpColor always reports the start color's space, even when the endpoints differ —
 * b is converted into a's space before blending.
 */
PAGX_TEST(PAGXColorSpaceTest, LerpResultStaysInStartSpace) {
  pagx::Color a{0.2f, 0.4f, 0.6f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::Color b{0.9f, 0.1f, 0.5f, 1.0f, pagx::ColorSpace::DisplayP3};
  auto result = pagx::LerpColor(a, b, 0.5);
  EXPECT_EQ(result.colorSpace, pagx::ColorSpace::SRGB);
}

/**
 * Test case: LerpColor at the midpoint of two same-space endpoints averages each channel.
 */
PAGX_TEST(PAGXColorSpaceTest, LerpMidpointSameSpace) {
  pagx::Color a{0.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::Color b{1.0f, 1.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  auto result = pagx::LerpColor(a, b, 0.5);
  EXPECT_NEAR(result.red, 0.5f, 1e-5f);
  EXPECT_NEAR(result.green, 0.5f, 1e-5f);
  EXPECT_NEAR(result.blue, 0.5f, 1e-5f);
  EXPECT_NEAR(result.alpha, 1.0f, 1e-5f);
}

/**
 * Test case: LerpColor interpolates the alpha channel alongside the color components.
 */
PAGX_TEST(PAGXColorSpaceTest, LerpInterpolatesAlpha) {
  pagx::Color a{0.0f, 0.0f, 0.0f, 0.0f, pagx::ColorSpace::SRGB};
  pagx::Color b{0.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  auto result = pagx::LerpColor(a, b, 0.25);
  EXPECT_NEAR(result.alpha, 0.25f, 1e-5f);
}

}  // namespace pag
