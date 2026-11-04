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
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
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

/**
 * Test case: the paired targetLayer/outputBounds root attributes round-trip through import and
 * export, and the parsed model stores the bare target id plus the document-space window.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewRoundTrip) {
  std::string xml =
      "<pagx width=\"300\" height=\"500\" targetLayer=\"@target\" outputBounds=\"0,280,80,40\">\n"
      "  <Layer id=\"container\">\n"
      "    <Layer id=\"preceding\"/>\n"
      "    <Layer id=\"target\">\n"
      "      <Rectangle position=\"40,20\" size=\"80,40\"/>\n"
      "      <Fill color=\"#3366FF\"/>\n"
      "    </Layer>\n"
      "    <Layer id=\"later\"/>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty());
  EXPECT_EQ(doc->targetLayerId, "target");
  ASSERT_TRUE(doc->outputBounds.has_value());
  EXPECT_EQ(doc->outputBounds->x, 0.0f);
  EXPECT_EQ(doc->outputBounds->y, 280.0f);
  EXPECT_EQ(doc->outputBounds->width, 80.0f);
  EXPECT_EQ(doc->outputBounds->height, 40.0f);

  auto exported = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_NE(exported.find("targetLayer=\"@target\""), std::string::npos);
  EXPECT_NE(exported.find("outputBounds=\"0,280,80,40\""), std::string::npos);

  auto reloaded = pagx::PAGXImporter::FromXML(exported);
  ASSERT_TRUE(reloaded != nullptr);
  EXPECT_TRUE(reloaded->errors.empty());
  EXPECT_EQ(reloaded->targetLayerId, "target");
  ASSERT_TRUE(reloaded->outputBounds.has_value());
  EXPECT_EQ(reloaded->outputBounds->y, 280.0f);
}

/**
 * Test case: a single target view attribute is rejected instead of guessing a default window.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewRequiresPairedAttributes) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@target\">\n"
      "  <Layer id=\"target\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(HasError(doc, "must be specified together"));
  EXPECT_FALSE(doc->outputViewError.empty());

  std::string boundsOnly =
      "<pagx width=\"100\" height=\"100\" outputBounds=\"0,0,10,10\">\n"
      "  <Layer id=\"target\"/>\n"
      "</pagx>\n";
  auto boundsDoc = pagx::PAGXImporter::FromXML(boundsOnly);
  ASSERT_TRUE(boundsDoc != nullptr);
  EXPECT_TRUE(HasError(boundsDoc, "must be specified together"));
}

/**
 * Test case: invalid outputBounds values (non-positive size, non-numeric, trailing junk) are
 * rejected rather than producing a zero-sized or partially parsed window.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewInvalidBoundsRejected) {
  const char* bounds[] = {"0,0,0,40", "0,0,80,-1", "0,0,80,40x", "a,b,c,d", "0,0,inf,40"};
  for (const auto* value : bounds) {
    std::string xml =
        "<pagx width=\"100\" height=\"100\" targetLayer=\"@target\" "
        "outputBounds=\"" +
        std::string(value) + "\">\n  <Layer id=\"target\"/>\n</pagx>\n";
    auto doc = pagx::PAGXImporter::FromXML(xml);
    ASSERT_TRUE(doc != nullptr) << value;
    EXPECT_TRUE(HasError(doc, "Invalid value")) << value;
  }
}

/**
 * Test case: the target must be a unique Layer reachable from the document layer tree. A missing
 * node, a non-Layer node, a duplicated id, and a Target inside a Composition resource are all
 * rejected with distinct diagnostics.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewRejectsInvalidTargets) {
  std::string missing =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@nope\" outputBounds=\"0,0,10,10\">\n"
      "  <Layer id=\"target\"/>\n"
      "</pagx>\n";
  auto missingDoc = pagx::PAGXImporter::FromXML(missing);
  ASSERT_TRUE(missingDoc != nullptr);
  EXPECT_TRUE(HasError(missingDoc, "does not match any node"));

  std::string notLayer =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@shape\" outputBounds=\"0,0,10,10\">\n"
      "  <Layer id=\"target\">\n"
      "    <Rectangle id=\"shape\" position=\"5,5\" size=\"10,10\"/>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto notLayerDoc = pagx::PAGXImporter::FromXML(notLayer);
  ASSERT_TRUE(notLayerDoc != nullptr);
  EXPECT_TRUE(HasError(notLayerDoc, "does not resolve to a Layer"));

  std::string unreachable =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@inner\" outputBounds=\"0,0,10,10\">\n"
      "  <Resources>\n"
      "    <Composition id=\"comp\">\n"
      "      <Layer id=\"inner\"/>\n"
      "    </Composition>\n"
      "  </Resources>\n"
      "  <Layer id=\"host\" composition=\"@comp\"/>\n"
      "</pagx>\n";
  auto unreachableDoc = pagx::PAGXImporter::FromXML(unreachable);
  ASSERT_TRUE(unreachableDoc != nullptr);
  EXPECT_TRUE(HasError(unreachableDoc, "not reachable"));

  std::string duplicate =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@target\" outputBounds=\"0,0,10,10\">\n"
      "  <Layer id=\"target\"/>\n"
      "  <Layer id=\"target\"/>\n"
      "</pagx>\n";
  auto duplicateDoc = pagx::PAGXImporter::FromXML(duplicate);
  ASSERT_TRUE(duplicateDoc != nullptr);
  EXPECT_TRUE(HasError(duplicateDoc, "not uniquely reachable"));
}

/**
 * Test case: target view documents are static-only this round. Declaring animations or data binds
 * alongside the target fields is rejected at parse time so consumers never silently ignore motion.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewRejectsDynamicContent) {
  std::string xml =
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@target\" outputBounds=\"0,0,10,10\">\n"
      "  <Layer id=\"target\"/>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"target\">\n"
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
  EXPECT_TRUE(HasError(doc, "animations"));
}

/**
 * Test case: a target view declaration that fails to parse keeps its authored attributes through
 * export, so reloading the exported file reports the same error instead of silently becoming a
 * whole-canvas document.
 */
