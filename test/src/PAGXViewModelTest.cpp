#include "base/PAGTest.h"
#include "pagx/DataBindRuntime.h"
#include "pagx/DataContext.h"
#include "pagx/DataConverterRegistry.h"
#include "pagx/ObserverHandle.h"
#include "pagx/PAGImage.h"
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
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "pagx/tgfx.h"
#include "pagx/utils/Base64.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/vectors/ImagePattern.h"
#include "tgfx/layers/vectors/Rectangle.h"

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
  EXPECT_FLOAT_EQ(color->value().green, 0.0f);
  EXPECT_FLOAT_EQ(color->value().blue, 0.0f);
  EXPECT_FLOAT_EQ(color->value().alpha, 1.0f);
}
PAGX_TEST(PAGXViewModelTest, ImagePropertyReadWrite) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 5);
  auto vm = scene->viewModel();
  auto img = vm->propertyImage("image");
  ASSERT_NE(img, nullptr);
  EXPECT_EQ(img->value(), nullptr);
  auto pagImage = pagx::PAGImage::MakeFromDataURI(redPNG);
  ASSERT_NE(pagImage, nullptr);
  img->value(pagImage);
  EXPECT_EQ(img->value(), pagImage);
  EXPECT_EQ(img->value()->source(), redPNG);
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
  EXPECT_EQ(props[0]->valueType(), pagx::ViewModelPropertyType::String);
  EXPECT_EQ(props[1]->valueType(), pagx::ViewModelPropertyType::Number);
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
  auto exportedXml = pagx::PAGXExporter::ToXML(*doc);
  auto roundTripped = pagx::PAGXImporter::FromXML(exportedXml);
  ASSERT_NE(roundTripped, nullptr);
  ASSERT_NE(roundTripped->viewModel, nullptr);
  EXPECT_EQ(roundTripped->viewModel->id, "MainVM");
  ASSERT_EQ(roundTripped->viewModel->properties.size(), 1u);
  EXPECT_EQ(roundTripped->viewModel->properties[0]->name, "title");
  auto* comp = roundTripped->findNode<pagx::Composition>("Main");
  ASSERT_NE(comp, nullptr);
  ASSERT_EQ(comp->dataBinds.size(), 1u);
  EXPECT_EQ(comp->dataBinds[0]->source, "$vm.title");
  EXPECT_EQ(comp->dataBinds[0]->target, "@textLayer");
  EXPECT_EQ(comp->dataBinds[0]->channel, "text");
}

// An Image property's default references an <Image> resource by id and round-trips as "@id".
PAGX_TEST(PAGXViewModelTest, ImageDefaultResourceRefRoundTrip) {
  std::string xml = VMXml(
      "    <ViewModel id=\"MainVM\">\n      <Property name=\"avatar\" type=\"Image\" "
      "default=\"@avatarRes\"/>\n    </ViewModel>\n    <Image id=\"avatarRes\" "
      "source=\"assets/avatar.png\"/>\n");
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->viewModel, nullptr);
  ASSERT_EQ(doc->viewModel->properties.size(), 1u);
  auto* prop = doc->viewModel->properties[0];
  ASSERT_NE(prop->defaultImage, nullptr);
  EXPECT_EQ(prop->defaultImage->id, "avatarRes");
  EXPECT_EQ(prop->defaultImage->filePath, "assets/avatar.png");
  auto exportedXml = pagx::PAGXExporter::ToXML(*doc);
  auto roundTripped = pagx::PAGXImporter::FromXML(exportedXml);
  ASSERT_NE(roundTripped, nullptr);
  ASSERT_NE(roundTripped->viewModel, nullptr);
  ASSERT_EQ(roundTripped->viewModel->properties.size(), 1u);
  auto* prop2 = roundTripped->viewModel->properties[0];
  ASSERT_NE(prop2->defaultImage, nullptr);
  EXPECT_EQ(prop2->defaultImage->id, "avatarRes");
  EXPECT_EQ(prop2->defaultImage->filePath, "assets/avatar.png");
}

PAGX_TEST(PAGXViewModelTest, HasChangedFlag) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto scene = MakeScene(doc.get(), "TestVM", 2);
  auto vm = scene->viewModel();
  auto score = vm->propertyNumber("score");
  EXPECT_FALSE(score->hasChanged());
  score->value(200.0f);
  EXPECT_TRUE(score->hasChanged());
  score->clearDirty();
  EXPECT_FALSE(score->hasChanged());
}

