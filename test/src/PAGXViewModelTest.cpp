#include <sstream>
#include "base/PAGTest.h"
#include "pagx/DataBindRuntime.h"
#include "pagx/DataContext.h"
#include "pagx/DataConverterRegistry.h"
#include "pagx/ObserverHandle.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGViewModel.h"
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueColor.h"
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PAGViewModelValueViewModel.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PropertyData.h"
#include "pagx/SuppressDelegation.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/DataConverter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pag {

static std::shared_ptr<pagx::PAGScene> MakeScene(pagx::PAGXDocument* doc, const std::string& vmId,
                                                 int propCount) {
  auto* schema = doc->makeNode<pagx::ViewModel>(vmId);
  auto add = [&](const std::string& name, pagx::ViewModelPropertyType type) {
    auto* p = doc->makeNode<pagx::ViewModelProperty>();
    p->name = name;
    p->propertyType = type;
    schema->properties.push_back(p);
    return p;
  };
  if (propCount >= 1) add("name", pagx::ViewModelPropertyType::String);
  if (propCount >= 2) {
    auto* p = add("score", pagx::ViewModelPropertyType::Number);
    p->defaultNumber = 100.0f;
  }
  if (propCount >= 3) {
    auto* p = add("visible", pagx::ViewModelPropertyType::Boolean);
    p->defaultBoolean = true;
  }
  if (propCount >= 4) add("color", pagx::ViewModelPropertyType::Color);
  if (propCount >= 5) add("image", pagx::ViewModelPropertyType::Image);
  doc->viewModel = schema;
  return pagx::PAGScene::Make(std::shared_ptr<pagx::PAGXDocument>(doc, [](pagx::PAGXDocument*) {}));
}

// ========== ToTarget ==========
PAGX_TEST(PAGXViewModelTest, NumberPropertyReadWrite) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto score = vm->propertyNumber("score");
  ASSERT_NE(score, nullptr);
  EXPECT_FLOAT_EQ(score->value(), 100.0f);
  score->value(200.0f);
  EXPECT_FLOAT_EQ(score->value(), 200.0f);
}
PAGX_TEST(PAGXViewModelTest, StringPropertyReadWrite) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 1);
  auto vm = scene->viewModel();
  auto name = vm->propertyString("name");
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->value(), "");
  name->value("hello");
  EXPECT_EQ(name->value(), "hello");
}
PAGX_TEST(PAGXViewModelTest, BooleanPropertyReadWrite) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 3);
  auto vm = scene->viewModel();
  auto vis = vm->propertyBoolean("visible");
  ASSERT_NE(vis, nullptr);
  EXPECT_EQ(vis->value(), true);
  vis->value(false);
  EXPECT_EQ(vis->value(), false);
}
PAGX_TEST(PAGXViewModelTest, ColorPropertyReadWrite) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 4);
  auto vm = scene->viewModel();
  auto color = vm->propertyColor("color");
  ASSERT_NE(color, nullptr);
  pagx::Color red = {1.0f, 0.0f, 0.0f, 1.0f};
  color->value(red);
  EXPECT_FLOAT_EQ(color->value().red, 1.0f);
}
PAGX_TEST(PAGXViewModelTest, ImagePropertyReadWrite) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 5);
  auto vm = scene->viewModel();
  auto img = vm->propertyImage("image");
  ASSERT_NE(img, nullptr);
  EXPECT_EQ(img->value(), "");
  img->value("assets/hero.png");
  EXPECT_EQ(img->value(), "assets/hero.png");
}
PAGX_TEST(PAGXViewModelTest, SameValueNoOp) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  score->value(100.0f);
  int c = 0;
  auto h = score->addObserver([&]() { c++; });
  score->value(100.0f);
  EXPECT_EQ(c, 0);
  score->value(200.0f);
  EXPECT_EQ(c, 1);
}
PAGX_TEST(PAGXViewModelTest, SameColorValueNoOp) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 4);
  auto vm = scene->viewModel();
  auto color = vm->propertyColor("color");
  pagx::Color c2 = {0.5f, 0.5f, 0.5f, 1.0f};
  color->value(c2);
  int c = 0;
  auto h = color->addObserver([&]() { c++; });
  color->value(c2);
  EXPECT_EQ(c, 0);
  c2.red = 0.6f;
  color->value(c2);
  EXPECT_EQ(c, 1);
}
PAGX_TEST(PAGXViewModelTest, TypeMismatchReturnsNull) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  EXPECT_EQ(vm->propertyNumber("name"), nullptr);
}
PAGX_TEST(PAGXViewModelTest, MissingPropertyReturnsNull) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  EXPECT_EQ(vm->propertyNumber("nope"), nullptr);
}
PAGX_TEST(PAGXViewModelTest, NoSchemaReturnsNullViewModel) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  EXPECT_EQ(scene->viewModel(), nullptr);
}
PAGX_TEST(PAGXViewModelTest, PropertiesReflection) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 5);
  auto vm = scene->viewModel();
  auto props = vm->properties();
  ASSERT_EQ(props.size(), 5u);
}

// ========== Observer ==========
PAGX_TEST(PAGXViewModelTest, ObserverCallbackOnChange) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  int c = 0;
  auto h = score->addObserver([&]() { c++; });
  score->value(150.0f);
  EXPECT_EQ(c, 1);
  score->value(200.0f);
  EXPECT_EQ(c, 2);
}

