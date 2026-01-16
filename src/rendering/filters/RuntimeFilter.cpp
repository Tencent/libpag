/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "RuntimeFilter.h"
#include "base/utils/Log.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
static constexpr char VERTEX_SHADER[] = R"(
    in vec2 aPosition;
    in vec2 aTextureCoord;
    out vec2 vertexColor;
    void main() {
      gl_Position = vec4(aPosition.xy, 0, 1);
      vertexColor = aTextureCoord;
    }
)";

static constexpr char FRAGMENT_SHADER[] = R"(
    precision mediump float;
    in vec2 vertexColor;
    uniform sampler2D sTexture;
    out vec4 tgfx_FragColor;

    void main() {
        tgfx_FragColor = texture(sTexture, vertexColor);
    }
)";

std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};
  auto deltaX = outputBounds.left - inputBounds.left;
  auto deltaY = outputBounds.top - inputBounds.top;
  tgfx::Point texturePoints[4] = {
      {deltaX, (outputBounds.height() + deltaY)},
      {(outputBounds.width() + deltaX), (outputBounds.height() + deltaY)},
      {deltaX, deltaY},
      {(outputBounds.width() + deltaX), deltaY}};
  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}

std::string RuntimeFilter::onBuildVertexShader() const {
  return VERTEX_SHADER;
}

std::string RuntimeFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::Attribute> RuntimeFilter::vertexAttributes() const {
  return {{"aPosition", tgfx::VertexFormat::Float2}, {"aTextureCoord", tgfx::VertexFormat::Float2}};
}

std::shared_ptr<tgfx::RenderPipeline> RuntimeFilter::createPipeline(tgfx::GPU* gpu) const {
  auto info = gpu->info();
  auto isDesktop = info->version.find("OpenGL ES") == std::string::npos;
  std::string versionPrefix = isDesktop ? "#version 150\n\n" : "#version 300 es\n\n";

  tgfx::ShaderModuleDescriptor vertexModule = {};
  vertexModule.code = versionPrefix + onBuildVertexShader();
  vertexModule.stage = tgfx::ShaderStage::Vertex;
  auto vertexShader = gpu->createShaderModule(vertexModule);
  if (vertexShader == nullptr) {
    LOGE("RuntimeFilter: Failed to create vertex shader");
    return nullptr;
  }

  tgfx::ShaderModuleDescriptor fragmentModule = {};
  fragmentModule.code = versionPrefix + onBuildFragmentShader();
  fragmentModule.stage = tgfx::ShaderStage::Fragment;
  auto fragmentShader = gpu->createShaderModule(fragmentModule);
  if (fragmentShader == nullptr) {
    LOGE("RuntimeFilter: Failed to create fragment shader");
    return nullptr;
  }

  tgfx::RenderPipelineDescriptor descriptor = {};
  descriptor.vertex = tgfx::VertexDescriptor(vertexAttributes());
  descriptor.vertex.module = vertexShader;
  descriptor.fragment.module = fragmentShader;
  tgfx::PipelineColorAttachment colorAttachment = {};
  colorAttachment.blendEnable = true;
  colorAttachment.srcColorBlendFactor = tgfx::BlendFactor::One;
  colorAttachment.dstColorBlendFactor = tgfx::BlendFactor::OneMinusSrcAlpha;
  colorAttachment.srcAlphaBlendFactor = tgfx::BlendFactor::One;
  colorAttachment.dstAlphaBlendFactor = tgfx::BlendFactor::OneMinusSrcAlpha;
  descriptor.fragment.colorAttachments.push_back(colorAttachment);
  descriptor.layout.textureSamplers = textureSamplers();
  descriptor.layout.uniformBlocks = uniformBlocks();
  onConfigurePipeline(&descriptor);
  return gpu->createRenderPipeline(descriptor);
}

void RuntimeFilter::onConfigurePipeline(tgfx::RenderPipelineDescriptor*) const {
}

std::unique_ptr<FilterResources> RuntimeFilter::onCreateFilterResources() const {
  return std::make_unique<FilterResources>();
}

void RuntimeFilter::onConfigureRenderPass(tgfx::RenderPassDescriptor*, FilterResources*, tgfx::GPU*,
                                          const std::shared_ptr<tgfx::Texture>&) const {
}

std::vector<float> RuntimeFilter::computeVertices(const tgfx::Texture* source,
                                                  const tgfx::Texture* target,
                                                  const tgfx::Point& offset) const {
  auto inputBounds = tgfx::Rect::MakeWH(source->width(), source->height());
  auto targetBounds = filterBounds(inputBounds);
  tgfx::Point contentPoint[4] = {{targetBounds.left, targetBounds.bottom},
                                 {targetBounds.right, targetBounds.bottom},
                                 {targetBounds.left, targetBounds.top},
                                 {targetBounds.right, targetBounds.top}};
  tgfx::Point texturePoints[4] = {{inputBounds.left, inputBounds.bottom},
                                  {inputBounds.right, inputBounds.bottom},
                                  {inputBounds.left, inputBounds.top},
                                  {inputBounds.right, inputBounds.top}};

  std::vector<float> vertices = {};
  vertices.reserve(16);
  for (size_t i = 0; i < 4; i++) {
    auto vertexPoint = ToVertexPoint(target, contentPoint[i] + offset);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToTexturePoint(source, texturePoints[i]);
    vertices.push_back(texturePoint.x);
    vertices.push_back(texturePoint.y);
  }
  return vertices;
}