PAGX_TEST(PAGXViewModelTest, ManualVMConstruction) {
  auto vm = std::make_shared<pagx::PAGViewModel>();
  auto val = std::make_shared<pagx::PAGViewModelValueNumber>();
  val->propertyName = "x";
  val->propertyValue = 5.0f;
  val->type = pagx::ViewModelPropertyType::Number;
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

// Repro/guard: after a layer is refreshed in place (e.g. a late loadFileData edit on that layer),
// a ToTarget DataBind must re-apply the ViewModel's current value to the rebuilt target. Otherwise
// the refresh resets the channel to the node's authored default and the VM-pushed value is lost.
PAGX_TEST(PAGXViewModelTest, DataBindReappliesAfterLayerRefresh) {
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
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  // Push a non-default alpha through the ToTarget binding.
  vm->propertyNumber("alpha")->value(0.3f);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
  // Refresh the layer in place (simulates a late edit / loadFileData touching this layer). The
  // rebuilt target resets alpha to the node default (1.0); the binding must re-apply 0.3 on the
  // next draw rather than leaving the VM-pushed value lost.
  doc->notifyChange({layer}, false);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
}

// ========== Rendering ==========
PAGX_TEST(PAGXViewModelTest, RenderWithViewModel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* np = doc->makeNode<pagx::ViewModelProperty>();
  np->name = "bgAlpha";
  np->propertyType = pagx::ViewModelPropertyType::Number;
  np->defaultNumber = 0.4f;
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
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.bgAlpha";
  db->target = "@bg";
  db->channel = "alpha";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->viewModel(), nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  // Verify the ViewModel actually drives the rendered layer: the bound bgAlpha default reaches the
  // bg layer's alpha rather than the layer keeping its built-in default.
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.4f);
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
  aChan->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  aChan->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(aChan);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  db->direction = pagx::DataBindDirection::TwoWay;
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
  // First frame: the ViewModel's configured default takes priority and is pushed to the target,
  // so the animation value is not yet synced back.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(alphaProp->value(), 1.0f);
  // Second frame: the binding is no longer dirty, so syncBack writes the animation value back into
  // the ViewModel.
  timeline->apply();
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
  c1->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  c1->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
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
  c2->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  c2->keyframes.push_back({60, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  o2->channels.push_back(c2);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  db->direction = pagx::DataBindDirection::TwoWay;
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
  // First frame: the ViewModel default takes priority and is pushed to the target, so no syncBack
  // writeback happens and the observer is not notified.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 0);
  // Second frame: the binding is no longer dirty, so syncBack writes the animation value back into
  // the ViewModel once, notifying the observer a single time.
  t1->apply();
  t2->apply();
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 1);
  // Third frame: the value is unchanged, so the observer is not notified again.
  obs = 0;
  t1->apply();
  t2->apply();
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 0);
}

// Verify syncBack writes a non-float channel (the layer name string) back into a String ViewModel
// property, exercising the reader path added for full-type two-way binding.
PAGX_TEST(PAGXViewModelTest, StringSyncBackToViewModel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "label";
  prop->propertyType = pagx::ViewModelPropertyType::String;
  prop->defaultString = "init";
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
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.label";
  db->target = "@rect";
  db->channel = "name";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto labelProp = vm->propertyString("label");
  ASSERT_NE(labelProp, nullptr);
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // First frame: the ViewModel default "init" is pushed to the layer name.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(labelProp->value(), "init");
  EXPECT_EQ(tgfxLayer->name(), "init");
  // Drive the layer name externally, then draw again: syncBack reads the name back into the VM.
  tgfxLayer->setName("changed");
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(labelProp->value(), "changed");
}

// Verify the DataBind flags (direction) attribute round-trips through XML export/import.
PAGX_TEST(PAGXViewModelTest, DataBindFlagsRoundTrip) {
  std::string xml = VMXml(
      "    <ViewModel id=\"MainVM\">\n      <Property name=\"title\" type=\"String\"/>\n    "
      "</ViewModel>\n    <Composition id=\"Main\" width=\"400\" height=\"300\">\n      <Layer "
      "id=\"textLayer\" name=\"Text\"/>\n      <DataBind source=\"$vm.title\" "
      "target=\"@textLayer\" channel=\"name\" direction=\"TwoWay\"/>\n    </Composition>\n");
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  auto* comp = doc->findNode<pagx::Composition>("Main");
  ASSERT_NE(comp, nullptr);
  ASSERT_EQ(comp->dataBinds.size(), 1u);
  EXPECT_EQ(comp->dataBinds[0]->direction, pagx::DataBindDirection::TwoWay);
  auto exportedXml = pagx::PAGXExporter::ToXML(*doc);
  auto roundTripped = pagx::PAGXImporter::FromXML(exportedXml);
  ASSERT_NE(roundTripped, nullptr);
  auto* comp2 = roundTripped->findNode<pagx::Composition>("Main");
  ASSERT_NE(comp2, nullptr);
  ASSERT_EQ(comp2->dataBinds.size(), 1u);
  EXPECT_EQ(comp2->dataBinds[0]->direction, pagx::DataBindDirection::TwoWay);
}

