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

#include "base/utils/TimeUtil.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

#define PAG_COMPLEX_FILE_PATH TestConstants::PAG_ROOT + "resources/apitest/complex_test.pag"
#define PAG_ERROR_FILE_PATH_EMPTYPATH ""

namespace pag {
using nlohmann::json;

/**
 * 用例描述: PAGFile基础信息获取
 */
PAG_TEST(PAGFileTest, TestPAGFileBase) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  //基本信息校验
  ASSERT_EQ(TestFile->duration(), 250);
  ASSERT_EQ(TestFile->frameRate(), 25);
  Color bgColor = {171, 161, 161};
  ASSERT_TRUE(TestFile->backgroundColor() == bgColor);
  ASSERT_EQ(TestFile->width(), 720);
  ASSERT_EQ(TestFile->height(), 1080);
  ASSERT_EQ(TestFile->tagLevel(), 53);
  ASSERT_EQ(TestFile->numLayers(), 6);
  ASSERT_EQ(TestFile->numTexts(), 2);
  ASSERT_EQ(TestFile->numImages(), 2);
}

/**
 * 用例描述: PAGFile getImageAt接口校验
 */
PAG_TEST(PAGFileTest, TestPAGFileImage) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  // imageLayer 正常获取
  auto imageLayer = TestFile->getImageAt(1);
  ASSERT_TRUE(imageLayer.size() > 0);
  ASSERT_EQ(imageLayer[0]->name, "PAGImageLayer1");
  ASSERT_EQ(TestFile->getEditableIndex(imageLayer[0]), 1);
  // imageLayer 异常获取 <0
  imageLayer = TestFile->getImageAt(-1);
  ASSERT_TRUE(imageLayer.size() == 0);
  // imageLayer 异常获取 >= size
  imageLayer = TestFile->getImageAt(TestFile->numImages());
  ASSERT_TRUE(imageLayer.size() == 0);
}

/**
 * 用例描述: PAGFile getTextAt校验
 */
PAG_TEST(PAGFileTest, TestPAGFileText) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  // textLayer 正常获取 >= size
  // ASSERT_EQ(TestFile->getEditableIndex(textLayer), 1);
  // textLayer 异常获取 >= size
  auto textLayer = TestFile->getTextAt(TestFile->numTexts());
  ASSERT_EQ(textLayer, nullptr);
  // textLayer 异常获取 < 0
  textLayer = TestFile->getTextAt(-1);
  ASSERT_EQ(textLayer, nullptr);
}

/**
 * 用例描述: PAGFile getTextData校验
 */
PAG_TEST(PAGFileTest, TestPAGFileTextData) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  // textData 异常获取 >= size
  auto textData = TestFile->getTextData(TestFile->numTexts());
  ASSERT_EQ(textData, nullptr);
  // textData 异常获取 < 0
  textData = TestFile->getTextData(-1);
  ASSERT_EQ(textData, nullptr);
  // auto fontFamily = textData->fontFamily;
}

/**
 * 用例描述: PAGFile编解码校验
 */
PAG_TEST(PAGFileTest, TestPAGFileEncodeDecode) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  // encode decode验证
  //验证文件数据与测试结果是否一致
  auto encodeByteData = Codec::Encode(TestFile);  // 再编码检查一下
  auto verifyByteData = ByteData::FromPath(ProjectPath::Absolute("resources/apitest/test.pag"));
  ASSERT_EQ(verifyByteData->length(), encodeByteData->length());
  auto verifyData = verifyByteData->data();
  auto encodeData = encodeByteData->data();
  for (size_t i = 0; i < encodeByteData->length(); ++i) {
    ASSERT_EQ(verifyData[i], encodeData[i]);
  }
}

/**
 * 用例描述: PAGFile numImages 接口
 */
PAG_TEST(PAGFileComplexTest, numImages) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numImages(), 1);
}

/**
 * 用例描述: PAGFile numTexts 接口
 */
PAG_TEST(PAGFileComplexTest, numTexts) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numTexts(), 4);
}

/**
 * 用例描述: PAGFile numLayers 接口
 */
PAG_TEST(PAGFileComplexTest, numLayers) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numLayers(), 10);
}

