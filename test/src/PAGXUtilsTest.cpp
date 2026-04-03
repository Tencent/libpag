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

#include <cmath>
#include <cstring>
#include <fstream>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/ExporterUtils.h"
#include "utils/TestUtils.h"

namespace pag {

// ---------------------------------------------------------------------------
// CollectFillStroke
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, CollectFillStroke_Empty) {
  std::vector<pagx::Element*> contents;
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, nullptr);
  EXPECT_EQ(info.stroke, nullptr);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXUtilsTest, CollectFillStroke_FillOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill = doc->makeNode<pagx::Fill>();
  std::vector<pagx::Element*> contents = {fill};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill);
  EXPECT_EQ(info.stroke, nullptr);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXUtilsTest, CollectFillStroke_StrokeOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto stroke = doc->makeNode<pagx::Stroke>();
  std::vector<pagx::Element*> contents = {stroke};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, nullptr);
  EXPECT_EQ(info.stroke, stroke);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXUtilsTest, CollectFillStroke_AllTypes) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill = doc->makeNode<pagx::Fill>();
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  std::vector<pagx::Element*> contents = {fill, stroke, textBox};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill);
  EXPECT_EQ(info.stroke, stroke);
  EXPECT_EQ(info.textBox, textBox);
}

PAGX_TEST(PAGXUtilsTest, CollectFillStroke_FirstOfEachType) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto stroke1 = doc->makeNode<pagx::Stroke>();
  auto stroke2 = doc->makeNode<pagx::Stroke>();
  std::vector<pagx::Element*> contents = {fill1, stroke1, fill2, stroke2};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill1);
  EXPECT_EQ(info.stroke, stroke1);
}

// ---------------------------------------------------------------------------
// BuildLayerMatrix
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, BuildLayerMatrix_Identity) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_TRUE(m.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, BuildLayerMatrix_WithPosition) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 10.0f;
  layer->y = 20.0f;
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_FLOAT_EQ(m.tx, 10.0f);
  EXPECT_FLOAT_EQ(m.ty, 20.0f);
}

PAGX_TEST(PAGXUtilsTest, BuildLayerMatrix_WithMatrixAndPosition) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Scale(2.0f, 3.0f);
  layer->x = 5.0f;
  layer->y = 7.0f;
  auto m = pagx::BuildLayerMatrix(layer);
  auto expected = pagx::Matrix::Translate(5.0f, 7.0f) * pagx::Matrix::Scale(2.0f, 3.0f);
  EXPECT_FLOAT_EQ(m.a, expected.a);
  EXPECT_FLOAT_EQ(m.d, expected.d);
  EXPECT_FLOAT_EQ(m.tx, expected.tx);
  EXPECT_FLOAT_EQ(m.ty, expected.ty);
}

PAGX_TEST(PAGXUtilsTest, BuildLayerMatrix_OnlyXNonZero) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 15.0f;
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_FLOAT_EQ(m.tx, 15.0f);
  EXPECT_FLOAT_EQ(m.ty, 0.0f);
}

// ---------------------------------------------------------------------------
// BuildGroupMatrix
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_Identity) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_TRUE(m.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_PositionOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->position = {50.0f, 30.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.tx, 50.0f);
  EXPECT_FLOAT_EQ(m.ty, 30.0f);
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_ScaleOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->scale = {2.0f, 3.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.a, 2.0f);
  EXPECT_FLOAT_EQ(m.d, 3.0f);
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_RotationOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->rotation = 90.0f;
  auto m = pagx::BuildGroupMatrix(group);
  auto expected = pagx::Matrix::Rotate(90.0f);
  EXPECT_NEAR(m.a, expected.a, 1e-5f);
  EXPECT_NEAR(m.b, expected.b, 1e-5f);
  EXPECT_NEAR(m.c, expected.c, 1e-5f);
  EXPECT_NEAR(m.d, expected.d, 1e-5f);
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_AnchorOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {25.0f, 25.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.tx, -25.0f);
  EXPECT_FLOAT_EQ(m.ty, -25.0f);
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_SkewOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->skew = 30.0f;
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FALSE(m.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, BuildGroupMatrix_AnchorPositionScaleRotation) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {10.0f, 10.0f};
  group->position = {100.0f, 100.0f};
  group->scale = {2.0f, 2.0f};
  group->rotation = 45.0f;
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.tx, 100.0f);
  EXPECT_FLOAT_EQ(m.ty, 100.0f);
  EXPECT_FALSE(m.isIdentity());
}

