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

#include "GLUnrolledBinaryGradientColorizer.h"
#include "utils/MathExtra.h"

namespace tgfx {
struct UnrolledBinaryUniformName {
  std::string scale0_1;
  std::string scale2_3;
  std::string scale4_5;
  std::string scale6_7;
  std::string scale8_9;
  std::string scale10_11;
  std::string scale12_13;
  std::string scale14_15;
  std::string bias0_1;
  std::string bias2_3;
  std::string bias4_5;
  std::string bias6_7;
  std::string bias8_9;
  std::string bias10_11;
  std::string bias12_13;
  std::string bias14_15;
  std::string thresholds1_7;
  std::string thresholds9_13;
};

static constexpr int kMaxIntervals = 8;

std::unique_ptr<UnrolledBinaryGradientColorizer> UnrolledBinaryGradientColorizer::Make(
    const Color* colors, const float* positions, int count) {
  // Depending on how the positions resolve into hard stops or regular stops, the number of
  // intervals specified by the number of colors/positions can change. For instance, a plain
  // 3 color gradient is two intervals, but a 4 color gradient with a hard stop is also
  // two intervals. At the most extreme end, an 8 interval gradient made entirely of hard
  // stops has 16 colors.

  if (count > kMaxColorCount) {
    // Definitely cannot represent this gradient configuration
    return nullptr;
  }

  // The raster implementation also uses scales and biases, but since they must be calculated
  // after the dst color space is applied, it limits our ability to cache their values.
  Color scales[kMaxIntervals];
  Color biases[kMaxIntervals];
  float thresholds[kMaxIntervals];

  int intervalCount = 0;

  for (int i = 0; i < count - 1; i++) {
    if (intervalCount >= kMaxIntervals) {
      // Already reached kMaxIntervals, and haven't run out of color stops so this
      // gradient cannot be represented by this shader.
      return nullptr;
    }

    auto t0 = positions[i];
    auto t1 = positions[i + 1];
    auto dt = t1 - t0;
    // If the interval is empty, skip to the next interval. This will automatically create
    // distinct hard stop intervals as needed. It also protects against malformed gradients
    // that have repeated hard stops at the very beginning that are effectively unreachable.
    if (FloatNearlyZero(dt)) {
      continue;
    }

    for (int j = 0; j < 4; ++j) {
      auto c0 = colors[i][j];
      auto c1 = colors[i + 1][j];
      auto scale = (c1 - c0) / dt;
      auto bias = c0 - t0 * scale;
      scales[intervalCount][j] = scale;
      biases[intervalCount][j] = bias;
    }
    thresholds[intervalCount] = t1;
    intervalCount++;
  }

  // set the unused values to something consistent
  for (int i = intervalCount; i < kMaxIntervals; i++) {
    scales[i] = Color::Transparent();
    biases[i] = Color::Transparent();
    thresholds[i] = 0.0;
  }

  return std::unique_ptr<UnrolledBinaryGradientColorizer>(new GLUnrolledBinaryGradientColorizer(
      intervalCount, scales, biases,
      Rect::MakeLTRB(thresholds[0], thresholds[1], thresholds[2], thresholds[3]),
      Rect::MakeLTRB(thresholds[4], thresholds[5], thresholds[6], 0.0)));
}

GLUnrolledBinaryGradientColorizer::GLUnrolledBinaryGradientColorizer(int intervalCount,
                                                                     Color* scales, Color* biases,
                                                                     Rect thresholds1_7,
                                                                     Rect thresholds9_13)
    : UnrolledBinaryGradientColorizer(intervalCount, scales, biases, thresholds1_7,
                                      thresholds9_13) {
}

std::string AddUniform(UniformHandler* uniformHandler, const std::string& name, int intervalCount,
                       int limit) {
  if (intervalCount > limit) {
    return uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, name);
  }
  return "";
}

