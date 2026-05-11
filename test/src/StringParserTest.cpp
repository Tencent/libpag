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
#include <limits>
#include "base/PAGTest.h"
#include "pagx/utils/StringParser.h"

namespace pag {

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
  // 21/255, the alpha that previously emitted as "0.0823529".
  EXPECT_EQ(pagx::CssFloatToString(21.0f / 255.0f), "0.082");
}

CLI_TEST(StringParserTest, CssFloatToString_AngleSmallFraction) {
  // Previously "-1.90915" (six significant digits).
  EXPECT_EQ(pagx::CssFloatToString(-1.90915f), "-1.909");
}

CLI_TEST(StringParserTest, CssFloatToString_AngleTwoDecimal) {
  // Already-short value, unchanged after zero-strip.
  EXPECT_EQ(pagx::CssFloatToString(-11.31f), "-11.31");
}

CLI_TEST(StringParserTest, CssFloatToString_MatrixCoeff45Deg) {
  // sin/cos of 45° truncated from 0.7071067 to 3 dp.
  EXPECT_EQ(pagx::CssFloatToString(0.7071067f), "0.707");
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