// ========== Suppress ==========
PAGX_TEST(PAGXViewModelTest, SuppressDelegationDeduplicatesSameProperty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "score";
  p->propertyType = pagx::ViewModelPropertyType::Number;
  s->properties.push_back(p);
  doc->viewModel = s;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  int c = 0;
  auto h = score->addObserver([&]() { c++; });
  score->value(10.0f);
  EXPECT_EQ(c, 1);
  c = 0;
  {
    pagx::SuppressDelegation g(scene);
    score->value(1.0f);
    score->value(2.0f);
    score->value(3.0f);
  }
  EXPECT_EQ(c, 1);
}
PAGX_TEST(PAGXViewModelTest, SuppressDelegationStillMarksDirty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "score";
  p->propertyType = pagx::ViewModelPropertyType::Number;
  s->properties.push_back(p);
  doc->viewModel = s;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  {
    pagx::SuppressDelegation g(scene);
    score->value(1.0f);
    EXPECT_TRUE(score->hasChanged());
  }
  EXPECT_TRUE(score->hasChanged());
}

// ========== DataContext ==========
PAGX_TEST(PAGXViewModelTest, DataContextResolveFlatPath) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "speed";
  p->propertyType = pagx::ViewModelPropertyType::Number;
  p->defaultNumber = 1.0f;
  s->properties.push_back(p);
  doc->viewModel = s;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto ctx = std::make_shared<pagx::DataContext>(scene->viewModel());
  ASSERT_NE(ctx->resolve({"$vm", "speed"}), nullptr);
}
PAGX_TEST(PAGXViewModelTest, DataContextParentChainFallback) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* s = doc->makeNode<pagx::ViewModel>("P");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "theme";
  p->propertyType = pagx::ViewModelPropertyType::String;
  p->defaultString = "dark";
  s->properties.push_back(p);
  doc->viewModel = s;
  auto parentScene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto childVM = std::make_shared<pagx::PAGViewModel>();
  auto ctx = std::make_shared<pagx::DataContext>(childVM);
  ctx->parent(std::make_shared<pagx::DataContext>(parentScene->viewModel()));
  ASSERT_NE(ctx->resolve({"$vm", "theme"}), nullptr);
}
PAGX_TEST(PAGXViewModelTest, DataContextMissingPropertyReturnsNull) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto ctx = std::make_shared<pagx::DataContext>(scene->viewModel());
  EXPECT_EQ(ctx->resolve({"$vm", "nope"}), nullptr);
}

// ========== ValueType ==========
PAGX_TEST(PAGXViewModelTest, ValueTypeEnum) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 5);
  auto vm = scene->viewModel();
  auto props = vm->properties();
  EXPECT_EQ(props[0]->valueType(), pagx::ViewModelValueType::String);
  EXPECT_EQ(props[1]->valueType(), pagx::ViewModelValueType::Number);
}

// ========== XML ==========
static std::string VMXml(const std::string& resources, const std::string& body = "") {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<pagx width=\"400\" height=\"300\" "
         "viewModel=\"@MainVM\">\n  <Resources>\n" +
         resources + "  </Resources>\n" + body + "</pagx>\n";
}
PAGX_TEST(PAGXViewModelTest, ViewModelXMLImport) {
  std::string xml = VMXml(
      "    <ViewModel id=\"MainVM\">\n      <Property name=\"title\" type=\"String\" "
      "default=\"Hello\"/>\n    </ViewModel>\n");
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->viewModel, nullptr);
  EXPECT_EQ(doc->viewModel->id, "MainVM");
}
PAGX_TEST(PAGXViewModelTest, DataBindXMLImport) {
  std::string xml = VMXml(
      "    <ViewModel id=\"MainVM\">\n      <Property name=\"title\" type=\"String\"/>\n    "
      "</ViewModel>\n    <Composition id=\"Main\" width=\"400\" height=\"300\">\n      <Layer "
      "id=\"textLayer\" name=\"Text\"/>\n      <DataBind source=\"$vm.title\" "
      "target=\"@textLayer\" channel=\"text\"/>\n    </Composition>\n");
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  auto* comp = doc->findNode<pagx::Composition>("Main");
  ASSERT_NE(comp, nullptr);
  EXPECT_EQ(comp->dataBinds.size(), 1u);
}
PAGX_TEST(PAGXViewModelTest, DataBindXMLRoundTrip) {
  std::string xml = VMXml(
      "    <ViewModel id=\"MainVM\">\n      <Property name=\"title\" type=\"String\"/>\n    "
      "</ViewModel>\n    <Composition id=\"Main\" width=\"400\" height=\"300\">\n      <Layer "
      "id=\"textLayer\" name=\"Text\"/>\n      <DataBind source=\"$vm.title\" "
      "target=\"@textLayer\" channel=\"text\"/>\n    </Composition>\n");
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  pagx::PAGXExporter::ToXML(*doc);
}

PAGX_TEST(PAGXViewModelTest, HasChangedFlag) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  EXPECT_FALSE(score->hasChanged());
  score->value(200.0f);
  EXPECT_TRUE(score->hasChanged());
  score->advanced();
  EXPECT_FALSE(score->hasChanged());
}