PAGX_TEST(PAGXImporterValidationTest, OutputViewInvalidDeclarationSurvivesExport) {
  struct Case {
    std::string declaration = {};
    std::vector<std::string> expectedAttributes = {};
  };
  const Case cases[] = {
      {"targetLayer=\"@target\"", {"targetLayer=\"@target\""}},
      {"targetLayer=\"target\" outputBounds=\"0,0,10,10\"",
       {"targetLayer=\"target\"", "outputBounds=\"0,0,10,10\""}},
      {"targetLayer=\"@target\" outputBounds=\"0,0,0,10\"",
       {"targetLayer=\"@target\"", "outputBounds=\"0,0,0,10\""}},
  };
  for (const auto& testCase : cases) {
    std::string xml = "<pagx width=\"100\" height=\"100\" " + testCase.declaration +
                      ">\n  <Layer id=\"target\"/>\n</pagx>\n";
    auto doc = pagx::PAGXImporter::FromXML(xml);
    ASSERT_TRUE(doc != nullptr) << testCase.declaration;
    EXPECT_FALSE(doc->outputViewError.empty()) << testCase.declaration;

    auto exported = pagx::PAGXExporter::ToXML(*doc);
    for (const auto& expected : testCase.expectedAttributes) {
      EXPECT_NE(exported.find(expected), std::string::npos) << testCase.declaration;
    }
    auto reloaded = pagx::PAGXImporter::FromXML(exported);
    ASSERT_TRUE(reloaded != nullptr) << testCase.declaration;
    EXPECT_FALSE(reloaded->outputViewError.empty()) << testCase.declaration;
  }
}

}  // namespace pag
