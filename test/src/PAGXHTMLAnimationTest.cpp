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

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLAnimationBuilder.h"
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/TextBox.h"

namespace pag {
namespace {

using Style = std::unordered_map<std::string, std::string>;

struct AnimationInput {
  float width = 80;
  float height = 40;
  std::shared_ptr<pagx::PAGXDocument> document = pagx::PAGXDocument::Make(width, height);
  pagx::HTMLDiagnosticSink diagnostics{false};
  pagx::HTMLValueParser values{diagnostics, width, height};
  pagx::HTMLIdAllocator ids;
  pagx::HTMLAnimationBuilder builder{diagnostics, values, ids};
  std::unordered_map<std::string, pagx::html::CssKeyframesRule> rules;
  pagx::Layer* layer = document->makeNode<pagx::Layer>("box");

  AnimationInput() {
    diagnostics.bindDocument(document.get());
    values.bindDocument(document.get());
    builder.bindDocument(document.get());
    builder.setKeyframes(&rules);
    layer->width = width;
    layer->height = height;
    document->layers.push_back(layer);
    setStops({{0, "opacity:0"}, {100, "opacity:1"}});
  }

  void setStops(std::vector<pagx::html::CssKeyframeStop> stops) {
    rules["pulse"] = {"pulse", std::move(stops)};
  }

  bool build(const std::string& animation = "pulse 1s linear both") {
    return builder.buildForElement({{"animation", animation}}, layer);
  }

  bool buildSvg(const Style& style = {{"animation", "pulse 1s linear both"}},
                const std::string& shape = "shape", const std::string& fill = "fill",
                const std::string& stroke = "stroke", const std::string& rotation = "") {
    return builder.buildForInlineSvgShape(style, shape, rotation, fill, stroke, 2.0f);
  }

  bool warned(const std::string& text) const {
    return std::any_of(document->errors.begin(), document->errors.end(),
                       [&](const auto& error) { return error.find(text) != std::string::npos; });
  }

  pagx::Animation* animation() const {
    return document->animations.empty()
               ? nullptr
               : static_cast<pagx::Animation*>(document->animations.back());
  }

  pagx::Channel* channel(const std::string& name, const std::string& target = "") const {
    if (animation() == nullptr) return nullptr;
    for (const auto* object : animation()->objects) {
      if (!target.empty() && object->target != target) continue;
      for (auto* channel : object->channels) {
        if (channel->name == name) return channel;
      }
    }
    return nullptr;
  }
};

template <typename T>
T Sample(pagx::Channel* channel, pagx::Frame frame) {
  if (channel == nullptr) {
    ADD_FAILURE() << "Missing channel at frame " << frame;
    return {};
  }
  auto value = channel->evaluateAt(frame);
  auto* typed = std::get_if<T>(&value);
  if (typed == nullptr) {
    ADD_FAILURE() << "Wrong channel value type for " << channel->name;
    return {};
  }
  return *typed;
}

void ExpectMatrix(const pagx::Matrix& actual, const pagx::Matrix& expected) {
  EXPECT_NEAR(actual.a, expected.a, 1e-5f);
  EXPECT_NEAR(actual.b, expected.b, 1e-5f);
  EXPECT_NEAR(actual.c, expected.c, 1e-5f);
  EXPECT_NEAR(actual.d, expected.d, 1e-5f);
  EXPECT_NEAR(actual.tx, expected.tx, 1e-5f);
  EXPECT_NEAR(actual.ty, expected.ty, 1e-5f);
}

}  // namespace

PAG_TEST(PAGXHTMLAnimationTest, UnboundBuildersAndEmptyDeclarationsDoNotEmitAnimations) {
  AnimationInput input;
  Style style{{"animation", "pulse 1s"}};
  input.builder.bindDocument(nullptr);
  EXPECT_FALSE(input.builder.buildForElement(style, input.layer));
  EXPECT_FALSE(input.buildSvg(style));
  input.builder.bindDocument(input.document.get());
  input.builder.setKeyframes(nullptr);
  EXPECT_FALSE(input.build());
  EXPECT_FALSE(input.buildSvg());
  input.builder.setKeyframes(&input.rules);
  EXPECT_FALSE(input.builder.buildForElement(style, nullptr));
  for (const auto& empty : {Style{}, Style{{"animation", "  "}}, Style{{"animation", "none"}},
                            Style{{"animation-name", "none"}}}) {
    EXPECT_FALSE(input.builder.buildForElement(empty, input.layer));
    EXPECT_FALSE(input.buildSvg(empty));
  }
  EXPECT_TRUE(input.document->animations.empty());
}

PAG_TEST(PAGXHTMLAnimationTest, MillisecondLonghandsOverrideShorthandAndClampNegativeDelay) {
  AnimationInput input;
  ASSERT_TRUE(input.builder.buildForElement({{"animation", "missing 4s infinite paused"},
                                             {"animation-name", "pulse"},
                                             {"animation-duration", "500ms"},
                                             {"animation-delay", "-200ms"},
                                             {"animation-iteration-count", "2"},
                                             {"animation-direction", "alternate"},
                                             {"animation-fill-mode", "both"}},
                                            input.layer));
  EXPECT_EQ(input.animation()->duration, 30);
  EXPECT_EQ(input.animation()->loop, pagx::LoopMode::Once);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 15), 0.5f);
  EXPECT_TRUE(input.warned("negative animation-delay"));
  EXPECT_TRUE(input.warned("finite animation-iteration-count"));
  AnimationInput alternate;
  ASSERT_TRUE(alternate.build("pulse 1s alternate both"));
  EXPECT_EQ(alternate.animation()->loop, pagx::LoopMode::Once);
  EXPECT_TRUE(alternate.warned("finite alternate"));
}