PAGX_TEST(PAGXViewModelTest, ManualVMConstruction) {
  auto vm = std::make_shared<pagx::PAGViewModel>();
  auto val = std::make_shared<pagx::PAGViewModelValueNumber>();
  val->propertyName = "x";
  val->propertyValue = 5.0f;
  val->type = pagx::ViewModelValueType::Number;
  vm->propertyMap["x"] = val;
  EXPECT_EQ(vm->propertyNumber("x")->value(), 5.0f);
}

// ========== DataBind pipeline ==========
PAGX_TEST(PAGXViewModelTest, DataBindAlphaSyncVerified) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  color->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 1.0f);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  vm->propertyNumber("alpha")->value(0.3f);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
}

PAGX_TEST(PAGXViewModelTest, DataBindPositionSyncVerified) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "x";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 0.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 100;
  layer->height = 100;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.x";
  db->target = "@rect";
  db->channel = "x";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto layers = scene->getLayersUnderPoint(50, 50);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  EXPECT_FLOAT_EQ(tgfxLayer->matrix().getTranslateX(), 0.0f);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  vm->propertyNumber("x")->value(50.0f);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->matrix().getTranslateX(), 50.0f);
}

// ========== Rendering ==========
PAGX_TEST(PAGXViewModelTest, RenderWithViewModel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* np = doc->makeNode<pagx::ViewModelProperty>();
  np->name = "bgRed";
  np->propertyType = pagx::ViewModelPropertyType::Number;
  np->defaultNumber = 0.2f;
  schema->properties.push_back(np);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("bg");
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.2f, 0.2f, 0.8f, 1.0f};
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->viewModel(), nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
}

PAGX_TEST(PAGXViewModelTest, RenderWithSuppressDelegation) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* np = doc->makeNode<pagx::ViewModelProperty>();
  np->name = "alpha";
  np->propertyType = pagx::ViewModelPropertyType::Number;
  np->defaultNumber = 1.0f;
  schema->properties.push_back(np);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("bg");
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  int obs = 0;
  auto h = vm->propertyNumber("alpha")->addObserver([&]() { obs++; });
  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_NE(surface, nullptr);
  {
    pagx::SuppressDelegation g(scene);
    vm->propertyNumber("alpha")->value(0.5f);
    EXPECT_TRUE(scene->draw(surface));
  }
  EXPECT_EQ(obs, 1);
}

// ========== TwoWay sync ==========
PAGX_TEST(PAGXViewModelTest, TwoWaySyncAnimationToViewModel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "rect";
  anim->objects.push_back(object);
  auto* aChan = doc->makeNode<pagx::TypedChannel<float>>();
  aChan->name = "alpha";
  aChan->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  aChan->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  object->channels.push_back(aChan);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto alphaProp = vm->propertyNumber("alpha");
  ASSERT_NE(alphaProp, nullptr);
  EXPECT_FLOAT_EQ(alphaProp->value(), 1.0f);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_NE(timeline, nullptr);
  timeline->setCurrentTime(500000);
  timeline->apply();
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_NEAR(alphaProp->value(), 0.5f, 0.01f);
}

PAGX_TEST(PAGXViewModelTest, TwoWaySyncNotifiesObserverOnce) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto anim1 = doc->makeNode<pagx::Animation>("a1");
  anim1->duration = 60;
  anim1->frameRate = 60;
  doc->animations.push_back(anim1);
  auto* o1 = doc->makeNode<pagx::AnimationObject>();
  o1->target = "rect";
  anim1->objects.push_back(o1);
  auto* c1 = doc->makeNode<pagx::TypedChannel<float>>();
  c1->name = "alpha";
  c1->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  c1->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  o1->channels.push_back(c1);
  auto anim2 = doc->makeNode<pagx::Animation>("a2");
  anim2->duration = 60;
  anim2->frameRate = 60;
  doc->animations.push_back(anim2);
  auto* o2 = doc->makeNode<pagx::AnimationObject>();
  o2->target = "rect";
  anim2->objects.push_back(o2);
  auto* c2 = doc->makeNode<pagx::TypedChannel<float>>();
  c2->name = "alpha";
  c2->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::None, {}, {}});
  c2->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  o2->channels.push_back(c2);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto alphaProp = vm->propertyNumber("alpha");
  ASSERT_NE(alphaProp, nullptr);
  int obs = 0;
  auto h = alphaProp->addObserver([&]() { obs++; });
  EXPECT_EQ(obs, 0);
  auto t1 = scene->getTimeline("a1");
  ASSERT_NE(t1, nullptr);
  auto t2 = scene->getTimeline("a2");
  ASSERT_NE(t2, nullptr);
  int64_t f30 = 500000;
  t1->setCurrentTime(f30);
  t1->apply();
  t2->setCurrentTime(f30);
  t2->apply();
  EXPECT_EQ(obs, 0);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 1);
  obs = 0;
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 0);
}

// ========== animation → VM sync ==========
PAGX_TEST(PAGXViewModelTest, SyncsAnimationToVM) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("r");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "r";
  anim->objects.push_back(object);
  auto* aChan = doc->makeNode<pagx::TypedChannel<float>>();
  aChan->name = "alpha";
  aChan->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  aChan->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  object->channels.push_back(aChan);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@r";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto alphaProp = vm->propertyNumber("alpha");
  ASSERT_NE(alphaProp, nullptr);
  EXPECT_FLOAT_EQ(alphaProp->value(), 1.0f);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_NE(timeline, nullptr);
  timeline->setCurrentTime(500000);
  timeline->apply();
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_NEAR(alphaProp->value(), 0.5f, 0.01f);
}

