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
//  license is distributed on "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the details.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/utils/StringParser.h"

namespace pag {

CLI_TEST(StringParserTest, DocumentDimensionsRoundTripWithoutPrecisionLoss) {
  for (float value : {3372.84375f, 10000000.0f, 0.0000001f}) {
    auto document = pagx::PAGXDocument::Make(value, 100);
    auto xml = pagx::PAGXExporter::ToXML(*document);
    auto restored = pagx::PAGXImporter::FromXML(xml);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->width, value) << xml;
    EXPECT_EQ(xml.find("e+"), std::string::npos) << xml;
    EXPECT_EQ(xml.find("e-"), std::string::npos) << xml;
  }
}

CLI_TEST(StringParserTest, FloatToStringPreservesFiniteValuesWithoutExponentNotation) {
  // Sample every finite exponent and representative mantissas, including subnormals and the
  // largest finite float. This checks the serialization contract across the entire float range.
  for (uint32_t exponent = 0; exponent < 255; ++exponent) {
    for (uint32_t mantissa : {0u, 1u, 0x123456u, 0x400000u, 0x7fffffu}) {
      for (uint32_t sign : {0u, 0x80000000u}) {
        auto bits = sign | (exponent << 23) | mantissa;
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        auto serialized = pagx::FloatToString(value);
        SCOPED_TRACE(serialized);
        EXPECT_EQ(serialized.find_first_not_of("-0123456789."), std::string::npos);
        char* end = nullptr;
        auto restored = std::strtof(serialized.c_str(), &end);
        EXPECT_EQ(end, serialized.c_str() + serialized.size());
        EXPECT_EQ(restored, value);
      }
    }
  }
  EXPECT_EQ(pagx::FloatToString(-0.f), "0");
  EXPECT_EQ(pagx::FloatToString(NAN), "0");
  EXPECT_EQ(pagx::FloatToString(std::numeric_limits<float>::infinity()), "0");
  EXPECT_EQ(pagx::FloatToString(-std::numeric_limits<float>::infinity()), "0");
}

// CssFloatToString: fixed 3-decimal-place format with trailing-zero stripping.
// Used for every numeric CSS/HTML value other than px coordinates (which go through
// CoordToString with 2 dp). Produces visually-equivalent output with consistent
// precision instead of %g's variable-width significant-digit behaviour.

CLI_TEST(StringParserTest, CssFloatToString_Integer) {
  EXPECT_EQ(pagx::CssFloatToString(1.0f), "1");
}

CLI_TEST(StringParserTest, CssFloatToString_Zero) {
  EXPECT_EQ(pagx::CssFloatToString(0.0f), "0");
}

CLI_TEST(StringParserTest, CssFloatToString_NegativeZero) {
  EXPECT_EQ(pagx::CssFloatToString(-0.0f), "0");
}

CLI_TEST(StringParserTest, CssFloatToString_Half) {
  EXPECT_EQ(pagx::CssFloatToString(0.5f), "0.5");
}

CLI_TEST(StringParserTest, CssFloatToString_AlphaQuantized) {
  // 21/255 ≈ 0.08235, rounded to 4 decimal places.
  EXPECT_EQ(pagx::CssFloatToString(21.0f / 255.0f), "0.0824");
}

CLI_TEST(StringParserTest, CssFloatToString_AngleSmallFraction) {
  // -1.90915 rounded to 4 decimal places.
  EXPECT_EQ(pagx::CssFloatToString(-1.90915f), "-1.9092");
}

CLI_TEST(StringParserTest, CssFloatToString_AngleTwoDecimal) {
  // Already-short value, unchanged after zero-strip.
  EXPECT_EQ(pagx::CssFloatToString(-11.31f), "-11.31");
}

CLI_TEST(StringParserTest, CssFloatToString_MatrixCoeff45Deg) {
  // sin/cos of 45° ≈ 0.7071067, rounded to 4 decimal places.
  EXPECT_EQ(pagx::CssFloatToString(0.7071067f), "0.7071");
}

CLI_TEST(StringParserTest, CssFloatToString_PathLargeMagnitude) {
  // Larger numbers keep their integer part untouched.
  EXPECT_EQ(pagx::CssFloatToString(100.123f), "100.123");
}

CLI_TEST(StringParserTest, CssFloatToString_NaN) {
  EXPECT_EQ(pagx::CssFloatToString(std::numeric_limits<float>::quiet_NaN()), "0");
}

CLI_TEST(StringParserTest, CssFloatToString_Infinity) {
  EXPECT_EQ(pagx::CssFloatToString(std::numeric_limits<float>::infinity()), "0");
  EXPECT_EQ(pagx::CssFloatToString(-std::numeric_limits<float>::infinity()), "0");
}

CLI_TEST(StringParserTest, CssFloatToString_StripsTrailingZeros) {
  EXPECT_EQ(pagx::CssFloatToString(2.5f), "2.5");
  EXPECT_EQ(pagx::CssFloatToString(2.50f), "2.5");
}

CLI_TEST(StringParserTest, CssFloatToString_StripsAllZerosAndDot) {
  EXPECT_EQ(pagx::CssFloatToString(5.0f), "5");
}

CLI_TEST(StringParserTest, CssFloatToString_NegativeFraction) {
  EXPECT_EQ(pagx::CssFloatToString(-0.123f), "-0.123");
}

}  // namespace pag