PAG_TEST(PAGXHTMLAnimationTest, NonPositiveDurationAndIterationSuppressPlayback) {
  for (const auto& shorthand :
       {"pulse 0s", "pulse -1s", "pulse 1s 0", "pulse 1s -2", "pulse 1s infinite 0"}) {
    SCOPED_TRACE(shorthand);
    AnimationInput input;
    EXPECT_FALSE(input.build(shorthand));
    EXPECT_TRUE(input.document->animations.empty());
    EXPECT_FALSE(input.document->errors.empty());
  }
}

PAG_TEST(PAGXHTMLAnimationTest, StepTimingModesHaveCorrectValuesAtDiscontinuities) {
  struct Case {
    const char* timing;
    std::array<float, 5> values;
  };
  const Case cases[] = {
      {"steps(4)", {0, .25f, .5f, .75f, 1}},
      {"steps(4, start)", {.25f, .5f, .75f, 1, 1}},
      {"steps(4, jump-start)", {.25f, .5f, .75f, 1, 1}},
      {"steps(4, jump-none)", {0, 1.f / 3, 2.f / 3, 1, 1}},
      {"steps(4, jump-both)", {.2f, .4f, .6f, .8f, 1}},
      {"steps(1, jump-none)", {0, 0, 0, 0, 1}},
      {"step-start", {1, 1, 1, 1, 1}},
      {"step-end", {0, 0, 0, 0, 1}},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.timing);
    AnimationInput input;
    ASSERT_TRUE(input.build(std::string("pulse 1s both ") + test.timing));
    auto* channel = input.channel("alpha");
    for (int i = 0; i < 5; i++) {
      EXPECT_NEAR(Sample<float>(channel, i * 15), test.values[i], 1e-6f);
      if (i > 0) EXPECT_NEAR(Sample<float>(channel, i * 15 - 1), test.values[i - 1], 1e-6f);
    }
  }
  AnimationInput reversed;
  ASSERT_TRUE(reversed.build("pulse 1s steps(4, jump-start) reverse both"));
  EXPECT_FLOAT_EQ(Sample<float>(reversed.channel("alpha"), 0), 1);
  EXPECT_FLOAT_EQ(Sample<float>(reversed.channel("alpha"), 30), .5f);
  EXPECT_FLOAT_EQ(Sample<float>(reversed.channel("alpha"), 60), 0);
}