// Verify one ViewModel property bound to multiple targets (default ToTarget) drives every target,
// and that a ToTarget binding never writes a target change back into the ViewModel.
PAGX_TEST(PAGXViewModelTest, OneSourceToManyTargetsToTarget) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 1.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  for (int i = 0; i < 2; i++) {
    auto layer = doc->makeNode<pagx::Layer>(i == 0 ? "rectA" : "rectB");
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
    db->source = "$vm.alpha";
    db->target = i == 0 ? "@rectA" : "@rectB";
    db->channel = "alpha";
    doc->dataBinds.push_back(db);
  }
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto alphaProp = vm->propertyNumber("alpha");
  ASSERT_NE(alphaProp, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // The single ViewModel property drives both targets.
  alphaProp->value(0.5f);
  EXPECT_TRUE(scene->draw(surface));
  auto rootComp = scene->rootComposition();
  ASSERT_NE(rootComp, nullptr);
  ASSERT_EQ(rootComp->children.size(), 2u);
  auto layerA = rootComp->children[0]->runtimeLayer;
  auto layerB = rootComp->children[1]->runtimeLayer;
  ASSERT_NE(layerA, nullptr);
  ASSERT_NE(layerB, nullptr);
  EXPECT_FLOAT_EQ(layerA->alpha(), 0.5f);
  EXPECT_FLOAT_EQ(layerB->alpha(), 0.5f);
  // Changing one target externally must NOT flow back into the ViewModel for ToTarget bindings.
  layerA->setAlpha(0.2f);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(alphaProp->value(), 0.5f);
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
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  const std::string greenPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGNg+M8AAAICAQB7CYF4AAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* s = doc->makeNode<pagx::ViewModel>("T");
  auto* p = doc->makeNode<pagx::ViewModelProperty>();
  p->name = "img";
  p->propertyType = pagx::ViewModelPropertyType::Image;
  s->properties.push_back(p);
  doc->viewModel = s;
  auto l = doc->makeNode<pagx::Layer>("r");
  l->width = 200;
  l->height = 200;
  auto r = doc->makeNode<pagx::Rectangle>();
  r->size.width = 200;
  r->size.height = 200;
  auto imgNode = doc->makeNode<pagx::Image>("imgRes");
  imgNode->filePath = redPNG;
  p->defaultImage = imgNode;
  auto pattern = doc->makeNode<pagx::ImagePattern>("bgImg");
  pattern->image = imgNode;
  auto f = doc->makeNode<pagx::Fill>();
  f->color = pattern;
  auto g = doc->makeNode<pagx::Group>();
  g->elements.push_back(r);
  g->elements.push_back(f);
  l->contents.push_back(g);
  doc->layers.push_back(l);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.img";
  db->target = "@bgImg";
  db->channel = "image";
  doc->dataBinds.push_back(db);
  auto sc = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(sc, nullptr);
  auto vm = sc->viewModel();
  ASSERT_NE(vm, nullptr);
  auto v = vm->propertyImage("img");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->value(), nullptr);
  EXPECT_EQ(v->value()->source(), redPNG);
  auto greenImage = pagx::PAGImage::MakeFromDataURI(greenPNG);
  ASSERT_NE(greenImage, nullptr);
  v->value(greenImage);
  auto sf = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(sf, nullptr);
  EXPECT_TRUE(sc->draw(sf));
  EXPECT_EQ(v->value(), greenImage);
  std::array<uint8_t, 200 * 200 * 4> pixels = {};
  ASSERT_TRUE(sf->readPixels(pixels.data(), 200 * 4));
  auto& px = *reinterpret_cast<uint32_t*>(pixels.data() + (100 * 200 + 100) * 4);
  EXPECT_NEAR((px >> 8) & 0xFF, 255, 5);
  EXPECT_NEAR(px & 0xFF, 0, 5);
}

// Regression: a TwoWay Boolean binding must sync the real layer visibility back into the
// ViewModel. KeyValueToFloat previously ignored the bool KeyValue alternative, so syncBack always
// wrote false regardless of the layer's actual visibility.
PAGX_TEST(PAGXViewModelTest, BooleanSyncBackToViewModel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "vis";
  prop->propertyType = pagx::ViewModelPropertyType::Boolean;
  prop->defaultBoolean = false;
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
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.vis";
  db->target = "@rect";
  db->channel = "visible";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto visProp = vm->propertyBoolean("vis");
  ASSERT_NE(visProp, nullptr);
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // First frame: the ViewModel default (false) is pushed to the layer.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(visProp->value(), false);
  EXPECT_EQ(tgfxLayer->visible(), false);
  // Drive layer visibility true externally, then draw: syncBack must write true back into the VM
  // (not the bug's constant false).
  tgfxLayer->setVisible(true);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(visProp->value(), true);
}