// ---------------------------------------------------------------------------
// GetPNGDimensions
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_ValidPNG) {
  // Minimal 1x1 PNG header (24 bytes needed)
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  // PNG signature
                   0x00, 0x00, 0x00, 0x0D,                            // IHDR chunk length
                   0x49, 0x48, 0x44, 0x52,                            // "IHDR"
                   0x00, 0x00, 0x00, 0x01,                            // width = 1
                   0x00, 0x00, 0x00, 0x01};                           // height = 1
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetPNGDimensions(png, sizeof(png), &w, &h));
  EXPECT_EQ(w, 1);
  EXPECT_EQ(h, 1);
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_LargerSize) {
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                   0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                   0x00, 0x00, 0x01, 0x00,   // width = 256
                   0x00, 0x00, 0x00, 0x80};  // height = 128
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetPNGDimensions(png, sizeof(png), &w, &h));
  EXPECT_EQ(w, 256);
  EXPECT_EQ(h, 128);
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_TooSmall) {
  uint8_t data[] = {0x89, 0x50, 0x4E, 0x47};
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_InvalidSignature) {
  uint8_t data[24] = {};
  data[0] = 0xFF;
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_ZeroWidth) {
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                   0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                   0x00, 0x00, 0x00, 0x00,   // width = 0
                   0x00, 0x00, 0x00, 0x01};  // height = 1
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(png, sizeof(png), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensions_ZeroHeight) {
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                   0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                   0x00, 0x00, 0x00, 0x01,   // width = 1
                   0x00, 0x00, 0x00, 0x00};  // height = 0
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(png, sizeof(png), &w, &h));
}

// ---------------------------------------------------------------------------
// GetJPEGDimensions
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_TooSmall) {
  uint8_t data[] = {0xFF};
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_InvalidHeader) {
  uint8_t data[] = {0x00, 0x00};
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_ValidSOF0) {
  // Construct minimal JPEG with SOF0 marker
  uint8_t data[] = {
      0xFF, 0xD8,              // SOI
      0xFF, 0xC0,              // SOF0 marker
      0x00, 0x11,              // segment length = 17
      0x08,                    // precision = 8 bits
      0x00, 0x64,              // height = 100
      0x00, 0xC8,              // width = 200
      0x03,                    // number of components
      0x01, 0x11, 0x00,        // component 1
      0x02, 0x11, 0x01,        // component 2
      0x03, 0x11, 0x01,        // component 3
  };
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
  EXPECT_EQ(w, 200);
  EXPECT_EQ(h, 100);
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_SOF2Progressive) {
  uint8_t data[] = {
      0xFF, 0xD8,              // SOI
      0xFF, 0xC2,              // SOF2 (progressive)
      0x00, 0x11,              // segment length
      0x08,                    // precision
      0x00, 0x50,              // height = 80
      0x00, 0x60,              // width = 96
      0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
  };
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
  EXPECT_EQ(w, 96);
  EXPECT_EQ(h, 80);
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_SkipsNonSOFSegments) {
  // APP0 marker followed by SOF0
  uint8_t data[] = {
      0xFF, 0xD8,        // SOI
      0xFF, 0xE0,        // APP0
      0x00, 0x04,        // segment length = 4 (includes length bytes)
      0x00, 0x00,        // dummy data
      0xFF, 0xC0,        // SOF0
      0x00, 0x11, 0x08,  // length + precision
      0x00, 0x30,        // height = 48
      0x00, 0x40,        // width = 64
      0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
  };
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
  EXPECT_EQ(w, 64);
  EXPECT_EQ(h, 48);
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_HitsSOS) {
  // SOS marker before any SOF => no dimensions
  uint8_t data[] = {
      0xFF, 0xD8,  // SOI
      0xFF, 0xDA,  // SOS
      0x00, 0x04, 0x00, 0x00,
  };
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_HitsEOI) {
  uint8_t data[] = {
      0xFF, 0xD8,  // SOI
      0xFF, 0xD9,  // EOI
  };
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetJPEGDimensions_InvalidMarkerByte) {
  uint8_t data[] = {
      0xFF, 0xD8,  // SOI
      0x00, 0xC0,  // invalid: first byte should be 0xFF
  };
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetJPEGDimensions(data, sizeof(data), &w, &h));
}

