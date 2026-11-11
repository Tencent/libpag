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

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/VideoExporter.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationState.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateRegion.h"

namespace pag {
namespace {

std::shared_ptr<pagx::PAGXDocument> MakeStaticDocument() {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer>\n"
      "    <Layer id=\"static\">\n"
      "      <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "      <Fill color=\"#FF0000\"/>\n"
      "    </Layer>\n"
      "  </Layer>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeNestedAnimationDocument(
    const std::string& frameRate = "60") {
  return pagx::PAGXImporter::FromXML(
      "<pagx width=\"60\" height=\"60\">\n"
      "  <Resources>\n"
      "    <Composition id=\"card\" width=\"60\" height=\"60\">\n"
      "      <Layer>\n"
      "        <Rectangle position=\"30,30\" size=\"60,60\"/>\n"
      "        <Fill><SolidColor id=\"cardColor\" color=\"#00FF00\"/></Fill>\n"
      "      </Layer>\n"
      "      <Animations>\n"
      "        <Animation id=\"cardShift\" duration=\"20\" frameRate=\"" +
      frameRate +
      "\" loop=\"once\">\n"
      "          <Object target=\"cardColor\">\n"
      "            <Channel name=\"color\" type=\"color\">\n"
      "              <Key time=\"0\" value=\"#00FF00\" interpolation=\"hold\"/>\n"
      "              <Key time=\"10\" value=\"#FF0000\" interpolation=\"hold\"/>\n"
      "            </Channel>\n"
      "          </Object>\n"
      "        </Animation>\n"
      "      </Animations>\n"
      "    </Composition>\n"
      "  </Resources>\n"
      "  <Layer width=\"60\" height=\"60\" composition=\"@card\">\n"
      "    <Timelines><Animation ref=\"@cardShift\"/></Timelines>\n"
      "  </Layer>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeStateMachineDocument() {
  auto document = MakeStaticDocument();
  auto animation = document->makeNode<pagx::Animation>("stateAnimation");
  animation->duration = 10;
  animation->frameRate = 10.0f;
  animation->loop = pagx::LoopMode::Once;
  auto object = document->makeNode<pagx::AnimationObject>();
  object->target = "static";
  animation->objects.push_back(object);
  auto channel = document->makeNode<pagx::TypedChannel<float>>();
  channel->name = "x";
  channel->keyframes.push_back({0, 100.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  channel->keyframes.push_back({10, 200.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(channel);

  auto stateMachine = document->makeNode<pagx::StateMachine>("stateMachine");
  document->animations.push_back(stateMachine);
  auto region = document->makeNode<pagx::StateRegion>();
  region->name = "main";
  region->initialState = "active";
  auto state = document->makeNode<pagx::AnimationState>();
  state->name = "active";
  state->animationId = animation->id;
  region->states.push_back(state);
  stateMachine->regions.push_back(region);
  return document;
}

std::shared_ptr<pagx::PAGXDocument> MakeAnimatedDocument(const std::string& loop,
                                                         const std::string& frameRate = "10") {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"animated\">\n"
      "    <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "    <Fill color=\"#FF0000\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"animation\" duration=\"10\" frameRate=\"" +
      frameRate + "\" loop=\"" + loop +
      "\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"x\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"100\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeMultiAnimationDocument(const std::string& firstFrameRate,
                                                               const std::string& secondFrameRate) {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"animated\">\n"
      "    <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "    <Fill color=\"#FF0000\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"first\" duration=\"10\" frameRate=\"" +
      firstFrameRate +
      "\" loop=\"once\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"x\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"100\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "    <Animation id=\"second\" duration=\"10\" frameRate=\"" +
      secondFrameRate +
      "\" loop=\"once\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"y\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"50\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n");
}

void SetDocumentFrameRate(const std::shared_ptr<pagx::PAGXDocument>& document, float frameRate) {
  auto* animation = static_cast<pagx::Animation*>(document->animations.front());
  animation->frameRate = frameRate;
}

}  // namespace

/**
 * Test case: a null document is rejected and the caller gets a message to show.
 */
PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNullDocument) {
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(nullptr, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_FALSE(errorMsg.empty());
}

/**
 * Test case: the documented 1-120 frame rate range is enforced instead of producing a broken
 * sequence, and the rejection happens before any scene or GPU resource is created.
 */
PAGX_TEST(PAGXVideoExporterTest, MakeRejectsOutOfRangeFrameRate) {
  auto document = MakeStaticDocument();
  ASSERT_TRUE(document != nullptr);
  pagx::VideoExportOptions options = {};
  options.frameRate = 240;
  std::string errorMsg = {};
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("frame rate"), std::string::npos);

  options.frameRate = -1;
  errorMsg.clear();
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("frame rate"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeExportsStaticDocumentAsSingleFrameByDefault) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeStaticDocument());
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 60);
  EXPECT_EQ(exporter->frameCount(), 1);
  ASSERT_TRUE(exporter->nextFrame());
  EXPECT_NE(exporter->pixels(), nullptr);
  EXPECT_EQ(exporter->currentFrame(), 1);
  EXPECT_FALSE(exporter->nextFrame());
}

PAGX_TEST(PAGXVideoExporterTest, MakeRepeatsStaticDocumentForExplicitDuration) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 2.0f;
  auto exporter = pagx::VideoExporter::Make(MakeStaticDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 30);
  ASSERT_EQ(exporter->frameCount(), 60);
  ASSERT_TRUE(exporter->nextFrame());
  auto frameBytes = exporter->rowBytes() * static_cast<size_t>(exporter->height());
  std::vector<uint8_t> firstFrame(exporter->pixels(), exporter->pixels() + frameBytes);
  while (exporter->nextFrame()) {
    ASSERT_EQ(std::memcmp(firstFrame.data(), exporter->pixels(), frameBytes), 0);
  }
  EXPECT_EQ(exporter->currentFrame(), 60);
}

PAGX_TEST(PAGXVideoExporterTest, DoesNotReuseFramesForNestedAnimations) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 60;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(MakeNestedAnimationDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  auto centerOffset = static_cast<size_t>(30) * exporter->rowBytes() + 30 * 4;
  ASSERT_TRUE(exporter->nextFrame());
  EXPECT_EQ(exporter->pixels()[centerOffset + 1], 255);  // Green initially.
  for (int frame = 1; frame <= 10; ++frame) {
    ASSERT_TRUE(exporter->nextFrame());
  }
  EXPECT_EQ(exporter->pixels()[centerOffset], 255);  // Red after frame 10.
  EXPECT_EQ(exporter->pixels()[centerOffset + 1], 0);
}

PAGX_TEST(PAGXVideoExporterTest, MakeUsesNestedAnimationPeriodByDefault) {
  auto exporter = pagx::VideoExporter::Make(MakeNestedAnimationDocument());
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 60);
  EXPECT_EQ(exporter->frameCount(), 20);
}

