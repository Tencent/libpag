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

#include "Transform3DSceneFilter.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include "base/utils/Log.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/filters/utils/Std140Uniforms.h"

namespace pag {
namespace {

// Q32 depth codes avoid software binary64 arithmetic except near uncertain boundaries.
static constexpr char PEEL_SHADER[] = R"(
  precision highp float;
  precision highp int;
  in vec2 vertexColor;
  uniform sampler2D sTexture;
  uniform sampler2D sDepth;
  uniform sampler2D sIndex;
  layout(std140) uniform FragmentUniforms {
    uvec4 uSlopeBits;
    uvec4 uOriginBits;
    uvec4 uFixedSlopeBits;
    uint uIndex;
    uint uMode;
    int uFlags;
    int uOutput;
  };
  out vec4 tgfx_FragColor;
  uvec2 AddWords(uvec2 a, uvec2 b) {
    uint low = a.x + b.x;
    return uvec2(low, a.y + b.y + uint(low < a.x));
  }
  uvec2 SubtractWords(uvec2 a, uvec2 b) {
    return uvec2(a.x - b.x, a.y - b.y - uint(a.x < b.x));
  }
  uvec2 ShiftLeft(uvec2 value, uint count) {
    if (count == 0u) { return value; }
    if (count < 32u) { return uvec2(value.x << count, (value.y << count) | (value.x >> (32u - count))); }
    if (count < 64u) { return uvec2(0u, value.x << (count - 32u)); }
    return uvec2(0u);
  }
  uvec2 ShiftRight(uvec2 value, uint count) {
    if (count == 0u) { return value; }
    if (count < 32u) { return uvec2((value.x >> count) | (value.y << (32u - count)), value.y >> count); }
    if (count < 64u) { return uvec2(value.y >> (count - 32u), 0u); }
    return uvec2(0u);
  }
  uvec2 ShiftRightSticky(uvec2 value, uint count) {
    uvec2 result = ShiftRight(value, count);
    uvec2 restored = ShiftLeft(result, count);
    if (any(notEqual(value, restored))) { result.x |= 1u; }
    return result;
  }
  int Exponent(uvec2 value) {
    return max(int((value.y >> 20u) & 2047u), 1) - 1023;
  }
  uvec2 Mantissa(uvec2 value) {
    uint high = value.y & 1048575u;
    if ((value.y & 2146435072u) != 0u) { high |= 1048576u; }
    return uvec2(value.x, high);
  }
  uvec2 Normalize(uvec2 value, int exponent, uint sign) {
    if (all(equal(value, uvec2(0u)))) { return uvec2(0u); }
    if (value.y >= 16777216u) {
      value = ShiftRightSticky(value, 1u);
      exponent++;
    }
    while (value.y < 8388608u && exponent > -1022) {
      value = ShiftLeft(value, 1u);
      exponent--;
    }
    if (exponent < -1022) {
      value = ShiftRightSticky(value, uint(-1022 - exponent));
      exponent = -1022;
    }
    uint remainder = value.x & 7u;
    if (remainder > 4u || (remainder == 4u && (value.x & 8u) != 0u)) {
      value = AddWords(value, uvec2(8u, 0u));
    }
    value = ShiftRight(value, 3u);
    if (value.y >= 2097152u) {
      value = ShiftRight(value, 1u);
      exponent++;
    }
    if (exponent > 1023) { return uvec2(0u, sign | 2146435072u); }
    uint exponentBits = value.y < 1048576u ? 0u : uint(exponent + 1023);
    return uvec2(value.x, sign | (exponentBits << 20u) | (value.y & 1048575u));
  }
  uvec2 AddDepth(uvec2 a, uvec2 b) {
    int exponentA = Exponent(a);
    int exponentB = Exponent(b);
    int exponent = max(exponentA, exponentB);
    uvec2 mantissaA = ShiftRightSticky(ShiftLeft(Mantissa(a), 3u), uint(exponent - exponentA));
    uvec2 mantissaB = ShiftRightSticky(ShiftLeft(Mantissa(b), 3u), uint(exponent - exponentB));
    uint signA = a.y & 2147483648u;
    uint signB = b.y & 2147483648u;
    if (signA == signB) { return Normalize(AddWords(mantissaA, mantissaB), exponent, signA); }
    bool aGreater = mantissaA.y > mantissaB.y || (mantissaA.y == mantissaB.y && mantissaA.x >= mantissaB.x);
    return aGreater ? Normalize(SubtractWords(mantissaA, mantissaB), exponent, signA)
                    : Normalize(SubtractWords(mantissaB, mantissaA), exponent, signB);
  }
  uvec2 MultiplyWords(uint a, uint b) {
    uint low = (a & 65535u) * (b & 65535u);
    uint middle = (a >> 16u) * (b & 65535u) + (low >> 16u);
    uint upper = (a & 65535u) * (b >> 16u) + (middle & 65535u);
    uint high = (a >> 16u) * (b >> 16u) + (middle >> 16u) + (upper >> 16u);
    return uvec2((upper << 16u) | (low & 65535u), high);
  }
  int HighestBit(uint value) {
    if (value == 0u) { return -1; }
    int bit = 0;
    if (value >= 65536u) { value >>= 16u; bit += 16; }
    if (value >= 256u) { value >>= 8u; bit += 8; }
    if (value >= 16u) { value >>= 4u; bit += 4; }
    if (value >= 4u) { value >>= 2u; bit += 2; }
    return bit + int(value >= 2u);
  }
  uvec2 MultiplyDepth(uvec2 value, uint point) {
    if (point == 0u) { return uvec2(0u); }
    uvec2 mantissa = Mantissa(value);
    if (all(equal(mantissa, uvec2(0u)))) { return uvec2(0u); }
    uvec2 lowProduct = MultiplyWords(mantissa.x, point);
    uvec2 highProduct = MultiplyWords(mantissa.y, point);
    uint middle = lowProduct.y + highProduct.x;
    uvec3 product = uvec3(lowProduct.x, middle, highProduct.y + uint(middle < lowProduct.y));
    int highest = product.z != 0u ? HighestBit(product.z) + 64 : product.y != 0u ? HighestBit(product.y) + 32 : HighestBit(product.x);
    int shift = highest - 55;
    uvec2 extended;
    if (shift <= 0) {
      extended = ShiftLeft(product.xy, uint(-shift));
    } else {
      uint count = uint(shift);
      extended = uvec2((product.x >> count) | (product.y << (32u - count)),
                      (product.y >> count) | (product.z << (32u - count)));
      if ((product.x & ((1u << count) - 1u)) != 0u) { extended.x |= 1u; }
    }
    return Normalize(extended, Exponent(value) + highest - 52, value.y & 2147483648u);
  }
  bool EncodeDepth(uvec2 depth, out uint encoded) {
    if ((depth.y & 2147483648u) != 0u && any(notEqual(Mantissa(depth), uvec2(0u)))) { return false; }
    int exponent = Exponent(depth);
    if (exponent > 23) { return false; }
    if (exponent < 0) {
      encoded = exponent == -1 ? 1u : 0u;
      return true;
    }
    uvec2 mantissa = Mantissa(depth);
    uint shift = uint(52 - exponent);
    uint whole = ShiftRight(mantissa, shift).x;
    uvec2 remainder = SubtractWords(mantissa, ShiftLeft(uvec2(whole, 0u), shift));
    if (whole == 16777215u && any(notEqual(remainder, uvec2(0u)))) { return false; }
    encoded = whole + (ShiftRight(remainder, shift - 1u).x & 1u);
    return true;
  }
  uvec2 FixedProduct(uvec2 value, uint point) {
    uvec2 product = MultiplyWords(value.x, point);
    return uvec2(product.x, product.y + value.y * point);
  }
  bool FixedEncoding(ivec2 point, out uint encoded, out bool certain) {
    uvec2 x = FixedProduct(uFixedSlopeBits.xy, uint(point.x));
    uvec2 y = FixedProduct(uFixedSlopeBits.zw, uint(point.y));
    uvec2 value = AddWords(AddWords(uOriginBits.zw, x), y);
    uint error = (uint(point.x) >> 1u) + (uint(point.y) >> 1u) + 65538u;
    bool clipBoundary = (value.y == 0u && value.x <= error) ||
                        (value.y == 4294967295u && value.x >= 0u - error) ||
                        (value.y == 16777215u && value.x <= error) ||
                        (value.y == 16777214u && value.x >= 0u - error);
    bool roundBoundary = value.x >= 2147483648u - error &&
                         value.x <= 2147483648u + error;
    certain = !clipBoundary && !roundBoundary;
    if (value.y > 16777215u || (value.y == 16777215u && value.x != 0u)) {
      return false;
    }
    encoded = value.y + uint(value.x >= 2147483648u);
    return true;
  }
  uint unpackIndex(vec4 value) {
    uvec4 bytes = uvec4(round(value * 255.0));
    return bytes.x | (bytes.y << 8u) | (bytes.z << 16u) | (bytes.w << 24u);
  }
  void main() {
    ivec2 point = ivec2(vertexColor * vec2(textureSize(sTexture, 0)));
    vec4 color = texelFetch(sTexture, point, 0);
    if (color.a <= 0.0) { discard; }
    uint encodedDepth;
    bool certain = false;
    bool visible = false;
    if ((uFlags & 2) != 0) {
      visible = FixedEncoding(point, encodedDepth, certain);
    }
    if (!certain) {
      uvec2 xDepth = MultiplyDepth(uSlopeBits.xy, uint(point.x));
      uvec2 yDepth = MultiplyDepth(uSlopeBits.zw, uint(point.y));
      uvec2 depth = AddDepth(AddDepth(uOriginBits.xy, xDepth), yDepth);
      visible = EncodeDepth(depth, encodedDepth);
    }
    if (!visible) { discard; }
    if ((uFlags & 1) == 0) {
      vec4 previous = texelFetch(sDepth, point, 0);
      uvec3 bytes = uvec3(round(previous.rgb * 255.0));
      uint previousDepth = bytes.x | (bytes.y << 8u) | (bytes.z << 16u);
      uint previousIndex = unpackIndex(texelFetch(sIndex, point, 0));
      if (previous.a == 0.0 || encodedDepth > previousDepth ||
          (encodedDepth == previousDepth && uIndex <= previousIndex)) { discard; }
    }
    gl_FragDepth = float(encodedDepth) / 16777215.0;
    vec4 depthColor = vec4(float(encodedDepth & 255u), float((encodedDepth >> 8u) & 255u),
                      float((encodedDepth >> 16u) & 255u), float(uMode + 1u)) / 255.0;
    vec4 indexColor = vec4(float(uIndex & 255u), float((uIndex >> 8u) & 255u),
                      float((uIndex >> 16u) & 255u), float((uIndex >> 24u) & 255u)) / 255.0;
    tgfx_FragColor = uOutput == 0 ? color : uOutput == 1 ? depthColor : indexColor;
  }
)";

static constexpr char BLEND_SHADER[] = R"(
  precision highp float;
  precision highp int;
  in vec2 vertexColor;
  uniform sampler2D sTexture;
  uniform sampler2D sLayer;
  uniform sampler2D sDepth;
  out vec4 tgfx_FragColor;
  float luminance(vec3 color) { return dot(color, vec3(0.3, 0.59, 0.11)); }
  float saturation(vec3 color) { return max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b); }
  vec3 clipColor(vec3 color) {
    float lum = luminance(color);
    float low = min(min(color.r, color.g), color.b);
    float high = max(max(color.r, color.g), color.b);
    if (low < 0.0) { color = vec3(lum) + (color - vec3(lum)) * lum / (lum - low); }
    if (high > 1.0) { color = vec3(lum) + (color - vec3(lum)) * (1.0 - lum) / (high - lum); }
    return color;
  }
  vec3 setLum(vec3 color, float lum) { return clipColor(color + vec3(lum - luminance(color))); }
  vec3 setSat(vec3 color, float sat) {
    float low = min(min(color.r, color.g), color.b);
    float high = max(max(color.r, color.g), color.b);
    return high > low ? (color - vec3(low)) * sat / (high - low) : vec3(0.0);
  }
  float softLight(float source, float destination) {
    if (source <= 0.5) { return destination - (1.0 - 2.0 * source) * destination * (1.0 - destination); }
    float value = destination <= 0.25 ? ((16.0 * destination - 12.0) * destination + 4.0) * destination : sqrt(destination);
    return destination + (2.0 * source - 1.0) * (value - destination);
  }
  float dodge(float source, float destination) {
    return destination == 0.0 ? 0.0 : source == 1.0 ? 1.0 : min(1.0, destination / (1.0 - source));
  }
  float burn(float source, float destination) {
    return destination == 1.0 ? 1.0 : source == 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - destination) / source);
  }
  vec3 blendColor(vec3 source, vec3 destination, int mode) {
    switch (mode) {
      case 1: return source * destination;
      case 2: return source + destination - source * destination;
      case 3: return mix(2.0 * source * destination, 1.0 - 2.0 * (1.0 - source) * (1.0 - destination), step(vec3(0.5), destination));
      case 4: return min(source, destination);
      case 5: return max(source, destination);
      case 6: return vec3(dodge(source.r, destination.r), dodge(source.g, destination.g), dodge(source.b, destination.b));
      case 7: return vec3(burn(source.r, destination.r), burn(source.g, destination.g), burn(source.b, destination.b));
      case 8: return mix(2.0 * source * destination, 1.0 - 2.0 * (1.0 - source) * (1.0 - destination), step(vec3(0.5), source));
      case 9: return vec3(softLight(source.r, destination.r), softLight(source.g, destination.g), softLight(source.b, destination.b));
      case 10: return abs(destination - source);
      case 11: return source + destination - 2.0 * source * destination;
      case 12: return setLum(setSat(source, saturation(destination)), luminance(destination));
      case 13: return setLum(setSat(destination, saturation(source)), luminance(destination));
      case 14: return setLum(source, luminance(destination));
      case 15: return setLum(destination, luminance(source));
      default: return source;
    }
  }
  void main() {
    ivec2 point = ivec2(vertexColor * vec2(textureSize(sTexture, 0)));
    vec4 destination = texelFetch(sTexture, point, 0);
    vec4 metadata = texelFetch(sDepth, point, 0);
    if (metadata.a == 0.0) { tgfx_FragColor = destination; return; }
    vec4 source = texelFetch(sLayer, point, 0);
    int mode = int(round(metadata.a * 255.0)) - 1;
    if (mode == 0) { tgfx_FragColor = source + destination * (1.0 - source.a); return; }
    if (mode == 16) { tgfx_FragColor = min(source + destination, vec4(1.0)); return; }
    vec3 src = source.a > 0.0 ? source.rgb / source.a : vec3(0.0);
    vec3 dst = destination.a > 0.0 ? destination.rgb / destination.a : vec3(0.0);
    vec3 color = blendColor(src, dst, mode);
    tgfx_FragColor = vec4(source.rgb * (1.0 - destination.a) + destination.rgb * (1.0 - source.a) + color * source.a * destination.a,
                          source.a + destination.a - source.a * destination.a);
  }
)";

