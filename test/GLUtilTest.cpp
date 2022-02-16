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

#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/opengl/GLCaps.h"
#include "gpu/opengl/GLUtil.h"

namespace pag {
using namespace tgfx;

PAG_TEST_SUIT(GLUtilTest)

int i = 0;
std::vector<std::pair<std::string, GLVendor>> vendors = {
    {"ATI Technologies Inc.", GLVendor::ATI},
    {"ARM", GLVendor::ARM},
    {"NVIDIA Corporation", GLVendor::NVIDIA},
    {"Qualcomm", GLVendor::Qualcomm},
    {"Intel", GLVendor::Intel},
    {"Imagination Technologies", GLVendor::Imagination},
};
const unsigned char* glGetStringMock(unsigned name) {
  if (name == GL::VENDOR) {
    return reinterpret_cast<const unsigned char*>(vendors[i].first.c_str());
  } else if (name == GL::VERSION) {
    if (i != 0) {
      return reinterpret_cast<const unsigned char*>("2.0");
    } else {
      return reinterpret_cast<const unsigned char*>("5.0");
    }
  }
  return nullptr;
}

void getIntegervMock(unsigned pname, int* params) {
  if (pname == GL::MAX_TEXTURE_SIZE) {
    *params = 1024;
  }
}

void glGetInternalformativMock(unsigned target, unsigned, unsigned pname, int, int* params) {
  if (target != GL::RENDERBUFFER) {
    return;
  }
  if (pname == GL::NUM_SAMPLE_COUNTS) {
    *params = 2;
    return;
  }
  if (pname == GL::SAMPLES) {
    params[0] = 4;
    params[1] = 8;
  }
}

void glGetShaderPrecisionFormatMock(unsigned, unsigned, int* range, int* precision) {
  range[0] = 127;
  range[1] = 127;
  *precision = 32;
}

/**
 * 用例描述: 测试OpenGL版本获取是否正确
 */
PAG_TEST_F(GLUtilTest, Version_ID82986995) {
  // 实参为空指针验证
  auto glVersion = GetGLVersion(nullptr);
  EXPECT_EQ(glVersion.majorVersion, -1);
  EXPECT_EQ(glVersion.minorVersion, -1);
  // 实参为空字符串验证
  glVersion = GetGLVersion("");
  EXPECT_EQ(glVersion.majorVersion, -1);
  EXPECT_EQ(glVersion.minorVersion, -1);
  // 实参为多段版本号验证
  glVersion = GetGLVersion("2.1 Mesa 10.1.1");
  EXPECT_EQ(glVersion.majorVersion, 2);
  EXPECT_EQ(glVersion.minorVersion, 1);
  // 实参为单版本字符验证
  glVersion = GetGLVersion("3.1");
  EXPECT_EQ(glVersion.majorVersion, 3);
  EXPECT_EQ(glVersion.minorVersion, 1);
  // 实参为多段版本号加括号的字符串
  glVersion = GetGLVersion("OpenGL ES 2.0 (WebGL 1.0 (OpenGL ES 2.0 Chromium))");
  EXPECT_EQ(glVersion.majorVersion, 1);
  EXPECT_EQ(glVersion.minorVersion, 0);
  // 实参为OpenGL ES-CM
  glVersion = GetGLVersion("OpenGL ES-CM 1.1 Apple A8 GPU - 50.5.1");
  EXPECT_EQ(glVersion.majorVersion, 1);
  EXPECT_EQ(glVersion.minorVersion, 1);
  glVersion = GetGLVersion("OpenGL ES 2.0 Apple A8 GPU - 50.5.1");
  EXPECT_EQ(glVersion.majorVersion, 2);
  EXPECT_EQ(glVersion.minorVersion, 0);
}

/**
 * 用例描述: 测试GLCaps
 */
PAG_TEST_F(GLUtilTest, Caps_ID82991749) {
  {
    GLInfo info(glGetStringMock, nullptr, getIntegervMock, glGetInternalformativMock,
                glGetShaderPrecisionFormatMock);
    GLCaps caps(info);
    EXPECT_EQ(caps.vendor, vendors[i].second);
    EXPECT_EQ(caps.standard, GLStandard::GL);
    EXPECT_TRUE(caps.textureRedSupport);
    EXPECT_TRUE(caps.multisampleDisableSupport);
    EXPECT_EQ(caps.getSampleCount(5, PixelConfig::RGBA_8888), 8);
    EXPECT_EQ(caps.getSampleCount(10, PixelConfig::RGBA_8888), 1);
    EXPECT_EQ(caps.getSampleCount(0, PixelConfig::RGBA_8888), 1);
    EXPECT_EQ(caps.getSampleCount(5, PixelConfig::ALPHA_8), 1);
  }
  {
    ++i;
    int size = static_cast<int>(vendors.size());
    for (; i < size; ++i) {
      GLInfo info(glGetStringMock, nullptr, getIntegervMock, glGetInternalformativMock,
                  glGetShaderPrecisionFormatMock);
      GLCaps caps(info);
      EXPECT_EQ(caps.vendor, vendors[i].second);
    }
  }
}
}  // namespace pag