PAG_TEST(PAGXHTMLAnimationTest, BezierPresetsAndMalformedTimingFallback) {
  struct Case {
    const char* timing;
    float x1;
    float y1;
    float x2;
    float y2;
  };
  for (const auto& test : {Case{"ease", .25f, .1f, .25f, 1}, Case{"ease-out", 0, 0, .58f, 1},
                           Case{"ease-in-out", .42f, 0, .58f, 1}}) {
    SCOPED_TRACE(test.timing);
    AnimationInput input;
    ASSERT_TRUE(input.build(std::string("pulse 1s both ") + test.timing));
    auto* channel = input.channel("alpha");
    ASSERT_NE(channel, nullptr);
    ASSERT_EQ(channel->valueType(), pagx::ChannelValueType::Float);
    const auto& keys = static_cast<pagx::TypedChannel<float>*>(channel)->keyframes;
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0].interpolation, pagx::KeyframeInterpolationType::Bezier);
    EXPECT_FLOAT_EQ(keys[0].bezierOut.x, test.x1);
    EXPECT_FLOAT_EQ(keys[0].bezierOut.y, test.y1);
    EXPECT_FLOAT_EQ(keys[1].bezierIn.x, test.x2);
    EXPECT_FLOAT_EQ(keys[1].bezierIn.y, test.y2);
  }
  for (const auto& timing : {"steps(0)", "steps(-2)", "unknown(2)", "cubic-bezier(0,1,2)",
                             "cubic-bezier(-1,0,1,1)", "cubic-bezier(0,nan,1,1)"}) {
    SCOPED_TRACE(timing);
    AnimationInput input;
    ASSERT_TRUE(input.builder.buildForElement(
        {{"animation", "pulse 1s both"}, {"animation-timing-function", timing}}, input.layer));
    EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 30), .5f);
    if (std::string(timing).find("cubic-bezier") == 0) EXPECT_TRUE(input.warned("invalid cubic"));
  }
}

PAG_TEST(PAGXHTMLAnimationTest, SingleAndDuplicateStopsRemainPlayableWithSteps) {
  AnimationInput single;
  single.layer->alpha = .8f;
  single.setStops({{50, "opacity:.25"}});
  ASSERT_TRUE(single.build("pulse 1s steps(4)"));
  EXPECT_FLOAT_EQ(Sample<float>(single.channel("alpha"), 0), .8f);
  EXPECT_FLOAT_EQ(Sample<float>(single.channel("alpha"), 30), .25f);
  EXPECT_FLOAT_EQ(Sample<float>(single.channel("alpha"), 60), .25f);
  EXPECT_FLOAT_EQ(Sample<float>(single.channel("alpha"), 61), .8f);
  EXPECT_EQ(single.animation()->duration, 61);

  AnimationInput duplicate;
  duplicate.setStops({{0, "opacity:0"}, {0, "opacity:.4"}, {100, "opacity:1"}});
  ASSERT_TRUE(duplicate.build("pulse 1s steps(2) both"));
  EXPECT_FLOAT_EQ(Sample<float>(duplicate.channel("alpha"), 30), .7f);
  EXPECT_FLOAT_EQ(Sample<float>(duplicate.channel("alpha"), 60), 1);
}

