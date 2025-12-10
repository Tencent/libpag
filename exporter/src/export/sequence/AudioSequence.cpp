/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "ffaudio.h"
#include "utils/PAGExportSession.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

void CombineAudioMarkers(std::vector<pag::Composition*>& compositions) {
  if (compositions.size() <= 1) {
    return;
  }
  auto mainComposition = compositions[compositions.size() - 1];
  if (mainComposition->audioBytes == nullptr) {
    return;
  }
  for (size_t index = 0; index < compositions.size() - 1; index++) {
    mainComposition->audioMarkers.insert(mainComposition->audioMarkers.end(),
                                         compositions[index]->audioMarkers.begin(),
                                         compositions[index]->audioMarkers.end());
    compositions[index]->audioMarkers.clear();
  }
}

void GetAudioSequence(const AEGP_ItemH& itemHandle, const std::string& outputPath,
                      pag::Composition* composition) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  std::string path = outputPath;

  AEGP_ItemFlags flags = 0;
  Suites->ItemSuite6()->AEGP_GetItemFlags(itemHandle, &flags);
  if (!(flags & AEGP_ItemFlag_HAS_ACTIVE_AUDIO)) {
    return;
  }

  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);

  A_Time workAreaStart = {};
  A_Time workAreaDuration = {};
  AEGP_CompH compositionHandle = GetItemCompH(itemHandle);
  Suites->CompSuite6()->AEGP_GetCompWorkAreaStart(compositionHandle, &workAreaStart);
  Suites->CompSuite6()->AEGP_GetCompWorkAreaDuration(compositionHandle, &workAreaDuration);

  AEGP_SoundDataFormat soundFormat = {44100, AEGP_SoundEncoding_SIGNED_PCM, 2, 2};
  AEGP_SoundDataH soundData = nullptr;
  Suites->RenderSuite4()->AEGP_RenderNewItemSoundData(itemHandle, &workAreaStart, &workAreaDuration,
                                                      &soundFormat, nullptr, nullptr, &soundData);
  if (soundData != nullptr) {
    A_long numSamples = 0;
    Suites->SoundDataSuite1()->AEGP_GetNumSamples(soundData, &numSamples);
    if (numSamples > 0) {
      auto muxer = new AudioMuxer();
      AEGP_SoundDataFormat soundFormat2;
      Suites->SoundDataSuite1()->AEGP_GetSoundDataFormat(soundData, &soundFormat2);
      bool ret = muxer->init(path, static_cast<int>(soundFormat2.num_channelsL),
                             static_cast<int>(soundFormat2.sample_rateF));
      if (ret) {
        void* samples = nullptr;
        Suites->SoundDataSuite1()->AEGP_LockSoundDataSamples(soundData, &samples);
        int startSamples = 0;
        int offset = startSamples * soundFormat2.num_channelsL;
        muxer->putData(static_cast<int16_t*>(samples) + offset, numSamples);
        muxer->close();
        composition->audioBytes = pag::ByteData::FromPath(muxer->getFilename()).get();
        composition->audioStartTime = static_cast<pag::Frame>(
            (startSamples / soundFormat2.sample_rateF) * composition->frameRate);
        Suites->SoundDataSuite1()->AEGP_UnlockSoundDataSamples(soundData);
      } else {
        PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::AudioEncodeFail);
      }
      delete muxer;
    }
    Suites->SoundDataSuite1()->AEGP_DisposeSoundData(soundData);
  }
  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

}  // namespace exporter
