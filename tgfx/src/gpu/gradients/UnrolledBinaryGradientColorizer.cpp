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

#include "UnrolledBinaryGradientColorizer.h"

namespace tgfx {
void UnrolledBinaryGradientColorizer::onComputeProcessorKey(BytesKey* bytesKey) const {
  bytesKey->write(static_cast<uint32_t>(intervalCount));
}

bool UnrolledBinaryGradientColorizer::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const UnrolledBinaryGradientColorizer&>(processor);
  return intervalCount == that.intervalCount && scale0_1 == that.scale0_1 &&
         scale2_3 == that.scale2_3 && scale4_5 == that.scale4_5 && scale6_7 == that.scale6_7 &&
         scale8_9 == that.scale8_9 && scale10_11 == that.scale10_11 &&
         scale12_13 == that.scale12_13 && scale14_15 == that.scale14_15 &&
         bias0_1 == that.bias0_1 && bias2_3 == that.bias2_3 && bias4_5 == that.bias4_5 &&
         bias6_7 == that.bias6_7 && bias8_9 == that.bias8_9 && bias10_11 == that.bias10_11 &&
         bias12_13 == that.bias12_13 && bias14_15 == that.bias14_15 &&
         thresholds1_7 == that.thresholds1_7 && thresholds9_13 == that.thresholds9_13;
}
}  // namespace tgfx
