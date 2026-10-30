/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "base/PAGTest.h"
#include "rendering/filters/utils/Std140Uniforms.h"

namespace pag {
namespace {
struct alignas(16) ScalarUniforms {
  float value = 0.0f;
};

struct UnalignedUniforms {
  float values[4] = {};
};

struct alignas(16) NonTrivialUniforms {
  float values[4] = {};
  ~NonTrivialUniforms() {
  }
};

struct alignas(16) VectorUniforms {
  float center[2] = {};
  float amount = 0.0f;
};
}  // namespace

PAG_TEST(Std140UniformTest, ChecksBlockRepresentation) {
  EXPECT_TRUE((HasStd140UniformLayout<ScalarUniforms, 16>()));
  EXPECT_FALSE((HasStd140UniformLayout<ScalarUniforms, 32>()));
  EXPECT_FALSE((HasStd140UniformLayout<UnalignedUniforms, 16>()));
  EXPECT_FALSE((HasStd140UniformLayout<NonTrivialUniforms, 16>()));
  EXPECT_FALSE((HasStd140UniformLayout<VectorUniforms, 12>()));
}

PAG_TEST(Std140UniformTest, PreservesVectorMemberOffsets) {
  static_assert(HasStd140UniformLayout<VectorUniforms, 16>());
  EXPECT_EQ(offsetof(VectorUniforms, center), 0u);
  EXPECT_EQ(offsetof(VectorUniforms, amount), 8u);
  EXPECT_EQ(sizeof(VectorUniforms), 16u);
}
}  // namespace pag