struct alignas(16) PeelUniforms {
  std::array<uint32_t, 4> slopeBits;
  std::array<uint32_t, 4> originBits;
  std::array<uint32_t, 4> fixedSlopeBits;
  uint32_t index;
  uint32_t mode;
  int32_t flags;
  int32_t output;
};
static_assert(HasStd140UniformLayout<PeelUniforms, 64>());
static_assert(offsetof(PeelUniforms, slopeBits) == 0);
static_assert(offsetof(PeelUniforms, originBits) == 16);
static_assert(offsetof(PeelUniforms, fixedSlopeBits) == 32);
static_assert(offsetof(PeelUniforms, index) == 48);
static_assert(offsetof(PeelUniforms, mode) == 52);
static_assert(offsetof(PeelUniforms, flags) == 56);
static_assert(offsetof(PeelUniforms, output) == 60);
static_assert(sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);

std::array<uint32_t, 2> PackDepthCoefficient(double value) {
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return {static_cast<uint32_t>(bits), static_cast<uint32_t>(bits >> 32)};
}

std::array<uint32_t, 2> PackFixedCoefficient(double value) {
  auto fixed = static_cast<int64_t>(std::round(value * 4294967296.0));
  auto bits = static_cast<uint64_t>(fixed);
  return {static_cast<uint32_t>(bits), static_cast<uint32_t>(bits >> 32)};
}