PAG_TEST(PAGXHTMLAnimationTest, AffineTransformsPreserveExplicitOrigins) {
  struct Case {
    const char* transform;
    pagx::Matrix matrix;
  };
  const Case cases[] = {
      {"scaleX(2)", {2, 0, 0, 1, 0, 0}},
      {"scaleY(3)", {1, 0, 0, 3, 0, 0}},
      {"skewX(45deg)", {1, 0, 1, 1, 0, 0}},
      {"skewY(45deg)", {1, 1, 0, 1, 0, 0}},
      {"skew(45deg, 45deg)", {1, 1, 1, 1, 0, 0}},
      {"skew(45deg)", {1, 0, 1, 1, 0, 0}},
      {"translate(7px, 9px) scale(2, 3)", {2, 0, 0, 3, 7, 9}},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.transform);
    AnimationInput input;
    input.setStops({{0, "transform:none"}, {100, std::string("transform:") + test.transform}});
    ASSERT_TRUE(input.builder.buildForElement(
        {{"animation", "pulse 1s both"}, {"transform-origin", "left top"}}, input.layer));
    ExpectMatrix(Sample<pagx::Matrix>(input.channel("matrix"), 60), test.matrix);
    EXPECT_EQ(input.channel("x"), nullptr);
  }
  struct OriginCase {
    const char* origin;
    float x;
    float y;
  };
  for (const auto& test : {OriginCase{"right bottom", 80, 40}, OriginCase{"25% 75%", 20, 30},
                           OriginCase{"center top", 40, 0}, OriginCase{"12px 7px", 12, 7},
                           OriginCase{"invalid% 50%", 40, 20}, OriginCase{"center", 40, 20}}) {
    SCOPED_TRACE(test.origin);
    AnimationInput input;
    input.setStops({{0, "transform:none"}, {100, "transform:scale(2)"}});
    ASSERT_TRUE(input.builder.buildForElement(
        {{"animation", "pulse 1s both"}, {"transform-origin", test.origin}}, input.layer));
    ExpectMatrix(Sample<pagx::Matrix>(input.channel("matrix"), 60), {2, 0, 0, 2, -test.x, -test.y});
  }
}

PAG_TEST(PAGXHTMLAnimationTest, InvalidTransformStopsPreserveSupportedChannels) {
  for (const auto& transform :
       {"broken", "translate(50%)", "translate(1px, bad)", "translateX(20%)", "translateY(auto)",
        "scale(nan)", "scale(1, inf)", "scaleX(inf)", "scaleY(no)", "matrix(1, 0, 0, 1, nan, 0)",
        "rotateX(30deg)"}) {
    SCOPED_TRACE(transform);
    AnimationInput input;
    input.setStops({{0, "opacity:0"}, {100, std::string("opacity:1;transform:") + transform}});
    ASSERT_TRUE(input.build());
    EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 60), 1);
    EXPECT_EQ(input.channel("matrix"), nullptr);
    EXPECT_EQ(input.channel("x"), nullptr);
    EXPECT_TRUE(input.warned("unsupported 'transform'"));
  }
  AnimationInput unresolved;
  unresolved.layer->width = NAN;
  unresolved.layer->height = NAN;
  unresolved.setStops({{0, "transform:scale(1)"}, {100, "transform:scale(2)"}});
  ASSERT_TRUE(unresolved.build());
  ExpectMatrix(Sample<pagx::Matrix>(unresolved.channel("matrix"), 60), pagx::Matrix::Scale(2, 2));
  EXPECT_TRUE(unresolved.warned("without resolvable transform-origin"));
}

PAG_TEST(PAGXHTMLAnimationTest, ColorAnimationFindsNestedTextPaintAndRestoresBaseline) {
  AnimationInput input;
  auto* child = input.document->makeNode<pagx::Layer>();
  input.layer->children.push_back(child);
  auto* group = input.document->makeNode<pagx::TextBox>();
  child->contents.push_back(group);
  auto* fill = input.document->makeNode<pagx::Fill>();
  auto* color = input.document->makeNode<pagx::SolidColor>();
  color->color = {0, 1, 0, 1, pagx::ColorSpace::SRGB};
  fill->color = color;
  group->elements.push_back(fill);
  input.setStops({{0, "color:red"}, {100, "color:blue"}});
  ASSERT_TRUE(input.build("pulse 1s linear 1s"));
  auto* channel = input.channel("color", color->id);
  ASSERT_NE(channel, nullptr);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(channel, 0).green, 1);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(channel, 60).red, 1);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(channel, 120).blue, 1);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(channel, 121).green, 1);
}

