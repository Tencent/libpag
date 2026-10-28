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

#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>
#include "base/PAGTest.h"
#include "pagx/utils/StringParser.h"

namespace pag {

static void ExpectFloatRoundTrip(float value) {
  auto serialized = pagx::FloatToString(value);
  SCOPED_TRACE(serialized);
  EXPECT_FALSE(serialized.empty());
  EXPECT_EQ(serialized.find_first_not_of("-0123456789."), std::string::npos);
  char* end = nullptr;
  auto restored = std::strtof(serialized.c_str(), &end);
  EXPECT_EQ(end, serialized.c_str() + serialized.size());
  EXPECT_EQ(restored, value);
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
        ExpectFloatRoundTrip(value);
      }
    }
  }
  EXPECT_EQ(pagx::FloatToString(-0.f), "0");
  EXPECT_EQ(pagx::FloatToString(NAN), "0");
  EXPECT_EQ(pagx::FloatToString(std::numeric_limits<float>::infinity()), "0");
  EXPECT_EQ(pagx::FloatToString(-std::numeric_limits<float>::infinity()), "0");
}

CLI_TEST(StringParserTest, FloatToStringKeepsCommonDecimalsCompact) {
  EXPECT_EQ(pagx::FloatToString(0.f), "0");
  EXPECT_EQ(pagx::FloatToString(1.f), "1");
  EXPECT_EQ(pagx::FloatToString(-0.1f), "-0.1");
  EXPECT_EQ(pagx::FloatToString(12.5f), "12.5");
  EXPECT_EQ(pagx::FloatToString(0.000001f), "0.000001");
  EXPECT_EQ(pagx::FloatToString(1000000.f), "1000000");
  ExpectFloatRoundTrip(3372.84375f);
  ExpectFloatRoundTrip(598853.375f);
}

CLI_TEST(StringParserTest, FloatToStringPreservesValuesAroundDecimalBoundaries) {
  for (int exponent = -45; exponent <= 38; ++exponent) {
    auto boundary = static_cast<float>(std::pow(10.0, exponent));
    for (auto value : {std::nextafter(boundary, 0.f), boundary,
                       std::nextafter(boundary, std::numeric_limits<float>::infinity())}) {
      ExpectFloatRoundTrip(value);
      ExpectFloatRoundTrip(-value);
    }
  }
  // Deterministically sample mantissas beyond the representative patterns above.
  uint32_t bits = 0x12345678;
  for (int i = 0; i < 10000; ++i) {
    bits ^= bits << 13;
    bits ^= bits >> 17;
    bits ^= bits << 5;
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (std::isfinite(value)) {
      ExpectFloatRoundTrip(value);
    }
  }
}

CLI_TEST(StringParserTest, FloatToStringUsesDecimalPointInOtherNumericLocales) {
  std::string originalLocale = std::setlocale(LC_NUMERIC, nullptr);
  bool localeAvailable = false;
  for (const char* locale : {"de_DE.UTF-8", "fr_FR.UTF-8", "de_DE", "fr_FR"}) {
    if (std::setlocale(LC_NUMERIC, locale) != nullptr) {
      localeAvailable = true;
      break;
    }
  }
  if (!localeAvailable) {
    GTEST_SKIP() << "No decimal-comma locale is installed";
  }
  EXPECT_EQ(pagx::FloatToString(12.5f), "12.5");
  for (float value :
       {3372.84375f, -3372.84375f, 1.25e-20f, -1.25e20f, std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::max()}) {
    auto serialized = pagx::FloatToString(value);
    SCOPED_TRACE(serialized);
    EXPECT_EQ(serialized.find_first_not_of("-0123456789."), std::string::npos);
    std::istringstream input(serialized);
    input.imbue(std::locale::classic());
    // Parse in double's range so valid float subnormals do not set the stream's failbit.
    double restored = 0;
    input >> restored;
    EXPECT_FALSE(input.fail());
    EXPECT_TRUE(input.eof());
    EXPECT_EQ(static_cast<float>(restored), value);
  }
  EXPECT_NE(std::setlocale(LC_NUMERIC, originalLocale.c_str()), nullptr);
}

// CssFloatToString: fixed 4-decimal-place format with trailing-zero stripping.
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