void RuntimeFilter::onUpdateUniforms(tgfx::RenderPass*, tgfx::GPU*,
                                     const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                     const tgfx::Point&) const {
}

std::shared_ptr<tgfx::RenderPipeline> RuntimeFilter::getPipeline(tgfx::GPU* gpu) const {
  auto resources = getFilterResources(gpu);
  if (resources == nullptr) {
    return nullptr;
  }
  return resources->pipeline;
}

FilterResources* RuntimeFilter::getFilterResources(tgfx::GPU* gpu) const {
  auto type = filterType();
  auto resources = cache->findFilterResources(type);
  if (resources == nullptr) {
    auto pipeline = createPipeline(gpu);
    if (pipeline == nullptr) {
      return nullptr;
    }
    tgfx::SamplerDescriptor samplerDesc(tgfx::AddressMode::ClampToEdge,
                                        tgfx::AddressMode::ClampToEdge, tgfx::FilterMode::Linear,
                                        tgfx::FilterMode::Linear, tgfx::MipmapMode::None);
    auto sampler = gpu->createSampler(samplerDesc);
    auto newResources = onCreateFilterResources();
    newResources->pipeline = std::move(pipeline);
    newResources->sampler = std::move(sampler);
    resources = newResources.get();
    cache->addFilterResources(type, std::move(newResources));
  }
  DEBUG_ASSERT(resources->pipeline != nullptr);
  return resources;
}

bool RuntimeFilter::onDraw(tgfx::CommandEncoder* encoder,
                           const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                           std::shared_ptr<tgfx::Texture> outputTexture,
                           const tgfx::Point& offset) const {
  if (inputTextures.empty() || outputTexture == nullptr || encoder == nullptr) {
    LOGE("RuntimeFilter::onDraw() invalid arguments");
    return false;
  }

  auto gpu = encoder->gpu();
  auto resources = getFilterResources(gpu);
  if (resources == nullptr) {
    LOGE("RuntimeFilter::onDraw() failed to get resources or pipeline");
    return false;
  }

  auto msaaSampleCount = sampleCount();
  std::shared_ptr<tgfx::Texture> renderTexture = nullptr;
  if (msaaSampleCount > 1) {
    tgfx::TextureDescriptor textureDesc(outputTexture->width(), outputTexture->height(),
                                        outputTexture->format(), false, msaaSampleCount,
                                        tgfx::TextureUsage::RENDER_ATTACHMENT);
    renderTexture = gpu->createTexture(textureDesc);
    if (renderTexture == nullptr) {
      renderTexture = outputTexture;
      msaaSampleCount = 1;
    }
  } else {
    renderTexture = outputTexture;
  }

  tgfx::RenderPassDescriptor renderPassDesc;
  if (msaaSampleCount > 1) {
    renderPassDesc =
        tgfx::RenderPassDescriptor(renderTexture, tgfx::LoadAction::Clear, tgfx::StoreAction::Store,
                                   tgfx::PMColor::Transparent(), outputTexture);
  } else {
    renderPassDesc = tgfx::RenderPassDescriptor(renderTexture, tgfx::LoadAction::Clear,
                                                tgfx::StoreAction::Store);
  }
  onConfigureRenderPass(&renderPassDesc, resources, gpu, outputTexture);

  auto renderPass = encoder->beginRenderPass(renderPassDesc);
  if (renderPass == nullptr) {
    LOGE("RuntimeFilter::onDraw() failed to begin render pass");
    return false;
  }

  renderPass->setPipeline(resources->pipeline);

  auto vertices = computeVertices(inputTextures[0].get(), outputTexture.get(), offset);
  auto vertexBuffer =
      gpu->createBuffer(vertices.size() * sizeof(float), tgfx::GPUBufferUsage::VERTEX);
  if (vertexBuffer == nullptr) {
    LOGE("RuntimeFilter::onDraw() failed to create vertex buffer");
    renderPass->end();
    return false;
  }

  auto data = vertexBuffer->map();
  if (data == nullptr) {
    LOGE("RuntimeFilter::onDraw() failed to map vertex buffer");
    renderPass->end();
    return false;
  }
  memcpy(data, vertices.data(), vertices.size() * sizeof(float));
  vertexBuffer->unmap();

  renderPass->setVertexBuffer(vertexBuffer);
  renderPass->setTexture(0, inputTextures[0], resources->sampler);

  for (size_t i = 1; i < inputTextures.size(); i++) {
    renderPass->setTexture(static_cast<unsigned>(i), inputTextures[i], resources->sampler);
  }

  onUpdateUniforms(renderPass.get(), gpu, inputTextures, offset);

  renderPass->draw(tgfx::PrimitiveType::TriangleStrip, 0, vertexCount());
  renderPass->end();
  return true;
}

}  // namespace pag