PAGX_TEST(PAGXViewModelTest, EmptyViewModelPropertiesReturnsEmpty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("EmptyVM");
  doc->viewModel = schema;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto props = vm->properties();
  EXPECT_EQ(props.size(), 0u);
  EXPECT_TRUE(vm->properties().empty());
}

PAGX_TEST(PAGXViewModelTest, ObserverHandleRAIIAutoDetach) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  int c = 0;
  {
    auto h = score->addObserver([&]() { c++; });
    score->value(150.0f);
    EXPECT_EQ(c, 1);
  }
  score->value(200.0f);
  EXPECT_EQ(c, 1);
}

PAGX_TEST(PAGXViewModelTest, ObserverRecursivePrevention) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* pA = doc->makeNode<pagx::ViewModelProperty>();
  pA->name = "a";
  pA->propertyType = pagx::ViewModelPropertyType::Number;
  pA->defaultNumber = 0.0f;
  schema->properties.push_back(pA);
  auto* pB = doc->makeNode<pagx::ViewModelProperty>();
  pB->name = "b";
  pB->propertyType = pagx::ViewModelPropertyType::Number;
  pB->defaultNumber = 0.0f;
  schema->properties.push_back(pB);
  doc->viewModel = schema;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto propA = vm->propertyNumber("a");
  auto propB = vm->propertyNumber("b");
  int countA = 0, countB = 0;
  auto hA = propA->addObserver([&]() {
    countA++;
    propB->value(1.0f);
  });
  auto hB = propB->addObserver([&]() { countB++; });
  propA->value(1.0f);
  EXPECT_EQ(countA, 1);
  EXPECT_EQ(countB, 1);
  // Same-value recursion: delegate that modifies A itself is blocked.
  countA = 0;
  hA.detach();
  auto hA2 = propA->addObserver([&]() {
    countA++;
    propA->value(2.0f);
  });
  propA->value(3.0f);
  EXPECT_EQ(countA, 1);
}

// ========== notifyDependents: all value types trigger DataBind dirty ==========
PAGX_TEST(PAGXViewModelTest, StringValueTriggersDataBindSync) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "name";
  p->propertyType = pagx::ViewModelPropertyType::String;
  p->defaultString = "A";
  s->properties.push_back(p);
  doc->viewModel = s;
  auto l = doc->makeNode<pagx::Layer>("r");
  l->width = 200;
  l->height = 200;
  auto r = doc->makeNode<pagx::Rectangle>();
  r->size.width = 200;
  r->size.height = 200;
  auto f = doc->makeNode<pagx::Fill>();
  auto c = doc->makeNode<pagx::SolidColor>();
  f->color = c;
  auto g = doc->makeNode<pagx::Group>();
  g->elements.push_back(r);
  g->elements.push_back(f);
  l->contents.push_back(g);
  doc->layers.push_back(l);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.name";
  db->target = "@r";
  db->channel = "name";
  doc->dataBinds.push_back(db);
  auto sc = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(sc, nullptr);
  auto vm = sc->viewModel();
  ASSERT_NE(vm, nullptr);
  auto v = vm->propertyString("name");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->value(), "A");
  v->value("B");
  auto sf = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(sf, nullptr);
  EXPECT_TRUE(sc->draw(sf));
  auto layers = sc->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  EXPECT_EQ(layers[0]->runtimeLayer->name(), "B");
}

PAGX_TEST(PAGXViewModelTest, BooleanValueTriggersDataBindSync) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "vis";
  p->propertyType = pagx::ViewModelPropertyType::Boolean;
  p->defaultBoolean = true;
  s->properties.push_back(p);
  doc->viewModel = s;
  auto l = doc->makeNode<pagx::Layer>("r");
  l->width = 200;
  l->height = 200;
  auto r = doc->makeNode<pagx::Rectangle>();
  r->size.width = 200;
  r->size.height = 200;
  auto f = doc->makeNode<pagx::Fill>();
  auto c = doc->makeNode<pagx::SolidColor>();
  f->color = c;
  auto g = doc->makeNode<pagx::Group>();
  g->elements.push_back(r);
  g->elements.push_back(f);
  l->contents.push_back(g);
  doc->layers.push_back(l);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.vis";
  db->target = "@r";
  db->channel = "visible";
  doc->dataBinds.push_back(db);
  auto sc = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(sc, nullptr);
  auto vm = sc->viewModel();
  ASSERT_NE(vm, nullptr);
  auto v = vm->propertyBoolean("vis");
  ASSERT_NE(v, nullptr);
  auto layers = sc->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  EXPECT_EQ(v->value(), true);
  EXPECT_EQ(tgfxLayer->visible(), true);
  v->value(false);
  auto sf = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(sf, nullptr);
  EXPECT_TRUE(sc->draw(sf));
  EXPECT_EQ(tgfxLayer->visible(), false);
}

