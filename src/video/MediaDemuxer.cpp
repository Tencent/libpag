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

#include "MediaDemuxer.h"
#include <functional>

namespace pag {
/**
 * satisfy condition: array[?] <= target and the last one
 */
int BinarySearch(int start, int end, const std::function<bool(int)>& condition) {
    while (start <= end) {
        int mid = (start + end) / 2;
        if (condition(mid)) {
            start = mid + 1;
        } else {
            end = mid - 1;
        }
    }
    return start == 0 ? 0 : start - 1;
}

int PTSDetail::findKeyframeIndex(int64_t atTime) const {
    if (ptsVector.empty()) {
        return 0;
    }
    int start = 0;
    auto end = static_cast<int>(keyframeIndexVector.size()) - 1;
    return BinarySearch(start, end, [this, atTime](int mid) {
        return ptsVector[keyframeIndexVector[mid]] <= atTime;
    });
}

int64_t PTSDetail::getKeyframeTime(int withKeyframeIndex) const {
    if (withKeyframeIndex < 0) {
        return INT64_MIN;
    }
    if (withKeyframeIndex >= static_cast<int>(keyframeIndexVector.size())) {
        return INT64_MAX;
    }
    return ptsVector[keyframeIndexVector[withKeyframeIndex]];
}

int64_t PTSDetail::getSampleTimeAt(int64_t targetTime) const {
    if (targetTime < ptsVector.front()) {
        if (targetTime >= 0 && !ptsVector.empty()) {
            return ptsVector[0];
        }
        return INT64_MIN;
    }
    if (targetTime >= duration) {
        return INT64_MAX;
    }
    auto frameIndex = findFrameIndex(targetTime);
    return ptsVector[frameIndex];
}

int64_t PTSDetail::getNextSampleTimeAt(int64_t targetTime) {
    if (targetTime < ptsVector.front()) {
        if (targetTime >= 0 && !ptsVector.empty()) {
            return ptsVector[0];
        }
        return INT64_MAX;
    }
    if (targetTime >= duration) {
        return INT64_MAX;
    }
    size_t frameIndex = findFrameIndex(targetTime) + 1;
    if (frameIndex >= ptsVector.size()) {
        return INT64_MAX;
    }
    return ptsVector[frameIndex];
}

int PTSDetail::findFrameIndex(int64_t targetTime) const {
    auto start = findKeyframeIndex(targetTime);
    auto end = start + 1;
    start = keyframeIndexVector[start];
    if (end >= static_cast<int>(keyframeIndexVector.size())) {
        end = static_cast<int>(ptsVector.size()) - 1;
    } else {
        end = keyframeIndexVector[end];
    }
    return BinarySearch(start, end,
    [this, targetTime](int mid) {
        return ptsVector[mid] <= targetTime;
    });
}

int64_t MediaDemuxer::getSampleTimeAt(int64_t targetTime) {
    return ptsDetail()->getSampleTimeAt(targetTime);
}

int64_t MediaDemuxer::getNextSampleTimeAt(int64_t targetTime) {
    return ptsDetail()->getNextSampleTimeAt(targetTime);
}

bool MediaDemuxer::trySeek(int64_t targetTime, int64_t currentTime) {
    if (currentTime <= targetTime && targetTime < maxPendingTime) {
        return false;
    }
    auto targetKeyframeIndex = ptsDetail()->findKeyframeIndex(targetTime);
    if (currentTime <= targetTime && currentKeyframeIndex == targetKeyframeIndex) {
        return false;
    }
    seekTo(ptsDetail()->getKeyframeTime(targetKeyframeIndex));
    return true;
}

void MediaDemuxer::afterAdvance(bool isKeyframe) {
    auto sampleTime = getSampleTime();
    if (maxPendingTime < 0 || maxPendingTime < sampleTime) {
        maxPendingTime = sampleTime;
    }
    if (currentKeyframeIndex < 0) {
        currentKeyframeIndex = ptsDetail()->findKeyframeIndex(sampleTime);
    } else {
        if (isKeyframe) {
            ++currentKeyframeIndex;
        }
    }
}

void MediaDemuxer::resetParams() {
    maxPendingTime = INT64_MIN;
    currentKeyframeIndex = -1;
}

std::shared_ptr<PTSDetail> MediaDemuxer::ptsDetail() {
    if (ptsDetail_ == nullptr) {
        ptsDetail_ = createPTSDetail();
    }
    return ptsDetail_;
}
}  // namespace pag