// Regression: a TwoWay x binding must sync back the animated x channel value (animX), not the
// composed layer-matrix translate. With a non-zero authored matrix translation the old read()
// returned animX + matrix.tx, polluting the ViewModel.
PAGX_TEST(PAGXViewModelTest, PositionSyncBackIgnoresAuthoredMatrixTranslate) {
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
  // Authored matrix carries a non-zero translation; the x channel value must round-trip
  // independently of it.
  layer->matrix = pagx::Matrix::Translate(30.0f, 0.0f);
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
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "rect";
  anim->objects.push_back(object);
  auto* xChan = doc->makeNode<pagx::TypedChannel<float>>();
  xChan->name = "x";
  xChan->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  xChan->keyframes.push_back({60, 100.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(xChan);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.x";
  db->target = "@rect";
  db->channel = "x";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto xProp = vm->propertyNumber("x");
  ASSERT_NE(xProp, nullptr);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_NE(timeline, nullptr);
  timeline->setCurrentTime(500000);
  timeline->apply();
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // First frame: the ViewModel default (0) takes priority and is pushed to the target.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(xProp->value(), 0.0f);
  // Second frame: syncBack writes the animated x channel value (~50) back, NOT 50 + 30 authored
  // translate.
  timeline->apply();
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_NEAR(xProp->value(), 50.0f, 0.01f);
}

// Regression: image syncBack must use tgfx::Image comparison, not PAGImage wrapper identity. An
// unchanged target image must NOT write back (the per-read wrapper would otherwise overwrite the VM
// image, dropping its source URI, and fire observers every frame). A genuine image change at the
// target MUST sync back into the ViewModel.
PAGX_TEST(PAGXViewModelTest, ImageTwoWaySyncBack) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  const std::string greenPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGNg+M8AAAICAQB7CYF4AAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "img";
  prop->propertyType = pagx::ViewModelPropertyType::Image;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto imgNode = doc->makeNode<pagx::Image>("imgRes");
  imgNode->filePath = redPNG;
  prop->defaultImage = imgNode;
  auto pattern = doc->makeNode<pagx::ImagePattern>("bgImg");
  pattern->image = imgNode;
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.img";
  db->target = "@bgImg";
  db->channel = "image";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto imgProp = vm->propertyImage("img");
  ASSERT_NE(imgProp, nullptr);
  auto original = imgProp->value();
  ASSERT_NE(original, nullptr);
  EXPECT_EQ(original->source(), redPNG);
  int obs = 0;
  auto h = imgProp->addObserver([&]() { obs++; });
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // Unchanged target image across several frames: syncBack compares the underlying tgfx::Image, so
  // the VM image identity and source are preserved and the observer is never fired.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(imgProp->value(), original);
  EXPECT_EQ(imgProp->value()->source(), redPNG);
  EXPECT_EQ(obs, 0);
  // Drive the runtime pattern's image to a genuinely different tgfx::Image, then draw: syncBack must
  // detect the change and write the new image back into the ViewModel.
  auto greenImage = pagx::PAGImage::MakeFromDataURI(greenPNG);
  ASSERT_NE(greenImage, nullptr);
  auto binding = scene->mutableBinding();
  ASSERT_NE(binding, nullptr);
  auto runtimePattern = binding->get<tgfx::ImagePattern>(pattern);
  ASSERT_NE(runtimePattern, nullptr);
  runtimePattern->setImage(pagx::GetTGFXImage(greenImage));
  EXPECT_TRUE(scene->draw(surface));
  ASSERT_NE(imgProp->value(), nullptr);
  EXPECT_EQ(pagx::GetTGFXImage(imgProp->value()), pagx::GetTGFXImage(greenImage));
  EXPECT_EQ(obs, 1);
}

// A late loadFileData on an <Image> that a ViewModel image default references must refresh the VM
// image value (was null for an unresolved external path) and fire its observer.
PAGX_TEST(PAGXViewModelTest, ImageResourceLoadRefreshesViewModelDefault) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "img";
  prop->propertyType = pagx::ViewModelPropertyType::Image;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  // External file path: unresolved at build time (host has not provided the bytes yet).
  auto imgNode = doc->makeNode<pagx::Image>("imgRes");
  imgNode->filePath = "assets/avatar.png";
  prop->defaultImage = imgNode;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto imgProp = vm->propertyImage("img");
  ASSERT_NE(imgProp, nullptr);
  // The external path could not be decoded at build time.
  EXPECT_EQ(imgProp->value(), nullptr);
  int obs = 0;
  auto h = imgProp->addObserver([&]() { obs++; });
  // Host injects the bytes for the external path.
  auto data = pagx::DecodeBase64DataURI(redPNG);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(doc->loadFileData("assets/avatar.png", data));
  // The VM image value is now decoded and the observer fired once.
  ASSERT_NE(imgProp->value(), nullptr);
  EXPECT_EQ(obs, 1);
}

// A business-side image override must NOT be clobbered when the referenced <Image> resource loads.
PAGX_TEST(PAGXViewModelTest, ImageResourceLoadPreservesOverride) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  const std::string greenPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGNg+M8AAAICAQB7CYF4AAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "img";
  prop->propertyType = pagx::ViewModelPropertyType::Image;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto imgNode = doc->makeNode<pagx::Image>("imgRes");
  imgNode->filePath = "assets/avatar.png";
  prop->defaultImage = imgNode;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto imgProp = scene->viewModel()->propertyImage("img");
  ASSERT_NE(imgProp, nullptr);
  // Business side assigns its own image before the resource loads.
  auto custom = pagx::PAGImage::MakeFromDataURI(greenPNG);
  ASSERT_NE(custom, nullptr);
  imgProp->value(custom);
  // The resource loads afterwards; the override must survive.
  auto data = pagx::DecodeBase64DataURI(redPNG);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(doc->loadFileData("assets/avatar.png", data));
  EXPECT_EQ(imgProp->value(), custom);
}