PAG_TEST(PAGXHTMLAnimationTest, MissingOrTooDeepSolidFillIsDiagnosed) {
  for (bool nestedLayers : {false, true}) {
    SCOPED_TRACE(nestedLayers);
    AnimationInput input;
    auto* layer = input.layer;
    auto* elements = &layer->contents;
    for (int i = 0; i < 6; i++) {
      if (nestedLayers) {
        auto* child = input.document->makeNode<pagx::Layer>();
        layer->children.push_back(child);
        layer = child;
        elements = &layer->contents;
      } else {
        auto* group = input.document->makeNode<pagx::Group>();
        elements->push_back(group);
        elements = &group->elements;
      }
    }
    auto* fill = input.document->makeNode<pagx::Fill>();
    fill->color = input.document->makeNode<pagx::SolidColor>();
    elements->push_back(fill);
    input.setStops({{0, "color:red"}, {100, "color:blue"}});
    EXPECT_FALSE(input.build());
    EXPECT_TRUE(input.document->animations.empty());
    EXPECT_TRUE(input.warned("requires a solid"));
  }
  AnimationInput gradient;
  auto* fill = gradient.document->makeNode<pagx::Fill>();
  fill->color = gradient.document->makeNode<pagx::LinearGradient>();
  gradient.layer->contents = {nullptr, fill};
  gradient.setStops({{0, "color:red"}, {100, "color:blue"}});
  EXPECT_FALSE(gradient.build());
  EXPECT_TRUE(gradient.warned("requires a solid"));
}

PAG_TEST(PAGXHTMLAnimationTest, SteppedFiltersReuseStaticTargetsAndRestoreBaselines) {
  AnimationInput input;
  auto* shadow = input.document->makeNode<pagx::DropShadowFilter>("shadow");
  shadow->offsetX = 3;
  auto* blur = input.document->makeNode<pagx::BlurFilter>("blur");
  blur->blurX = 2;
  blur->blurY = 4;
  input.layer->filters = {shadow, blur};
  input.setStops({{0, "filter:none"},
                  {100, "filter:drop-shadow(8px 12px 6px red) blur(10px) brightness(.5)"}});
  ASSERT_TRUE(input.build("pulse 1s steps(2) 1s"));
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("offsetX", "shadow"), 0), 3);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("offsetX", "shadow"), 90), 4);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("offsetY", "shadow"), 90), 6);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("blurX", "shadow"), 90), 3);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("blurY", "shadow"), 120), 6);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(input.channel("color", "shadow"), 90).alpha, .5f);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("blurX", "blur"), 90), 5);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("blurX", "blur"), 121), 2);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("blurY", "blur"), 121), 4);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 90), .75f);
  ASSERT_EQ(input.layer->children.size(), 1u);
  EXPECT_EQ(input.layer->children.front()->filters.size(), 2u);
}

PAG_TEST(PAGXHTMLAnimationTest, UnsupportedAnimatedPropertiesKeepSupportedChannels) {
  AnimationInput input;
  input.setStops({{0, "opacity:0;filter:url(#missing) unsupported(2);width:5px"},
                  {100, "opacity:1;filter:contrast(2);width:10px"}});
  ASSERT_TRUE(input.build());
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 30), .5f);
  EXPECT_TRUE(input.layer->filters.empty());
  EXPECT_TRUE(input.warned("animated filter component"));
  EXPECT_TRUE(input.warned("animated property 'width'"));
}

PAG_TEST(PAGXHTMLAnimationTest, CurvedClipMorphAnimatesBothAxesAndReusesMask) {
  AnimationInput input;
  auto* mask = input.document->makeNode<pagx::Layer>();
  input.layer->mask = mask;
  input.layer->children.push_back(mask);
  input.setStops({{0, "clip-path:path('M0 0 Q10 20 30 40 C40 50 60 70 80 90 Z')"},
                  {100, "clip-path:path('M2 4 Q12 24 32 44 C42 54 62 74 82 94 Z')"}});
  ASSERT_TRUE(input.build("pulse 1s steps(2) both"));
  EXPECT_EQ(input.layer->mask, mask);
  EXPECT_EQ(input.layer->maskType, pagx::MaskType::Contour);
  ASSERT_EQ(mask->contents.size(), 2u);
  ASSERT_EQ(mask->contents.front()->nodeType(), pagx::NodeType::Path);
  auto* path = static_cast<pagx::Path*>(mask->contents.front());
  ASSERT_NE(path->data, nullptr);
  EXPECT_EQ(path->data->points().size(), 6u);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("point0.x", path->id), 30), 1);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("point0.y", path->id), 30), 2);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("point5.x", path->id), 60), 82);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("point5.y", path->id), 60), 94);
}