// ---------------------------------------------------------------------------
// IsJPEG
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, IsJPEG_Valid) {
  uint8_t data[] = {0xFF, 0xD8, 0xFF, 0xE0};
  EXPECT_TRUE(pagx::IsJPEG(data, sizeof(data)));
}

PAGX_TEST(PAGXUtilsTest, IsJPEG_TooSmall) {
  uint8_t data[] = {0xFF};
  EXPECT_FALSE(pagx::IsJPEG(data, sizeof(data)));
}

PAGX_TEST(PAGXUtilsTest, IsJPEG_InvalidFirstByte) {
  uint8_t data[] = {0x00, 0xD8};
  EXPECT_FALSE(pagx::IsJPEG(data, sizeof(data)));
}

PAGX_TEST(PAGXUtilsTest, IsJPEG_InvalidSecondByte) {
  uint8_t data[] = {0xFF, 0x00};
  EXPECT_FALSE(pagx::IsJPEG(data, sizeof(data)));
}

PAGX_TEST(PAGXUtilsTest, IsJPEG_EmptyBuffer) {
  EXPECT_FALSE(pagx::IsJPEG(nullptr, 0));
}

// ---------------------------------------------------------------------------
// HasNonASCII
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, HasNonASCII_Empty) {
  EXPECT_FALSE(pagx::HasNonASCII(""));
}

PAGX_TEST(PAGXUtilsTest, HasNonASCII_PureASCII) {
  EXPECT_FALSE(pagx::HasNonASCII("Hello World 123!@#"));
}

PAGX_TEST(PAGXUtilsTest, HasNonASCII_WithUTF8) {
  EXPECT_TRUE(pagx::HasNonASCII("\xc3\xa9"));  // é
}

PAGX_TEST(PAGXUtilsTest, HasNonASCII_WithChinese) {
  EXPECT_TRUE(pagx::HasNonASCII("\xe4\xb8\xad\xe6\x96\x87"));  // 中文
}

PAGX_TEST(PAGXUtilsTest, HasNonASCII_HighByte) {
  std::string s(1, static_cast<char>(0x80));
  EXPECT_TRUE(pagx::HasNonASCII(s));
}

PAGX_TEST(PAGXUtilsTest, HasNonASCII_MaxASCII) {
  std::string s(1, static_cast<char>(127));
  EXPECT_FALSE(pagx::HasNonASCII(s));
}

// ---------------------------------------------------------------------------
// UTF8ToUTF16BEHex
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_Empty) {
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex(""), "");
}

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_ASCII) {
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("A"), "0041");
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("AB"), "00410042");
}

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_TwoByte) {
  // é = U+00E9, UTF-8: C3 A9
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("\xc3\xa9"), "00E9");
}

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_ThreeByte) {
  // 中 = U+4E2D, UTF-8: E4 B8 AD
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("\xe4\xb8\xad"), "4E2D");
}

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_FourByteSurrogatePair) {
  // 𝄞 = U+1D11E (Musical Symbol G Clef), UTF-8: F0 9D 84 9E
  // UTF-16 surrogate pair: D834 DD1E
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("\xf0\x9d\x84\x9e"), "D834DD1E");
}

PAGX_TEST(PAGXUtilsTest, UTF8ToUTF16BEHex_Mixed) {
  // "A中" = U+0041 U+4E2D
  EXPECT_EQ(pagx::UTF8ToUTF16BEHex("A\xe4\xb8\xad"), "00414E2D");
}

// ---------------------------------------------------------------------------
// StripQuotes
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, StripQuotes_Quoted) {
  EXPECT_EQ(pagx::StripQuotes("\"hello\""), "hello");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_NotQuoted) {
  EXPECT_EQ(pagx::StripQuotes("hello"), "hello");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_Empty) {
  EXPECT_EQ(pagx::StripQuotes(""), "");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_SingleQuote) {
  EXPECT_EQ(pagx::StripQuotes("\""), "\"");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_EmptyQuoted) {
  EXPECT_EQ(pagx::StripQuotes("\"\""), "");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_OnlyFrontQuote) {
  EXPECT_EQ(pagx::StripQuotes("\"hello"), "\"hello");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_OnlyBackQuote) {
  EXPECT_EQ(pagx::StripQuotes("hello\""), "hello\"");
}

