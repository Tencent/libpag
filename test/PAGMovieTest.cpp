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

#ifdef FILE_MOVIE

#include <base/utils/TimeUtil.h>
#include <fstream>
#include "gpu/Surface.h"
#include "ffmpeg/FFmpegDecoder.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"
#include "platform/swiftshader/NativePlatform.h"
#include "platform/NativeGLDevice.h"
#include "video/FFmpegDemuxer.h"
#include "video/VideoDecoder.h"

namespace pag {
using nlohmann::json;

json dumpJson_movie;
json compareJson_movie;
bool dumpStatus_movie;

PAG_TEST_SUIT(PAGMovieTest)

void initMovieData() {
  std::ifstream inputFile("../test/res/compare_pagmovie_md5.json");
  if (inputFile) {
    inputFile >> compareJson_movie;
    dumpJson_movie = compareJson_movie;
  }
}

void saveMovieFile() {
  if (dumpStatus_movie) {
    std::ofstream outFile("../test/out/pagmovie_md5_dump.json");
    outFile << std::setw(4) << dumpJson_movie << std::endl;
    outFile.close();
  }
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"解码",
"功能":"PAGMovie兼容性",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"FromVideoPath",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Compatibility_NULLPATH_ID79147585",
"用例描述":"PAGMovie兼容性测试-NULLPATH"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Compatibility_NULLPATH_ID79147585) {
  initMovieData();
  auto pagMovieFromPath = PAGMovie::FromVideoPath("");
  GTEST_ASSERT_EQ(pagMovieFromPath, nullptr);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"解码",
"功能":"PAGMovie兼容性",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"FromVideoPath",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Compatibility_VALIDPATH_ID79147585",
"用例描述":"PAGMovie兼容性测试-Valid PATH"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Compatibility_VALIDPATH_ID79147585) {
  std::string videoPath = "../asset/data_vector.pag";
  auto pagMovieFromPath = PAGMovie::FromVideoPath(videoPath);
  GTEST_ASSERT_EQ(pagMovieFromPath, nullptr);
}

/*CaseAdditionInfo start
  {
  "FT":"内核FT",
  "模块":"解码",
  "功能":"PAGMovie兼容性",
  "测试分类":"功能",
  "测试阶段":"全特性",
  "被测函数":"FromVideoPath",
  "管理者":"kevingpqi",
  "用例等级":"P0",
  "用例类型":"5",
  "className":"PAGMovieTest",
  "methodName":"Compatibility_PATH_ID79147585",
  "用例描述":"PAGMovie兼容性测试-PATH"
  }
  CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Compatibility_PATH_ID79147585) {
  std::string videoPath = "../resources/video/h265.MOV";
  auto pagMovieFromPath = PAGMovie::FromVideoPath(videoPath, 36000000, 20000000);
  GTEST_ASSERT_EQ(pagMovieFromPath, nullptr);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"解码",
"功能":"PAGMovie解码",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"isMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Attributes_Video_ID79166203",
"用例描述":"PAGMovie解码测试-视频"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Attributes_Video_ID79166203) {
  std::string videoPath = "../resources/video/h264.mp4";
  auto pagMovie = PAGMovie::FromVideoPath(videoPath);
  auto movie = PAGMovie::FromVideoPath(videoPath, 1000000, 2000000);
  GTEST_ASSERT_NE(pagMovie, nullptr);
  GTEST_ASSERT_NE(movie, nullptr);
  GTEST_ASSERT_EQ(pagMovie->isMovie(), true);
  GTEST_ASSERT_NE(pagMovie, movie);
  GTEST_ASSERT_NE(pagMovie->uniqueID(), movie->uniqueID());
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"解码",
"功能":"PAGMovie解码",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"isMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Attributes_NOTVIDEO_ID79166203",
"用例描述":"PAGMovie解码测试-非视频"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Attributes_NOTVIDEO_ID79166203) {
  auto pagImage = PAGImage::FromPath("../assets/scene.png");
  ASSERT_TRUE(pagImage != nullptr);
  EXPECT_EQ(pagImage->isMovie(), false);
}

void DemuxerDecoderTest(const std::string& path, std::string startCode) {
  if (compareJson_movie == nullptr) {
    initMovieData();
  }
  std::string decoderKey;
  if (path.rfind("h265") != std::string::npos) {
    if (path.rfind("h265_r90") != std::string::npos) {
      decoderKey = "decoder_h265_r90_" + startCode;
    } else {
      decoderKey = "decoder_h265_" + startCode;
    }
  } else {
    decoderKey = "decoder_h264_" + startCode;
  }

  auto demuxer = FFmpegDemuxer::Make(path, Platform::Current()->naluType());
  EXPECT_NE(demuxer, nullptr);
  int trackIndex = -1;
  for (int i = 0; i < demuxer->getTrackCount(); ++i) {
    auto format = demuxer->getTrackFormat(i);
    if (!format) {
      continue;
    }
    if (format->getString(KEY_MIME).rfind("video", 0) == 0) {
      trackIndex = format->getInteger(KEY_TRACK_ID);
      break;
    }
  }
  demuxer->selectTrack(trackIndex);
  auto format = demuxer->getTrackFormat(trackIndex);

  VideoConfig config = {};
  config.headers = format->headers();
  demuxer->selectTrack(trackIndex);
  demuxer->seekTo(0);
  config.colorSpace = format->getString(KEY_COLOR_SPACE) == COLORSPACE_REC709
                          ? YUVColorSpace::Rec709
                          : YUVColorSpace::Rec601;
  config.width = format->getInteger(KEY_WIDTH);
  config.height = format->getInteger(KEY_HEIGHT);
  config.mimeType = format->getString(KEY_MIME);
  auto decoder = VideoDecoder::Make(config, false);
  while (true) {
    demuxer->advance();
    auto byteData = demuxer->readSampleData();
    decoder->onSendBytes(byteData.data, byteData.length, demuxer->getSampleTime());
    auto result = decoder->onDecodeFrame();
    if (result == DecodingResult::Success) {
      break;
    }
  }
  auto videoImage = decoder->onRenderFrame();
  auto device = NativeGLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto texture = videoImage->makeTexture(context);
  auto surface = Surface::Make(context, texture->width(), texture->height());
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());
  Bitmap bitmap = {};
  bitmap.allocPixels(texture->width(), texture->height());
  auto pixels = bitmap.lockPixels();
  surface->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  device->unlock();
  std::string compareMD5 = compareJson_movie["DemuxerDecoder"][decoderKey];
  auto decoderMD5 = DumpMD5(bitmap);
  std::string imagePath = "../test/out/" + decoderKey + ".png";
  TraceIf(bitmap, imagePath, decoderMD5 != compareMD5);
  if (decoderMD5 != compareMD5) {
    dumpStatus_movie = true;
  }
  EXPECT_EQ(decoderMD5, compareMD5);
  dumpJson_movie["DemuxerDecoder"][decoderKey] = decoderMD5;
}

void KeyFramesTest(nlohmann::json& dumpJson_movie, nlohmann::json& compareJson_movie,
                   const std::string& path, std::string errorMsg) {
  std::string keyFramesKey;
  if (path.rfind("h264") != std::string::npos) {
    keyFramesKey = "keyFrames_h264";
  } else {
    keyFramesKey = "keyFrames_h265";
  }

  std::vector<std::string> keyFrameCompareVector;

  auto demuxer = FFmpegDemuxer::Make(path, Platform::Current()->naluType());
  EXPECT_NE(demuxer, nullptr);
  int trackIndex = -1;
  for (int i = 0; i < demuxer->getTrackCount(); ++i) {
    auto format = demuxer->getTrackFormat(i);
    if (!format) {
      continue;
    }
    if (format->getString(KEY_MIME).rfind("video", 0) == 0) {
      trackIndex = format->getInteger(KEY_TRACK_ID);
      break;
    }
  }
  demuxer->selectTrack(trackIndex);
  auto format = demuxer->getTrackFormat(trackIndex);

  std::vector<ByteData*> headers;
  for (auto& header : format->headers()) {
    headers.push_back(header.get());
  }

  demuxer->selectTrack(trackIndex);
  demuxer->seekTo(0);

  auto ptsDetail = demuxer->ptsDetail();
  std::vector<int64_t> keyFrames{};
  for (auto keyframeIndex : ptsDetail->keyframeIndexVector) {
    keyFrames.push_back(ptsDetail->ptsVector[keyframeIndex]);
  }
  if (compareJson_movie.contains(keyFramesKey) && compareJson_movie[keyFramesKey].is_array()) {
    keyFrameCompareVector = compareJson_movie[keyFramesKey].get<std::vector<std::string>>();
  }
  std::vector<std::string> keyFramesVector;
  for (size_t i = 0; i < keyFrames.size(); i++) {
    auto keyFrameString = std::to_string(keyFrames[i]);
    keyFramesVector.push_back(keyFrameString);
    if (keyFrameCompareVector.size() > i && keyFrameCompareVector[i] != keyFrameString) {
      errorMsg += (std::to_string(i) + "_keyFrame;");
      dumpStatus_movie = true;
    }
  }
  EXPECT_EQ(errorMsg, "") << keyFramesKey << " frame fail";
  dumpJson_movie[keyFramesKey] = keyFramesVector;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h264_AVCC_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h264-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h264_AVCC_Decoder_ID79166399) {
  std::string videoPathAVC = "../resources/video/h264.mp4";

  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AVCC);
  DemuxerDecoderTest(videoPathAVC, "AVCC");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染 外部软解注入",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h264_AVCC_Decoder_ID862231499",
"用例描述":"PAGMovie渲染校验-h264-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h264_AVCC_Decoder_ID862231499) {
  std::string videoPathAVC = "../resources/video/h264.mp4";

  auto oldFactory = VideoDecoder::GetExternalSoftwareDecoderFactory();
  pagTest::FFmpegDecoderFactory decoderFactory = {};
  PAGVideoDecoder::RegisterSoftwareDecoderFactory(&decoderFactory);
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AVCC);
  DemuxerDecoderTest(videoPathAVC, "AVCC_Injection");
  platform->setNALUType(oldNALUType);
  PAGVideoDecoder::RegisterSoftwareDecoderFactory(oldFactory);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染 外部软解注入",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h264_AVCC_Decoder_ID862231499",
"用例描述":"PAGMovie渲染校验-h264-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_AVCC_Decoder_ID862231499) {
  std::string videoPathAVC = "../resources/video/h265.MOV";

  auto oldFactory = VideoDecoder::GetExternalSoftwareDecoderFactory();
  pagTest::FFmpegDecoderFactory decoderFactory = {};
  PAGVideoDecoder::RegisterSoftwareDecoderFactory(&decoderFactory);
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AVCC);
  DemuxerDecoderTest(videoPathAVC, "AVCC_Injection");
  platform->setNALUType(oldNALUType);
  PAGVideoDecoder::RegisterSoftwareDecoderFactory(oldFactory);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h265_AVCC_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h265-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_AVCC_Decoder_ID79166399) {
  std::string videoPathHEVC = "../resources/video/h265.MOV";
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AVCC);
  DemuxerDecoderTest(videoPathHEVC, "AVCC");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h264_AnnexB_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h264-AnnexB"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h264_AnnexB_Decoder_ID79166399) {
  std::string videoPathAVC = "../resources/video/h264.mp4";
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AnnexB);
  DemuxerDecoderTest(videoPathAVC, "AnnexB");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h265_AnnexB_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h265-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_AnnexB_Decoder_ID79166399) {
  std::string videoPathHEVC = "../resources/video/h265.MOV";
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AnnexB);
  DemuxerDecoderTest(videoPathHEVC, "AnnexB");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h265_r90_AVCC_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h265_r90-AVCC"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_r90_AVCC_Decoder_ID79166399) {
  std::string videoPathHEVC = "../resources/video/h265_r90.mp4";
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AVCC);
  DemuxerDecoderTest(videoPathHEVC, "AVCC");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h265_r90_AnnexB_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h264-AnnexB"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_r90_AnnexB_Decoder_ID79166399) {
  std::string videoPathAVC = "../resources/video/h265_r90.mp4";
  auto platform = static_cast<const NativePlatform*>(Platform::Current());
  auto oldNALUType = platform->naluType();
  platform->setNALUType(NALUType::AnnexB);
  DemuxerDecoderTest(videoPathAVC, "AnnexB");
  platform->setNALUType(oldNALUType);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h264_Keyframes_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h264-Keyframes"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h264_Keyframes_Decoder_ID79166399) {
  std::string errorMsg;
  std::string videoPathAVC = "../resources/video/h264.mp4";
  if (compareJson_movie == nullptr) {
    initMovieData();
  }
  KeyFramesTest(dumpJson_movie, compareJson_movie, videoPathAVC, errorMsg);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"渲染",
"功能":"PAGMovie渲染",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"KeyFrame_h265_Keyframes_Decoder_ID79166399",
"用例描述":"PAGMovie渲染校验-h265-Keyframes"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, KeyFrame_h265_Keyframes_Decoder_ID79166399) {
  std::string errorMsg;
  std::string videoPathHEVC = "../resources/video/h265.MOV";
  if (compareJson_movie == nullptr) {
    initMovieData();
  }
  KeyFramesTest(dumpJson_movie, compareJson_movie, videoPathHEVC, errorMsg);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGLayerTest图层基础功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGCompositionLayer",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"PAGMovie_MD5_ID79148011",
"用例描述":"PAGLayerTest基础功能"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, PAGMovie_MD5_ID79148011) {
  std::string fileName = "pagmovie";
  std::vector<std::string> compareVector;
  if (compareJson_movie == nullptr) {
    initMovieData();
  }
  if (compareJson_movie.contains(fileName) && compareJson_movie[fileName].is_array()) {
    compareVector = compareJson_movie[fileName].get<std::vector<std::string>>();
  }

  std::string videoPathAVC = "../resources/video/h264.mp4";
  std::string videoPathHEVC = "../resources/video/h265.MOV";
  std::string videoPathRotation = "../resources/video/h265_r90.mp4";

  int64_t time = 0;
  auto movieAVC = PAGMovie::FromVideoPath(videoPathAVC);
  auto width = movieAVC->width();
  auto height = movieAVC->height();
  auto durationAVC = movieAVC->duration();
  auto pagImageLayerAVC = PAGImageLayer::Make(width, height, durationAVC);
  pagImageLayerAVC->replaceImage(movieAVC);
  pagImageLayerAVC->setStartTime(time);
  time += durationAVC;

  auto movieHEVC = PAGMovie::FromVideoPath(videoPathHEVC, 4000000, 2000000);
  auto durationHEVC = movieHEVC->duration() * 2;
  auto pagImageLayerHEVC = PAGImageLayer::Make(width, height, durationHEVC);
  pagImageLayerHEVC->replaceImage(movieHEVC);
  pagImageLayerHEVC->setStartTime(time);
  time += durationHEVC;

  auto movieFixedFrame = PAGMovie::FromVideoPath(videoPathAVC, 2000000, 0);
  auto pagImageLayerFixedFrame = PAGImageLayer::Make(width, height, 1000000);
  pagImageLayerFixedFrame->replaceImage(movieFixedFrame);
  pagImageLayerFixedFrame->setStartTime(time);
  time += 1000000;

  auto movieRotation = PAGMovie::FromVideoPath(videoPathRotation, 500000, 2000000);
  auto durationRotation = movieRotation->duration() / 2;
  auto pagImageLayerRotation = PAGImageLayer::Make(width, height, durationRotation);
  pagImageLayerRotation->replaceImage(movieRotation);
  pagImageLayerRotation->setStartTime(time);
  time += durationRotation;

  auto compostiton = PAGComposition::Make(width, height);
  compostiton->addLayer(pagImageLayerAVC);
  compostiton->addLayer(pagImageLayerHEVC);
  compostiton->addLayer(pagImageLayerFixedFrame);
  compostiton->addLayer(pagImageLayerRotation);

  auto pagSurface = PAGSurface::MakeOffscreen(width, height);
  auto pagPlayer = new PAGPlayer();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(compostiton);
  pagPlayer->setMaxFrameRate(24);

  int totalFrames = static_cast<int>(pagPlayer->maxFrameRate() * compostiton->duration() / 1000000);
  bool status = true;

  std::vector<std::string> md5Vector;
  std::string errorMsg;
  std::vector<int> compareArray = {0, 60, 120, 180, 220, 260, 275};
  for (size_t i = 0; i < compareArray.size(); i++) {
    int currentFrame = compareArray[i];
    pagPlayer->setProgress(currentFrame * 1.0 / totalFrames);
    pagPlayer->flush();

    auto skImage = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(skImage);
    md5Vector.push_back(md5);
    if (compareVector.size() <= i || compareVector[i] != md5) {
      errorMsg += (std::to_string(currentFrame) + ";");
      dumpStatus_movie = true;
      if (status) {
        std::string imagePath =
            "../test/out/" + fileName + "_" + std::to_string(currentFrame) + ".png";
        Trace(skImage, imagePath);
        status = false;
      }
    }
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  dumpJson_movie[fileName] = md5Vector;

  saveMovieFile();

  delete pagPlayer;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"视频",
"功能":"视频seek",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"FFmpegDemuxer",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Seek_ID83033955",
"用例描述":"视频seek"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Seek_ID83033955) {
  // flag 是 AVSEEK_FLAG_BACKWARD 时，seek 传入关键帧的时间，得到的时间小于不等于传入的时间。
  // flag 是 0 时，seek 传入关键帧的时间，得到的时间大于等于传入的时间。
  auto demuxer = FFmpegDemuxer::Make("../resources/video/h264.mp4", NALUType::AVCC);
  demuxer->selectTrack(0);
  demuxer->getTrackFormat(0);
  int64_t keyframeTime = 6000000;
  demuxer->seekTo(keyframeTime);
  demuxer->advance();
  EXPECT_EQ(keyframeTime, demuxer->getSampleTime());
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"视频",
"功能":"视频seek",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"FFmpegDecoder",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"Decoder_ID83033955",
"用例描述":"视频解码,解码过程中不应出现TryAgainLater"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, Decoder_ID83033955) {
  auto demuxer =
      FFmpegDemuxer::Make("../resources/video/h264.mp4", Platform::Current()->naluType());
  int trackIndex = -1;
  for (int i = 0; i < demuxer->getTrackCount(); ++i) {
    auto format = demuxer->getTrackFormat(i);
    if (!format) {
      continue;
    }
    if (format->getString(KEY_MIME).rfind("video", 0) == 0) {
      trackIndex = format->getInteger(KEY_TRACK_ID);
      break;
    }
  }
  demuxer->selectTrack(trackIndex);
  auto format = demuxer->getTrackFormat(trackIndex);

  VideoConfig config = {};
  config.headers = format->headers();
  demuxer->selectTrack(trackIndex);
  config.colorSpace = format->getString(KEY_COLOR_SPACE) == COLORSPACE_REC709
                          ? YUVColorSpace::Rec709
                          : YUVColorSpace::Rec601;
  config.width = format->getInteger(KEY_WIDTH);
  config.height = format->getInteger(KEY_HEIGHT);
  config.mimeType = format->getString(KEY_MIME);
  auto decoder = VideoDecoder::Make(config, false);

  demuxer->seekTo(0);
  for (int i = 0; i < 30; i++) {
    demuxer->advance();
    auto byteData = demuxer->readSampleData();
    decoder->onSendBytes(byteData.data, byteData.length, demuxer->getSampleTime());
    auto result = decoder->onDecodeFrame();
    if (result != DecodingResult::TryAgainLater) {
      EXPECT_TRUE(result == DecodingResult::Success);
      break;
    }
  }
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"视频",
"功能":"视频首帧pts大于0",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"First_PTS_ID84522331",
"用例描述":"视频首帧pts大于0"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, First_PTS_ID84522331) {
  std::string videoPath = "../resources/video/h264_first_pts.mp4";
  auto pagMovie = PAGMovie::FromVideoPath(videoPath);
  auto imageLayer = PAGImageLayer::Make(pagMovie->width(), pagMovie->height(), 3000000);
  imageLayer->replaceImage(pagMovie);
  auto composition = PAGComposition::Make(pagMovie->width(), pagMovie->height());
  composition->addLayer(imageLayer);
  auto surface = PAGSurface::MakeOffscreen(pagMovie->width(), pagMovie->height());
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->setProgress(0);
  player->flush();
  auto md5 = DumpMD5(surface);
  dumpJson_movie["First_PTS_ID84522331"] = md5;
#ifdef COMPARE_JSON_PATH
  auto compareJson = compareJson_movie["First_PTS_ID84522331"];
  if (compareJson != nullptr) {
    TraceIf(surface, "../test/out/First_PTS_ID84522331.png", compareJson != md5);
    EXPECT_EQ(compareJson.get<std::string>(), md5);
  }
#endif
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"视频",
"功能":"视频帧率",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGMovie",
"管理者":"partyhuang",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGMovieTest",
"methodName":"FrameRate",
"用例描述":"视频帧率为24帧时，渲染不跳帧"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGMovieTest, FrameRate) {
  std::vector<std::string> compareVector;
  std::string errorMsg;
  initMovieData();
  if (compareJson_movie.contains("FrameRate") && compareJson_movie["FrameRate"].is_array()) {
    compareVector = compareJson_movie["FrameRate"].get<std::vector<std::string>>();
  }
  std::string videoPathAVC = "../resources/video/24.mov";
  auto movie = pag::PAGMovie::FromVideoPath(videoPathAVC);
  auto player = pag::PAGPlayer();
  auto pagSurface = PAGSurface::MakeOffscreen(720, 1280);
  player.setSurface(pagSurface);
  auto composition = pag::PAGComposition::Make(720, 1280);
  auto imageLayer = pag::PAGImageLayer::Make(720, 1280, movie->duration());
  imageLayer->replaceImage(movie);
  composition->addLayer(imageLayer);
  player.setComposition(composition);
  std::vector<std::string> md5Vector;
  bool status = true;
  for (size_t i = 0; i < 10; i++) {
    composition->setCurrentTime(FrameToTime(i, 24));
    player.flush();
    auto skImage = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(skImage);
    md5Vector.push_back(md5);
    if (compareVector.size() <= i || compareVector[i] != md5) {
      errorMsg += (std::to_string(i) + ";");
      dumpStatus_movie = true;
      if (status) {
        std::string imagePath = "../test/out/24_" + std::to_string(i) + ".png";
        Trace(skImage, imagePath);
        status = false;
      }
    }
  }
  EXPECT_EQ(errorMsg, "") << "24.mp4"
                          << " frame fail";
}

}  // namespace pag
#endif