PAG_TEST(PAGXHTMLAnimationTest, IncompatibleClipMorphsAreDroppedWithDiagnostic) {
  for (const auto& end : {"none", "circle(20px)", "path('M0 0 L1 1 L2 2 Z')", "path(noquote)"}) {
    SCOPED_TRACE(end);
    AnimationInput input;
    input.setStops({{0, "opacity:0;clip-path:path('M0 0 L1 1 Z')"},
                    {100, std::string("opacity:1;clip-path:") + end}});
    ASSERT_TRUE(input.build());
    EXPECT_EQ(input.layer->mask, nullptr);
    EXPECT_TRUE(input.warned("compatible shape"));
    EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha"), 60), 1);
  }
}

PAG_TEST(PAGXHTMLAnimationTest, SvgPaintChannelsClampAlphaAndScaleDashDistance) {
  AnimationInput input;
  input.setStops({{0,
                   "opacity:-1;fill:red;fill-opacity:-2;stroke:blue;stroke-opacity:2;"
                   "stroke-dashoffset:10;transform:translate(4px, 6px)"},
                  {100,
                   "opacity:2;fill:blue;fill-opacity:2;stroke:red;stroke-opacity:-2;"
                   "stroke-dashoffset:0;transform:translate(8px, 10px)"}});
  ASSERT_TRUE(input.buildSvg({{"animation", "pulse 1s steps(2) 1s"}}));
  EXPECT_EQ(input.animation()->duration, 121);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha", "shape"), 90), .5f);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha", "fill"), 90), .5f);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha", "stroke"), 90), .5f);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("dashOffset", "stroke"), 90), 10);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("dashOffset", "stroke"), 121), 20);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("x", "shape"), 0), 0);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("x", "shape"), 90), 6);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("y", "shape"), 90), 8);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("y", "shape"), 121), 0);
  auto fill = Sample<pagx::Color>(input.channel("color", "fill"), 120);
  EXPECT_FLOAT_EQ(fill.blue, 1);
  EXPECT_FLOAT_EQ(fill.red, 0);
  auto stroke = Sample<pagx::Color>(input.channel("color", "stroke"), 120);
  EXPECT_FLOAT_EQ(stroke.red, 1);
  EXPECT_FLOAT_EQ(stroke.blue, 0);
}

PAG_TEST(PAGXHTMLAnimationTest, SvgLonghandListsUseFirstEntryAndReverseEasing) {
  AnimationInput input;
  ASSERT_TRUE(input.buildSvg({{"animation-name", "pulse, missing"},
                              {"animation-duration", "500ms, 2s"},
                              {"animation-timing-function", "ease-in"},
                              {"animation-direction", "alternate-reverse"},
                              {"animation-iteration-count", "infinite"}}));
  EXPECT_EQ(input.animation()->duration, 30);
  EXPECT_EQ(input.animation()->loop, pagx::LoopMode::PingPong);
  EXPECT_TRUE(input.warned("animation-multiple"));
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha", "shape"), 0), 1);
  EXPECT_FLOAT_EQ(Sample<float>(input.channel("alpha", "shape"), 30), 0);
  EXPECT_LT(Sample<float>(input.channel("alpha", "shape"), 15), .5f);
}

