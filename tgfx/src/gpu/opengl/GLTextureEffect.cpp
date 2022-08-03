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

#include "GLTextureEffect.h"
#include "gpu/TextureEffect.h"

namespace tgfx {
bool GLTextureEffect::ShaderModeRequiresUnormCoord(TextureEffect::ShaderMode mode) {
  switch (mode) {
    case TextureEffect::ShaderMode::None:
    case TextureEffect::ShaderMode::Clamp:
    case TextureEffect::ShaderMode::MirrorRepeat:
      return false;
    case TextureEffect::ShaderMode::Repeat:
    case TextureEffect::ShaderMode::ClampToBorder:
      return true;
  }
}

bool GLTextureEffect::ShaderModeUsesSubset(TextureEffect::ShaderMode m) {
  switch (m) {
    case TextureEffect::ShaderMode::None:
    case TextureEffect::ShaderMode::Clamp:
      return false;
    case TextureEffect::ShaderMode::Repeat:
    case TextureEffect::ShaderMode::MirrorRepeat:
    case TextureEffect::ShaderMode::ClampToBorder:
      return true;
  }
}

bool GLTextureEffect::ShaderModeUsesClamp(TextureEffect::ShaderMode m) {
  switch (m) {
    case TextureEffect::ShaderMode::None:
      return false;
    case TextureEffect::ShaderMode::Clamp:
    case TextureEffect::ShaderMode::Repeat:
    case TextureEffect::ShaderMode::MirrorRepeat:
    case TextureEffect::ShaderMode::ClampToBorder:
      return true;
  }
}

void GLTextureEffect::readColor(EmitArgs& args, const std::string& coord, const char* out) {
  std::string normCoord;
  if (!dimensionsName.empty()) {
    normCoord = "(" + coord + ") * " + dimensionsName + "";
  } else {
    normCoord = coord;
  }
  auto* fragBuilder = args.fragBuilder;
  fragBuilder->codeAppendf("vec4 %s = ", out);
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], normCoord);
  fragBuilder->codeAppend(";");
}

void GLTextureEffect::subsetCoord(GLFragmentProcessor::EmitArgs& args,
                                  TextureEffect::ShaderMode mode, const char* coordSwizzle,
                                  const char* subsetStartSwizzle, const char* subsetStopSwizzle) {
  auto* fragBuilder = args.fragBuilder;
  switch (mode) {
    case TextureEffect::ShaderMode::None:
    case TextureEffect::ShaderMode::ClampToBorder:
    case TextureEffect::ShaderMode::Clamp:
      fragBuilder->codeAppendf("subsetCoord.%s = inCoord.%s;", coordSwizzle, coordSwizzle);
      break;
    case TextureEffect::ShaderMode::Repeat:
      fragBuilder->codeAppendf("subsetCoord.%s = mod(inCoord.%s - %s.%s, %s.%s - %s.%s) + %s.%s;",
                               coordSwizzle, coordSwizzle, subsetName.c_str(), subsetStartSwizzle,
                               subsetName.c_str(), subsetStopSwizzle, subsetName.c_str(),
                               subsetStartSwizzle, subsetName.c_str(), subsetStartSwizzle);
      break;
    case TextureEffect::ShaderMode::MirrorRepeat:
      fragBuilder->codeAppend("{");
      fragBuilder->codeAppendf("float w = %s.%s - %s.%s;", subsetName.c_str(), subsetStopSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppendf("float w2 = 2.0 * w;");
      fragBuilder->codeAppendf("float m = mod(inCoord.%s - %s.%s, w2);", coordSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppendf("subsetCoord.%s = mix(m, w2 - m, step(w, m)) + %s.%s;", coordSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppend("}");
      break;
  }
}

void GLTextureEffect::clampCoord(GLFragmentProcessor::EmitArgs& args, bool clamp,
                                 const char* coordSwizzle, const char* clampStartSwizzle,
                                 const char* clampStopSwizzle) {
  auto* fragBuilder = args.fragBuilder;
  if (clamp) {
    fragBuilder->codeAppendf("clampedCoord%s = clamp(subsetCoord%s, %s%s, %s%s);", coordSwizzle,
                             coordSwizzle, clampName.c_str(), clampStartSwizzle, clampName.c_str(),
                             clampStopSwizzle);
  } else {
    fragBuilder->codeAppendf("clampedCoord%s = subsetCoord%s;", coordSwizzle, coordSwizzle);
  }
}

void GLTextureEffect::clampCoord(GLFragmentProcessor::EmitArgs& args, const bool useClamp[2]) {
  if (useClamp[0] == useClamp[1]) {
    clampCoord(args, useClamp[0], "", ".xy", ".zw");
  } else {
    clampCoord(args, useClamp[0], ".x", ".x", ".z");
    clampCoord(args, useClamp[1], ".y", ".y", ".w");
  }
}

void GLTextureEffect::initUniform(GLFragmentProcessor::EmitArgs& args, const bool useSubset[2],
                                  const bool useClamp[2]) {
  auto* uniformHandler = args.uniformHandler;
  if (useSubset[0] || useSubset[1]) {
    subsetUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                               "Subset", &subsetName);
  }
  if (useClamp[0] || useClamp[1]) {
    clampUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                              "Clamp", &clampName);
  }
  const auto* textureFP = static_cast<const TextureEffect*>(args.fragmentProcessor);
  bool unormCoordsRequiredForShaderMode = ShaderModeRequiresUnormCoord(textureFP->shaderModeX) ||
                                          ShaderModeRequiresUnormCoord(textureFP->shaderModeY);
  bool sampleCoordsMustBeNormalized =
      textureFP->texture->getSampler()->type() != TextureType::Rectangle;
  if (unormCoordsRequiredForShaderMode && sampleCoordsMustBeNormalized) {
    dimensionsUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                   "Dimension", &dimensionsName);
  }
}

