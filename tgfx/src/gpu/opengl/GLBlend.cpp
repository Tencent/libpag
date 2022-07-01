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

#include "GLBlend.h"
#include "tgfx/gpu/opengl/GLDefines.h"

namespace tgfx {
static void HardLight(FragmentShaderBuilder* fsBuilder, const char* final, const char* src,
                      const char* dst) {
  static constexpr char kComponents[] = {'r', 'g', 'b'};
  for (auto& component : kComponents) {
    fsBuilder->codeAppendf("if (2.0 * %s.%c < %s.a) {", src, component, src);
    fsBuilder->codeAppendf("%s.%c = 2.0 * %s.%c * %s.%c;", final, component, src, component, dst,
                           component);
    fsBuilder->codeAppend("} else {");
    fsBuilder->codeAppendf("%s.%c = %s.a * %s.a - 2.0 * (%s.a - %s.%c) * (%s.a - %s.%c);", final,
                           component, src, dst, dst, dst, component, src, src, component);
    fsBuilder->codeAppend("}");
  }
  fsBuilder->codeAppendf("%s.rgb += %s.rgb * (1.0 - %s.a) + %s.rgb * (1.0 - %s.a);", final, src,
                         dst, dst, src);
}

// Does one component of color-dodge
static void ColorDodgeComponent(FragmentShaderBuilder* fsBuilder, const char* final,
                                const char* src, const char* dst, const char component) {
  fsBuilder->codeAppendf("if (0.0 == %s.%c) {", dst, component);
  fsBuilder->codeAppendf("%s.%c = %s.%c * (1.0 - %s.a);", final, component, src, component, dst);
  fsBuilder->codeAppend("} else {");
  fsBuilder->codeAppendf("float d = %s.a - %s.%c;", src, src, component);
  fsBuilder->codeAppend("if (0.0 == d) {");
  fsBuilder->codeAppendf("%s.%c = %s.a * %s.a + %s.%c * (1.0 - %s.a) + %s.%c * (1.0 - %s.a);",
                         final, component, src, dst, src, component, dst, dst, component, src);
  fsBuilder->codeAppend("} else {");
  fsBuilder->codeAppendf("d = min(%s.a, %s.%c * %s.a / d);", dst, dst, component, src);
  fsBuilder->codeAppendf("%s.%c = d * %s.a + %s.%c * (1.0 - %s.a) + %s.%c * (1.0 - %s.a);", final,
                         component, src, src, component, dst, dst, component, src);
  fsBuilder->codeAppend("}");
  fsBuilder->codeAppend("}");
}

// Does one component of color-burn
static void ColorBurnComponent(FragmentShaderBuilder* fsBuilder, const char* final, const char* src,
                               const char* dst, const char component) {
  fsBuilder->codeAppendf("if (%s.a == %s.%c) {", dst, dst, component);
  fsBuilder->codeAppendf("%s.%c = %s.a * %s.a + %s.%c * (1.0 - %s.a) + %s.%c * (1.0 - %s.a);",
                         final, component, src, dst, src, component, dst, dst, component, src);
  fsBuilder->codeAppendf("} else if (0.0 == %s.%c) {", src, component);
  fsBuilder->codeAppendf("%s.%c = %s.%c * (1.0 - %s.a);", final, component, dst, component, src);
  fsBuilder->codeAppend("} else {");
  fsBuilder->codeAppendf("float d = max(0.0, %s.a - (%s.a - %s.%c) * %s.a / %s.%c);", dst, dst, dst,
                         component, src, src, component);
  fsBuilder->codeAppendf("%s.%c = %s.a * d + %s.%c * (1.0 - %s.a) + %s.%c * (1.0 - %s.a);", final,
                         component, src, src, component, dst, dst, component, src);
  fsBuilder->codeAppend("}");
}

// Does one component of soft-light. Caller should have already checked that dst alpha > 0.
static void SoftLightComponentPosDstAlpha(FragmentShaderBuilder* fsBuilder, const char* final,
                                          const char* src, const char* dst, const char component) {
  // if (2S < Sa)
  fsBuilder->codeAppendf("if (2.0 * %s.%c <= %s.a) {", src, component, src);
  // (D^2 (Sa-2 S))/Da+(1-Da) S+D (-Sa+2 S+1)
  fsBuilder->codeAppendf(
      "%s.%c = (%s.%c*%s.%c*(%s.a - 2.0*%s.%c)) / %s.a +"
      "(1.0 - %s.a) * %s.%c + %s.%c*(-%s.a + 2.0*%s.%c + 1.0);",
      final, component, dst, component, dst, component, src, src, component, dst, dst, src,
      component, dst, component, src, src, component);
  // else if (4D < Da)
  fsBuilder->codeAppendf("} else if (4.0 * %s.%c <= %s.a) {", dst, component, dst);
  fsBuilder->codeAppendf("float DSqd = %s.%c * %s.%c;", dst, component, dst, component);
  fsBuilder->codeAppendf("float DCub = DSqd * %s.%c;", dst, component);
  fsBuilder->codeAppendf("float DaSqd = %s.a * %s.a;", dst, dst);
  fsBuilder->codeAppendf("float DaCub = DaSqd * %s.a;", dst);
  // (Da^3 (-S)+Da^2 (S-D (3 Sa-6 S-1))+12 Da D^2 (Sa-2 S)-16 D^3 (Sa-2 S))/Da^2
  fsBuilder->codeAppendf(
      "%s.%c ="
      "(DaSqd*(%s.%c - %s.%c * (3.0*%s.a - 6.0*%s.%c - 1.0)) +"
      " 12.0*%s.a*DSqd*(%s.a - 2.0*%s.%c) - 16.0*DCub * (%s.a - 2.0*%s.%c) -"
      " DaCub*%s.%c) / DaSqd;",
      final, component, src, component, dst, component, src, src, component, dst, src, src,
      component, src, src, component, src, component);
  fsBuilder->codeAppend("} else {");
  // -sqrt(Da * D) (Sa-2 S)-Da S+D (Sa-2 S+1)+S
  fsBuilder->codeAppendf(
      "%s.%c = %s.%c*(%s.a - 2.0*%s.%c + 1.0) + %s.%c -"
      " sqrt(%s.a*%s.%c)*(%s.a - 2.0*%s.%c) - %s.a*%s.%c;",
      final, component, dst, component, src, src, component, src, component, dst, dst, component,
      src, src, component, dst, src, component);
  fsBuilder->codeAppend("}");
}

// Adds a function that takes two colors and an alpha as input. It produces a color with the
// hue and saturation of the first color, the luminosity of the second color, and the input
// alpha. It has this signature:
//      vec3 set_luminance(vec3 hueSatColor, float alpha, vec3 lumColor).
static void AddLumFunction(FragmentShaderBuilder* fsBuilder, std::string* setLumFunction) {
  // Emit a helper that gets the luminance of a color.
  fsBuilder->addFunction(R"(
float luminance(vec3 color) {
 return dot(vec3(0.3, 0.59, 0.11), color);
}
)");

  // Emit the set luminance function.
  fsBuilder->addFunction(R"(
vec3 set_luminance(vec3 hueSat, float alpha, vec3 lumColor) {
  float diff = luminance(lumColor - hueSat);
  vec3 outColor = hueSat + diff;
  float outLum = luminance(outColor);
  float minComp = min(min(outColor.r, outColor.g), outColor.b);
  float maxComp = max(max(outColor.r, outColor.g), outColor.b);
  if (minComp < 0.0 && outLum != minComp) {
    outColor = outLum + ((outColor - vec3(outLum, outLum, outLum)) * outLum) / (outLum - minComp);
  }
  if (maxComp > alpha && maxComp != outLum) {
    outColor = outLum + ((outColor - vec3(outLum, outLum, outLum)) * (alpha - outLum)) / (maxComp - outLum);
  }
  return outColor;
}
)");
  *setLumFunction = "set_luminance";
}

// Adds a function that creates a color with the hue and luminosity of one input color and
// the saturation of another color. It will have this signature:
//      float set_saturation(vec3 hueLumColor, vec3 satColor)
static void AddSatFunction(FragmentShaderBuilder* fsBuilder, std::string* setSatFunction) {
  // Emit a helper that gets the saturation of a color
  fsBuilder->addFunction(R"(
float saturation(vec3 color) {
 return max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
}
)");

  // Emit a helper that sets the saturation given sorted input channels. This used
  // to use inout params for min, mid, and max components but that seems to cause
  // problems on PowerVR drivers. So instead it returns a vec3 where r, g ,b are the
  // adjusted min, mid, and max inputs, respectively.
  fsBuilder->addFunction(R"(
vec3 set_saturation_helper(float minComp, float midComp, float maxComp, float sat) {
  if (minComp < maxComp) {
    vec3 result;
    result.r = 0.0;
    result.g = sat * (midComp - minComp) / (maxComp - minComp);
    result.b = sat;
    return result;
  } else {
    return vec3(0, 0, 0);
  }
}
)");

  fsBuilder->addFunction(R"(
vec3 set_saturation(vec3 hueLumColor, vec3 satColor) {
  float sat = saturation(satColor);
  if (hueLumColor.r <= hueLumColor.g) {
    if (hueLumColor.g <= hueLumColor.b) {
      hueLumColor.rgb = set_saturation_helper(hueLumColor.r, hueLumColor.g, hueLumColor.b, sat);
    } else if (hueLumColor.r <= hueLumColor.b) {
      hueLumColor.rbg = set_saturation_helper(hueLumColor.r, hueLumColor.b, hueLumColor.g, sat);
    } else {
      hueLumColor.brg = set_saturation_helper(hueLumColor.b, hueLumColor.r, hueLumColor.g, sat);
    }
  } else if (hueLumColor.r <= hueLumColor.b) {
    hueLumColor.grb = set_saturation_helper(hueLumColor.g, hueLumColor.r, hueLumColor.b, sat);
  } else if (hueLumColor.g <= hueLumColor.b) {
    hueLumColor.gbr = set_saturation_helper(hueLumColor.g, hueLumColor.b, hueLumColor.r, sat);
  } else {
    hueLumColor.bgr = set_saturation_helper(hueLumColor.b, hueLumColor.g, hueLumColor.r, sat);
  }
  return hueLumColor;
}
)");
  *setSatFunction = "set_saturation";
}

static void BlendHandler_Overlay(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                 const char* dstColor, const char* outputColor) {
  // Overlay is Hard-Light with the src and dst reversed
  HardLight(fsBuilder, outputColor, dstColor, srcColor);
}

static void BlendHandler_Darken(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf(
      "%s.rgb = min((1.0 - %s.a) * %s.rgb + %s.rgb, (1.0 - %s.a) * %s.rgb + %s.rgb);", outputColor,
      srcColor, dstColor, srcColor, dstColor, srcColor, dstColor);
}

static void BlendHandler_Lighten(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                 const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf(
      "%s.rgb = max((1.0 - %s.a) * %s.rgb + %s.rgb, (1.0 - %s.a) * %s.rgb + %s.rgb);", outputColor,
      srcColor, dstColor, srcColor, dstColor, srcColor, dstColor);
}

static void BlendHandler_ColorDodge(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                    const char* dstColor, const char* outputColor) {
  ColorDodgeComponent(fsBuilder, outputColor, srcColor, dstColor, 'r');
  ColorDodgeComponent(fsBuilder, outputColor, srcColor, dstColor, 'g');
  ColorDodgeComponent(fsBuilder, outputColor, srcColor, dstColor, 'b');
}

static void BlendHandler_ColorBurn(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                   const char* dstColor, const char* outputColor) {
  ColorBurnComponent(fsBuilder, outputColor, srcColor, dstColor, 'r');
  ColorBurnComponent(fsBuilder, outputColor, srcColor, dstColor, 'g');
  ColorBurnComponent(fsBuilder, outputColor, srcColor, dstColor, 'b');
}

static void BlendHandler_HardLight(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                   const char* dstColor, const char* outputColor) {
  HardLight(fsBuilder, outputColor, srcColor, dstColor);
}

static void BlendHandler_SoftLight(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                   const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf("if (0.0 == %s.a) {", dstColor);
  fsBuilder->codeAppendf("%s.rgba = %s;", outputColor, srcColor);
  fsBuilder->codeAppend("} else {");
  SoftLightComponentPosDstAlpha(fsBuilder, outputColor, srcColor, dstColor, 'r');
  SoftLightComponentPosDstAlpha(fsBuilder, outputColor, srcColor, dstColor, 'g');
  SoftLightComponentPosDstAlpha(fsBuilder, outputColor, srcColor, dstColor, 'b');
  fsBuilder->codeAppend("}");
}

static void BlendHandler_Difference(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                    const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf("%s.rgb = %s.rgb + %s.rgb - 2.0 * min(%s.rgb * %s.a, %s.rgb * %s.a);",
                         outputColor, srcColor, dstColor, srcColor, dstColor, dstColor, srcColor);
}

static void BlendHandler_Exclusion(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                   const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf("%s.rgb = %s.rgb + %s.rgb - 2.0 * %s.rgb * %s.rgb;", outputColor, dstColor,
                         srcColor, dstColor, srcColor);
}

static void BlendHandler_Multiply(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                  const char* dstColor, const char* outputColor) {
  fsBuilder->codeAppendf(
      "%s.rgb = (1.0 - %s.a) * %s.rgb + (1.0 - %s.a) * %s.rgb + %s.rgb * %s.rgb;", outputColor,
      srcColor, dstColor, dstColor, srcColor, srcColor, dstColor);
}

static void BlendHandler_Hue(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                             const char* dstColor, const char* outputColor) {
  // SetLum(SetSat(S * Da, Sat(D * Sa)), Sa*Da, D*Sa) + (1 - Sa) * D + (1 - Da) * S
  std::string setSat, setLum;
  AddSatFunction(fsBuilder, &setSat);
  AddLumFunction(fsBuilder, &setLum);
  fsBuilder->codeAppendf("vec4 dstSrcAlpha = %s * %s.a;", dstColor, srcColor);
  fsBuilder->codeAppendf(
      "%s.rgb = %s(%s(%s.rgb * %s.a, dstSrcAlpha.rgb), dstSrcAlpha.a, dstSrcAlpha.rgb);",
      outputColor, setLum.c_str(), setSat.c_str(), srcColor, dstColor);
  fsBuilder->codeAppendf("%s.rgb += (1.0 - %s.a) * %s.rgb + (1.0 - %s.a) * %s.rgb;", outputColor,
                         srcColor, dstColor, dstColor, srcColor);
}

static void BlendHandler_Saturation(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                    const char* dstColor, const char* outputColor) {
  // SetLum(SetSat(D * Sa, Sat(S * Da)), Sa*Da, D*Sa)) + (1 - Sa) * D + (1 - Da) * S
  std::string setSat, setLum;
  AddSatFunction(fsBuilder, &setSat);
  AddLumFunction(fsBuilder, &setLum);
  fsBuilder->codeAppendf("vec4 dstSrcAlpha = %s * %s.a;", dstColor, srcColor);
  fsBuilder->codeAppendf(
      "%s.rgb = %s(%s(dstSrcAlpha.rgb, %s.rgb * %s.a), dstSrcAlpha.a, dstSrcAlpha.rgb);",
      outputColor, setLum.c_str(), setSat.c_str(), srcColor, dstColor);
  fsBuilder->codeAppendf("%s.rgb += (1.0 - %s.a) * %s.rgb + (1.0 - %s.a) * %s.rgb;", outputColor,
                         srcColor, dstColor, dstColor, srcColor);
}

static void BlendHandler_Color(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                               const char* dstColor, const char* outputColor) {
  // SetLum(S * Da, Sa* Da, D * Sa) + (1 - Sa) * D + (1 - Da) * S
  std::string setLum;
  AddLumFunction(fsBuilder, &setLum);
  fsBuilder->codeAppendf("vec4 srcDstAlpha = %s * %s.a;", srcColor, dstColor);
  fsBuilder->codeAppendf("%s.rgb = %s(srcDstAlpha.rgb, srcDstAlpha.a, %s.rgb * %s.a);", outputColor,
                         setLum.c_str(), dstColor, srcColor);
  fsBuilder->codeAppendf("%s.rgb += (1.0 - %s.a) * %s.rgb + (1.0 - %s.a) * %s.rgb;", outputColor,
                         srcColor, dstColor, dstColor, srcColor);
}

static void BlendHandler_Luminosity(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                                    const char* dstColor, const char* outputColor) {
  // SetLum(D * Sa, Sa* Da, S * Da) + (1 - Sa) * D + (1 - Da) * S
  std::string setLum;
  AddLumFunction(fsBuilder, &setLum);
  fsBuilder->codeAppendf("vec4 srcDstAlpha = %s * %s.a;", srcColor, dstColor);
  fsBuilder->codeAppendf("%s.rgb = %s(%s.rgb * %s.a, srcDstAlpha.a, srcDstAlpha.rgb);", outputColor,
                         setLum.c_str(), dstColor, srcColor);
  fsBuilder->codeAppendf("%s.rgb += (1.0 - %s.a) * %s.rgb + (1.0 - %s.a) * %s.rgb;", outputColor,
                         srcColor, dstColor, dstColor, srcColor);
}

using BlendHandler = void (*)(FragmentShaderBuilder* fsBuilder, const char* srcColor,
                              const char* dstColor, const char* outputColor);

static constexpr std::pair<BlendMode, BlendHandler> kBlendHandlers[] = {
    {BlendMode::Overlay, BlendHandler_Overlay},
    {BlendMode::Darken, BlendHandler_Darken},
    {BlendMode::Lighten, BlendHandler_Lighten},
    {BlendMode::ColorDodge, BlendHandler_ColorDodge},
    {BlendMode::ColorBurn, BlendHandler_ColorBurn},
    {BlendMode::HardLight, BlendHandler_HardLight},
    {BlendMode::SoftLight, BlendHandler_SoftLight},
    {BlendMode::Difference, BlendHandler_Difference},
    {BlendMode::Exclusion, BlendHandler_Exclusion},
    {BlendMode::Multiply, BlendHandler_Multiply},
    {BlendMode::Hue, BlendHandler_Hue},
    {BlendMode::Saturation, BlendHandler_Saturation},
    {BlendMode::Color, BlendHandler_Color},
    {BlendMode::Luminosity, BlendHandler_Luminosity}};

static void HandleBlendModes(FragmentShaderBuilder* fsBuilder, const std::string& srcColor,
                             const std::string& dstColor, const std::string& outputColor,
                             BlendMode blendMode) {
  // These all perform src-over on the alpha channel.
  fsBuilder->codeAppendf("%s.a = %s.a + (1.0 - %s.a) * %s.a;", outputColor.c_str(),
                         srcColor.c_str(), srcColor.c_str(), dstColor.c_str());
  for (const auto& pair : kBlendHandlers) {
    if (pair.first == blendMode) {
      pair.second(fsBuilder, srcColor.c_str(), dstColor.c_str(), outputColor.c_str());
      break;
    }
  }
}

static constexpr std::pair<BlendMode, std::pair<unsigned, unsigned>> kBlendCoeffMap[] = {
    {BlendMode::Clear, {GL_ZERO, GL_ZERO}},
    {BlendMode::Src, {GL_ONE, GL_ZERO}},
    {BlendMode::Dst, {GL_ZERO, GL_ONE}},
    {BlendMode::SrcOver, {GL_ONE, GL_ONE_MINUS_SRC_ALPHA}},
    {BlendMode::DstOver, {GL_ONE_MINUS_DST_ALPHA, GL_ONE}},
    {BlendMode::SrcIn, {GL_DST_ALPHA, GL_ZERO}},
    {BlendMode::DstIn, {GL_ZERO, GL_SRC_ALPHA}},
    {BlendMode::SrcOut, {GL_ONE_MINUS_DST_ALPHA, GL_ZERO}},
    {BlendMode::DstOut, {GL_ZERO, GL_ONE_MINUS_SRC_ALPHA}},
    {BlendMode::SrcATop, {GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA}},
    {BlendMode::DstATop, {GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA}},
    {BlendMode::Xor, {GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA}},
    {BlendMode::Plus, {GL_ONE, GL_ONE}},
    {BlendMode::Modulate, {GL_ZERO, GL_SRC_COLOR}},
    {BlendMode::Screen, {GL_ONE, GL_ONE_MINUS_SRC_COLOR}}};

bool BlendAsCoeff(BlendMode blendMode, unsigned* first, unsigned* second) {
  for (const auto& pair : kBlendCoeffMap) {
    if (pair.first == blendMode) {
      if (first) {
        *first = pair.second.first;
      }
      if (second) {
        *second = pair.second.second;
      }
      return true;
    }
  }
  return false;
}

static void CoeffHandler_ONE(FragmentShaderBuilder*, const char*, const char*) {
}

static void CoeffHandler_SRC_COLOR(FragmentShaderBuilder* fsBuilder, const char* srcColorName,
                                   const char*) {
  fsBuilder->codeAppendf(" * %s", srcColorName);
}

static void CoeffHandler_ONE_MINUS_SRC_COLOR(FragmentShaderBuilder* fsBuilder,
                                             const char* srcColorName, const char*) {
  fsBuilder->codeAppendf(" * (vec4(1.0) - %s)", srcColorName);
}

static void CoeffHandler_DST_COLOR(FragmentShaderBuilder* fsBuilder, const char*,
                                   const char* dstColorName) {
  fsBuilder->codeAppendf(" * %s", dstColorName);
}

static void CoeffHandler_ONE_MINUS_DST_COLOR(FragmentShaderBuilder* fsBuilder, const char*,
                                             const char* dstColorName) {
  fsBuilder->codeAppendf(" * (vec4(1.0) - %s)", dstColorName);
}

static void CoeffHandler_SRC_ALPHA(FragmentShaderBuilder* fsBuilder, const char* srcColorName,
                                   const char*) {
  fsBuilder->codeAppendf(" * %s.a", srcColorName);
}

static void CoeffHandler_ONE_MINUS_SRC_ALPHA(FragmentShaderBuilder* fsBuilder,
                                             const char* srcColorName, const char*) {
  fsBuilder->codeAppendf(" * (1.0 - %s.a)", srcColorName);
}

static void CoeffHandler_DST_ALPHA(FragmentShaderBuilder* fsBuilder, const char*,
                                   const char* dstColorName) {
  fsBuilder->codeAppendf(" * %s.a", dstColorName);
}

static void CoeffHandler_ONE_MINUS_DST_ALPHA(FragmentShaderBuilder* fsBuilder, const char*,
                                             const char* dstColorName) {
  fsBuilder->codeAppendf(" * (1.0 - %s.a)", dstColorName);
}

using CoeffHandler = void (*)(FragmentShaderBuilder* fsBuilder, const char* srcColorName,
                              const char* dstColorName);

static constexpr std::pair<unsigned, CoeffHandler> kCoeffHandleMap[] = {
    {GL_ONE, CoeffHandler_ONE},
    {GL_SRC_COLOR, CoeffHandler_SRC_COLOR},
    {GL_ONE_MINUS_SRC_COLOR, CoeffHandler_ONE_MINUS_SRC_COLOR},
    {GL_DST_COLOR, CoeffHandler_DST_COLOR},
    {GL_ONE_MINUS_DST_COLOR, CoeffHandler_ONE_MINUS_DST_COLOR},
    {GL_SRC_ALPHA, CoeffHandler_SRC_ALPHA},
    {GL_ONE_MINUS_SRC_ALPHA, CoeffHandler_ONE_MINUS_SRC_ALPHA},
    {GL_DST_ALPHA, CoeffHandler_DST_ALPHA},
    {GL_ONE_MINUS_DST_ALPHA, CoeffHandler_ONE_MINUS_DST_ALPHA}};

static bool AppendPorterDuffTerm(FragmentShaderBuilder* fsBuilder, unsigned coeff,
                                 const std::string& colorName, const std::string& srcColorName,
                                 const std::string& dstColorName, bool hasPrevious) {
  if (GL_ZERO == coeff) {
    return hasPrevious;
  } else {
    if (hasPrevious) {
      fsBuilder->codeAppend(" + ");
    }
    fsBuilder->codeAppendf("%s", colorName.c_str());
    for (const auto& pair : kCoeffHandleMap) {
      if (pair.first == coeff) {
        pair.second(fsBuilder, srcColorName.c_str(), dstColorName.c_str());
        break;
      }
    }
    return true;
  }
}

void AppendMode(FragmentShaderBuilder* fsBuilder, const std::string& srcColor,
                const std::string& dstColor, const std::string& outColor, BlendMode blendMode) {
  unsigned srcCoeff, dstCoeff;
  if (BlendAsCoeff(blendMode, &srcCoeff, &dstCoeff)) {
    // The only coeff mode that can go out of range is plus.
    bool clamp = blendMode == BlendMode::Plus;

    fsBuilder->codeAppendf("%s = ", outColor.c_str());
    if (clamp) {
      fsBuilder->codeAppend("clamp(");
    }
    // append src blend
    bool didAppend = AppendPorterDuffTerm(fsBuilder, srcCoeff, srcColor, srcColor, dstColor, false);
    // append dst blend
    if (!AppendPorterDuffTerm(fsBuilder, dstCoeff, dstColor, srcColor, dstColor, didAppend)) {
      fsBuilder->codeAppend("vec4(0, 0, 0, 0)");
    }
    if (clamp) {
      fsBuilder->codeAppend(", 0, 1);");
    }
    fsBuilder->codeAppend(";");
  } else {
    HandleBlendModes(fsBuilder, srcColor, dstColor, outColor, blendMode);
  }
}

const char* BlendModeName(BlendMode mode) {
  const char* ModeStrings[] = {"Clear",     "Src",        "Dst",        "SrcOver",    "DstOver",
                               "SrcIn",     "DstIn",      "SrcOut",     "DstOut",     "SrcATop",
                               "DstATop",   "Xor",        "Plus",       "Modulate",   "Screen",
                               "Overlay",   "Darken",     "Lighten",    "ColorDodge", "ColorBurn",
                               "HardLight", "SoftLight",  "Difference", "Exclusion",  "Multiply",
                               "Hue",       "Saturation", "Color",      "Luminosity"};
  return ModeStrings[static_cast<int>(mode)];
}
}  // namespace tgfx