PAGX_TEST(PAGXUtilsTest, StripQuotes_NestedQuotes) {
  EXPECT_EQ(pagx::StripQuotes("\"say \"hi\"\""), "say \"hi\"");
}

// ---------------------------------------------------------------------------
// DetectMaskFillRule
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, DetectMaskFillRule_DefaultWinding) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::Winding);
}

PAGX_TEST(PAGXUtilsTest, DetectMaskFillRule_EvenOddInContents) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  layer->contents.push_back(fill);
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::EvenOdd);
}

PAGX_TEST(PAGXUtilsTest, DetectMaskFillRule_WindingInContents) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::Winding;
  layer->contents.push_back(fill);
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::Winding);
}

PAGX_TEST(PAGXUtilsTest, DetectMaskFillRule_EvenOddInChild) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  auto child = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  child->contents.push_back(fill);
  parent->children.push_back(child);
  EXPECT_EQ(pagx::DetectMaskFillRule(parent), pagx::FillRule::EvenOdd);
}

PAGX_TEST(PAGXUtilsTest, DetectMaskFillRule_NestedChildEvenOdd) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  auto child = doc->makeNode<pagx::Layer>();
  auto grandchild = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  grandchild->contents.push_back(fill);
  child->children.push_back(grandchild);
  parent->children.push_back(child);
  EXPECT_EQ(pagx::DetectMaskFillRule(parent), pagx::FillRule::EvenOdd);
}

// ---------------------------------------------------------------------------
// GetImageData
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetImageData_NullImage) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  auto data = pagx::GetImageData(image);
  EXPECT_EQ(data, nullptr);
}

PAGX_TEST(PAGXUtilsTest, GetImageData_WithData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t raw[] = {1, 2, 3, 4};
  image->data = pagx::Data::MakeWithCopy(raw, sizeof(raw));
  auto data = pagx::GetImageData(image);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->size(), 4u);
}

PAGX_TEST(PAGXUtilsTest, GetImageData_WithEmptyFilePath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  image->filePath = "";
  auto data = pagx::GetImageData(image);
  EXPECT_EQ(data, nullptr);
}

// ---------------------------------------------------------------------------
// GetImagePNGDimensions
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetImagePNGDimensions_WithData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                   0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                   0x00, 0x00, 0x00, 0x10,   // width = 16
                   0x00, 0x00, 0x00, 0x20};  // height = 32
  image->data = pagx::Data::MakeWithCopy(png, sizeof(png));
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetImagePNGDimensions(image, &w, &h));
  EXPECT_EQ(w, 16);
  EXPECT_EQ(h, 32);
}

PAGX_TEST(PAGXUtilsTest, GetImagePNGDimensions_NeitherDataNorFile) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetImagePNGDimensions(image, &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetImagePNGDimensions_InvalidData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t raw[] = {0xFF, 0xD8, 0xFF, 0xE0};
  image->data = pagx::Data::MakeWithCopy(raw, sizeof(raw));
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetImagePNGDimensions(image, &w, &h));
}

// ---------------------------------------------------------------------------
// GetImageDimensions
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetImageDimensions_PNG) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                   0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                   0x00, 0x00, 0x00, 0x08,   // width = 8
                   0x00, 0x00, 0x00, 0x04};  // height = 4
  image->data = pagx::Data::MakeWithCopy(png, sizeof(png));
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetImageDimensions(image, &w, &h));
  EXPECT_EQ(w, 8);
  EXPECT_EQ(h, 4);
}

PAGX_TEST(PAGXUtilsTest, GetImageDimensions_JPEG) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t jpeg[] = {
      0xFF, 0xD8, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x20,
      0x00, 0x40, 0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
  };
  image->data = pagx::Data::MakeWithCopy(jpeg, sizeof(jpeg));
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetImageDimensions(image, &w, &h));
  EXPECT_EQ(w, 64);
  EXPECT_EQ(h, 32);
}