// Conversion and every partial sum stay below signed64 capacity. The fixed rounding error is
// at most (x + y + 1) / 2 low-word units; 65536 extra units conservatively cover binary64
// rounding over the permitted magnitude. Uncertain clipping and half-integer codes use EncodeDepth.
PeelUniforms MakeDepthUniforms(const Transform3DDepthPlane& plane, int width, int height) {
  auto stepX = plane.x * (2.0 / width) * 16777215.0;
  auto stepY = plane.y * (2.0 / height) * 16777215.0;
  auto origin = (plane.z - plane.x - plane.y) * 16777215.0 + (stepX + stepY) * 0.5;
  auto xBits = PackDepthCoefficient(stepX);
  auto yBits = PackDepthCoefficient(stepY);
  auto originBits = PackDepthCoefficient(origin);
  PeelUniforms result = {};
  result.slopeBits = {xBits[0], xBits[1], yBits[0], yBits[1]};
  result.originBits = {originBits[0], originBits[1], 0, 0};
  auto bound = std::abs(origin) + std::abs(stepX) * std::max(width - 1, 1) +
               std::abs(stepY) * std::max(height - 1, 1);
  auto margin = static_cast<double>(width) + height + 65536.0;
  if (std::isfinite(bound) && bound * 4294967296.0 < 9223372036854775808.0 - margin) {
    auto fixedX = PackFixedCoefficient(stepX);
    auto fixedY = PackFixedCoefficient(stepY);
    auto fixedOrigin = PackFixedCoefficient(origin);
    result.fixedSlopeBits = {fixedX[0], fixedX[1], fixedY[0], fixedY[1]};
    result.originBits[2] = fixedOrigin[0];
    result.originBits[3] = fixedOrigin[1];
    result.flags = 2;
  }
  return result;
}

