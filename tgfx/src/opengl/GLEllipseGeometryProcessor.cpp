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

namespace tgfx {
std::unique_ptr<EllipseGeometryProcessor> EllipseGeometryProcessor::Make(
    int width, int height, bool stroke, bool useScale, const Matrix& localMatrix) {
  return std::unique_ptr<EllipseGeometryProcessor>(
      new GLEllipseGeometryProcessor(width, height, stroke, useScale, localMatrix));
}

GLEllipseGeometryProcessor::GLEllipseGeometryProcessor(int width, int height, bool stroke,
                                                       bool useScale, const Matrix& localMatrix)
    : EllipseGeometryProcessor(width, height, stroke, useScale, localMatrix) {
}

void GLEllipseGeometryProcessor::emitCode(EmitArgs& args) const {
  auto* vertBuilder = args.vertBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  // emit attributes
  varyingHandler->emitAttributes(*this);

  auto offsetType = useScale ? ShaderVar::Type::Float3 : ShaderVar::Type::Float2;
  auto ellipseOffsets = varyingHandler->addVarying("EllipseOffsets", offsetType);
  vertBuilder->codeAppendf("%s = %s;", ellipseOffsets.vsOut().c_str(),
                           inEllipseOffset.name().c_str());

  auto ellipseRadii = varyingHandler->addVarying("EllipseRadii", ShaderVar::Type::Float4);
  vertBuilder->codeAppendf("%s = %s;", ellipseRadii.vsOut().c_str(), inEllipseRadii.name().c_str());

  auto* fragBuilder = args.fragBuilder;
  // setup pass through color
  auto color = varyingHandler->addVarying("Color", ShaderVar::Type::Float4);
  vertBuilder->codeAppendf("%s = %s;", color.vsOut().c_str(), inColor.name().c_str());
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), color.fsIn().c_str());

  // Setup position
  args.vertBuilder->emitNormalizedPosition(inPosition.name());
  // emit transforms
  emitTransforms(vertBuilder, varyingHandler, uniformHandler, inPosition.asShaderVar(),
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
  if (stroke) {
    fragBuilder->codeAppendf("offset *= %s.xy;", ellipseRadii.fsIn().c_str());
  }
  fragBuilder->codeAppend("float test = dot(offset, offset) - 1.0;");
  if (useScale) {
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
  if (useScale) {
    fragBuilder->codeAppendf("float invlen = %s.z*inversesqrt(grad_dot);",
                             ellipseOffsets.fsIn().c_str());
  } else {
    fragBuilder->codeAppend("float invlen = inversesqrt(grad_dot);");
  }
  fragBuilder->codeAppend("float edgeAlpha = clamp(0.5-test*invlen, 0.0, 1.0);");

  // for inner curve
  if (stroke) {
    fragBuilder->codeAppendf("offset = %s.xy*%s.zw;", ellipseOffsets.fsIn().c_str(),
                             ellipseRadii.fsIn().c_str());
    fragBuilder->codeAppend("test = dot(offset, offset) - 1.0;");
    if (useScale) {
      fragBuilder->codeAppendf("grad = 2.0*offset*(%s.z*%s.zw);", ellipseOffsets.fsIn().c_str(),
                               ellipseRadii.fsIn().c_str());
    } else {
      fragBuilder->codeAppendf("grad = 2.0*offset*%s.zw;", ellipseRadii.fsIn().c_str());
    }
    fragBuilder->codeAppend("grad_dot = dot(grad, grad);");
    if (!args.caps->floatIs32Bits) {
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 6.1036e-5);");
    }
    if (useScale) {
      fragBuilder->codeAppendf("invlen = %s.z*inversesqrt(grad_dot);",
                               ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("invlen = inversesqrt(grad_dot);");
    }
    fragBuilder->codeAppend("edgeAlpha *= saturate(0.5+test*invlen);");
  }

  fragBuilder->codeAppendf("%s = vec4(edgeAlpha);", args.outputCoverage.c_str());
}

void GLEllipseGeometryProcessor::setData(UniformBuffer* uniformBuffer,
                                         FPCoordTransformIter* transformIter) const {
  setTransformDataHelper(localMatrix, uniformBuffer, transformIter);
}
}  // namespace tgfx
