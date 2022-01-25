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

#include "base/utils/Verify.h"
#include "codec/utils/WebpDecoder.h"
#include "pag/file.h"

namespace pag {
BitmapComposition::~BitmapComposition() {
    for (auto sequence : sequences) {
        delete sequence;
    }
}

CompositionType BitmapComposition::type() const {
    return CompositionType::Bitmap;
}

static bool IsEmptyBitmapFrame(BitmapFrame* frame) {
    // There was a bug in PAGExporter that causes an empty frame being exported as 1x1 frame, so we
    // need to identify this kind of empty frame here.
    for (auto bitmap : frame->bitmaps) {
        if (bitmap->x != 0 || bitmap->y != 0) {
            return false;
        }
        if (bitmap->fileBytes->length() > 150) {
            return false;
        }

        int width = 0;
        int height = 0;
        if (!WebPGetInfo(bitmap->fileBytes->data(), bitmap->fileBytes->length(), &width, &height)) {
            LOGE("Get webP size fail.");
        }
        if (width > 1 || height > 1) {
            return false;
        }
    }
    return true;
}

static TimeRange MakeTimeRange(Frame start, Frame end, float timeScale) {
    auto startFrame = static_cast<Frame>(roundf(start * timeScale));
    auto endFrame = static_cast<Frame>(roundf(end * timeScale));
    return {startFrame, endFrame};
}

void BitmapComposition::updateStaticTimeRanges() {
    staticTimeRanges = {};
    if (duration <= 1) {
        return;
    }
    if (!sequences.empty()) {
        Frame start = 0;
        Frame end = 0;
        size_t index = 0;
        auto sequence = sequences[0];
        for (size_t i = 1; i < sequences.size(); i++) {
            auto item = sequences[i];
            if (item->frameRate > sequence->frameRate) {
                sequence = item;
            }
        }
        float timeScale = frameRate / sequence->frameRate;
        for (auto frame : sequence->frames) {
            if (IsEmptyBitmapFrame(frame)) {
                end = index;
            } else {
                if (end > start) {
                    auto range = MakeTimeRange(start, end, timeScale);
                    staticTimeRanges.push_back(range);
                }
                start = end = index;
            }
            index++;
        }
        if (end > start) {
            auto range = MakeTimeRange(start, end, timeScale);
            staticTimeRanges.push_back(range);
        }
    } else {
        TimeRange range = {0, duration - 1};
        staticTimeRanges.push_back(range);
    }
}

bool BitmapComposition::hasImageContent() const {
    return true;
}

bool BitmapComposition::verify() const {
    if (!Composition::verify()) {
        VerifyFailed();
        return false;
    }
    if (sequences.empty()) {
        VerifyFailed();
        return false;
    }
    auto sequenceValid = [](BitmapSequence* sequence) {
        return sequence != nullptr && sequence->verify();
    };
    if (!std::all_of(sequences.begin(), sequences.end(), sequenceValid)) {
        VerifyFailed();
        return false;
    }
    return true;
}
}  // namespace pag