class SceneBlendFilter : public RuntimeFilter {
 public:
  explicit SceneBlendFilter(RenderCache* cache) : RuntimeFilter(cache) {
  }
  std::shared_ptr<tgfx::RenderPipeline> pipeline(tgfx::GPU* gpu) const {
    return getPipeline(gpu);
  }

 protected:
  DEFINE_RUNTIME_FILTER_TYPE
  std::string onBuildFragmentShader() const override {
    return BLEND_SHADER;
  }
  std::vector<tgfx::BindingEntry> textureSamplers() const override {
    return {{"sTexture", 0}, {"sLayer", 1}, {"sDepth", 2}};
  }
  void onConfigurePipeline(tgfx::RenderPipelineDescriptor* descriptor) const override {
    descriptor->fragment.colorAttachments[0].blendEnable = false;
  }
};

}  // namespace

size_t Transform3DSceneFilter::Resources::memoryUsage() const {
  std::array<const tgfx::Texture*, 8> textures = {
      depth.get(),    color.get(),    encodedDepth[0].get(), encodedDepth[1].get(),
      index[0].get(), index[1].get(), accumulated[0].get(),  accumulated[1].get()};
  size_t memory = 0;
  for (auto texture : textures) {
    if (texture != nullptr) {
      memory += static_cast<size_t>(texture->width()) * texture->height() * 4;
    }
  }
  return memory;
}