PAGX_TEST(PAGXViewModelTest, ColorValueTriggersDataBindSync) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "bg";
  p->propertyType = pagx::ViewModelPropertyType::Color;
  p->defaultColor = pagx::Color{1, 0, 0, 1};
  s->properties.push_back(p);
  doc->viewModel = s;
  auto l = doc->makeNode<pagx::Layer>("r");
  l->width = 200;
  l->height = 200;
  auto r = doc->makeNode<pagx::Rectangle>();
  r->size.width = 200;
  r->size.height = 200;
  auto f = doc->makeNode<pagx::Fill>();
  auto c = doc->makeNode<pagx::SolidColor>("fillColor");
  f->color = c;
  auto g = doc->makeNode<pagx::Group>();
  g->elements.push_back(r);
  g->elements.push_back(f);
  l->contents.push_back(g);
  doc->layers.push_back(l);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.bg";
  db->target = "@fillColor";
  db->channel = "color";
  doc->dataBinds.push_back(db);
  auto sc = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(sc, nullptr);
  auto vm = sc->viewModel();
  ASSERT_NE(vm, nullptr);
  auto v = vm->propertyColor("bg");
  ASSERT_NE(v, nullptr);
  EXPECT_FLOAT_EQ(v->value().red, 1.0f);
  v->value(pagx::Color{0, 1, 0, 1});
  auto sf = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(sf, nullptr);
  EXPECT_TRUE(sc->draw(sf));
  std::array<uint8_t, 200 * 200 * 4> pixels = {};
  ASSERT_TRUE(sf->readPixels(pixels.data(), 200 * 4));
  auto& px = *reinterpret_cast<uint32_t*>(pixels.data() + (100 * 200 + 100) * 4);
  EXPECT_EQ((px >> 8) & 0xFF, 255u);
  EXPECT_EQ(px & 0xFF, 0u);
}

PAGX_TEST(PAGXViewModelTest, ImageValueTriggersDataBindSync) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "img";
  p->propertyType = pagx::ViewModelPropertyType::Image;
  p->defaultImage = "a.png";
  s->properties.push_back(p);
  doc->viewModel = s;
  auto l = doc->makeNode<pagx::Layer>("r");
  l->width = 200;
  l->height = 200;
  auto r = doc->makeNode<pagx::Rectangle>();
  r->size.width = 200;
  r->size.height = 200;
  auto f = doc->makeNode<pagx::Fill>();
  auto c = doc->makeNode<pagx::SolidColor>();
  f->color = c;
  auto g = doc->makeNode<pagx::Group>();
  g->elements.push_back(r);
  g->elements.push_back(f);
  l->contents.push_back(g);
  doc->layers.push_back(l);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.img";
  db->target = "@r";
  db->channel = "image";
  doc->dataBinds.push_back(db);
  auto sc = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(sc, nullptr);
  auto vm = sc->viewModel();
  ASSERT_NE(vm, nullptr);
  auto v = vm->propertyImage("img");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->value(), "a.png");
  v->value("b.png");
  auto sf = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(sf, nullptr);
  EXPECT_TRUE(sc->draw(sf));
  EXPECT_EQ(v->value(), "b.png");
  // TODO: "image" writer is not yet implemented. The plan is to use
  // tgfx::ImagePattern::setImage() (already added to tgfx) to swap the runtime image when the VM
  // image property changes. This requires DataBindRuntime to load a tgfx::Image from the VM string
  // value (file path or resource id) and call pattern->setImage() on the target. Tracked as a
  // follow-up task.
}

// ========== DataConverter runtime ==========

PAGX_TEST(PAGXViewModelTest, DataConverterSecondsToFrames) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  // Setup converter
  auto* conv = doc->makeNode<pagx::DataConverter>("c1");
  conv->converterType = "secondsToFrames";
  conv->params["frameRate"] = "60";

  // ViewModel property with converter
  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "duration";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  prop->dataConverter = conv;
  schema->properties.push_back(prop);
  doc->viewModel = schema;

  auto layer = doc->makeNode<pagx::Layer>("r");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.duration";
  db->target = "@r";
  db->channel = "x";
  doc->dataBinds.push_back(db);

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto propV = vm->propertyNumber("duration");
  ASSERT_NE(propV, nullptr);
  EXPECT_FLOAT_EQ(propV->value(), 1.0f);

  // Set duration to 2.0 seconds → converter outputs 120.0 frames (2.0 * 60).
  propV->value(2.0f);

  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));

  // Verify converter was applied by checking the converter registry directly.
  auto& registry = pagx::DataConverterRegistry::instance();
  pagx::KeyValue input{2.0f};
  auto output = registry.apply(conv, input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 120.0f);

  // Also verify the draw pipeline doesn't crash with converter applied.
  auto surface2 = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface2, nullptr);
  EXPECT_TRUE(scene->draw(surface2));
}

PAGX_TEST(PAGXViewModelTest, DataConverterPriceFormat) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto* conv = doc->makeNode<pagx::DataConverter>("c1");
  conv->converterType = "priceFormat";
  conv->params["prefix"] = "$";
  conv->params["decimals"] = "2";

  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "price";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 0.0f;
  prop->dataConverter = conv;
  schema->properties.push_back(prop);
  doc->viewModel = schema;

  auto layer = doc->makeNode<pagx::Layer>("r");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>();
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.price";
  db->target = "@r";
  db->channel = "name";
  doc->dataBinds.push_back(db);

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto propV = vm->propertyNumber("price");
  ASSERT_NE(propV, nullptr);
  propV->value(5999.0f);

  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  // Converter output is "$5999.00" (string), applied to the "name" channel.
  auto& registry = pagx::DataConverterRegistry::instance();
  pagx::KeyValue priceInput{5999.0f};
  auto priceOutput = registry.apply(conv, priceInput);
  ASSERT_TRUE(std::holds_alternative<std::string>(priceOutput));
  EXPECT_EQ(std::get<std::string>(priceOutput), "$5999.00");
  auto layers = scene->getLayersUnderPoint(100, 100);
  EXPECT_GT(layers.size(), 0u);
}

