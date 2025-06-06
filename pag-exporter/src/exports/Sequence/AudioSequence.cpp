/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "AudioSequence.h"
// #include "src/VideoEncoder/AudioMuxer.h"
#include "src/utils/AEUtils.h"
#include "ffaudio.h"

#define CHECK_RET(statements)                                             \
  do {                                                                    \
    if ((statements) != A_Err_NONE) {                                     \
      context->pushWarning(pagexporter::AlertInfoType::ExportAudioError); \
      return;                                                             \
    }                                                                     \
  } while (0)

// 消除静音数据
static int RemoveSilence(int16_t* data, int channel, int numSamples, int& startSamples) {
  int totalLen = numSamples * channel;
  int startIndex = 0;
  int endIndex = 0;

  for (startIndex = 0; startIndex < totalLen; startIndex++) {
    if (data[startIndex] != 0) {
      break;
    }
  }

  startSamples = startIndex / channel;
  if (startSamples == numSamples) {
    return 0;
  }

  for (endIndex = totalLen - 1; endIndex > startIndex; endIndex--) {
    if (data[endIndex] != 0) {
      break;
    }
  }

  auto endSamples = endIndex / channel;

  return endSamples - startSamples + 1;
}

std::unique_ptr<pag::ByteData> getByteData(const std::string& filename) {
  return pag::ByteData::FromPath(filename);
}

void ExportAudioSequence(pagexporter::Context* context, const AEGP_ItemH& itemHandle,
                         pag::Composition* composition) {
  auto& suites = context->suites;

  AEGP_ItemFlags flags;
  suites.ItemSuite6()->AEGP_GetItemFlags(itemHandle, &flags);
  if (!(flags & AEGP_ItemFlag_HAS_ACTIVE_AUDIO)) {
    return;
  }

  AEGP_RenderOptionsH renderOptions = nullptr;
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle,
                                                           &renderOptions));

  // 获取工作区域
  A_Time workAreaStart = {};
  A_Time workAreaDuration = {};
  auto compHandle = AEUtils::GetCompFromItem(itemHandle);
  suites.CompSuite6()->AEGP_GetCompWorkAreaStart(compHandle, &workAreaStart);
  suites.CompSuite6()->AEGP_GetCompWorkAreaDuration(compHandle, &workAreaDuration);

  AEGP_SoundDataFormat soundFormat = {44100, AEGP_SoundEncoding_SIGNED_PCM, 2, 2};
  AEGP_SoundDataH soundData;
  CHECK_RET(suites.RenderSuite4()->AEGP_RenderNewItemSoundData(
      itemHandle, &workAreaStart, &workAreaDuration, &soundFormat, nullptr, nullptr, &soundData));
  if (soundData != nullptr) {
    A_long numSamples;
    suites.SoundDataSuite1()->AEGP_GetNumSamples(soundData, &numSamples);

    if (numSamples > 0) {
      AEGP_SoundDataFormat soundFormat2;
      suites.SoundDataSuite1()->AEGP_GetSoundDataFormat(soundData, &soundFormat2);

      void* samples;
      suites.SoundDataSuite1()->AEGP_LockSoundDataSamples(soundData, &samples);

      int startSamples = 0;
      //numSamples = RemoveSilence(static_cast<int16_t*>(samples), static_cast<int>(soundFormat2.num_channelsL), static_cast<int>(numSamples), startSamples);

      if (numSamples > 0) {
        auto muxer = new AudioMuxer();
        auto ret = muxer->init(context->outputPath, static_cast<int>(soundFormat2.num_channelsL),
                               static_cast<int>(soundFormat2.sample_rateF));
        if (ret) {
          int offset = startSamples * static_cast<int>(soundFormat2.num_channelsL);
          muxer->putData(static_cast<int16_t*>(samples) + offset, numSamples);
          muxer->close();
          composition->audioBytes = getByteData(muxer->getFilename()).release();
          composition->audioStartTime = static_cast<pag::Frame>(
              (startSamples / soundFormat2.sample_rateF) * composition->frameRate);
        } else {
          context->pushWarning(pagexporter::AlertInfoType::AudioEncodeFail);
        }
        delete muxer;
      }

      suites.SoundDataSuite1()->AEGP_UnlockSoundDataSamples(soundData);
    }

    suites.SoundDataSuite1()->AEGP_DisposeSoundData(soundData);
  }

  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions));
}