void Transform3DSceneFilter::Resources::onFrameEnd() {
  if (used) {
    used = false;
    idleFrames = 0;
    return;
  }
  idleFrames = std::min(idleFrames + 1, 2);
  if (idleFrames >= 2) {
    releaseTextures();
  }
}

void Transform3DSceneFilter::Resources::onQueryEnd() {
  releaseTextures();
}

void Transform3DSceneFilter::Resources::releaseTextures() {
  width = 0;
  height = 0;
  depth = nullptr;
  color = nullptr;
  encodedDepth = {};
  index = {};
  accumulated = {};
}

bool Transform3DSceneFilter::prepareTextures(Resources* resources, tgfx::GPU* gpu) const {
  if (!Has3DSupport(gpu) || width <= 0 || height <= 0 ||
      width > gpu->limits()->maxTextureDimension2D ||
      height > gpu->limits()->maxTextureDimension2D) {
    return false;
  }
  resources->used = true;
  if (resources->width == width && resources->height == height) {
    return true;
  }
  tgfx::TextureDescriptor color(
      width, height, tgfx::PixelFormat::RGBA_8888, false, 1,
      tgfx::TextureUsage::RENDER_ATTACHMENT | tgfx::TextureUsage::TEXTURE_BINDING);
  tgfx::TextureDescriptor depth(width, height, tgfx::PixelFormat::DEPTH24_STENCIL8, false, 1,
                                tgfx::TextureUsage::RENDER_ATTACHMENT);
  resources->width = 0;
  resources->height = 0;
  resources->depth = gpu->createTexture(depth);
  resources->color = gpu->createTexture(color);
  if (resources->depth == nullptr || resources->color == nullptr) {
    LOGE("Transform3DSceneFilter: failed to allocate scene textures %dx%d", width, height);
    return false;
  }
  for (size_t index = 0; index < 2; index++) {
    resources->encodedDepth[index] = gpu->createTexture(color);
    resources->index[index] = gpu->createTexture(color);
    resources->accumulated[index] = gpu->createTexture(color);
    if (resources->encodedDepth[index] == nullptr || resources->index[index] == nullptr ||
        resources->accumulated[index] == nullptr) {
      LOGE("Transform3DSceneFilter: failed to allocate scene textures %dx%d", width, height);
      return false;
    }
  }
  resources->width = width;
  resources->height = height;
  return true;
}

