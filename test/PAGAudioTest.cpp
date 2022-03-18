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

#include "audio/model/AudioAsset.h"
#include "audio/model/AudioComposition.h"
#include "audio/reader/AudioReader.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "pag/audio.h"

#define BOHEMIAN_RHAPSODY_PATH "../resources/audio/Queen-BohemianRhapsody.mp3"
#define PAG_AUDIO_MARKER_PATH "../assets/AudioMarker.pag"
#define H264_MP4_PATH "../resources/video/h264.mp4"

namespace pag {
uint8_t* concat(uint8_t* a, size_t a_size, uint8_t* b, size_t b_size) {
  auto c = static_cast<uint8_t*>(realloc(a, a_size + b_size));
  memcpy(c + a_size, b, b_size);
  return c;
}

void exportPCM(uint8_t** output, size_t& size, const std::shared_ptr<PAGAudioReader>& reader) {
  if (reader == nullptr) {
    return;
  }
  auto outputLength = SampleTimeToLength(reader->audioReader->audio->duration(),
                                         reader->audioReader->outputConfig().get());
  outputLength -= reader->audioReader->currentOutputLength;
  auto totalLength = static_cast<size_t>(outputLength);
  while (totalLength > size) {
    auto frame = reader->copyNextFrame();
    if (frame == nullptr) {
      break;
    }
    auto dataSize =
        std::min(static_cast<size_t>(frame->length), static_cast<size_t>(totalLength) - size);
    *output = concat(*output, size, frame->data, dataSize);
    size += frame->length;
  }
}

void SavePCM(const char* audio_dst_filename, uint8_t* bytes, size_t size) {
  if (bytes == nullptr || size == 0) {
    return;
  }
  auto audioDstFile = fopen(audio_dst_filename, "wb");
  if (!audioDstFile) {
    printf("Could not open destination file %s\n", audio_dst_filename);
    return;
  }
  fwrite(bytes, 1, size, audioDstFile);
  fclose(audioDstFile);
}

PAG_TEST_SUIT(PAGAudioTest)

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"MakeMedia_ID79666987",
"用例描述":"测试 media 创建"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, MakeMedia_ID79666987) {
  auto media = AudioAsset::Make("");
  EXPECT_EQ(media, nullptr);
  media = AudioAsset::Make(std::shared_ptr<ByteData>());
  EXPECT_EQ(media, nullptr);
  media = AudioAsset::Make(BOHEMIAN_RHAPSODY_PATH);
  EXPECT_EQ(media->duration(), 356231837);
  media = AudioAsset::Make(H264_MP4_PATH);
  EXPECT_EQ(media->duration(), 6016000);
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  auto data = new uint8_t[pagFile->audioBytes()->length()];
  memcpy(data, pagFile->audioBytes()->data(), pagFile->audioBytes()->length());
  auto byteData = std::make_shared<ByteData>(data, pagFile->audioBytes()->length());
  media = AudioAsset::Make(byteData);
  EXPECT_EQ(media->duration(), 19991995);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"MakeMedia_ID80324199",
"用例描述":"从缓存创建Audio"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, MakeMedia_ID80324199) {
  auto media = AudioAsset::Make(BOHEMIAN_RHAPSODY_PATH);
  auto media2 = AudioAsset::Make(BOHEMIAN_RHAPSODY_PATH);
  EXPECT_EQ(media, media2);
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  auto data = new uint8_t[pagFile->audioBytes()->length()];
  memcpy(data, pagFile->audioBytes()->data(), pagFile->audioBytes()->length());
  auto byteData = std::make_shared<ByteData>(data, pagFile->audioBytes()->length());
  media = AudioAsset::Make(byteData);
  media2 = AudioAsset::Make(byteData);
  EXPECT_EQ(media, media2);
  data = new uint8_t[pagFile->audioBytes()->length()];
  memcpy(data, pagFile->audioBytes()->data(), pagFile->audioBytes()->length());
  byteData = std::make_shared<ByteData>(data, pagFile->audioBytes()->length());
  media = AudioAsset::Make(byteData);
  EXPECT_NE(media, media2);
  media2 = AudioAsset::Make(byteData);
  EXPECT_EQ(media, media2);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"PAGAudioReader_ID79814287",
"用例描述":"测试PAGAudioReader参数"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, PAGAudioReader_ID79814287) {
  auto audio = std::make_shared<PAGAudio>();
  auto filePath = BOHEMIAN_RHAPSODY_PATH;
  PAGAudioTrack track{};
  track.insertTimeRange({195000000, 200000000}, filePath, 0);
  audio->addTrack(track);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio, 48000, 100, 1));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["PAGAudioReader_ID79814287"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["PAGAudioReader_ID79814287"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;

  audio->removeAllTracks();
  EXPECT_EQ(audio->audioComposition->duration(), 0);
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"AddTrack_ID79666989",
"用例描述":"测试音频中间裁剪拼接声音是否连续"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, AddTrack_ID79666989) {
  auto audio = std::make_shared<PAGAudio>();
  auto filePath = BOHEMIAN_RHAPSODY_PATH;
  PAGAudioTrack track{};
  track.insertTimeRange({25000000, 28000000}, filePath, 0);
  track.insertTimeRange({100000000, 115000000}, filePath, 5000000);
  track.insertTimeRange({115000000, 130000000}, filePath, 20000000);

  track.setVolumeRamp(0, 3, {15000000, 20000000});
  track.setVolumeRamp(3, 0, {20000000, 25000000});
  track.setVolumeRamp(1, 1, {26000000, 27000000});
  audio->addTrack(track);

  track.insertTimeRange({115000000, 130000000}, filePath, 30000000);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AddTrack_ID79666989"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AddTrack_ID79666989"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"AddTrack_ID79666991",
"用例描述":"测试音频变速"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, AddTrack_ID79666991) {
  auto audio = std::make_shared<PAGAudio>();
  auto filePath = BOHEMIAN_RHAPSODY_PATH;
  PAGAudioTrack track{};
  track.insertTimeRange({100000000, 130000000}, filePath, 3000000, 1.6);
  track.insertTimeRange({100000000, 102000000}, filePath, 5000000, 1.2);

  audio->addTrack(track);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AddTrack_ID79666991"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AddTrack_ID79666991"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"AddTrack_ID79666993",
"用例描述":"测试音频track编辑"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, AddTrack_ID79666993) {
  uint8_t* output = nullptr;
  size_t size = 0;
  auto audio = std::make_shared<PAGAudio>();
  auto filePath = BOHEMIAN_RHAPSODY_PATH;
  PAGAudioTrack track{};
  // 0-3, 3-35
  track.insertTimeRange({100000000, 132000000}, filePath, 3000000);
  // 0-3, 3-5, 5-13, 13-33
  track.track->scaleTimeRange({5000000, 15000000}, 8000000);
  // 0-3, 3-5, 5-10, 10-12, 12-14, 14-31
  track.track->scaleTimeRange({10000000, 16000000}, 4000000);
  // 0-3, 3-5, 5-10, 10-12, 12-14, 14-21, 21-25, 25-29
  track.track->scaleTimeRange({21000000, 27000000}, 4000000);
  audio->addTrack(track);
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AddTrack_ID79666993"]["1"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AddTrack_ID79666993"]["1"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif

  delete[] output;
  output = nullptr;
  size = 0;

  auto audio1 = std::make_shared<PAGAudio>();
  // 0-1, 1-3, 3-8, 8-10, 10-12, 12-19, 19-23, 23-27
  track.track->removeTimeRange({0, 2000000});
  // 0-1, 1-3, 3-8, 8-10, 10-12, 12-16, 16-18
  track.track->removeTimeRange({16000000, 25000000});
  audio1->addTrack(track);
  exportPCM(&output, size, PAGAudioReader::Make(audio1));

  md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AddTrack_ID79666993"]["2"] = md5;
#ifdef COMPARE_JSON_PATH
  audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AddTrack_ID79666993"]["2"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
  output = nullptr;
  size = 0;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79666995",
"用例描述":"测试 PAG 里面的音频导出"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79666995) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79666995"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79666995"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827345",
"用例描述":"测试PAGFile audioBytes repeat情况"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827345) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->setDuration(50000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827345"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827345"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827367",
"用例描述":"测试 PAGFile audioBytes scale 情况，没有 scaleTimeRange"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827367) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(10000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827367"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827367"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827417",
"用例描述":"测试 PAGFile audioBytes，scaleTimeRange 和 audioTimeRange 不相交，audioTimeRange
在右边的 "
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827417) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->getFile()->scaledTimeRange = {1, 10};
  static_cast<PreComposeLayer*>(pagFile->layer)->composition->audioStartTime = 20;
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(10000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827417"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827417"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827425",
"用例描述":"测试PAGFile audioBytes，audioTimeRange 包含 scaleTimeRange"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827425) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->getFile()->scaledTimeRange = {100, 200};
  static_cast<PreComposeLayer*>(pagFile->layer)->composition->audioStartTime = 20;
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(18000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827425"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827425"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827427",
"用例描述":"测试PAGFile audioBytes，scaleTimeRange 包含 audioTimeRange"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827427) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->getFile()->scaledTimeRange = {100, 500};
  static_cast<PreComposeLayer*>(pagFile->layer)->composition->audioStartTime = 200;
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(18000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827427"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827427"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827431",
"用例描述":"测试PAGFile audioBytes，scaleTimeRange 和 audioTimeRange 右相交"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827431) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->getFile()->scaledTimeRange = {100, 300};
  static_cast<PreComposeLayer*>(pagFile->layer)->composition->audioStartTime = 200;
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(18000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827431"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827431"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpPAGAudioBytes_ID79827461",
"用例描述":"测试PAGFile audioBytes，scaleTimeRange 和 audioTimeRange 左相交"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpPAGAudioBytes_ID79827461) {
  auto pagFile = PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagFile->getFile()->scaledTimeRange = {100, 500};
  static_cast<PreComposeLayer*>(pagFile->layer)->composition->audioStartTime = 0;
  pagFile->setTimeStretchMode(PAGTimeStretchMode::Scale);
  pagFile->setDuration(15000000);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827461"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpPAGAudioBytes_ID79827461"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpAudioFromPAGComposition_ID79666997",
"用例描述":"测试PAGImageLayer裁剪"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudioFromPAGComposition_ID79666997) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 8000000, 6000000);
  auto imageLayer = PAGImageLayer::Make(1170, 540, 6000000);
  imageLayer->replaceImage(movie);
  auto composition = PAGComposition::Make(1170, 540);
  composition->addLayer(imageLayer);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(composition.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79666997"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 =
      PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79666997"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpAudioFromPAGComposition_ID79667001",
"用例描述":"测试 imageLayer.duration 和 movie.duration 不一致时的处理"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudioFromPAGComposition_ID79667001) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 8000000, 6000000);
  auto imageLayer = PAGImageLayer::Make(1170, 540, 3000000);
  imageLayer->replaceImage(movie);
  auto composition = PAGComposition::Make(1170, 540);
  composition->addLayer(imageLayer);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(composition.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667001"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 =
      PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667001"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpAudioFromPAGComposition_ID79667003",
"用例描述":"测试图层变速"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudioFromPAGComposition_ID79667003) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 8000000, 6000000, 2);
  auto imageLayer = PAGImageLayer::Make(1170, 540, 6000000);
  imageLayer->replaceImage(movie);
  auto composition = PAGComposition::Make(1170, 540);
  composition->addLayer(imageLayer);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(composition.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667003"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 =
      PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667003"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"图层",
"功能":"PAGAudio音频功能",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"PAGAudio",
"管理者":"pengweilv",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGAudioTest",
"methodName":"DumpAudioFromPAGComposition_ID79667007",
"用例描述":"测试 startTime 和 PAGImageLayer.volumeRanges"
}
CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudioFromPAGComposition_ID79667007) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 1000000, 4000000);
  auto imageLayer = PAGImageLayer::Make(1170, 540, 6000000);
  imageLayer->replaceImage(movie);
  imageLayer->setStartTime(2000000);
  movie->setVolumeRamp(0, 2, {0, 2000000});
  movie->setVolumeRamp(2, 0, {4000000, 6000000});
  auto composition = PAGComposition::Make(1170, 540);
  composition->addLayer(imageLayer);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(composition.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667007"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 =
      PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667007"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"DumpAudioFromPAGComposition_ID79667009",
 "用例描述":"测试 timeRamp"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudioFromPAGComposition_ID79667009) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 8000000, 2500000, 0.25);
  auto pagFile = PAGFile::Load("../resources/apitest/ImageFillRule.pag");
  auto layer = pagFile->getLayersByEditableIndex(0, LayerType::Image)[0];
  auto imageLayer = std::static_pointer_cast<PAGImageLayer>(layer);
  imageLayer->replaceImage(movie);
  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());
  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667009"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 =
      PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudioFromPAGComposition_ID79667009"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"RemoveTrack_ID79825037",
 "用例描述":"测试删除轨道"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, RemoveTrack_ID79825037) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 1000000, 500000, 0.25);
  auto pagFile = PAGFile::Load("../resources/apitest/ImageFillRule.pag");
  auto layer = pagFile->getLayersByEditableIndex(0, LayerType::Image)[0];
  auto imageLayer = std::static_pointer_cast<PAGImageLayer>(layer);
  imageLayer->replaceImage(movie);
  auto audio = std::make_shared<PAGAudio>();
  auto trackID = audio->addTracks(pagFile.get());
  EXPECT_FALSE(audio->isEmpty());
  audio->removeTrack(trackID);
  EXPECT_TRUE(audio->isEmpty());
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"DumpAudio_ID80121541",
 "用例描述":"测试 dump 音频-PAGFile 的 ImageLayer startTime 为负的情况"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudio_ID80121541) {
  auto filePath = H264_MP4_PATH;
  auto pagFile = PAGFile::Load("../resources/apitest/ZC2.pag");

  auto movie1 = PAGMovie::FromVideoPath(filePath, 13000000, 2500000);
  pagFile->replaceImage(0, movie1);

  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(pagFile.get());

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudio_ID80121541"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudio_ID80121541"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"DumpAudio_ID80122365",
 "用例描述":"测试 dump 音频-自定义创建的 PAGImageLayer startTime 为负的情况"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, DumpAudio_ID80122365) {
  auto filePath = H264_MP4_PATH;
  auto movie = PAGMovie::FromVideoPath(filePath, 3000000, 3000000);
  auto imageLayer = PAGImageLayer::Make(movie->width(), movie->height(), 3000000);
  imageLayer->replaceImage(movie);
  imageLayer->setStartTime(-1000000);

  auto composition = PAGComposition::Make(movie->width(), movie->height());
  composition->addLayer(imageLayer);

  auto audio = std::make_shared<PAGAudio>();
  audio->addTracks(composition.get());

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["DumpAudio_ID80122365"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["DumpAudio_ID80122365"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"AudioReaderSeek_ID83095165",
 "用例描述":"测试AudioReader的seek"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, AudioReaderSeek_ID83095165) {
  auto audio = std::make_shared<PAGAudio>();
  auto filePath = BOHEMIAN_RHAPSODY_PATH;
  PAGAudioTrack track{};
  auto targetTimeRange = TimeRange{195000000, 200000000};
  track.insertTimeRange(targetTimeRange, filePath, 0, 2);
  audio->addTrack(track);
  auto reader = PAGAudioReader::Make(audio, 44100, 1024, 2);
  auto seekTime = static_cast<int64_t>(static_cast<double>(targetTimeRange.duration()) * 0.25);
  reader->seek(seekTime);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, reader);

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AudioReaderSeek_ID83095165"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AudioReaderSeek_ID83095165"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

class AudioStream : public PAGPCMStream {
 public:
  explicit AudioStream(std::shared_ptr<AudioReader> reader, int64_t duration)
      : reader(std::move(reader)), duration_(duration) {
  }

  int64_t duration() override {
    return duration_;
  }

  void seek(int64_t toTime) override {
    reader->seekTo(toTime);
  }

  std::shared_ptr<PAGAudioFrame> copyNextFrame() override {
    return reader->copyNextFrame();
  }

 private:
  std::shared_ptr<AudioReader> reader = nullptr;
  int64_t duration_ = 0;
};

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"AudioStream_ID83080785",
 "用例描述":"测试 PAGAudioStream"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, AudioStream_ID83080785) {
  auto asset = AudioAsset::Make(BOHEMIAN_RHAPSODY_PATH);
  auto audioComposition = std::make_shared<AudioComposition>();
  auto compositionTrack = audioComposition->addTrack();
  auto speed = 0.5;
  auto sourceTimeRange = TimeRange{195000000, 200000000};
  auto targetDuration =
      static_cast<int64_t>(static_cast<double>(sourceTimeRange.duration()) * speed);
  compositionTrack->insertTimeRange(sourceTimeRange, *asset->tracks().front(), 0);
  compositionTrack->scaleTimeRange({0, sourceTimeRange.duration()}, targetDuration);
  auto stream = std::make_shared<AudioStream>(AudioReader::Make(audioComposition),
                                              audioComposition->duration());

  auto audio = std::make_shared<PAGAudio>();
  PAGAudioTrack track{};
  track.insertTimeRange({0, audioComposition->duration()}, stream, 0, static_cast<float>(speed));
  audio->addTrack(track);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, PAGAudioReader::Make(audio, 44100, 1024, 2));

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["AudioStream_ID83080785"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["AudioStream_ID83080785"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}

/*CaseAdditionInfo start
 {
 "FT":"内核FT",
 "模块":"图层",
 "功能":"PAGAudio音频功能",
 "测试分类":"功能",
 "测试阶段":"全特性",
 "被测函数":"PAGAudio",
 "管理者":"pengweilv",
 "用例等级":"P0",
 "用例类型":"5",
 "className":"PAGAudioTest",
 "methodName":"Audio_ID83186171",
 "用例描述":"PAGFile设置了startTime之后，音频偏移"
 }
 CaseAdditionInfo end*/
PAG_TEST(PAGAudioTest, Audio_ID83186171) {
  auto pagComposition = pag::PAGComposition::Make(1280, 720);
  auto pagAudioFile = pag::PAGFile::Load(PAG_AUDIO_MARKER_PATH);
  pagComposition->addLayer(pagAudioFile);
  pagAudioFile->setStartTime(1000000);

  auto pagAudio = std::make_shared<pag::PAGAudio>();
  pagAudio->addTracks(pagComposition.get());

  auto pagAudioReader = pag::PAGAudioReader::Make(pagAudio);
  pagAudioReader->seek(0);

  uint8_t* output = nullptr;
  size_t size = 0;
  exportPCM(&output, size, pagAudioReader);

  auto md5 = DumpMD5(output, size);
  PAGTestEnvironment::DumpJson["PAGAudioTest"]["Audio_ID83186171"] = md5;
#ifdef COMPARE_JSON_PATH
  auto audioMD5 = PAGTestEnvironment::CompareJson["PAGAudioTest"]["Audio_ID83186171"];
  if (audioMD5 != nullptr) {
    EXPECT_EQ(audioMD5.get<std::string>(), md5);
  }
#endif
  delete[] output;
}
}  // namespace pag
#endif