void AppendCode1(FragmentShaderBuilder* fragBuilder, int intervalCount,
                 const UnrolledBinaryUniformName& name) {
  if (intervalCount >= 2) {
    fragBuilder->codeAppend("// thresholds1_7.y is mid-point for intervals (0,3) and (4,7)\n");
    fragBuilder->codeAppendf("if (t < %s.y) {", name.thresholds1_7.c_str());
  }
  fragBuilder->codeAppend("// thresholds1_7.x is mid-point for intervals (0,1) and (2,3)\n");
  fragBuilder->codeAppendf("if (t < %s.x) {", name.thresholds1_7.c_str());
  fragBuilder->codeAppendf("scale = %s;", name.scale0_1.c_str());
  fragBuilder->codeAppendf("bias = %s;", name.bias0_1.c_str());
  if (intervalCount > 1) {
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale2_3.empty() ? "vec4(0.0)" : name.scale2_3.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias2_3.empty() ? "vec4(0.0)" : name.bias2_3.c_str());
  }
  fragBuilder->codeAppend("}");
  if (intervalCount > 2) {
    fragBuilder->codeAppend("} else {");
  }
  if (intervalCount >= 3) {
    fragBuilder->codeAppend("// thresholds1_7.z is mid-point for intervals (4,5) and (6,7)\n");
    fragBuilder->codeAppendf("if (t < %s.z) {", name.thresholds1_7.c_str());
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale4_5.empty() ? "vec4(0.0)" : name.scale4_5.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias4_5.empty() ? "vec4(0.0)" : name.bias4_5.c_str());
  }
  if (intervalCount > 3) {
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale6_7.empty() ? "vec4(0.0)" : name.scale6_7.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias6_7.empty() ? "vec4(0.0)" : name.bias6_7.c_str());
  }
  if (intervalCount >= 3) {
    fragBuilder->codeAppend("}");
  }
  if (intervalCount >= 2) {
    fragBuilder->codeAppend("}");
  }
}

void AppendCode2(FragmentShaderBuilder* fragBuilder, int intervalCount,
                 const UnrolledBinaryUniformName& name) {
  if (intervalCount >= 6) {
    fragBuilder->codeAppend("// thresholds9_13.y is mid-point for intervals (8,11) and (12,15)\n");
    fragBuilder->codeAppendf("if (t < %s.y) {", name.thresholds9_13.c_str());
  }
  if (intervalCount >= 5) {
    fragBuilder->codeAppend("// thresholds9_13.x is mid-point for intervals (8,9) and (10,11)\n");
    fragBuilder->codeAppendf("if (t < %s.x) {", name.thresholds9_13.c_str());
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale8_9.empty() ? "vec4(0.0)" : name.scale8_9.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias8_9.empty() ? "vec4(0.0)" : name.bias8_9.c_str());
  }
  if (intervalCount > 5) {
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale10_11.empty() ? "vec4(0.0)" : name.scale10_11.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias10_11.empty() ? "vec4(0.0)" : name.bias10_11.c_str());
  }

  if (intervalCount >= 5) {
    fragBuilder->codeAppend("}");
  }
  if (intervalCount > 6) {
    fragBuilder->codeAppend("} else {");
  }
  if (intervalCount >= 7) {
    fragBuilder->codeAppend("// thresholds9_13.z is mid-point for intervals (12,13) and (14,15)\n");
    fragBuilder->codeAppendf("if (t < %s.z) {", name.thresholds9_13.c_str());
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale12_13.empty() ? "vec4(0.0)" : name.scale12_13.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias12_13.empty() ? "vec4(0.0)" : name.bias12_13.c_str());
  }
  if (intervalCount > 7) {
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("scale = %s;",
                             name.scale14_15.empty() ? "vec4(0.0)" : name.scale14_15.c_str());
    fragBuilder->codeAppendf("bias = %s;",
                             name.bias14_15.empty() ? "vec4(0.0)" : name.bias14_15.c_str());
  }
  if (intervalCount >= 7) {
    fragBuilder->codeAppend("}");
  }
  if (intervalCount >= 6) {
    fragBuilder->codeAppend("}");
  }
}