// tgfx 的 OpenGL 管线只有一个颜色附件，三个输出须重复同一次深度选择而不能依赖 MRT。
bool Transform3DSceneFilter::drawPeel(tgfx::CommandEncoder* encoder, Resources* resources,
                                      const std::vector<std::shared_ptr<tgfx::Texture>>& inputs,
                                      const std::shared_ptr<tgfx::GPUBuffer>& uniforms,
                                      size_t uniformStride,
                                      const std::shared_ptr<tgfx::GPUBuffer>& vertices,
                                      size_t passIndex) const {
  auto current = passIndex % 2;
  auto previous = 1 - current;
  std::array<std::shared_ptr<tgfx::Texture>, 3> targets = {
      resources->color, resources->encodedDepth[current], resources->index[current]};
  auto layerCount = inputs.size() - 1;
  for (size_t output = 0; output < targets.size(); output++) {
    tgfx::RenderPassDescriptor descriptor(targets[output], tgfx::LoadAction::Clear);
    descriptor.depthStencilAttachment = tgfx::DepthStencilAttachment(
        resources->depth, tgfx::LoadAction::Clear, tgfx::StoreAction::DontCare, 0.0f);
    auto pass = encoder->beginRenderPass(descriptor);
    if (pass == nullptr) {
      return false;
    }
    pass->setPipeline(resources->pipeline);
    pass->setVertexBuffer(0, vertices);
    pass->setTexture(1, resources->encodedDepth[previous], resources->sampler);
    pass->setTexture(2, resources->index[previous], resources->sampler);
    for (size_t index = inputs.size() - 1; index > 0; index--) {
      pass->setTexture(0, inputs[index], resources->sampler);
      auto slot = ((passIndex == 0 ? 0 : 3) + output) * layerCount + index - 1;
      pass->setUniformBuffer(0, uniforms, slot * uniformStride, sizeof(PeelUniforms));
      pass->draw(tgfx::PrimitiveType::TriangleStrip, 4);
    }
    pass->end();
  }
  return true;
}