// A business-side explicit clear-to-null must be preserved when the (unresolved external) default
// resource loads later. The clear is intentional even though it leaves the value null, so a late
// load must not resurrect the default image.
PAGX_TEST(PAGXViewModelTest, ImageResourceLoadPreservesExplicitNull) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "img";
  prop->propertyType = pagx::ViewModelPropertyType::Image;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  // Unresolved external path: the default decodes to null at build time.
  auto imgNode = doc->makeNode<pagx::Image>("imgRes");
  imgNode->filePath = "assets/avatar.png";
  prop->defaultImage = imgNode;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto imgProp = scene->viewModel()->propertyImage("img");
  ASSERT_NE(imgProp, nullptr);
  EXPECT_EQ(imgProp->value(), nullptr);
  // Business explicitly clears the value (already null, but an intentional assignment).
  imgProp->value(nullptr);
  // The resource loads afterwards; the explicit clear must survive (no resurrection of the default).
  auto data = pagx::DecodeBase64DataURI(redPNG);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(doc->loadFileData("assets/avatar.png", data));
  EXPECT_EQ(imgProp->value(), nullptr);
}

// End-to-end: a DataBind bound to a ViewModel image must push the refreshed image to its target
// after the referenced <Image> resource loads. Proves resource-load -> VM refresh -> DataBind push.
PAGX_TEST(PAGXViewModelTest, ImageResourceLoadPropagatesThroughDataBind) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "img";
  prop->propertyType = pagx::ViewModelPropertyType::Image;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rect");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  // The pattern uses a SEPARATE inline image so the binding (not the pattern's own resource) is
  // what drives the target; the VM default references the external-path resource being loaded.
  auto patternImg = doc->makeNode<pagx::Image>("patternImg");
  patternImg->filePath = redPNG;
  auto pattern = doc->makeNode<pagx::ImagePattern>("bgImg");
  pattern->image = patternImg;
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto vmImg = doc->makeNode<pagx::Image>("vmImg");
  vmImg->filePath = "assets/avatar.png";
  prop->defaultImage = vmImg;
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.img";
  db->target = "@bgImg";
  db->channel = "image";
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto imgProp = scene->viewModel()->propertyImage("img");
  ASSERT_NE(imgProp, nullptr);
  EXPECT_EQ(imgProp->value(), nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // Host loads the external resource the VM default references.
  auto data = pagx::DecodeBase64DataURI(redPNG);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(doc->loadFileData("assets/avatar.png", data));
  ASSERT_NE(imgProp->value(), nullptr);
  // Draw so the now-dirty DataBind applies the refreshed VM image onto the pattern target.
  EXPECT_TRUE(scene->draw(surface));
  auto binding = scene->mutableBinding();
  ASSERT_NE(binding, nullptr);
  auto runtimePattern = binding->get<tgfx::ImagePattern>(pattern);
  ASSERT_NE(runtimePattern, nullptr);
  EXPECT_EQ(runtimePattern->image(), pagx::GetTGFXImage(imgProp->value()));
}

// A nested composition's own ViewModel image default must also refresh when its referenced <Image>
// resource loads (RefreshViewModelImages walks the whole composition tree, not just the root VM).
PAGX_TEST(PAGXViewModelTest, ImageResourceLoadRefreshesNestedViewModel) {
  const std::string redPNG =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQ"
      "mCC";
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* rootVM = doc->makeNode<pagx::ViewModel>("RootVM");
  auto* vmSlot = doc->makeNode<pagx::ViewModelProperty>();
  vmSlot->name = "childSlot";
  vmSlot->propertyType = pagx::ViewModelPropertyType::ViewModel;
  rootVM->properties.push_back(vmSlot);
  doc->viewModel = rootVM;
  // Child composition with its own VM carrying an external-path image default.
  auto* childComp = doc->makeNode<pagx::Composition>("ChildComp");
  childComp->width = 200;
  childComp->height = 200;
  auto* childVM = doc->makeNode<pagx::ViewModel>("ChildVM");
  auto* childImgProp = doc->makeNode<pagx::ViewModelProperty>();
  childImgProp->name = "img";
  childImgProp->propertyType = pagx::ViewModelPropertyType::Image;
  childVM->properties.push_back(childImgProp);
  childComp->viewModel = childVM;
  auto childImg = doc->makeNode<pagx::Image>("childImg");
  childImg->filePath = "assets/child.png";
  childImgProp->defaultImage = childImg;
  auto childLayer = doc->makeNode<pagx::Layer>("childText");
  childLayer->width = 200;
  childLayer->height = 200;
  childComp->layers.push_back(childLayer);
  auto rootLayer = doc->makeNode<pagx::Layer>("rootLayer");
  rootLayer->width = 200;
  rootLayer->height = 200;
  rootLayer->composition = childComp;
  rootLayer->vmContext = "$vm.childSlot";
  doc->layers.push_back(rootLayer);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto slot = scene->viewModel()->propertyViewModel("childSlot");
  ASSERT_NE(slot, nullptr);
  auto childInstance = slot->referenceViewModel();
  ASSERT_NE(childInstance, nullptr);
  auto childImgValue = childInstance->propertyImage("img");
  ASSERT_NE(childImgValue, nullptr);
  EXPECT_EQ(childImgValue->value(), nullptr);
  // Loading the nested VM's referenced resource refreshes the nested image value.
  auto data = pagx::DecodeBase64DataURI(redPNG);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(doc->loadFileData("assets/child.png", data));
  EXPECT_NE(childImgValue->value(), nullptr);
}

