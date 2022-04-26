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

#include "GLEllipseGeometryProcessor.h"
#include "gpu/EllipseGeometryProcessor.h"
#include "tgfx/gpu/Caps.h"

namespace tgfx {
void GLEllipseGeometryProcessor::emitCode(EmitArgs& args) {
  const auto& egp = *static_cast<const EllipseGeometryProcessor*>(args.gp);
  auto* vertBuilder = args.vertBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  // emit attributes
  varyingHandler->emitAttributes(egp);
  std::string matrixUniformName;
  viewMatrixUniform = uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float3x3,
                                                 "Matrix", &matrixUniformName);
  vertBuilder->codeAppendf("vec3 position = %s * vec3(%s.xy, 1);", matrixUniformName.c_str(),
                           egp.inPosition.name().c_str());
  std::string screenSizeUniformName;
  screenSizeUniform = uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float2,
                                                 "ScreenSize", &screenSizeUniformName);
  vertBuilder->codeAppendf("vec2 clipSpace = (position.xy / %s) * 2.0 - 1.0;",
                           screenSizeUniformName.c_str());
  auto position = ShaderVar("clipSpace", ShaderVar::Type::Float2, ShaderVar::TypeModifier::None);

  auto offsetType = egp.useScale ? ShaderVar::Type::Float3 : ShaderVar::Type::Float2;
  auto ellipseOffsets = varyingHandler->addVarying("EllipseOffsets", offsetType);
  vertBuilder->codeAppendf("%s = %s;", ellipseOffsets.vsOut().c_str(),
                           egp.inEllipseOffset.name().c_str());

  auto ellipseRadii = varyingHandler->addVarying("EllipseRadii", ShaderVar::Type::Float4);
  vertBuilder->codeAppendf("%s = %s;", ellipseRadii.vsOut().c_str(),
                           egp.inEllipseRadii.name().c_str());

  auto* fragBuilder = args.fragBuilder;
  // setup pass through color
  fragBuilder->codeAppendf("vec4 %s;", args.outputColor.c_str());
  fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputColor.c_str());

  // Setup position
  args.vertBuilder->emitNormalizedPosition(position.name());
  // emit transforms
  emitTransforms(vertBuilder, varyingHandler, uniformHandler, egp.inPosition.asShaderVar(),
                 args.fpCoordTransformHandler);
  // For stroked ellipses, we use the full ellipse equation (x^2/a^2 + y^2/b^2 = 1)
  // to compute both the edges because we need two separate test equations for
  // the single offset.
  // For filled ellipses we can use a unit circle equation (x^2 + y^2 = 1), and warp
  // the distance by the gradient, non-uniformly scaled by the inverse of the
  // ellipse size.

  // On medium precision devices, we scale the denominator of the distance equation
  // before taking the inverse square root to minimize the chance that we're dividing
  // by zero, then we scale the result back.

  // for outer curve
  fragBuilder->codeAppendf("vec2 offset = %s.xy;", ellipseOffsets.fsIn().c_str());
  if (egp.stroke) {
    fragBuilder->codeAppendf("offset *= %s.xy;", ellipseRadii.fsIn().c_str());
  }
  fragBuilder->codeAppend("float test = dot(offset, offset) - 1.0;");
  if (egp.useScale) {
    fragBuilder->codeAppendf("vec2 grad = 2.0*offset*(%s.z*%s.xy);", ellipseOffsets.fsIn().c_str(),
                             ellipseRadii.fsIn().c_str());
  } else {
    fragBuilder->codeAppendf("vec2 grad = 2.0*offset*%s.xy;", ellipseRadii.fsIn().c_str());
  }
  fragBuilder->codeAppend("float grad_dot = dot(grad, grad);");

  // avoid calling inversesqrt on zero.
  if (args.caps->floatIs32Bits) {
    fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.1755e-38);");
  } else {
    fragBuilder->codeAppend("grad_dot = max(grad_dot, 6.1036e-5);");
  }
  if (egp.useScale) {
    fragBuilder->codeAppendf("float invlen = %s.z*inversesqrt(grad_dot);",
                             ellipseOffsets.fsIn().c_str());
  } else {
    fragBuilder->codeAppend("float invlen = inversesqrt(grad_dot);");
  }
  fragBuilder->codeAppend("float edgeAlpha = clamp(0.5-test*invlen, 0.0, 1.0);");

  // for inner curve
  if (egp.stroke) {
    fragBuilder->codeAppendf("offset = %s.xy*%s.zw;", ellipseOffsets.fsIn().c_str(),
                             ellipseRadii.fsIn().c_str());
    fragBuilder->codeAppend("test = dot(offset, offset) - 1.0;");
    if (egp.useScale) {
      fragBuilder->codeAppendf("grad = 2.0*offset*(%s.z*%s.zw);", ellipseOffsets.fsIn().c_str(),
                               ellipseRadii.fsIn().c_str());
    } else {
      fragBuilder->codeAppendf("grad = 2.0*offset*%s.zw;", ellipseRadii.fsIn().c_str());
    }
    fragBuilder->codeAppend("grad_dot = dot(grad, grad);");
    if (!args.caps->floatIs32Bits) {
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 6.1036e-5);");
    }
    if (egp.useScale) {
      fragBuilder->codeAppendf("invlen = %s.z*inversesqrt(grad_dot);",
                               ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("invlen = inversesqrt(grad_dot);");
    }
    fragBuilder->codeAppend("edgeAlpha *= saturate(0.5+test*invlen);");
  }

  fragBuilder->codeAppendf("%s = vec4(edgeAlpha);", args.outputCoverage.c_str());
}

void GLEllipseGeometryProcessor::setData(const ProgramDataManager& programDataManager,
                                         const GeometryProcessor& priProc,
                                         FPCoordTransformIter* transformIter) {
  const auto& egp = static_cast<const EllipseGeometryProcessor&>(priProc);
  setTransformDataHelper(egp.localMatrix, programDataManager, transformIter);
  if (viewMatrixPrev != egp.viewMatrix) {
    viewMatrixPrev = egp.viewMatrix;
    programDataManager.setMatrix(viewMatrixUniform, egp.viewMatrix);
  }
  if (widthPrev != egp.width || heightPrev != egp.height) {
    widthPrev = egp.width;
    heightPrev = egp.height;
    programDataManager.set2f(screenSizeUniform, static_cast<float>(egp.width),
                             static_cast<float>(egp.height));
  }
}
}  // namespace tgfx