PAGX_TEST(PAGXUtilsTest, GetImageDimensions_NoData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetImageDimensions(image, &w, &h));
}

// ---------------------------------------------------------------------------
// ComputeGlyphPaths
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_EmptyText) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_EmptyGlyphRun) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_NullFont) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_GlyphIDZero) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L10 0 L10 10 Z");
  glyph->advance = 10.0f;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 1000.0f;
  run->glyphs = {0};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_GlyphIDOutOfRange) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L10 0 L10 10 Z");
  glyph->advance = 10.0f;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 1000.0f;
  run->glyphs = {5};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_NullGlyphPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto glyph = doc->makeNode<pagx::Glyph>();
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 1000.0f;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_EmptyGlyphPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  glyph->advance = 10.0f;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 1000.0f;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_SingleGlyphWithPositions) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{10.0f, 5.0f}};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 100.0f, 200.0f);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_NE(result[0].pathData, nullptr);
  EXPECT_FLOAT_EQ(result[0].transform.tx, 110.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 205.0f);
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_WithXOffsets) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10.0f;
  run->glyphs = {1};
  run->xOffsets = {15.0f};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FLOAT_EQ(result[0].transform.tx, 15.0f);
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_DefaultPositioning) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = doc->makeNode<pagx::PathData>();
  *glyph1->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph1->advance = 500.0f;
  auto glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = doc->makeNode<pagx::PathData>();
  *glyph2->path = pagx::PathDataFromSVGString("M0 0 L400 0 L400 700 L0 700 Z");
  glyph2->advance = 400.0f;
  font->glyphs.push_back(glyph1);
  font->glyphs.push_back(glyph2);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10.0f;
  run->glyphs = {1, 2};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_FLOAT_EQ(result[0].transform.tx, 0.0f);
  float expectedSecondX = glyph1->advance * (10.0f / 1000.0f);
  EXPECT_FLOAT_EQ(result[1].transform.tx, expectedSecondX);
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_WithRotation) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{0, 0}};
  run->rotations = {45.0f};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FALSE(result[0].transform.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_WithScale) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{0, 0}};
  run->scales = {{2.0f, 2.0f}};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FALSE(result[0].transform.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_WithSkew) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{0, 0}};
  run->skews = {30.0f};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FALSE(result[0].transform.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_WithAnchors) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{0, 0}};
  run->rotations = {45.0f};
  run->anchors = {{10.0f, 20.0f}};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FALSE(result[0].transform.isIdentity());
}

PAGX_TEST(PAGXUtilsTest, ComputeGlyphPaths_PositionWithXOffset) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M0 0 L500 0 L500 700 L0 700 Z");
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->x = 10.0f;
  run->y = 5.0f;
  run->glyphs = {1};
  run->positions = {{20.0f, 30.0f}};
  run->xOffsets = {3.0f};
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 100.0f, 200.0f);
  ASSERT_EQ(result.size(), 1u);
  // posX = textPosX + run->x + positions[i].x + xOffsets[i] = 100 + 10 + 20 + 3 = 133
  // posY = textPosY + run->y + positions[i].y = 200 + 5 + 30 = 235
  EXPECT_FLOAT_EQ(result[0].transform.tx, 133.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 235.0f);
}

// ---------------------------------------------------------------------------
// GetPNGDimensionsFromPath (data URI variant)
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetPNGDimensionsFromPath_InvalidDataURI) {
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensionsFromPath("data:invalid", &w, &h));
}

PAGX_TEST(PAGXUtilsTest, GetPNGDimensionsFromPath_NonexistentFile) {
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensionsFromPath("/nonexistent/path.png", &w, &h));
}

// ---------------------------------------------------------------------------
// GetImageDPI (null/empty image data)
// ---------------------------------------------------------------------------

PAGX_TEST(PAGXUtilsTest, GetImageDPI_NullData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  float dpiX = 0, dpiY = 0;
  EXPECT_FALSE(pagx::GetImageDPI(image, &dpiX, &dpiY));
}

PAGX_TEST(PAGXUtilsTest, GetImageDPI_EmptyData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(nullptr, 0);
  float dpiX = 0, dpiY = 0;
  EXPECT_FALSE(pagx::GetImageDPI(image, &dpiX, &dpiY));
}

}  // namespace pag