PAGX_TEST(PAGXViewModelTest, DataConverterNotFoundPassthrough) {
  // Non-existent converter → value passes through unchanged.
  pagx::KeyValue input{42.0f};
  auto output = pagx::DataConverterRegistry::instance().apply(nullptr, input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 42.0f);
}

PAGX_TEST(PAGXViewModelTest, DataConverterInverseSecondsToFrames) {
  auto conv = std::unique_ptr<pagx::DataConverter>(new pagx::DataConverter());
  conv->converterType = "secondsToFrames";
  conv->params["frameRate"] = "60";

  pagx::KeyValue input{120.0f};
  auto output = pagx::DataConverterRegistry::instance().applyInverse(conv.get(), input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 2.0f);
}

PAGX_TEST(PAGXViewModelTest, DataConverterInversePassthrough) {
  // No registered inverse → value unchanged.
  pagx::KeyValue input{99.0f};
  auto output = pagx::DataConverterRegistry::instance().applyInverse(nullptr, input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 99.0f);
}

// ========== DataBindRuntime null dataBind safety ==========

PAGX_TEST(PAGXViewModelTest, DataBindRuntimeNullDataBindSafety) {
  // Verify that update() does not crash when an entry's dataBind becomes null
  // (e.g. after the owning PAGXDocument is destroyed before the runtime).
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "val";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("r");
  layer->width = 200;
  layer->height = 200;
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.val";
  db->target = "@r";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);

  auto runtime = std::make_unique<pagx::DataBindRuntime>();
  auto ctx = scene->viewModel() != nullptr ? std::make_shared<pagx::DataContext>(scene->viewModel())
                                           : nullptr;
  std::vector<pagx::DataBind*> binds = {db};
  auto binding = std::make_unique<pagx::RuntimeBinding>();
  runtime->bind(binds, ctx.get(), doc.get());
  EXPECT_FALSE(runtime->entries.empty());

  // After binding, nullify dataBind on one entry to simulate document lifetime ending.
  runtime->entries[0].dataBind = nullptr;
  runtime->markDirty(db);

  // This should not crash — null dataBind entries are skipped.
  runtime->update(binding.get(), 1.0f);
}

// ========== DataBindRuntime destructor safety ==========

PAGX_TEST(PAGXViewModelTest, DataBindRuntimeDestructorWithDeadSource) {
  // Verify that the destructor does not crash when source values have already
  // been destroyed (weak_ptr guard prevents use-after-free).
  auto runtime = std::make_unique<pagx::DataBindRuntime>();

  // Create a source value in a nested scope so it goes out of scope before runtime.
  {
    auto sourceValue = std::make_shared<pagx::PAGViewModelValueNumber>();
    sourceValue->propertyValue = 42.0f;
    sourceValue->type = pagx::ViewModelValueType::Number;

    pagx::DataBindRuntime::BindingEntry entry;
    entry.source = sourceValue.get();
    entry.sourceGuard = sourceValue->weak_from_this();
    entry.targetNode = nullptr;
    entry.channel = "alpha";
    runtime->entries.push_back(entry);
  }
  // sourceValue is now destroyed; source points to freed memory but sourceGuard is expired.

  // Destructor must not crash when it encounters an expired sourceGuard.
  runtime.reset();
}

// ========== DataContext parent fallback for null currentVM ==========

PAGX_TEST(PAGXViewModelTest, DataContextParentFallbackNullVmRef) {
  // Verify that when a ViewModel-typed property's reference instance is null,
  // resolve() falls back to the parent context instead of returning nullptr.
  auto doc = pagx::PAGXDocument::Make(400, 300);

  // Parent VM: has a ViewModel property "container" that holds a nested VM with "theme".
  auto* parentSchema = doc->makeNode<pagx::ViewModel>("Parent");
  auto* parentProp = doc->makeNode<pagx::ViewModelProperty>();
  parentProp->name = "container";
  parentProp->propertyType = pagx::ViewModelPropertyType::ViewModel;
  parentSchema->properties.push_back(parentProp);
  doc->viewModel = parentSchema;

  auto parentScene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(parentScene, nullptr);

  // Build a real nested VM for parent's "container" property.
  auto nestedVM = std::shared_ptr<pagx::PAGViewModel>(new pagx::PAGViewModel());
  auto themeVal = std::make_shared<pagx::PAGViewModelValueString>();
  themeVal->propertyName = "theme";
  themeVal->propertyValue = "dark";
  themeVal->type = pagx::ViewModelValueType::String;
  nestedVM->propertyMap["theme"] = themeVal;

  auto containerVal = std::make_shared<pagx::PAGViewModelValueViewModel>();
  containerVal->propertyName = "container";
  containerVal->type = pagx::ViewModelValueType::ViewModel;
  containerVal->referenceInstance = nestedVM;
  parentScene->viewModel()->propertyMap["container"] = containerVal;

  // Child VM: has a ViewModel-typed property "container" with null reference.
  auto* childSchema = doc->makeNode<pagx::ViewModel>("Child");
  auto* childProp = doc->makeNode<pagx::ViewModelProperty>();
  childProp->name = "container";
  childProp->propertyType = pagx::ViewModelPropertyType::ViewModel;
  childSchema->properties.push_back(childProp);

  auto childVM = std::shared_ptr<pagx::PAGViewModel>(new pagx::PAGViewModel());
  auto childContainerVal = std::make_shared<pagx::PAGViewModelValueViewModel>();
  childContainerVal->propertyName = "container";
  childContainerVal->type = pagx::ViewModelValueType::ViewModel;
  // referenceInstance is intentionally left null.
  childVM->propertyMap["container"] = childContainerVal;

  auto childCtx = std::make_shared<pagx::DataContext>(childVM);
  auto parentCtx = std::make_shared<pagx::DataContext>(parentScene->viewModel());
  childCtx->parent(parentCtx);

  // The path "$vm.container.theme" traverses container (ViewModel type, null ref) in child,
  // then falls back to parent where container exists and holds "theme"="dark".
  auto* resolved = childCtx->resolve({"$vm", "container", "theme"});
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved->valueType(), pagx::ViewModelValueType::String);
  EXPECT_EQ(static_cast<const pagx::PAGViewModelValueString*>(resolved)->value(), "dark");
}

