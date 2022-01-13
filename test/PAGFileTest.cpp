/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

#define PAG_CORRECT_FILE_PATH "../resources/apitest/test.pag"
#define PAG_COMPLEX_FILE_PATH "../resources/apitest/complex_test.pag"
#define PAG_ERROR_FILE_PATH_ERRPATH "1.pag"
#define PAG_ERROR_FILE_PATH_ERRFILE "../resources/apitest/imageReplacement.png"
#define PAG_ERROR_FILE_PATH_EMPTYPATH ""

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT_WITH_PATH(PAGFileBaseTest, PAG_CORRECT_FILE_PATH)

/**
 * 用例描述: PAGFile基础信息获取
 */
PAG_TEST_F(PAGFileBaseTest, TestPAGFileBase) {
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
PAG_TEST_F(PAGFileBaseTest, TestPAGFileImage) {
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
PAG_TEST_F(PAGFileBaseTest, TestPAGFileText) {
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
PAG_TEST_F(PAGFileBaseTest, TestPAGFileTextData) {
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
PAG_TEST_F(PAGFileBaseTest, TestPAGFileEncodeDecode) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);

  // encode decode验证
  //验证文件数据与测试结果是否一致
  auto encodeByteData = Codec::Encode(TestFile);  // 再编码检查一下
  auto verifyByteData = ByteData::FromPath(PAG_CORRECT_FILE_PATH);
  ASSERT_EQ(verifyByteData->length(), encodeByteData->length());
  auto verifyData = verifyByteData->data();
  auto encodeData = encodeByteData->data();
  for (size_t i = 0; i < encodeByteData->length(); ++i) {
    ASSERT_EQ(verifyData[i], encodeData[i]);
  }
}

PAG_TEST_SUIT_WITH_PATH(PAGFileComplexTest, PAG_COMPLEX_FILE_PATH)

/**
 * 用例描述: PAGFile numImages 接口
 */
PAG_TEST_F(PAGFileComplexTest, numImages) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numImages(), 1);
}

/**
 * 用例描述: PAGFile numTexts 接口
 */
PAG_TEST_F(PAGFileComplexTest, numTexts) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numTexts(), 4);
}

/**
 * 用例描述: PAGFile numLayers 接口
 */
PAG_TEST_F(PAGFileComplexTest, numLayers) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->numLayers(), 10);
}

/**
 * 用例描述: PAGFile tagLevel 接口
 */
PAG_TEST_F(PAGFileComplexTest, tagLevel) {
  ASSERT_NE(TestPAGFile, nullptr);
  auto TestFile = TestPAGFile->getFile();
  ASSERT_NE(TestFile, nullptr);
  ASSERT_EQ(TestFile->tagLevel(), 53);
}

/**
 * 用例描述: PAGFile rootLayer 接口
 */