PAGX_TEST(PAGXVideoExporterTest, NestedAnimationDoesNotChangeTopLevelSamplingRate) {
  auto exporter = pagx::VideoExporter::Make(MakeNestedAnimationDocument("30"));
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 60);
  EXPECT_EQ(exporter->frameCount(), 40);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRequiresDurationForStateMachine) {
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(MakeStateMachineDocument(), {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("explicit duration"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, AdvancesTopLevelStateMachine) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 10;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(MakeStateMachineDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  ASSERT_TRUE(exporter->nextFrame());
  auto initialPixel = static_cast<size_t>(50) * exporter->rowBytes() + 180 * 4;
  EXPECT_EQ(exporter->pixels()[initialPixel + 3], 255);
  auto frameBytes = exporter->rowBytes() * static_cast<size_t>(exporter->height());
  std::vector<uint8_t> firstFrame(exporter->pixels(), exporter->pixels() + frameBytes);
  for (int frame = 1; frame < 10; ++frame) {
    ASSERT_TRUE(exporter->nextFrame());
  }
  EXPECT_NE(std::memcmp(firstFrame.data(), exporter->pixels(), frameBytes), 0);
}

PAGX_TEST(PAGXVideoExporterTest, MakeUsesTheFullPingPongPeriod) {
  auto once = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  auto pingPong = pagx::VideoExporter::Make(MakeAnimatedDocument("pingPong"));
  ASSERT_TRUE(once != nullptr);
  ASSERT_TRUE(pingPong != nullptr);
  EXPECT_EQ(once->frameRate(), 10);
  EXPECT_EQ(once->frameCount(), 10);
  EXPECT_EQ(pingPong->frameRate(), 10);
  EXPECT_EQ(pingPong->frameCount(), 20);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNonIntegerDocumentFrameRateByDefault) {
  for (const auto& frameRate : {"29.97", "0.25"}) {
    std::string errorMsg = {};
    auto exporter =
        pagx::VideoExporter::Make(MakeAnimatedDocument("once", frameRate), {}, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("must be an integer"), std::string::npos);
  }

  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once", "29.97"), options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRequiresConsistentDocumentFrameRatesByDefault) {
  auto consistent = pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", "60"));
  ASSERT_TRUE(consistent != nullptr);
  EXPECT_EQ(consistent->frameRate(), 60);

  for (const auto& secondFrameRate : {"29.97", "30"}) {
    std::string errorMsg = {};
    auto exporter =
        pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", secondFrameRate), {}, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("mixed frame rates"), std::string::npos);
  }

  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  auto overridden = pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", "29.97"), options);
  ASSERT_TRUE(overridden != nullptr);
  EXPECT_EQ(overridden->frameRate(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsEmptyAndDuplicateAnimationIds) {
  auto emptyIdDocument = MakeMultiAnimationDocument("60", "60");
  emptyIdDocument->animations.front()->id.clear();
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(emptyIdDocument, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("non-empty ids"), std::string::npos);

  auto duplicateIdDocument = MakeMultiAnimationDocument("60", "60");
  duplicateIdDocument->animations.back()->id = duplicateIdDocument->animations.front()->id;
  errorMsg.clear();
  exporter = pagx::VideoExporter::Make(duplicateIdDocument, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("duplicate top-level animation id"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsInvalidRawAnimationFrameRate) {
  for (float frameRate :
       {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), -1.0f}) {
    auto document = MakeAnimatedDocument("once");
    SetDocumentFrameRate(document, frameRate);
    pagx::VideoExportOptions options = {};
    options.frameRate = 30;
    options.duration = 1.0f;
    std::string errorMsg = {};
    auto exporter = pagx::VideoExporter::Make(document, options, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("invalid frame rate"), std::string::npos);
  }
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsOverflowingPlaybackPeriod) {
  auto document = MakeAnimatedDocument("once");
  SetDocumentFrameRate(document, std::numeric_limits<float>::min());
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 1.0f;
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(document, options, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("overflowing playback period"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeAllowsUndeclaredRateWithExplicitSamplingOptions) {
  auto document = MakeAnimatedDocument("once");
  SetDocumentFrameRate(document, 0.0f);
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(document, options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameCount(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNonFiniteDuration) {
  auto document = MakeAnimatedDocument("once");
  ASSERT_TRUE(document != nullptr);
  pagx::VideoExportOptions options = {};
  options.duration = std::numeric_limits<float>::infinity();
  std::string errorMsg = {};
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("finite"), std::string::npos);

  options.duration = std::numeric_limits<float>::quiet_NaN();
  errorMsg.clear();
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("finite"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsInvalidScale) {
  for (float scale : {std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(), 0.0f, -1.0f}) {
    pagx::VideoExportOptions options = {};
    options.scale = scale;
    std::string errorMsg = {};
    auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"), options, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("scale"), std::string::npos);
  }
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsExcessiveOutputDimensions) {
  pagx::VideoExportOptions options = {};
  options.scale = std::numeric_limits<float>::max();
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"), options, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("dimensions"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, NextFrameAdvancesSequentiallyAndStopsAtEnd) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  for (int64_t index = 0; index < exporter->frameCount(); ++index) {
    EXPECT_EQ(exporter->currentFrame(), index);
    ASSERT_TRUE(exporter->nextFrame());
    EXPECT_EQ(exporter->currentFrame(), index + 1);
    ASSERT_TRUE(exporter->pixels() != nullptr);
    EXPECT_GE(exporter->rowBytes(), static_cast<size_t>(exporter->width()) * 4);
  }
  EXPECT_FALSE(exporter->nextFrame());
  EXPECT_EQ(exporter->currentFrame(), exporter->frameCount());
}

}  // namespace pag