// Two-way for a templated SizeAxis channel: a TwoWay binding on Rectangle size.width must sync the
// runtime rectangle's width back into the ViewModel after it changes (validates ReadSizeAxis).
PAGX_TEST(PAGXViewModelTest, RectangleSizeWidthSyncBack) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "w";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 100.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rectLayer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>("rectShape");
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
  db->source = "$vm.w";
  db->target = "@rectShape";
  db->channel = "size.width";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto wProp = scene->viewModel()->propertyNumber("w");
  ASSERT_NE(wProp, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // First frame: VM default (100) is pushed to the rectangle.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(wProp->value(), 100.0f);
  // Drive the runtime rectangle width externally, then draw: syncBack reads it back into the VM.
  auto binding = scene->mutableBinding();
  ASSERT_NE(binding, nullptr);
  auto runtimeRect = binding->get<tgfx::Rectangle>(rect);
  ASSERT_NE(runtimeRect, nullptr);
  runtimeRect->setSize({160.0f, 100.0f});
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(wProp->value(), 160.0f);
}

// Two-way for a bespoke reader channel: Rectangle roundness uses WriteRectangleRoundness /
// ReadRectangleRoundness (single value <-> 4-corner array). Validates the bespoke reader path.
PAGX_TEST(PAGXViewModelTest, RectangleRoundnessSyncBack) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "r";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 0.0f;
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("rectLayer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>("rectShape");
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
  db->source = "$vm.r";
  db->target = "@rectShape";
  db->channel = "roundness";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto rProp = scene->viewModel()->propertyNumber("r");
  ASSERT_NE(rProp, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(rProp->value(), 0.0f);
  auto binding = scene->mutableBinding();
  ASSERT_NE(binding, nullptr);
  auto runtimeRect = binding->get<tgfx::Rectangle>(rect);
  ASSERT_NE(runtimeRect, nullptr);
  runtimeRect->setRoundness(12.0f);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(rProp->value(), 12.0f);
}

// An unset optional channel (TextModifier strokeColor never authored) must NOT sync back: its
// reader returns false so syncBack skips it, leaving a ToSource-bound ViewModel value untouched
// rather than clobbering it with a fabricated default.
PAGX_TEST(PAGXViewModelTest, UnsetOptionalChannelDoesNotSyncBack) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "stroke";
  prop->propertyType = pagx::ViewModelPropertyType::Color;
  prop->defaultColor = pagx::Color{1, 0, 0, 1};  // red source value
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("textLayer");
  layer->width = 200;
  layer->height = 200;
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hi";
  text->fontSize = 24;
  layer->contents.push_back(text);
  auto modifier = doc->makeNode<pagx::TextModifier>("mod");
  // strokeColor is intentionally left unset (std::nullopt).
  layer->contents.push_back(modifier);
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.stroke";
  db->target = "@mod";
  db->channel = "strokeColor";
  db->direction = pagx::DataBindDirection::ToSource;  // target -> VM only
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto strokeProp = scene->viewModel()->propertyColor("stroke");
  ASSERT_NE(strokeProp, nullptr);
  EXPECT_FLOAT_EQ(strokeProp->value().red, 1.0f);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // Draw: the modifier's strokeColor optional is empty, so its reader returns false and syncBack
  // skips it. The ViewModel source value must remain the red it was given, not be clobbered.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(strokeProp->value().red, 1.0f);
  EXPECT_FLOAT_EQ(strokeProp->value().green, 0.0f);
  EXPECT_FLOAT_EQ(strokeProp->value().blue, 0.0f);
  EXPECT_FLOAT_EQ(strokeProp->value().alpha, 1.0f);
}