bool Transform3DSceneFilter::drawBlend(tgfx::CommandEncoder* encoder, Resources* resources,
                                       const std::shared_ptr<tgfx::RenderPipeline>& pipeline,
                                       const std::shared_ptr<tgfx::Texture>& background,
                                       const std::shared_ptr<tgfx::GPUBuffer>& vertices,
                                       size_t passIndex) const {
  auto current = passIndex % 2;
  auto previous = passIndex == 0 ? background : resources->accumulated[1 - current];
  tgfx::RenderPassDescriptor descriptor(resources->accumulated[current],
                                        tgfx::LoadAction::DontCare);
  auto pass = encoder->beginRenderPass(descriptor);
  if (pass == nullptr) {
    return false;
  }
  pass->setPipeline(pipeline);
  pass->setVertexBuffer(0, vertices);
  pass->setTexture(0, previous, resources->sampler);
  pass->setTexture(1, resources->color, resources->sampler);
  pass->setTexture(2, resources->encodedDepth[current], resources->sampler);
  pass->draw(tgfx::PrimitiveType::TriangleStrip, 4);
  pass->end();
  return true;
}

Transform3DSceneFilter::Transform3DSceneFilter(
    RenderCache* cache, const std::vector<std::shared_ptr<tgfx::Image>>& layers,
    const std::vector<Transform3DDepthPlane>& planes, const std::vector<BlendMode>& blendModes,
    int width, int height)
    : RuntimeFilter(cache, layers), planes(planes), blendModes(blendModes), width(width),
      height(height) {
}

tgfx::Rect Transform3DSceneFilter::filterBounds(const tgfx::Rect&) const {
  return tgfx::Rect::MakeWH(width, height);
}

std::string Transform3DSceneFilter::onBuildFragmentShader() const {
  return PEEL_SHADER;
}
std::vector<tgfx::BindingEntry> Transform3DSceneFilter::uniformBlocks() const {
  return {{"FragmentUniforms", 0, tgfx::ShaderVisibility::Fragment}};
}
std::vector<tgfx::BindingEntry> Transform3DSceneFilter::textureSamplers() const {
  return {{"sTexture", 0}, {"sDepth", 1}, {"sIndex", 2}};
}
void Transform3DSceneFilter::onConfigurePipeline(tgfx::RenderPipelineDescriptor* descriptor) const {
  descriptor->fragment.colorAttachments[0].blendEnable = false;
  descriptor->depthStencil.format = tgfx::PixelFormat::DEPTH24_STENCIL8;
  descriptor->depthStencil.depthCompare = tgfx::CompareFunction::GreaterEqual;
  descriptor->depthStencil.depthWriteEnabled = true;
}
std::unique_ptr<FilterResources> Transform3DSceneFilter::onCreateFilterResources() const {
  return std::make_unique<Resources>();
}