// ========== PropertyData ==========

PAGX_TEST(PAGXViewModelTest, PropertyDataReflectsSchema) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "score";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 100.0f;
  prop->customData["min"] = "0";
  prop->customData["max"] = "200";
  schema->properties.push_back(prop);
  doc->viewModel = schema;

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto val = vm->propertyNumber("score");
  ASSERT_NE(val, nullptr);

  auto pd = val->propertyData();
  ASSERT_NE(pd, nullptr);
  EXPECT_EQ(pd->type(), pagx::PropertyData::Type::Number);
  EXPECT_EQ(pd->name(), "score");
  auto& custom = pd->customData();
  EXPECT_EQ(custom.at("min"), "0");
  EXPECT_EQ(custom.at("max"), "200");

  // Verify the subclass is correct via type + static_cast.
  auto numberPd = std::static_pointer_cast<pagx::NumberPropertyData>(pd);
  ASSERT_NE(numberPd, nullptr);
  ASSERT_TRUE(numberPd->defaultValue.has_value());
  EXPECT_FLOAT_EQ(*numberPd->defaultValue, 100.0f);
}

PAGX_TEST(PAGXViewModelTest, PropertyDataNumberWithMinMax) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "speed";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  prop->minValue = 0.5f;
  prop->maxValue = 2.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  auto speedPd = vm->propertyNumber("speed")->propertyData();
  ASSERT_NE(speedPd, nullptr);
  auto numberPd = std::static_pointer_cast<pagx::NumberPropertyData>(speedPd);
  ASSERT_NE(numberPd, nullptr);
  ASSERT_TRUE(numberPd->defaultValue.has_value());
  EXPECT_FLOAT_EQ(*numberPd->defaultValue, 1.0f);
  ASSERT_TRUE(numberPd->minValue.has_value());
  EXPECT_FLOAT_EQ(*numberPd->minValue, 0.5f);
  ASSERT_TRUE(numberPd->maxValue.has_value());
  EXPECT_FLOAT_EQ(*numberPd->maxValue, 2.0f);
}

// Verify Enum and ViewModel PropertyData subclasses.
PAGX_TEST(PAGXViewModelTest, PropertyDataSubclassReflectsType) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");

  auto* enumProp = doc->makeNode<pagx::ViewModelProperty>();
  enumProp->name = "theme";
  enumProp->propertyType = pagx::ViewModelPropertyType::Enum;
  enumProp->enumOptions = {"dark", "light", "auto"};
  schema->properties.push_back(enumProp);

  auto* vmProp = doc->makeNode<pagx::ViewModelProperty>();
  vmProp->name = "child";
  vmProp->propertyType = pagx::ViewModelPropertyType::ViewModel;
  schema->properties.push_back(vmProp);

  doc->viewModel = schema;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);

  auto enumPd = vm->propertyNumber("theme")->propertyData();
  ASSERT_NE(enumPd, nullptr);
  EXPECT_EQ(enumPd->type(), pagx::PropertyData::Type::Enum);
  auto enumData = std::static_pointer_cast<pagx::EnumPropertyData>(enumPd);
  ASSERT_NE(enumData, nullptr);
  EXPECT_EQ(enumData->options.size(), 3u);
  EXPECT_EQ(enumData->options[0], "dark");
  EXPECT_EQ(enumData->options[2], "auto");

  auto vmPd = vm->propertyViewModel("child")->propertyData();
  ASSERT_NE(vmPd, nullptr);
  EXPECT_EQ(vmPd->type(), pagx::PropertyData::Type::ViewModel);
  auto vmData = std::static_pointer_cast<pagx::ViewModelPropertyData>(vmPd);
  ASSERT_NE(vmData, nullptr);
}

// ========== Nested composition DataBind ==========