/**
 * 用例描述: PAGFile tagLevel 接口
 */
PAG_TEST(PAGFileComplexTest, tagLevel) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->tagLevel(), 53);
}

/**
 * 用例描述: PAGFile rootLayer 接口
 */
PAG_TEST(PAGFileComplexTest, rootLayer) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  // rootLayer 校验
  auto fileRootLayer = TestFile->getRootLayer();
  ASSERT_EQ(fileRootLayer->composition->type(), CompositionType::Vector);
  ASSERT_EQ(fileRootLayer->name, "");
}

/**
 * 用例描述: PAGFile getImageAt 接口
 */
PAG_TEST(PAGFileComplexTest, getImageAt) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  // imageLayer 正常获取
  auto imageLayer = TestFile->getImageAt(0);
  ASSERT_TRUE(imageLayer.size() > 0);
  ASSERT_EQ(imageLayer[0]->name, "PAGImageLayer2");
  ASSERT_EQ(TestFile->getEditableIndex(imageLayer[0]), 0);
  // imageLayer 异常获取 <0
  imageLayer = TestFile->getImageAt(-1);
  ASSERT_TRUE(imageLayer.size() == 0);
  // imageLayer 异常获取 >= size
  imageLayer = TestFile->getImageAt(TestFile->numImages());
  ASSERT_TRUE(imageLayer.size() == 0);
}

/**
 * 用例描述: PAGFile getTextAt 接口
 */
PAG_TEST(PAGFileComplexTest, getTextAt) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  // textLayer 正常获取
  auto textLayer = TestFile->getTextAt(0);
  ASSERT_EQ(TestFile->getEditableIndex(textLayer), 0);
  ASSERT_EQ(textLayer->name, "ReplaceTextLayer");
  auto textData1 = TestFile->getTextData(0);
  auto textData2 = textLayer->getTextDocument();
  ASSERT_EQ(textData1->fontFamily, textData2->fontFamily);
  ASSERT_EQ(textData1->fontStyle, textData2->fontStyle);
  ASSERT_EQ(textData1->fontSize, textData2->fontSize);
  // textLayer 异常获取 >= size
  textLayer = TestFile->getTextAt(TestFile->numTexts());
  ASSERT_EQ(textLayer, nullptr);
  // textLayer 异常获取 < 0
  textLayer = TestFile->getTextAt(-1);
  ASSERT_EQ(textLayer, nullptr);
}

/**
 * 用例描述: PAGFile textData 接口
 */
PAG_TEST(PAGFileComplexTest, getTextData) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  // textData 异常获取 >= size
  auto textData = TestFile->getTextData(TestFile->numTexts());
  ASSERT_EQ(textData, nullptr);
  // textData 异常获取 < 0
  textData = TestFile->getTextData(-1);
  ASSERT_EQ(textData, nullptr);
  // auto fontFamily = textData->fontFamily;
}

/**
 * 用例描述: PAGFile基础功能
 */
