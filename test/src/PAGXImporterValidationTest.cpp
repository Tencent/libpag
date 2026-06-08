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

#include <string>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "utils/TestUtils.h"

namespace pag {

static bool HasError(const std::shared_ptr<pagx::PAGXDocument>& doc, const std::string& needle) {
  for (const auto& error : doc->errors) {
    if (error.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

/**
 * Test case: an Animation duration with trailing non-numeric characters ("123abc") is rejected by
 * GetInt64Attribute's strict trailing-char check (*endPtr != '\0'), surfacing an "Invalid value"
 * parse error rather than silently keeping the truncated 123.
 */
PAGX_TEST(PAGXImporterValidationTest, TrailingGarbageRejected) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"box\">\n"
      "    <Rectangle position=\"50,50\" size=\"80,60\"/>\n"
      "    <Fill color=\"#00FF00\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"123abc\" frameRate=\"60\">\n"
      "      <Object target=\"box\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(HasError(doc, "Invalid value"));
}

/**
 * Test case: an Animation duration that overflows int64 ("99999999999999999999999") trips strtoll's
 * ERANGE branch, producing a "Value out of range" parse error instead of an undefined clamped value.
 */
PAGX_TEST(PAGXImporterValidationTest, OutOfRangeRejected) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"box\">\n"
      "    <Rectangle position=\"50,50\" size=\"80,60\"/>\n"
      "    <Fill color=\"#00FF00\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"99999999999999999999999\" frameRate=\"60\">\n"
      "      <Object target=\"box\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(HasError(doc, "out of range"));
}

/**
 * Test case: a legitimate integer duration ("180") parses with no int64-related error, confirming
 * the strict trailing-char and ERANGE checks do not reject valid values.
 */
PAGX_TEST(PAGXImporterValidationTest, ValidDurationAccepted) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"box\">\n"
      "    <Rectangle position=\"50,50\" size=\"80,60\"/>\n"
      "    <Fill color=\"#00FF00\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"180\" frameRate=\"60\">\n"
      "      <Object target=\"box\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_FALSE(HasError(doc, "Invalid value"));
  EXPECT_FALSE(HasError(doc, "out of range"));
}

/**
 * Test case: a ColorMatrixFilter exposes only a full 20-element matrix with no animatable scalar
 * channel, so an Animation whose Object targets it is rejected at parse time with a clear error
 * rather than being silently dropped at runtime. The filter declares a valid 20-number matrix so
 * the animation rejection is the only relevant error.
 */
PAGX_TEST(PAGXImporterValidationTest, AnimatingColorMatrixRejected) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"box\">\n"
      "    <Rectangle position=\"50,50\" size=\"80,60\"/>\n"
      "    <Fill color=\"#00FF00\"/>\n"
      "    <ColorMatrixFilter id=\"cmf\" "
      "matrix=\"1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"cmf\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(HasError(doc, "Animating a ColorMatrixFilter is not supported"));
}

}  // namespace pag