PAGX_TEST(PAGXViewModelTest, NestedCompositionDataBind) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  // Child composition: ViewModel + Layer + DataBind.
  auto* childComp = doc->makeNode<pagx::Composition>("ChildComp");
  childComp->width = 200;
  childComp->height = 200;
  auto* childVM = doc->makeNode<pagx::ViewModel>("ChildVM");
  auto* vmProp = doc->makeNode<pagx::ViewModelProperty>();
  vmProp->name = "opacity";
  vmProp->propertyType = pagx::ViewModelPropertyType::Number;
  vmProp->defaultNumber = 1.0f;
  childVM->properties.push_back(vmProp);
  childComp->viewModel = childVM;
  auto* childLayer = doc->makeNode<pagx::Layer>("childRect");
  childLayer->width = 100;
  childLayer->height = 100;
  auto* childRect = doc->makeNode<pagx::Rectangle>();
  childRect->size.width = 100;
  childRect->size.height = 100;
  auto* childFill = doc->makeNode<pagx::Fill>();
  auto* childColor = doc->makeNode<pagx::SolidColor>();
  childColor->color = {0.0f, 1.0f, 0.0f, 1.0f};
  childFill->color = childColor;
  auto* childGroup = doc->makeNode<pagx::Group>();
  childGroup->elements.push_back(childRect);
  childGroup->elements.push_back(childFill);
  childLayer->contents.push_back(childGroup);
  childComp->layers.push_back(childLayer);
  auto* childBind = doc->makeNode<pagx::DataBind>();
  childBind->source = "$vm.opacity";
  childBind->target = "@childRect";
  childBind->channel = "alpha";
  childComp->dataBinds.push_back(childBind);

  // Slot layer referencing child composition.
  auto* slot = doc->makeNode<pagx::Layer>("slot");
  slot->composition = childComp;
  slot->width = 200;
  slot->height = 200;
  doc->layers.push_back(slot);

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto rootComp = scene->rootComposition();
  ASSERT_NE(rootComp, nullptr);
  ASSERT_EQ(rootComp->children.size(), 1u);
  auto nestedComp = static_cast<pagx::PAGComposition*>(rootComp->children[0].get());
  ASSERT_NE(nestedComp, nullptr);
  ASSERT_NE(nestedComp->dataBindRuntime, nullptr);
  auto childVm = nestedComp->compositionViewModel;
  ASSERT_NE(childVm, nullptr);
  auto opacityProp = childVm->propertyNumber("opacity");
  ASSERT_NE(opacityProp, nullptr);

  // Verify the DataBind actually drives the child layer's alpha.
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);

  // Get the tgfx layer through the same path as root-level tests.
  auto nestedChild = nestedComp->children[0];
  ASSERT_NE(nestedChild, nullptr);
  auto tgfxRect = nestedChild->runtimeLayer;
  ASSERT_NE(tgfxRect, nullptr);

  // First draw should apply the ViewModel default (1.0) to the layer.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxRect->alpha(), 1.0f);

  // Set opacity to 0.3 via ViewModel, verify it propagates to the nested layer.
  opacityProp->value(0.3f);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxRect->alpha(), 0.3f);
}

// ========== vmContext connection ==========

PAGX_TEST(PAGXViewModelTest, VMContextConnection) {
  // Verify that vmContext on a Layer correctly connects the parent ViewModel's
  // ViewModel-typed property to the child composition's ViewModel instance.
  auto doc = pagx::PAGXDocument::Make(200, 200);

  // Root VM with a ViewModel-typed property.
  auto* rootVM = doc->makeNode<pagx::ViewModel>("RootVM");
  auto* vmSlot = doc->makeNode<pagx::ViewModelProperty>();
  vmSlot->name = "childSlot";
  vmSlot->propertyType = pagx::ViewModelPropertyType::ViewModel;
  rootVM->properties.push_back(vmSlot);
  doc->viewModel = rootVM;

  // Child composition with its own ViewModel.
  auto* childComp = doc->makeNode<pagx::Composition>("ChildComp");
  childComp->width = 200;
  childComp->height = 200;
  auto* childVM = doc->makeNode<pagx::ViewModel>("ChildVM");
  auto* childProp = doc->makeNode<pagx::ViewModelProperty>();
  childProp->name = "label";
  childProp->propertyType = pagx::ViewModelPropertyType::String;
  childProp->defaultString = "hello";
  childVM->properties.push_back(childProp);
  childComp->viewModel = childVM;

  auto childLayer = doc->makeNode<pagx::Layer>("childText");
  childLayer->width = 200;
  childLayer->height = 200;
  childComp->layers.push_back(childLayer);

  // Root layer that references the child composition with vmContext set.
  auto rootLayer = doc->makeNode<pagx::Layer>("rootLayer");
  rootLayer->width = 200;
  rootLayer->height = 200;
  rootLayer->composition = childComp;
  rootLayer->vmContext = "$vm.childSlot";
  doc->layers.push_back(rootLayer);

  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);

  // The root VM's childSlot property should now reference the child VM.
  auto rootVm = scene->viewModel();
  ASSERT_NE(rootVm, nullptr);
  auto slot = rootVm->propertyViewModel("childSlot");
  ASSERT_NE(slot, nullptr);
  auto refInstance = slot->referenceViewModel();
  ASSERT_NE(refInstance, nullptr);

  // Verify the child VM's property is accessible through the reference.
  auto label = refInstance->propertyString("label");
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->value(), "hello");
}

}  // namespace pag
