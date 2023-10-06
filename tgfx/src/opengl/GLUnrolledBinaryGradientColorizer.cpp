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
#include "gpu/gradients/UnrolledBinaryGradientColorizer.h"

namespace tgfx {
UniformHandle AddUniform(UniformHandler* uniformHandler, const std::string& name, int intervalCount,
                         int limit, std::string* out) {
  if (intervalCount > limit) {
    return uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, name, out);
  }
  return {};
}

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

void GLUnrolledBinaryGradientColorizer::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  const auto* fp = static_cast<const UnrolledBinaryGradientColorizer*>(args.fragmentProcessor);
  UnrolledBinaryUniformName name;
  scale0_1Uniform = AddUniform(uniformHandler, "scale0_1", fp->intervalCount, 0, &name.scale0_1);
  scale2_3Uniform = AddUniform(uniformHandler, "scale2_3", fp->intervalCount, 1, &name.scale2_3);
  scale4_5Uniform = AddUniform(uniformHandler, "scale4_5", fp->intervalCount, 2, &name.scale4_5);
  scale6_7Uniform = AddUniform(uniformHandler, "scale6_7", fp->intervalCount, 3, &name.scale6_7);
  scale8_9Uniform = AddUniform(uniformHandler, "scale8_9", fp->intervalCount, 4, &name.scale8_9);
  scale10_11Uniform =
      AddUniform(uniformHandler, "scale10_11", fp->intervalCount, 5, &name.scale10_11);
  scale12_13Uniform =
      AddUniform(uniformHandler, "scale12_13", fp->intervalCount, 6, &name.scale12_13);
  scale14_15Uniform =
      AddUniform(uniformHandler, "scale14_15", fp->intervalCount, 7, &name.scale14_15);
  bias0_1Uniform = AddUniform(uniformHandler, "bias0_1", fp->intervalCount, 0, &name.bias0_1);
  bias2_3Uniform = AddUniform(uniformHandler, "bias2_3", fp->intervalCount, 1, &name.bias2_3);
  bias4_5Uniform = AddUniform(uniformHandler, "bias4_5", fp->intervalCount, 2, &name.bias4_5);
  bias6_7Uniform = AddUniform(uniformHandler, "bias6_7", fp->intervalCount, 3, &name.bias6_7);
  bias8_9Uniform = AddUniform(uniformHandler, "bias8_9", fp->intervalCount, 4, &name.bias8_9);
  bias10_11Uniform = AddUniform(uniformHandler, "bias10_11", fp->intervalCount, 5, &name.bias10_11);
  bias12_13Uniform = AddUniform(uniformHandler, "bias12_13", fp->intervalCount, 6, &name.bias12_13);
  bias14_15Uniform = AddUniform(uniformHandler, "bias14_15", fp->intervalCount, 7, &name.bias14_15);
  thresholds1_7Uniform = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "thresholds1_7", &name.thresholds1_7);
  thresholds9_13Uniform = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "thresholds9_13", &name.thresholds9_13);

  fragBuilder->codeAppendf("float t = %s.x;", args.inputColor.c_str());
  fragBuilder->codeAppend("vec4 scale, bias;");
  fragBuilder->codeAppendf("// interval count: %d\n", fp->intervalCount);

  if (fp->intervalCount >= 4) {
    fragBuilder->codeAppend("// thresholds1_7.w is mid-point for intervals (0,7) and (8,15)\n");
    fragBuilder->codeAppendf("if (t < %s.w) {", name.thresholds1_7.c_str());
  }
  AppendCode1(fragBuilder, fp->intervalCount, name);
  if (fp->intervalCount > 4) {
    fragBuilder->codeAppend("} else {");
  }
  AppendCode2(fragBuilder, fp->intervalCount, name);
  if (fp->intervalCount >= 4) {
    fragBuilder->codeAppend("}");
  }

  fragBuilder->codeAppendf("%s = vec4(t * scale + bias);", args.outputColor.c_str());
}

void SetUniformData(UniformBuffer* uniformBuffer, const std::string& name,
                    const UniformHandle& handle, const Color& current) {
  if (handle.isValid()) {
    uniformBuffer->setData(name, current.array());
  }
}

void GLUnrolledBinaryGradientColorizer::onSetData(UniformBuffer* uniformBuffer,
                                                  const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const UnrolledBinaryGradientColorizer&>(fragmentProcessor);
  uniformBuffer->setData("scale0_1", fp.scale0_1.array());
  SetUniformData(uniformBuffer, "scale2_3", scale2_3Uniform, fp.scale2_3);
  SetUniformData(uniformBuffer, "scale4_5", scale4_5Uniform, fp.scale4_5);
  SetUniformData(uniformBuffer, "scale6_7", scale6_7Uniform, fp.scale6_7);
  SetUniformData(uniformBuffer, "scale8_9", scale8_9Uniform, fp.scale8_9);
  SetUniformData(uniformBuffer, "scale10_11", scale10_11Uniform, fp.scale10_11);
  SetUniformData(uniformBuffer, "scale12_13", scale12_13Uniform, fp.scale12_13);
  SetUniformData(uniformBuffer, "scale14_15", scale14_15Uniform, fp.scale14_15);
  uniformBuffer->setData("bias0_1", fp.bias0_1.array());
  SetUniformData(uniformBuffer, "bias2_3", bias2_3Uniform, fp.bias2_3);
  SetUniformData(uniformBuffer, "bias4_5", bias4_5Uniform, fp.bias4_5);
  SetUniformData(uniformBuffer, "bias6_7", bias6_7Uniform, fp.bias6_7);
  SetUniformData(uniformBuffer, "bias8_9", bias8_9Uniform, fp.bias8_9);
  SetUniformData(uniformBuffer, "bias10_11", bias10_11Uniform, fp.bias10_11);
  SetUniformData(uniformBuffer, "bias12_13", bias12_13Uniform, fp.bias12_13);
  SetUniformData(uniformBuffer, "bias14_15", bias14_15Uniform, fp.bias14_15);
  uniformBuffer->setData("thresholds1_7", reinterpret_cast<const float*>(&(fp.thresholds1_7)));
  uniformBuffer->setData("thresholds9_13", reinterpret_cast<const float*>(&(fp.thresholds9_13)));
}
}  // namespace tgfx