PAG_TEST(PAGFileComplexTest, TestPAGFile) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/complex_test.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  //基本信息校验
  ASSERT_EQ(TestFile->duration(), 250);
  ASSERT_EQ(TestFile->frameRate(), 25);
  Color bgColor = {171, 161, 161};
  ASSERT_TRUE(TestFile->backgroundColor() == bgColor);
  ASSERT_EQ(TestFile->width(), 720);
  ASSERT_EQ(TestFile->height(), 1080);

  auto composition = static_cast<VectorComposition*>(TestFile->getRootLayer()->composition);
  ASSERT_EQ(static_cast<int>(composition->layers.size()), 5);
  ASSERT_EQ(composition->layers[0]->type(), LayerType::Text);
  ASSERT_EQ(composition->layers[0]->name, "ReplaceTextLayer");
  ASSERT_EQ(composition->layers[1]->type(), LayerType::PreCompose);
  ASSERT_EQ(composition->layers[1]->name, "video_alpha_bmp");
  auto sublayerComposition = static_cast<PreComposeLayer*>(composition->layers[1])->composition;
  ASSERT_EQ(sublayerComposition->type(), CompositionType::Video);
  ASSERT_EQ(composition->layers[2]->type(), LayerType::PreCompose);
  ASSERT_EQ(composition->layers[2]->name, "PAGImageLayer2_bmp");
  sublayerComposition = static_cast<PreComposeLayer*>(composition->layers[2])->composition;
  ASSERT_EQ(sublayerComposition->type(), CompositionType::Video);
  ASSERT_EQ(composition->layers[3]->type(), LayerType::PreCompose);
  ASSERT_EQ(composition->layers[3]->name, "PAGImageLayer3_bmp");
  sublayerComposition = static_cast<PreComposeLayer*>(composition->layers[3])->composition;
  ASSERT_EQ(sublayerComposition->type(), CompositionType::Video);
  ASSERT_EQ(composition->layers[4]->type(), LayerType::PreCompose);
  ASSERT_EQ(composition->layers[4]->name, "RootLayer");
  sublayerComposition = static_cast<PreComposeLayer*>(composition->layers[4])->composition;
  ASSERT_EQ(sublayerComposition->type(), CompositionType::Vector);

  // encode decode验证
  //验证文件数据与测试结果是否一致
  auto encodeByteData = Codec::Encode(TestFile);  // 再编码检查一下
  auto verifyByteData =
      ByteData::FromPath(ProjectPath::Absolute("resources/apitest/complex_test.pag"));
  ASSERT_EQ(verifyByteData->length(), encodeByteData->length());
  auto verifyData = verifyByteData->data();
  auto encodeData = encodeByteData->data();
  for (size_t i = 0; i < encodeByteData->length(); ++i) {
    ASSERT_EQ(verifyData[i], encodeData[i]);
  }
}

/**
 * 用例描述: ByteData解码测试
 */
PAG_TEST(PAGFileLoadTest, byteData) {
  std::shared_ptr<PAGFile> file;
  // load from byte
  auto byteData = ByteData::FromPath(ProjectPath::Absolute("resources/apitest/test.pag"));
  ASSERT_TRUE(byteData != nullptr);
  ASSERT_TRUE(byteData->data() != nullptr);
  ASSERT_TRUE(byteData->length() != 0);
}

/**
 * 用例描述: PAGFile解码测试
 */
PAG_TEST(PAGFileLoadTest, loadTest) {
  std::shared_ptr<PAGFile> file;

  // load from byte
  auto byteData = ByteData::FromPath(ProjectPath::Absolute("resources/apitest/test.pag"));
  ASSERT_TRUE(byteData != nullptr);
  file = PAGFile::Load(byteData->data(), byteData->length());
  ASSERT_TRUE(file != nullptr);

  // error length
  file = PAGFile::Load(byteData->data(), byteData->length() - 20);
  ASSERT_TRUE(file == nullptr);

  // larger length, correct data
  file = PAGFile::Load(byteData->data(), byteData->length() + 20);
  ASSERT_TRUE(file != nullptr);

  // error data
  const void* errorBytes = "test error data can be loaded. ";
  file = PAGFile::Load(errorBytes, 31);
  ASSERT_TRUE(file == nullptr);

  // empty data
  errorBytes = "";
  file = PAGFile::Load(errorBytes, 0);
  ASSERT_TRUE(file == nullptr);

  // load from path
  file = LoadPAGFile("resources/apitest/test.pag");
  ASSERT_TRUE(file != nullptr);

  // error path
  file = LoadPAGFile("1.pag");
  ASSERT_TRUE(file == nullptr);

  // error file content
  file = LoadPAGFile("resources/apitest/imageReplacement.png");
  ASSERT_TRUE(file == nullptr);

  // empty path
  file = LoadPAGFile("");
  ASSERT_TRUE(file == nullptr);
}

/**
 * 用例描述: PAGFile children编辑测试
 */