void GLTextureEffect::emitCode(EmitArgs& args) {
  const auto* textureFP = static_cast<const TextureEffect*>(args.fragmentProcessor);
  auto* fragBuilder = args.fragBuilder;

  auto vertexColor = (*args.transformedCoords)[0].name();
  if (args.coordFunc) {
    vertexColor = args.coordFunc(vertexColor);
  }
  if (textureFP->shaderModeX == TextureEffect::ShaderMode::None &&
      textureFP->shaderModeY == TextureEffect::ShaderMode::None) {
    fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], vertexColor);
    fragBuilder->codeAppendf(" * %s;", args.inputColor.c_str());
  } else {
    fragBuilder->codeAppendf("vec2 inCoord = %s;", vertexColor.c_str());
    bool useSubset[2] = {ShaderModeUsesSubset(textureFP->shaderModeX),
                         ShaderModeUsesSubset(textureFP->shaderModeY)};
    bool useClamp[2] = {ShaderModeUsesClamp(textureFP->shaderModeX),
                        ShaderModeUsesClamp(textureFP->shaderModeY)};
    initUniform(args, useSubset, useClamp);

    fragBuilder->codeAppend("vec2 subsetCoord;");
    subsetCoord(args, textureFP->shaderModeX, "x", "x", "z");
    subsetCoord(args, textureFP->shaderModeY, "y", "y", "w");

    fragBuilder->codeAppend("vec2 clampedCoord;");
    clampCoord(args, useClamp);

    readColor(args, "clampedCoord", "textureColor");

    static const char* repeatReadX = "repeatReadX";
    static const char* repeatReadY = "repeatReadY";
    bool repeatX = textureFP->shaderModeX == TextureEffect::ShaderMode::Repeat;
    bool repeatY = textureFP->shaderModeY == TextureEffect::ShaderMode::Repeat;
    bool clampToBorderX = textureFP->shaderModeX == TextureEffect::ShaderMode::ClampToBorder;
    bool clampToBorderY = textureFP->shaderModeY == TextureEffect::ShaderMode::ClampToBorder;
    if (repeatX || clampToBorderX) {
      fragBuilder->codeAppend("float errX = subsetCoord.x - clampedCoord.x;");
      if (repeatX) {
        fragBuilder->codeAppendf("float repeatCoordX = errX > 0.0 ? %s.x : %s.z;",
                                 clampName.c_str(), clampName.c_str());
        readColor(args, "vec2(repeatCoordX, clampedCoord.y)", repeatReadX);
      }
    }
    if (repeatY || clampToBorderY) {
      fragBuilder->codeAppend("float errY = subsetCoord.y - clampedCoord.y;");
      if (repeatY) {
        fragBuilder->codeAppendf("float repeatCoordY = errY > 0.0 ? %s.y : %s.w;",
                                 clampName.c_str(), clampName.c_str());
        readColor(args, "vec2(clampedCoord.x, repeatCoordY)", repeatReadY);
      }
    }

    const char* ifStr = "if";
    if (repeatX && repeatY) {
      static const char* repeatReadXY = "repeatReadXY";
      readColor(args, "vec2(repeatCoordX, repeatCoordY)", repeatReadXY);
      fragBuilder->codeAppend("if (errX != 0.0 && errY != 0.0) {");
      fragBuilder->codeAppend("errX = abs(errX);");
      fragBuilder->codeAppendf(
          "textureColor = mix(mix(textureColor, %s, errX), mix(%s, %s, errX), abs(errY));",
          repeatReadXY, repeatReadX, repeatReadY);
      fragBuilder->codeAppend("}");
      ifStr = "else if";
    }
    if (repeatX) {
      fragBuilder->codeAppendf("%s (errX != 0.0) {", ifStr);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errX);", repeatReadX);
      fragBuilder->codeAppend("}");
    }
    if (repeatY) {
      fragBuilder->codeAppendf("%s (errY != 0.0) {", ifStr);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errY);", repeatReadY);
      fragBuilder->codeAppend("}");
    }

    if (clampToBorderX) {
      fragBuilder->codeAppend("float snappedX = floor(inCoord.x + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedX < %s.x || snappedX > %s.z) {", subsetName.c_str(),
                               subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    if (clampToBorderY) {
      fragBuilder->codeAppend("float snappedY = floor(inCoord.y + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedY < %s.y || snappedY > %s.w) {", subsetName.c_str(),
                               subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    fragBuilder->codeAppendf("%s = textureColor * %s;", args.outputColor.c_str(),
                             args.inputColor.c_str());
  }
}

void GLTextureEffect::onSetData(const ProgramDataManager& programDataManager,
                                const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const TextureEffect&>(fragmentProcessor);
  if (dimensionsUniform.isValid()) {
    auto dimensions =
        textureFP.texture->getTextureCoord(static_cast<float>(textureFP.texture->width()),
                                           static_cast<float>(textureFP.texture->height()));
    if (dimensions != dimensionsPrev) {
      dimensionsPrev = dimensions;
      programDataManager.set2f(dimensionsUniform, dimensions.x, dimensions.y);
    }
  }
  auto pushRect = [&](Rect subset, std::optional<Rect>& prev, UniformHandle uni) {
    if (subset == prev) {
      return;
    }
    prev = subset;
    auto lt = textureFP.texture->getTextureCoord(static_cast<float>(subset.left),
                                                 static_cast<float>(subset.top));
    auto rb = textureFP.texture->getTextureCoord(static_cast<float>(subset.right),
                                                 static_cast<float>(subset.bottom));
    float rect[4] = {lt.x, lt.y, rb.x, rb.y};
    if (textureFP.texture->origin() == ImageOrigin::BottomLeft) {
      auto h = static_cast<float>(textureFP.texture->height());
      rect[1] = h - rect[1];
      rect[3] = h - rect[3];
      std::swap(rect[1], rect[3]);
    }
    programDataManager.set4fv(uni, 1, rect);
  };
  if (subsetUniform.isValid()) {
    pushRect(textureFP.subset, subsetPrev, subsetUniform);
  }
  if (clampUniform.isValid()) {
    pushRect(textureFP.clamp, clampPrev, clampUniform);
  }
}
}  // namespace tgfx
