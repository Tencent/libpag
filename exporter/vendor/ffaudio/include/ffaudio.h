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
#ifndef FFAUDIO_H
#define FFAUDIO_H
#include <string>
#include <memory>

#ifdef _WIN32
#define FFAUDIO_EXPORT __declspec(dllexport)
#else
#define FFAUDIO_EXPORT __attribute__((visibility("default")))
#endif

class AudioMuxerImpl;

class FFAUDIO_EXPORT AudioMuxer {
public:
    AudioMuxer();
    ~AudioMuxer();

    bool init(std::string& outputPath, int channel, int sampleRate);

    void close();

    int putData(int16_t* data, int numSamples);

    std::string getFilename();

private:
    std::unique_ptr<AudioMuxerImpl> impl;
};
#endif  //FFAUDIO_H