bool Transform3DSceneFilter::onDraw(
    tgfx::CommandEncoder* encoder, const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
    std::shared_ptr<tgfx::Texture> outputTexture, const tgfx::Point& offset) const {
  if (encoder == nullptr || outputTexture == nullptr || planes.empty() ||
      inputTextures.size() != planes.size() + 1 || blendModes.size() != planes.size()) {
    return false;
  }
  auto gpu = encoder->gpu();
  if (!Has3DSupport(gpu)) {
    return false;
  }
  auto resources = static_cast<Resources*>(getFilterResources(gpu));
  SceneBlendFilter blend(cache);
  auto blendPipeline = blend.pipeline(gpu);
  if (resources == nullptr || blendPipeline == nullptr || !prepareTextures(resources, gpu)) {
    return false;
  }
  auto values =
      computeVertices(resources->color.get(), resources->color.get(), tgfx::Point::Zero());
  if (NeedsClipSpaceYFlip()) {
    for (size_t index = 1; index < values.size(); index += 4) {
      values[index] = -values[index];
    }
  }
  auto vertices = gpu->createBuffer(values.size() * sizeof(float), tgfx::GPUBufferUsage::VERTEX);
  if (vertices == nullptr) {
    return false;
  }
  auto data = vertices->map();
  if (data == nullptr) {
    return false;
  }
  memcpy(data, values.data(), values.size() * sizeof(float));
  vertices->unmap();
  std::vector<PeelUniforms> depthUniforms;
  for (const auto& plane : planes) {
    depthUniforms.push_back(MakeDepthUniforms(plane, width, height));
  }
  size_t alignment = std::max(gpu->limits()->minUniformBufferOffsetAlignment, 1);
  size_t stride = (sizeof(PeelUniforms) + alignment - 1) / alignment * alignment;
  // 将 2 pass x 3 output x N 层共 6N 份 uniform 合并到单个 UBO，按对齐偏移索引，
  // 避免每帧分配 6N 个独立缓冲。
  auto uniforms = gpu->createBuffer(planes.size() * 6 * stride, tgfx::GPUBufferUsage::UNIFORM);
  if (uniforms == nullptr) {
    LOGE("Transform3DSceneFilter: failed to allocate uniform buffer for %zu layers", planes.size());
    return false;
  }
  auto uniformData = static_cast<uint8_t*>(uniforms->map());
  if (uniformData == nullptr) {
    LOGE("Transform3DSceneFilter: failed to map uniform buffer");
    return false;
  }
  for (int pass = 0; pass < 2; pass++) {
    for (int output = 0; output < 3; output++) {
      for (size_t index = 0; index < planes.size(); index++) {
        auto value = depthUniforms[index];
        value.index = static_cast<uint32_t>(index);
        value.mode = static_cast<uint32_t>(blendModes[index]);
        value.flags |= pass == 0 ? 1 : 0;
        value.output = output;
        auto slot = (static_cast<size_t>(pass) * 3 + output) * planes.size() + index;
        memcpy(uniformData + slot * stride, &value, sizeof(value));
      }
    }
  }
  uniforms->unmap();
  for (size_t pass = 0; pass < planes.size(); pass++) {
    if (!drawPeel(encoder, resources, inputTextures, uniforms, stride, vertices, pass) ||
        !drawBlend(encoder, resources, blendPipeline, inputTextures[0], vertices, pass)) {
      return false;
    }
  }
  auto bounds = tgfx::Rect::MakeWH(outputTexture->width(), outputTexture->height());
  bounds.offset(-offset.x, -offset.y);
  if (!tgfx::Rect::MakeWH(width, height).contains(bounds)) {
    return false;
  }
  encoder->copyTextureToTexture(resources->accumulated[(planes.size() - 1) % 2], bounds,
                                std::move(outputTexture), tgfx::Point::Zero());
  return true;
}

}  // namespace pag
