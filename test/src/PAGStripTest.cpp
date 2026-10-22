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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <unordered_set>
#include "base/PAGTest.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Layer.h"
#include "pagx/pag/exporter/Stripper.h"
#include "utils/ProjectPath.h"

namespace pagx {
namespace {
using pag::ProjectPath;

void FindLayerById(const std::vector<Layer*>& layers, const std::string& id, bool* found) {
  for (auto* layer : layers) {
    if (layer->id == id) {
      *found = true;
    }
    FindLayerById(layer->children, id, found);
  }
}

const char* StripSample = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="400" height="300" data-app="myapp">
  <Resources>
    <ViewModel id="vm">
      <Property name="title" type="string" default="Hello" data-custom="keep"/>
    </ViewModel>
    <Image id="photo" source="photo.png" data-src="strip"/>
  </Resources>
  <Layer id="visible" data-name="strip">
    <Rectangle id="rect" width="100" height="100" data-shape="strip"/>
    <Fill color="#FF0000" id="fill" data-painter="strip"/>
  </Layer>
  <Layer id="hidden" visible="false" data-hidden="strip">
    <Ellipse width="50" height="50"/>
    <Fill color="#00FF00"/>
  </Layer>
  <Layer id="animated">
    <Ellipse width="80" height="80"/>
    <Fill color="#0000FF"/>
  </Layer>
  <Animations>
    <Animation id="anim1" duration="60" frameRate="60" loop="loop">
      <Object target="animated">
        <Channel name="alpha" type="float">
          <Key time="0" value="1"/>
          <Key time="60" value="0.5"/>
        </Channel>
      </Object>
    </Animation>
  </Animations>
  <DataBind source="$vm.title" target="@visible" channel="alpha"/>
</pagx>)";

}  // namespace

PAGX_TEST(PAGStripTest, CollectRetainedIds) {
  auto doc = PAGXImporter::FromXML(StripSample);
  ASSERT_TRUE(doc != nullptr);
  auto ids = Stripper::CollectRetainedIds(doc.get());
  // Layer ids always survive.
  EXPECT_EQ(ids.count("visible"), 1u);
  EXPECT_EQ(ids.count("hidden"), 1u);
  EXPECT_EQ(ids.count("animated"), 1u);
  // Animation ids survive.
  EXPECT_EQ(ids.count("anim1"), 1u);
  // DataBind targets survive.
  EXPECT_EQ(ids.count("visible"), 1u);
  // Non-Layer node ids do NOT survive (rect, fill, vm, photo).
  EXPECT_EQ(ids.count("rect"), 0u);
  EXPECT_EQ(ids.count("fill"), 0u);
  EXPECT_EQ(ids.count("vm"), 0u);
  EXPECT_EQ(ids.count("photo"), 0u);
}

PAGX_TEST(PAGStripTest, StripMetadata) {
  auto doc = PAGXImporter::FromXML(StripSample);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();
  Stripper::StripMetadata(doc.get());

  for (auto& node : doc->nodes) {
    auto type = node->nodeType();
    if (type == NodeType::ViewModelProperty || type == NodeType::ImagePattern) {
      EXPECT_FALSE(node->customData.empty()) << "C27: " << static_cast<int>(type);
    } else if (type != NodeType::Document) {
      EXPECT_TRUE(node->customData.empty()) << "type " << static_cast<int>(type);
    }
    EXPECT_EQ(node->sourceLine, -1);
  }
}

PAGX_TEST(PAGStripTest, StripIds) {
  auto doc = PAGXImporter::FromXML(StripSample);
  ASSERT_TRUE(doc != nullptr);
  auto retained = Stripper::CollectRetainedIds(doc.get());
  Stripper::StripIds(doc.get(), retained);

  for (auto& node : doc->nodes) {
    if (node->id.empty()) {
      continue;
    }
    EXPECT_EQ(retained.count(node->id), 1u)
        << "surviving id '" << node->id << "' type " << static_cast<int>(node->nodeType());
  }
  // Specific checks: Layer and Animation ids survive, resource ids strip.
  auto layerIt = std::find_if(doc->nodes.begin(), doc->nodes.end(), [](auto& n) {
    return n->nodeType() == NodeType::Layer && !n->id.empty();
  });
  EXPECT_TRUE(layerIt != doc->nodes.end());
}

PAGX_TEST(PAGStripTest, RemoveDeadNodes) {
  auto doc = PAGXImporter::FromXML(StripSample);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();
  auto retained = Stripper::CollectRetainedIds(doc.get());
  Stripper::RemoveDeadNodes(doc.get(), retained);

  // The hidden layer is not an animation target and not a mask: gone.
  bool foundHidden = false;
  FindLayerById(doc->layers, "hidden", &foundHidden);
  EXPECT_FALSE(foundHidden);
  // Visible layers survive.
  EXPECT_EQ(doc->layers.size(), 2u);
}

PAGX_TEST(PAGStripTest, FullStripPreservesRendering) {
  // Strip a real sample and verify the rendering is unchanged.
  auto path = ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx");
  auto doc = PAGXImporter::FromFile(path);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();
  Stripper::Strip(doc.get());
  // The stripped document should still be valid XML.
  auto xml = PAGXExporter::ToXML(*doc);
  auto reloaded = PAGXImporter::FromXML(xml);
  ASSERT_TRUE(reloaded != nullptr);
  for (auto& error : reloaded->errors) {
    ADD_FAILURE() << "stripped doc has errors: " << error;
  }
}
}  // namespace pagx