PAG_TEST(PAGFileContainerTest, ContainerEditing) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->removeAllLayers();
  ASSERT_EQ(TestPAGFile->numChildren(), 0);
  auto rootLayer = LoadPAGFile("resources/apitest/test.pag");
  auto pagCom = std::static_pointer_cast<PAGComposition>(rootLayer->getLayerAt(0));
  int size = pagCom->numChildren();
  for (int i = 0; i < size; i++) {
    auto layer = pagCom->getLayerAt(0);
    layer->setCurrentTime(3 * 1000000);
    TestPAGFile->addLayer(pagCom->getLayerAt(0));
  }

  ASSERT_EQ(TestPAGFile->width(), 720);
  ASSERT_EQ(TestPAGFile->height(), 1080);
  ASSERT_EQ(TestPAGFile->numChildren(), 6);
  ASSERT_EQ(TestPAGFile->numImages(), 2);

  auto editableLayers = TestPAGFile->getLayersByEditableIndex(0, LayerType::Text);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 1);
  ASSERT_EQ(editableLayers[0]->layerName(), "PAGTextLayer2");
  editableLayers = TestPAGFile->getLayersByEditableIndex(1, LayerType::Text);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 1);
  ASSERT_EQ(editableLayers[0]->layerName(), "PAGTextLayer1");
  editableLayers = TestPAGFile->getLayersByEditableIndex(2, LayerType::Text);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 0);
  editableLayers = TestPAGFile->getLayersByEditableIndex(0, LayerType::Image);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 1);
  ASSERT_EQ(editableLayers[0]->layerName(), "PAGImageLayer2");
  editableLayers = TestPAGFile->getLayersByEditableIndex(1, LayerType::Image);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 1);
  ASSERT_EQ(editableLayers[0]->layerName(), "PAGImageLayer1");
  editableLayers = TestPAGFile->getLayersByEditableIndex(2, LayerType::Image);
  ASSERT_EQ(static_cast<int>(editableLayers.size()), 0);

  auto imageLayer1 = std::static_pointer_cast<PAGImageLayer>(TestPAGFile->getLayerAt(2));
  ASSERT_NE(imageLayer1, nullptr);
  ASSERT_EQ(imageLayer1->layerName(), "PAGImageLayer1");
  ASSERT_EQ(TestPAGFile->getLayerIndex(imageLayer1), 2);
  ASSERT_TRUE(TestPAGFile->contains(imageLayer1));

  auto imageLayer2 = std::static_pointer_cast<PAGImageLayer>(TestPAGFile->getLayerAt(3));
  ASSERT_NE(imageLayer2, nullptr);
  ASSERT_EQ(imageLayer2->layerName(), "PAGImageLayer2");

  TestPAGFile->swapLayer(imageLayer1, imageLayer2);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/swapLayer"));

  TestPAGFile->swapLayerAt(2, 3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/swapLayerAt"));

  TestPAGFile->setLayerIndex(imageLayer1, 3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/setLayerIndex"));

  TestPAGFile->removeLayer(imageLayer1);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/removeLayer"));

  TestPAGFile->removeLayerAt(2);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/removeLayerAt"));

  TestPAGFile->removeAllLayers();
  TestPAGPlayer->flush();
  ASSERT_EQ(TestPAGFile->numChildren(), 0);
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/removeAllLayers"));

  auto pagFile2 = LoadPAGFile("resources/apitest/test.pag");
  auto root2 = pagFile2;
  auto pagComposition2 = std::static_pointer_cast<PAGComposition>(root2->getLayerAt(0));
  auto imageLayer = pagComposition2->getLayerAt(2);
  imageLayer->setCurrentTime(2000000);
  TestPAGFile->addLayer(imageLayer);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/addLayer"));

  auto pagLayer = pagComposition2->getLayerAt(3);
  pagLayer->setCurrentTime(4000000);
  TestPAGFile->addLayerAt(pagLayer, 0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileContainerTest/addLayerAt"));

  TestPAGFile->contains(nullptr);
  TestPAGFile->removeLayer(nullptr);
  TestPAGFile->removeLayerAt(-2);

  TestPAGFile->addLayer(nullptr);
  TestPAGFile->addLayerAt(imageLayer2, 30);

  TestPAGFile->addLayerAt(nullptr, 0);
  TestPAGFile->swapLayer(nullptr, imageLayer2);
}

/**
 * 用例描述: PAGFile时间伸缩属性-Repeat测试
 */
PAG_TEST(PAGFileTimeStretchTest, Repeat) {
  auto pagFile = LoadPAGFile("resources/apitest/test_repeat.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagFile->setDuration(pagFile->duration() * 2);
  //第1帧
  pagFile->setCurrentTime(1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Repeat_1"));
  //第61帧
  pagFile->setCurrentTime(2000000ll + 1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Repeat_1"));
}

/**
 * 用例描述: PAGFile时间伸缩属性-RepeatInverted测试
 */
PAG_TEST(PAGFileTimeStretchTest, RepeatInverted) {
  auto pagFile = LoadPAGFile("resources/apitest/test_repeatInverted.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagFile->setDuration(pagFile->duration() * 2);
  //第1帧
  pagFile->setCurrentTime(1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/RepeateInverted_1"));

  //第198帧
  pagFile->setCurrentTime(2000000ll - 2 * 1000000ll / 30 + 2000000ll);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/RepeateInverted_198"));
}

/**
 * 用例描述: PAGFile时间伸缩属性-Scale测试
 */
PAG_TEST(PAGFileTimeStretchTest, Scale) {
  auto pagFile = LoadPAGFile("resources/apitest/test_scale.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  //第30帧
  pagFile->setCurrentTime(30 * 1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Scale_30"));

  //第12帧
  pagFile->setCurrentTime(12 * 1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Scale_12"));
  pagFile->setDuration(pagFile->duration() * 2);

  //第90帧
  pagFile->setCurrentTime(4000000ll - 30 * 1000000ll / 30);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Scale_30"));
  //第17帧
  //计算会有一定误差：现在的结果是10->10 13->11 17->12
  pagFile->setCurrentTime(17 * 1000000ll / 30 + 1);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFileTimeStretchTest/Scale_12"));
}

/**
 * 用例描述: ImageFillRule编解码测试
 */
PAG_TEST(PAGFileImageFillRuleCodec, ImageFillRuleCodec) {
  auto testFile = LoadPAGFile("resources/apitest/test_ImageFillRule.pag");
  EXPECT_NE(testFile, nullptr);
  auto byteData = Codec::Encode(testFile->getFile());
  EXPECT_NE(byteData, nullptr);
  EXPECT_NE(byteData->data(), nullptr);
  EXPECT_NE(static_cast<int>(byteData->length()), 0);
}

/**
 * 用例描述: 竖排文本编解码测试
 */
PAG_TEST(PAGFileTextDirectionCodec, TextDirectionCodec) {
  auto testFile = LoadPAGFile("resources/apitest/test_TextDirection.pag");
  EXPECT_NE(testFile, nullptr);
  auto byteData = Codec::Encode(testFile->getFile());
  EXPECT_NE(byteData, nullptr);
  EXPECT_NE(byteData->data(), nullptr);
  EXPECT_NE(static_cast<int>(byteData->length()), 0);
}

/**
 * 用例描述: 竖排文本编解码测试
 */
PAG_TEST(PAGFileVerticalTextCodec, VerticalTextCodec) {
  auto testFile = LoadPAGFile("resources/apitest/test_VerticalText.pag");
  EXPECT_NE(testFile, nullptr);
  auto byteData = Codec::Encode(testFile->getFile());
  EXPECT_NE(byteData, nullptr);
  EXPECT_NE(byteData->data(), nullptr);
  EXPECT_NE(static_cast<int>(byteData->length()), 0);
}

/**
 * 用例描述: ShapeType测试
 */
PAG_TEST(PAGFileTest, ShapeType) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto testFile = LoadPAGFile("resources/apitest/ShapeType.pag");
  EXPECT_NE(testFile, nullptr);
  TestPAGPlayer->setComposition(testFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileTest/ShapeType"));
}

/**
 * 用例描述: ChildFrameToLocal接口测试，不改变时长
 */
PAG_TEST(PAGFileTest, ChildFrameToLocal) {
  auto testFile = LoadPAGFile("resources/apitest/ShapeType.pag");
  EXPECT_NE(testFile, nullptr);

  auto fileFrame = TimeToFrame(testFile->duration(), testFile->frameRate());

  //超过fileDuration
  auto result = testFile->childFrameToLocal(fileFrame + 10, testFile->frameRate());
  EXPECT_EQ(result, fileFrame + 10);

  //少于0
  result = testFile->childFrameToLocal(-10, testFile->frameRate());
  EXPECT_EQ(result, -10);

  //处于0~fileDuration中
  result = testFile->childFrameToLocal(fileFrame - 10, testFile->frameRate());
  EXPECT_EQ(result, fileFrame - 10);
}

/**
 * 用例描述: ChildFrameToLocal进阶测试，改变时长
 */
PAG_TEST(PAGFileTest, ChildFrameToLocalAdvanced) {
  auto testFile = LoadPAGFile("resources/apitest/ShapeType.pag");
  EXPECT_NE(testFile, nullptr);

  auto fileFrame = TimeToFrame(testFile->duration(), testFile->frameRate());
  EXPECT_EQ(fileFrame, 336);
  testFile->setDuration(FrameToTime(326, testFile->frameRate()));

  //超过fileDuration
  auto result = testFile->childFrameToLocal(336, testFile->frameRate());
  EXPECT_EQ(result, 326);

  //少于0
  result = testFile->childFrameToLocal(-10, testFile->frameRate());
  EXPECT_EQ(result, -10);

  // scale情况
  //处于0~fileDuration中
  testFile->setTimeStretchMode(pag::PAGTimeStretchMode::Scale);
  result = testFile->childFrameToLocal(316, testFile->frameRate());
  EXPECT_EQ(result, 306);

  //非scale情况
  testFile->setTimeStretchMode(pag::PAGTimeStretchMode::None);
  result = testFile->childFrameToLocal(316, testFile->frameRate());
  EXPECT_EQ(result, 316);
}

/**
 * 用例描述: 测试椭圆转path，iOS14圆角badcase
 */
PAG_TEST(PAGFileTest, EllipseToPath_ID80701969) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/apitest/ellipse_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileTest/EllipseToPath"));
}

/**
 * 用例描述: 测试矩形转path，iOS14badcase
 */
PAG_TEST(PAGFileTest, RectToPath) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/apitest/rect_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileTest/RectToPath"));
}

/**
 * 用例描述: 测试圆角矩形转path,iOS14badCase
 */
PAG_TEST(PAGFileTest, RoundRectToPath) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/apitest/round_rect_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileTest/RoundRectToPath"));
}

/**
 * 用例描述: PAGFile设置开始时间
 */
PAG_TEST(PAGFileTest, SetStartTime) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/replacement.pag");
  TestPAGPlayer->setComposition(pagFile);
  pagFile->setStartTime(2000000);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGFileTest/SetStartTime"));
}

PAG_TEST(PAGFileTest, EditableIndices) {
  auto pagFile = LoadPAGFile("resources/apitest/texture_bottom_left.pag");
  auto editableImages = pagFile->getEditableIndices(LayerType::Image);
  ASSERT_EQ(editableImages.size(), 6lu);
  for (size_t i = 0; i < editableImages.size(); i++) {
    ASSERT_EQ(editableImages[i], static_cast<int>(i));
  }
  pagFile = LoadPAGFile("resources/apitest/editImageLayers.pag");
  editableImages = pagFile->getEditableIndices(LayerType::Image);
  ASSERT_EQ(editableImages.size(), 3lu);
  ASSERT_EQ(editableImages[0], 3);
  ASSERT_EQ(editableImages[1], 2);
  ASSERT_EQ(editableImages[2], 4);

  pagFile = LoadPAGFile("resources/apitest/test.pag");
  auto editableTexts = pagFile->getEditableIndices(LayerType::Text);
  ASSERT_EQ(editableTexts.size(), 2lu);
  for (size_t i = 0; i < editableTexts.size(); i++) {
    ASSERT_EQ(editableTexts[i], static_cast<int>(i));
  }

  pagFile = LoadPAGFile("resources/apitest/editableTextLayer.pag");
  editableTexts = pagFile->getEditableIndices(LayerType::Text);
  ASSERT_EQ(editableTexts.size(), 2lu);
  ASSERT_EQ(editableTexts[0], static_cast<int>(1));
  ASSERT_EQ(editableTexts[1], static_cast<int>(0));
}

}  // namespace pag