void GLUnrolledBinaryGradientColorizer::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  UnrolledBinaryUniformName uniformNames = {};
  uniformNames.scale0_1 = AddUniform(uniformHandler, "scale0_1", intervalCount, 0);
  uniformNames.scale2_3 = AddUniform(uniformHandler, "scale2_3", intervalCount, 1);
  uniformNames.scale4_5 = AddUniform(uniformHandler, "scale4_5", intervalCount, 2);
  uniformNames.scale6_7 = AddUniform(uniformHandler, "scale6_7", intervalCount, 3);
  uniformNames.scale8_9 = AddUniform(uniformHandler, "scale8_9", intervalCount, 4);
  uniformNames.scale10_11 = AddUniform(uniformHandler, "scale10_11", intervalCount, 5);
  uniformNames.scale12_13 = AddUniform(uniformHandler, "scale12_13", intervalCount, 6);
  uniformNames.scale14_15 = AddUniform(uniformHandler, "scale14_15", intervalCount, 7);
  uniformNames.bias0_1 = AddUniform(uniformHandler, "bias0_1", intervalCount, 0);
  uniformNames.bias2_3 = AddUniform(uniformHandler, "bias2_3", intervalCount, 1);
  uniformNames.bias4_5 = AddUniform(uniformHandler, "bias4_5", intervalCount, 2);
  uniformNames.bias6_7 = AddUniform(uniformHandler, "bias6_7", intervalCount, 3);
  uniformNames.bias8_9 = AddUniform(uniformHandler, "bias8_9", intervalCount, 4);
  uniformNames.bias10_11 = AddUniform(uniformHandler, "bias10_11", intervalCount, 5);
  uniformNames.bias12_13 = AddUniform(uniformHandler, "bias12_13", intervalCount, 6);
  uniformNames.bias14_15 = AddUniform(uniformHandler, "bias14_15", intervalCount, 7);
  uniformNames.thresholds1_7 = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "thresholds1_7");
  uniformNames.thresholds9_13 = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "thresholds9_13");

  fragBuilder->codeAppendf("float t = %s.x;", args.inputColor.c_str());
  fragBuilder->codeAppend("vec4 scale, bias;");
  fragBuilder->codeAppendf("// interval count: %d\n", intervalCount);

  if (intervalCount >= 4) {
    fragBuilder->codeAppend("// thresholds1_7.w is mid-point for intervals (0,7) and (8,15)\n");
    fragBuilder->codeAppendf("if (t < %s.w) {", uniformNames.thresholds1_7.c_str());
  }
  AppendCode1(fragBuilder, intervalCount, uniformNames);
  if (intervalCount > 4) {
    fragBuilder->codeAppend("} else {");
  }
  AppendCode2(fragBuilder, intervalCount, uniformNames);
  if (intervalCount >= 4) {
    fragBuilder->codeAppend("}");
  }

  fragBuilder->codeAppendf("%s = vec4(t * scale + bias);", args.outputColor.c_str());
}

void SetUniformData(UniformBuffer* uniformBuffer, const std::string& name, int intervalCount,
                    int limit, const Color& value) {
  if (intervalCount > limit) {
    uniformBuffer->setData(name, value.array());
  }
}

void GLUnrolledBinaryGradientColorizer::onSetData(UniformBuffer* uniformBuffer) const {
  uniformBuffer->setData("scale0_1", scale0_1.array());
  SetUniformData(uniformBuffer, "scale2_3", intervalCount, 0, scale2_3);
  SetUniformData(uniformBuffer, "scale4_5", intervalCount, 1, scale4_5);
  SetUniformData(uniformBuffer, "scale6_7", intervalCount, 2, scale6_7);
  SetUniformData(uniformBuffer, "scale8_9", intervalCount, 3, scale8_9);
  SetUniformData(uniformBuffer, "scale10_11", intervalCount, 5, scale10_11);
  SetUniformData(uniformBuffer, "scale12_13", intervalCount, 6, scale12_13);
  SetUniformData(uniformBuffer, "scale14_15", intervalCount, 7, scale14_15);
  uniformBuffer->setData("bias0_1", bias0_1.array());
  SetUniformData(uniformBuffer, "bias2_3", intervalCount, 0, bias2_3);
  SetUniformData(uniformBuffer, "bias4_5", intervalCount, 1, bias4_5);
  SetUniformData(uniformBuffer, "bias6_7", intervalCount, 2, bias6_7);
  SetUniformData(uniformBuffer, "bias8_9", intervalCount, 3, bias8_9);
  SetUniformData(uniformBuffer, "bias10_11", intervalCount, 5, bias10_11);
  SetUniformData(uniformBuffer, "bias12_13", intervalCount, 6, bias12_13);
  SetUniformData(uniformBuffer, "bias14_15", intervalCount, 7, bias14_15);
  uniformBuffer->setData("thresholds1_7", reinterpret_cast<const float*>(&(thresholds1_7)));
  uniformBuffer->setData("thresholds9_13", reinterpret_cast<const float*>(&(thresholds9_13)));
}
}  // namespace tgfx