// Once direction: the ViewModel value is applied to the target exactly once on the first frame, and
// later ViewModel changes are ignored. Once is fire-and-forget, so an in-place layer refresh does
// NOT restore its value — the rebuilt target keeps the node default (only ToTarget/TwoWay follow
// the ViewModel continuously and are re-applied after a refresh).
PAGX_TEST(PAGXViewModelTest, OnceDirectionAppliesOnceAndIgnoresLaterChanges) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("TestVM");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "alpha";
  prop->propertyType = pagx::ViewModelPropertyType::Number;
  prop->defaultNumber = 0.5f;
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
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.alpha";
  db->target = "@rect";
  db->channel = "alpha";
  db->direction = pagx::DataBindDirection::Once;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);
  auto alphaProp = scene->viewModel()->propertyNumber("alpha");
  ASSERT_NE(alphaProp, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // First frame: the configured default (0.5) is applied once.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.5f);
  // A later ViewModel change is ignored by a Once binding.
  alphaProp->value(0.2f);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.5f);
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

  // Set duration to 2.0 seconds; converter outputs 120.0 frames (2.0 * 60). The DataBind applies
  // immediately, so the rect's x shifts to 120 before any draw: the hit point must target the
  // rect's new position to retrieve the layer.
  propV->value(2.0f);

  auto layers = scene->getLayersUnderPoint(150, 100);
  ASSERT_GT(layers.size(), 0u);
  auto tgfxLayer = layers[0]->runtimeLayer;
  ASSERT_NE(tgfxLayer, nullptr);

  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  EXPECT_TRUE(scene->draw(surface));

  // Verify the converter output reached the layer through the DataBind pipeline.
  EXPECT_FLOAT_EQ(tgfxLayer->matrix().getTranslateX(), 120.0f);

  // Verify the converter output via the registry (same path the pipeline uses).
  auto& registry = pagx::DataConverterRegistry::GetInstance();
  pagx::KeyValue input{2.0f};
  auto output = registry.apply(conv, input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 120.0f);
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
  auto& registry = pagx::DataConverterRegistry::GetInstance();
  pagx::KeyValue priceInput{5999.0f};
  auto priceOutput = registry.apply(conv, priceInput);
  ASSERT_TRUE(std::holds_alternative<std::string>(priceOutput));
  EXPECT_EQ(std::get<std::string>(priceOutput), "$5999.00");
  auto layers = scene->getLayersUnderPoint(100, 100);
  ASSERT_GT(layers.size(), 0u);
  EXPECT_EQ(layers[0]->runtimeLayer->name(), "$5999.00");
}

PAGX_TEST(PAGXViewModelTest, DataConverterNotFoundPassthrough) {
  // Non-existent converter → value passes through unchanged.
  pagx::KeyValue input{42.0f};
  auto output = pagx::DataConverterRegistry::GetInstance().apply(nullptr, input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 42.0f);
}

PAGX_TEST(PAGXViewModelTest, DataConverterInverseSecondsToFrames) {
  auto conv = std::unique_ptr<pagx::DataConverter>(new pagx::DataConverter());
  conv->converterType = "secondsToFrames";
  conv->params["frameRate"] = "60";

  pagx::KeyValue input{120.0f};
  auto output = pagx::DataConverterRegistry::GetInstance().applyInverse(conv.get(), input);
  ASSERT_TRUE(std::holds_alternative<float>(output));
  EXPECT_FLOAT_EQ(std::get<float>(output), 2.0f);
}

PAGX_TEST(PAGXViewModelTest, DataConverterInversePassthrough) {
  // No registered inverse → value unchanged.
  pagx::KeyValue input{99.0f};
  auto output = pagx::DataConverterRegistry::GetInstance().applyInverse(nullptr, input);
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
  runtime->bind(binds, ctx.get(), doc.get(), binding.get());
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
    sourceValue->type = pagx::ViewModelPropertyType::Number;

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
  themeVal->type = pagx::ViewModelPropertyType::String;
  nestedVM->propertyMap["theme"] = themeVal;

  auto containerVal = std::make_shared<pagx::PAGViewModelValueViewModel>();
  containerVal->propertyName = "container";
  containerVal->type = pagx::ViewModelPropertyType::ViewModel;
  containerVal->referenceInstance = nestedVM;
  parentScene->viewModel()->propertyMap["container"] = containerVal;

  // Child VM: has a ViewModel-typed property "container" with null reference.
  auto childVM = std::shared_ptr<pagx::PAGViewModel>(new pagx::PAGViewModel());
  auto childContainerVal = std::make_shared<pagx::PAGViewModelValueViewModel>();
  childContainerVal->propertyName = "container";
  childContainerVal->type = pagx::ViewModelPropertyType::ViewModel;
  // referenceInstance is intentionally left null.
  childVM->propertyMap["container"] = childContainerVal;

  auto childCtx = std::make_shared<pagx::DataContext>(childVM);
  auto parentCtx = std::make_shared<pagx::DataContext>(parentScene->viewModel());
  childCtx->parent(parentCtx);

  // The path "$vm.container.theme" traverses container (ViewModel type, null ref) in child,
  // then falls back to parent where container exists and holds "theme"="dark".
  auto* resolved = childCtx->resolve({"$vm", "container", "theme"});
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved->valueType(), pagx::ViewModelPropertyType::String);
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

  auto enumPd = vm->propertyEnum("theme")->propertyData();
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

// Verify Enum properties store and expose a zero-based integer option index.
PAGX_TEST(PAGXViewModelTest, EnumValueIntStorage) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");

  auto* enumProp = doc->makeNode<pagx::ViewModelProperty>();
  enumProp->name = "theme";
  enumProp->propertyType = pagx::ViewModelPropertyType::Enum;
  enumProp->enumOptions = {"dark", "light", "auto"};
  enumProp->defaultEnum = 2;
  schema->properties.push_back(enumProp);

  doc->viewModel = schema;
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);

  auto theme = vm->propertyEnum("theme");
  ASSERT_NE(theme, nullptr);
  EXPECT_EQ(theme->valueType(), pagx::ViewModelPropertyType::Enum);
  EXPECT_EQ(theme->value(), 2);
  EXPECT_FALSE(theme->hasChanged());

  theme->value(0);
  EXPECT_EQ(theme->value(), 0);
  EXPECT_TRUE(theme->hasChanged());

  // A number accessor must not return an enum property.
  EXPECT_EQ(vm->propertyNumber("theme"), nullptr);
}