PAG_TEST_F(PAGFileComplexTest, rootLayer) {
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
PAG_TEST_F(PAGFileComplexTest, getImageAt) {
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
PAG_TEST_F(PAGFileComplexTest, getTextAt) {
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
PAG_TEST_F(PAGFileComplexTest, getTextData) {
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
PAG_TEST_F(PAGFileComplexTest, TestPAGFile) {
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
  auto verifyByteData = ByteData::FromPath(PAG_COMPLEX_FILE_PATH);
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
  auto byteData = ByteData::FromPath(PAG_CORRECT_FILE_PATH);
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
  auto byteData = ByteData::FromPath(PAG_CORRECT_FILE_PATH);
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
  file = PAGFile::Load(PAG_CORRECT_FILE_PATH);
  ASSERT_TRUE(file != nullptr);

  // error path
  file = PAGFile::Load(PAG_ERROR_FILE_PATH_ERRPATH);
  ASSERT_TRUE(file == nullptr);

  // error file content
  file = PAGFile::Load(PAG_ERROR_FILE_PATH_ERRFILE);
  ASSERT_TRUE(file == nullptr);

  // empty path
  file = PAGFile::Load(PAG_ERROR_FILE_PATH_EMPTYPATH);
  ASSERT_TRUE(file == nullptr);
}

PAG_TEST_CASE(PAGFileContainerTest)

/**
 * 用例描述: PAGFile children编辑测试
 */
PAG_TEST_F(PAGFileContainerTest, getLayersByEditableIndex) {
  TestPAGFile->removeAllLayers();
  ASSERT_EQ(TestPAGFile->numChildren(), 0);
  auto rootLayer = PAGFile::Load(DEFAULT_PAG_PATH);
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
  auto swapLayerMd5 = getMd5FromSnap();

  TestPAGFile->swapLayerAt(2, 3);
  TestPAGPlayer->flush();
  auto swapLayerAtMd5 = getMd5FromSnap();

  TestPAGFile->setLayerIndex(imageLayer1, 3);
  TestPAGPlayer->flush();
  auto setLayerIndexMd5 = getMd5FromSnap();

  TestPAGFile->removeLayer(imageLayer1);
  TestPAGPlayer->flush();
  auto removeLayerMd5 = getMd5FromSnap();

  TestPAGFile->removeLayerAt(2);
  TestPAGPlayer->flush();
  auto removeLayerAtMd5 = getMd5FromSnap();

  TestPAGFile->removeAllLayers();
  TestPAGPlayer->flush();
  ASSERT_EQ(TestPAGFile->numChildren(), 0);
  auto removeAllLayersMd5 = getMd5FromSnap();

  auto pagFile2 = PAGFile::Load(DEFAULT_PAG_PATH);
  auto root2 = pagFile2;
  auto pagComposition2 = std::static_pointer_cast<PAGComposition>(root2->getLayerAt(0));
  auto imageLayer = pagComposition2->getLayerAt(2);
  TestPAGFile->addLayer(imageLayer);
  TestPAGPlayer->flush();
  auto addLayerMd5 = getMd5FromSnap();

  TestPAGFile->addLayerAt(pagComposition2->getLayerAt(3), 0);
  TestPAGPlayer->flush();
  auto addLayerAtMd5 = getMd5FromSnap();

  TestPAGFile->contains(nullptr);
  TestPAGFile->removeLayer(nullptr);
  TestPAGFile->removeLayerAt(-2);

  TestPAGFile->addLayer(nullptr);
  TestPAGFile->addLayerAt(imageLayer2, 30);

  TestPAGFile->addLayerAt(nullptr, 0);
  TestPAGFile->swapLayer(nullptr, imageLayer2);

  auto image = PAGImage::FromPath("../resources/apitest/imageReplacement.png");
  TestPAGFile->replaceImage(0, image);
  TestPAGPlayer->flush();
  auto replaceImageMd5 = getMd5FromSnap();

  auto textData = std::shared_ptr<TextDocument>(new TextDocument());
  textData->text = "测试ceshi";
  textData->fillColor = Red;
  TestPAGFile->replaceText(0, textData);
  TestPAGPlayer->flush();
  auto replaceTextMd5 = getMd5FromSnap();

  json out = {
      {"swapLayerMd5",       swapLayerMd5},
      {"setLayerIndexMd5",   setLayerIndexMd5},
      {"removeLayerMd5",     removeLayerMd5},
      {"removeLayerAtMd5",   removeLayerAtMd5},
      {"removeAllLayersMd5", removeAllLayersMd5},
      {"addLayerMd5",        addLayerMd5},
      {"addLayerAt",         addLayerAtMd5},
      {"replaceImageMd5",    replaceImageMd5},
      {"replaceTextMd5",     replaceTextMd5},
  };
  PAGTestEnvironment::DumpJson["PAGFileTest"]["PAGFileContainerTest"] = out;

#ifdef COMPARE_JSON_PATH
  auto cAddLayerAt =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["addLayerAt"];
  if (cAddLayerAt != nullptr) {
    EXPECT_EQ(cAddLayerAt.get<std::string>(), addLayerAtMd5);
  }
  auto cAddLayerMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["addLayerMd5"];
  if (cAddLayerMd5 != nullptr) {
    EXPECT_EQ(cAddLayerMd5.get<std::string>(), addLayerMd5);
  }
  auto cRemoveAllLayersMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["removeAllLayersMd5"];
  if (cRemoveAllLayersMd5 != nullptr) {
    EXPECT_EQ(cRemoveAllLayersMd5.get<std::string>(), removeAllLayersMd5);
  }
  auto cRemoveLayerAtMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["removeLayerAtMd5"];
  if (cRemoveLayerAtMd5 != nullptr) {
    EXPECT_EQ(cRemoveLayerAtMd5.get<std::string>(), removeLayerAtMd5);
  }
  auto cRemoveLayerMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["removeLayerMd5"];
  if (cRemoveLayerMd5 != nullptr) {
    EXPECT_EQ(cRemoveLayerMd5.get<std::string>(), removeLayerMd5);
  }
  auto cSetLayerIndexMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["setLayerIndexMd5"];
  if (cSetLayerIndexMd5 != nullptr) {
    EXPECT_EQ(cSetLayerIndexMd5.get<std::string>(), setLayerIndexMd5);
  }
  auto cSwapLayerMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["swapLayerMd5"];
  if (cSwapLayerMd5 != nullptr) {
    EXPECT_EQ(cSwapLayerMd5.get<std::string>(), swapLayerMd5);
  }
  auto cReplaceImageMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["replaceImageMd5"];
  if (cReplaceImageMd5 != nullptr) {
    EXPECT_EQ(cReplaceImageMd5.get<std::string>(), replaceImageMd5);
  }
  auto cReplaceTextMd5 =
      PAGTestEnvironment::CompareJson["PAGFileTest"]["PAGFileContainerTest"]["replaceTextMd5"];
  if (cSwapLayerMd5 != nullptr) {
    EXPECT_EQ(cReplaceTextMd5.get<std::string>(), replaceTextMd5);
  }
#endif
}

PAG_TEST_CASE_WITH_PATH(PAGFileTimeStretchRepeat, "../resources/apitest/test_repeat.pag")

/**
 * 用例描述: PAGFile时间伸缩属性-Repeat测试
 */
PAG_TEST_F(PAGFileTimeStretchRepeat, timeStretch) {
  TestPAGFile->setDuration(TestPAGFile->duration() * 2);
  //第1帧
  TestPAGFile->setCurrentTime(1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_1 = MakeSnapshot(TestPAGSurface);
  std::string md5 = DumpMD5(skImage_1);
  //第61帧
  TestPAGFile->setCurrentTime(2000000ll + 1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_61 = MakeSnapshot(TestPAGSurface);
  std::string compMd5 = DumpMD5(skImage_61);
  ASSERT_EQ(md5, compMd5);
  TraceIf(skImage_1, "../test/out/test_repeat.pag_PAGFileTimeStretchRepeate_1.png", md5 != compMd5);
  TraceIf(skImage_61, "../test/out/test_repeat.pag_PAGFileTimeStretchRepeate_61.png",
          md5 != compMd5);
}

PAG_TEST_CASE_WITH_PATH(PAGFileTimeStretchRepeatInverted,
                        "../resources/apitest/test_repeatInverted.pag")

/**
 * 用例描述: PAGFile时间伸缩属性-RepeatInverted测试
 */
PAG_TEST_F(PAGFileTimeStretchRepeatInverted, timeStretch) {
  TestPAGFile->setDuration(TestPAGFile->duration() * 2);
  //第1帧
  TestPAGFile->setCurrentTime(1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_1 = MakeSnapshot(TestPAGSurface);
  std::string md5 = DumpMD5(skImage_1);
  //第198帧
  TestPAGFile->setCurrentTime(2000000ll - 2 * 1000000ll / 30 + 2000000ll);
  TestPAGPlayer->flush();
  auto skImage_198 = MakeSnapshot(TestPAGSurface);
  std::string compMd5 = DumpMD5(skImage_198);
  ASSERT_EQ(md5, compMd5);
  TraceIf(skImage_1, "../test/out/test_repeat.pag_PAGFileTimeStretchRepeateInverted_1.png",
          md5 != compMd5);
  TraceIf(skImage_198, "../test/out/test_repeat.pag_PAGFileTimeStretchRepeateInverted_198.png",
          md5 != compMd5);
}

PAG_TEST_CASE_WITH_PATH(PAGFileTimeStretchScale, "../resources/apitest/test_scale.pag")

/**
 * 用例描述: PAGFile时间伸缩属性-Scale测试
 */
PAG_TEST_F(PAGFileTimeStretchScale, timeStretch) {
  //第30帧
  TestPAGFile->setCurrentTime(30 * 1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_30 = MakeSnapshot(TestPAGSurface);
  std::string md5 = DumpMD5(skImage_30);

  //第12帧
  TestPAGFile->setCurrentTime(12 * 1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_12 = MakeSnapshot(TestPAGSurface);
  std::string secMd5 = DumpMD5(skImage_12);

  TestPAGFile->setDuration(TestPAGFile->duration() * 2);

  //第90帧
  TestPAGFile->setCurrentTime(4000000ll - 30 * 1000000ll / 30);
  TestPAGPlayer->flush();
  auto skImage_90 = MakeSnapshot(TestPAGSurface);
  std::string compMd5 = DumpMD5(skImage_90);
  ASSERT_EQ(md5, compMd5);
  TraceIf(skImage_30, "../test/out/test_repeat.pag_PAGFileTimeStretchScale_30.png", md5 != compMd5);
  TraceIf(skImage_90, "../test/out/test_repeat.pag_PAGFileTimeStretchScale_90.png", md5 != compMd5);

  //第17帧
  //计算会有一定误差：现在的结果是10->10 13->11 17->12
  TestPAGFile->setCurrentTime(17 * 1000000ll / 30 + 1);
  TestPAGPlayer->flush();
  auto skImage_17 = MakeSnapshot(TestPAGSurface);
  std::string compSecMd5 = DumpMD5(skImage_17);
  ASSERT_EQ(secMd5, compSecMd5);
  TraceIf(skImage_12, "../test/out/test_repeat.pag_PAGFileTimeStretchScale_12.png",
          secMd5 != compSecMd5);
  TraceIf(skImage_17, "../test/out/test_repeat.pag_PAGFileTimeStretchScale_17.png",
          secMd5 != compSecMd5);
}

/**
 * 用例描述: ImageFillRule编解码测试
 */
PAG_TEST(PAGFileImageFillRuleCodec, ImageFillRuleCodec) {
  auto testFile = PAGFile::Load("../resources/apitest/test_ImageFillRule.pag");
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
  auto testFile = PAGFile::Load("../resources/apitest/test_TextDirection.pag");
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
  auto testFile = PAGFile::Load("../resources/apitest/test_VerticalText.pag");
  EXPECT_NE(testFile, nullptr);
  auto byteData = Codec::Encode(testFile->getFile());
  EXPECT_NE(byteData, nullptr);
  EXPECT_NE(byteData->data(), nullptr);
  EXPECT_NE(static_cast<int>(byteData->length()), 0);
}

/**
 * 用例描述: ShapeType测试
 */
PAG_TEST_F(PAGFileBaseTest, ShapeType) {
  auto testFile = PAGFile::Load("../resources/apitest/ShapeType.pag");
  EXPECT_NE(testFile, nullptr);
  TestPAGPlayer->setComposition(testFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGFileTest"]["ShapeType"] = md5;
#ifdef COMPARE_JSON_PATH
  auto cMd5 = PAGTestEnvironment::CompareJson["PAGFileTest"]["ShapeType"];
  if (cMd5 != nullptr) {
    EXPECT_EQ(cMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: ChildFrameToLocal接口测试，不改变时长
 */
PAG_TEST_F(PAGFileBaseTest, ChildFrameToLocal) {
  auto testFile = PAGFile::Load("../resources/apitest/ShapeType.pag");
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
PAG_TEST_F(PAGFileBaseTest, ChildFrameToLocalAdvanced) {
  auto testFile = PAGFile::Load("../resources/apitest/ShapeType.pag");
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
PAG_TEST_F(PAGFileBaseTest, EllipseToPath_ID80701969) {
  auto pagFile = PAGFile::Load("../resources/apitest/ellipse_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGFileTest"]["EllipseToPath_ID80701969"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGFileTest"]["EllipseToPath_ID80701969"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: 测试矩形转path，iOS14badcase
 */
PAG_TEST_F(PAGFileBaseTest, RectToPath_ID80703199) {
  auto pagFile = PAGFile::Load("../resources/apitest/rect_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGFileTest"]["RectToPath_ID80703199"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGFileTest"]["RectToPath_ID80703199"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: 测试圆角矩形转path,iOS14badCase
 */
PAG_TEST_F(PAGFileBaseTest, RoundRectToPath_ID80703201) {
  auto pagFile = PAGFile::Load("../resources/apitest/round_rect_to_path.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGFileTest"]["RoundRectToPath_ID80703201"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGFileTest"]["RoundRectToPath_ID80703201"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGFile设置开始时间
 */
PAG_TEST_F(PAGFileBaseTest, SetStartTime) {
  auto pagFile = PAGFile::Load("../assets/replacement.pag");
  TestPAGPlayer->setComposition(pagFile);
  pagFile->setStartTime(2000000);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto skImage = MakeSnapshot(TestPAGSurface);
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGFileTest"]["SetStartTime"] = md5;
#ifdef COMPARE_JSON_PATH
  auto setStartTimeMD5 = PAGTestEnvironment::CompareJson["PAGFileTest"]["SetStartTime"];
  if (setStartTimeMD5 != nullptr) {
    EXPECT_EQ(setStartTimeMD5.get<std::string>(), md5);
  }
#endif
}
}  // namespace pag