PAG_TEST(PAGXHTMLAnimationTest, SvgAnimationListKeepsIndependentDurationsAndTargets) {
  AnimationInput input;
  input.rules["paint"] = {"paint", {{0, "fill:red"}, {100, "fill:blue"}}};
  ASSERT_TRUE(input.buildSvg({{"animation",
                               "missing 1s, pulse 1s linear infinite running, "
                               "paint 2s linear forwards paused"}}));
  ASSERT_EQ(input.document->animations.size(), 2u);
  auto* first = static_cast<pagx::Animation*>(input.document->animations[0]);
  EXPECT_EQ(first->duration, 60);
  EXPECT_EQ(first->loop, pagx::LoopMode::Loop);
  EXPECT_EQ(input.animation()->duration, 120);
  EXPECT_EQ(input.animation()->loop, pagx::LoopMode::Once);
  EXPECT_FLOAT_EQ(Sample<pagx::Color>(input.channel("color", "fill"), 120).blue, 1);
  EXPECT_TRUE(input.warned("unknown @keyframes 'missing'"));
}

PAG_TEST(PAGXHTMLAnimationTest, SvgMissingTargetsAndInvalidValuesDoNotRegisterEmptyAnimations) {
  AnimationInput input;
  input.setStops({{0, "opacity:0;fill:red;stroke:blue;stroke-dashoffset:10"},
                  {100, "opacity:1;fill:blue;stroke:red;stroke-dashoffset:0"}});
  EXPECT_FALSE(input.buildSvg({{"animation", "pulse 1s both"}}, "", "", ""));
  EXPECT_TRUE(input.document->animations.empty());
  EXPECT_TRUE(input.warned("no shape layer"));
  EXPECT_TRUE(input.warned("no fill painter"));
  EXPECT_TRUE(input.warned("no stroke painter"));

  input.setStops({{0,
                   "opacity:bad;fill-opacity:bad;stroke-opacity:bad;stroke-dashoffset:bad;"
                   "transform:scale(1)"},
                  {100, "transform:rotateX(90deg)"}});
  EXPECT_FALSE(input.buildSvg());
  EXPECT_TRUE(input.document->animations.empty());
  EXPECT_TRUE(input.warned("unsupported inline-svg 'transform'"));
}

PAG_TEST(PAGXHTMLAnimationTest, SvgAffineAndRotationChannelsUseCorrectPivotAndFillMode) {
  AnimationInput input;
  input.setStops({{0, "transform:scale(1)"}, {100, "transform:scale(2)"}});
  ASSERT_TRUE(input.buildSvg(
      {{"animation", "pulse 1s steps(2) 1s"}, {"transform-origin", "10px 20px"}}, "shape", "", ""));
  ExpectMatrix(Sample<pagx::Matrix>(input.channel("matrix"), 60), pagx::Matrix::Identity());
  ExpectMatrix(Sample<pagx::Matrix>(input.channel("matrix"), 120), {2, 0, 0, 2, -10, -20});
  ExpectMatrix(Sample<pagx::Matrix>(input.channel("matrix"), 121), pagx::Matrix::Identity());

  AnimationInput unresolved;
  unresolved.setStops({{0, "transform:scale(1)"}, {100, "transform:scale(2)"}});
  ASSERT_TRUE(
      unresolved.buildSvg({{"animation", "pulse 1s both"}, {"transform-origin", "right bottom"}}));
  ExpectMatrix(Sample<pagx::Matrix>(unresolved.channel("matrix"), 60), pagx::Matrix::Scale(2, 2));
  EXPECT_TRUE(unresolved.warned("unresolvable transform-origin"));

  AnimationInput rotation;
  rotation.setStops({{0, "transform:rotate(0deg)"}, {100, "transform:rotate(90deg)"}});
  ASSERT_TRUE(
      rotation.buildSvg({{"animation", "pulse 1s steps(2) both"}}, "shape", "", "", "pivot"));
  EXPECT_FLOAT_EQ(Sample<float>(rotation.channel("rotation", "pivot"), 30), 45);
  EXPECT_EQ(rotation.channel("matrix"), nullptr);
}

}  // namespace pag