// Verify enum option values containing commas survive an XML export/import round trip via escaping.
PAGX_TEST(PAGXViewModelTest, EnumOptionCommaRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");
  auto* enumProp = doc->makeNode<pagx::ViewModelProperty>();
  enumProp->name = "layout";
  enumProp->propertyType = pagx::ViewModelPropertyType::Enum;
  enumProp->enumOptions = {"one, two", "three", "a\\b"};
  enumProp->defaultEnum = 0;
  schema->properties.push_back(enumProp);
  doc->viewModel = schema;

  auto exportedXml = pagx::PAGXExporter::ToXML(*doc);
  auto roundTripped = pagx::PAGXImporter::FromXML(exportedXml);
  ASSERT_NE(roundTripped, nullptr);
  ASSERT_NE(roundTripped->viewModel, nullptr);
  ASSERT_EQ(roundTripped->viewModel->properties.size(), 1u);
  auto* prop = roundTripped->viewModel->properties[0];
  ASSERT_EQ(prop->enumOptions.size(), 3u);
  EXPECT_EQ(prop->enumOptions[0], "one, two");
  EXPECT_EQ(prop->enumOptions[1], "three");
  EXPECT_EQ(prop->enumOptions[2], "a\\b");
}

// An Enum property with no options and a zero default exports neither an options list nor a default
// attribute. Re-importing that output must round-trip cleanly instead of reporting the default as
// out of range for zero options.
PAGX_TEST(PAGXViewModelTest, EnumEmptyOptionsRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* schema = doc->makeNode<pagx::ViewModel>("MainVM");
  auto* enumProp = doc->makeNode<pagx::ViewModelProperty>();
  enumProp->name = "layout";
  enumProp->propertyType = pagx::ViewModelPropertyType::Enum;
  enumProp->defaultEnum = 0;
  schema->properties.push_back(enumProp);
  doc->viewModel = schema;

  auto exportedXml = pagx::PAGXExporter::ToXML(*doc);
  auto roundTripped = pagx::PAGXImporter::FromXML(exportedXml);
  ASSERT_NE(roundTripped, nullptr);
  ASSERT_NE(roundTripped->viewModel, nullptr);
  ASSERT_EQ(roundTripped->viewModel->properties.size(), 1u);
  auto* prop = roundTripped->viewModel->properties[0];
  EXPECT_EQ(prop->propertyType, pagx::ViewModelPropertyType::Enum);
  EXPECT_TRUE(prop->enumOptions.empty());
  EXPECT_EQ(prop->defaultEnum, 0);
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

PAGX_TEST(PAGXViewModelTest, TwoWaySyncPreservesColorSpaceOnReadback) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* schema = doc->makeNode<pagx::ViewModel>("T");
  auto* prop = doc->makeNode<pagx::ViewModelProperty>();
  prop->name = "bg";
  prop->propertyType = pagx::ViewModelPropertyType::Color;
  prop->defaultColor = pagx::Color{1, 0, 0, 1, pagx::ColorSpace::DisplayP3};
  schema->properties.push_back(prop);
  doc->viewModel = schema;
  auto layer = doc->makeNode<pagx::Layer>("r");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  auto fill = doc->makeNode<pagx::Fill>();
  auto color = doc->makeNode<pagx::SolidColor>("fillColor");
  fill->color = color;
  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  auto db = doc->makeNode<pagx::DataBind>();
  db->source = "$vm.bg";
  db->target = "@fillColor";
  db->channel = "color";
  db->direction = pagx::DataBindDirection::TwoWay;
  doc->dataBinds.push_back(db);
  auto scene = pagx::PAGScene::Make(
      std::shared_ptr<pagx::PAGXDocument>(doc.get(), [](pagx::PAGXDocument*) {}));
  ASSERT_NE(scene, nullptr);
  auto vm = scene->viewModel();
  ASSERT_NE(vm, nullptr);
  auto bg = vm->propertyColor("bg");
  ASSERT_NE(bg, nullptr);
  EXPECT_EQ(bg->value().colorSpace, pagx::ColorSpace::DisplayP3);
  int obs = 0;
  auto h = bg->addObserver([&]() { obs++; });
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  // The color is never changed on the target side, so the first-frame syncBack must not clobber the
  // ViewModel's Display P3 value with the sRGB-labeled read-back, nor fire the observer. Without
  // color-space normalization the sRGB-labeled read-back would never equal the P3 source, forcing a
  // spurious first-pass writeback.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 0);
  EXPECT_EQ(bg->value().colorSpace, pagx::ColorSpace::DisplayP3);
  EXPECT_FLOAT_EQ(bg->value().red, 1.0f);
  EXPECT_FLOAT_EQ(bg->value().green, 0.0f);
  EXPECT_FLOAT_EQ(bg->value().blue, 0.0f);
  // A second draw keeps the value stable and still does not notify.
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_EQ(obs, 0);
  EXPECT_EQ(bg->value().colorSpace, pagx::ColorSpace::DisplayP3);
}

}  // namespace pag
