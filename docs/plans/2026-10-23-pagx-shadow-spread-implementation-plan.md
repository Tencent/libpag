# PAGX 阴影 spread 支持执行文档

- 日期：2026-10-23。
- 状态：执行方案；功能代码尚未实施。
- 已确认范围：仅在 PAGX 原生数据模型、XML 协议、原生场景渲染和属性／动画通道中支持 spread。
- 已确认接口：允许在 InnerShadowStyle 和 DropShadowStyle 的公开 C++ 节点类中新增 float 字段。
- 原始需求：CoCraft 的 packages/common/editor/docs/libpag-shadow-spread-implementation-plan.md。
- 本文路径均相对 libpag 仓库根目录；函数名优先于行号。执行前需重新核对目标分支。

> 本轮交付的是执行文档，不代表 spread 已在 libpag 中实现或通过验收。HTML、SVG、PPT 和 CoCraft 接入均不属于本次执行范围。

## 1. 目标与问题依据

### 1.1 问题

编辑器实际主 Tab 的浅色底板叠加了三层内阴影，spread 均约为 -19.587。编辑器画布将该值传给 TGFX，但当前 PAGX 两类阴影 Style 没有对应字段，设计数据进入 PAGXDocument 时无法保留，导出后的渲染使用 TGFX 默认值 0，出现上下发黑等外观差异。

原始需求文档记录的原生 GPU 对照已验证：仅将 spread 从 -19.587 改为 0 就能复现，移除玻璃仍可复现。因此本次修复数据表达和参数映射，不修改玻璃算法、背景采样或阴影底层算法。

### 1.2 完成目标

1. 两类阴影节点可保存正、负、零和小数 spread。
2. XML 导入、导出及重复往返不丢值、不改符号。
3. 直接构造的内存文档和从 XML 加载的文档均能将 spread 传给 TGFX。
4. 通用属性 API 可发现、读写、重置该字段。
5. spread 动画可序列化、插值和混合，运行时更新后正确重绘。
6. 中英文规范及 XSD 与生产实现一致。
7. 提供属性测试、运行时测试、同 GPU 参考像素对照和截图回归证据。

## 2. 范围与职责边界

### 2.1 本次修改

| 层次 | 职责 | 本次变更 |
| --- | --- | --- |
| PAGX 节点 | 保存协议与编辑数据，不持有 TGFX 运行时状态 | 两类阴影 Style 新增 spread |
| PAGX XML 读写 | 在 XML 与内存文档之间保真转换 | 两类 Style 解析和写出 spread |
| 通用属性注册 | 按节点类型发现、读写和重置字段，声明动画能力 | 注册 float、Anim 通道 |
| LayerBuilder | 将 PAGX 节点转换为 TGFX 对象，建立运行时绑定 | 首次构建设置 spread，注册 reader／writer |
| 现有动画系统 | float 关键帧求值与混合 | 复用现有链路，不新增动画类型 |
| TGFX | 实际扩散形状、边界、缓存和绘制 | 复用现有 spread 接口，不改算法 |
| 规范与测试 | 声明语义、能力边界并防止回归 | 同步 Markdown、XSD 和测试 |

### 2.2 明确不做

- 不给 LayerStyle 基类增加 spread。
- 不给 DropShadowFilter、InnerShadowFilter 增加字段或通道。
- 不修改二进制 PAG 格式、编解码标签或 PAG 的已有阴影参数。
- 不实现 HTML、SVG、PPT 的 spread 映射或回退策略。
- 不修改 CoCraft 的导入／导出映射、依赖包、SDK 或发布流程。
- 不修复与本需求独立的 blendMode、excludeChildEffects 映射遗漏。
- 不发布在线规范，不直接编辑生成页面。
- 不实现任意路径的精确形态学扩张／收缩，不在 PAGX 转换层重建阴影绘制算法。

其他格式不支持 spread 不阻塞本次 PAGX 原生验收，也不能因本次完成而宣称它们已支持。开启所有模块编译、运行现有全量测试属于回归检查，不等于扩大功能范围。

HTML 导入侧现有注释中“PAGX 阴影没有 spread”将在本次实施后过期；本次只记录，留待后续 HTML 任务改为“HTML 导入暂未映射 spread”。HTML 仍然忽略该 CSS 参数的行为不在本次修改范围。

### 2.3 数据流

~~~text
XML spread attribute ── PAGXImporter ──┐
                                      ↓
Direct field assignment ──────── PAGX shadow style node
                                      ↕
                           PAGXExporter / XML
                                      ↓
                              LayerBuilder
                                      ↓
                        TGFX style.setSpread(value)
                                      ↓
                           TGFX bounds / drawing

Get/Set/ResetNodeChannel ── field registry ── document node
                                                  ↓
                             notifyChange(owningLayer, false)
                                                  ↓
                                     existing scene refresh

Animation XML / TypedChannel<float>
                 ↓
          existing evaluator
                 ↓
         RuntimeBinding accessor
                 ↓
     MixFloat(current, target, mix)
                 ↓
        TGFX style.setSpread(value)
                 ↕
        runtime reader style.spread()
~~~

文档字段值与动画驱动的运行时值不是同一份状态。SetNodeChannel 修改文档后必须显式通知场景；动画 reader 必须读取 TGFX 当前值，不能读取文档初始值代替运行时值。

## 3. 当前实现核对与执行前检查

### 3.1 本文编写时已核对

- libpag HEAD：4fb312c53。该值仅记录检查版本，不要求实施锁定到此 commit。
- DEPS 中 TGFX：35be4ca2d1b52c4f385bbc9f88a0baa54725e1cd；本地 TGFX HEAD 为 35be4ca。
- 两类 PAGX 阴影公开节点均无 spread。
- ParseInnerShadowStyle、ParseDropShadowStyle 和对应 exporter 分支未读写 spread。
- convertLayerStyle 和两种 bind...StyleChannels 均未传值或绑定 spread。
- BuildInnerShadowStyleFields、BuildDropShadowStyleFields 未注册 spread。
- spec/pagx.xsd 的两类阴影类型均未声明 spread。
- TGFX 两类阴影已有 spread() 和 setSpread(float)。setSpread() 使用 invalidateTransform() 触发重绘，现有注释明确 spread 不改变缓存的 ImageFilter。
- TGFX SpreadUtils 已包含形状近似和几何完全收缩的处理。

### 3.2 执行前检查清单

- [ ] 阅读目标工作区的 CODEBUDDY.md、局部规则及已有变更。
- [ ] 记录分支、HEAD、暂存区和工作区状态；不覆盖或提交用户已有变更。
- [ ] 核对本文入口在目标分支仍存在，确认没有其他 spread 实现。
- [ ] 核对实际构建使用的 TGFX，而不只检查 DEPS；确认两个 setter／getter 均可用。
- [ ] 搜索两类节点的显式复制、比较、默认值和缓存键；存在时列入同一修改范围，不遗漏 spread。
- [ ] 确认阴影 Style 未被诊断宏禁用；不得为让测试通过而跳过 spread 用例。
- [ ] 确认依赖、CMake、Ninja、测试资源及 GPU 环境可用；依赖已经同步时不必重新运行 sync_deps.sh。
- [ ] 当前为 main 时，按仓库规则先创建 feature/codywwang_shadow_spread 分支；非 main 默认在当前分支执行。不自动 stash、reset、checkout 或清理文件。

若实际 TGFX 缺少 spread 能力，或测试证明需要修改 TGFX 算法，停止实施并说明障碍，重新确认方向；不得静默升级依赖、忽略字段、限制负值或降级为仅 XML 支持。

## 4. 接口与协议设计

### 4.1 C++ 节点字段

在以下两个类的 public 区域、blurY 后和 color 前新增：

~~~cpp
float spread = 0.0f;
~~~

| 文件 | 类型 | 注释必须说明 |
| --- | --- | --- |
| include/pagx/nodes/DropShadowStyle.h | pagx::DropShadowStyle | 局部坐标长度；正值向外扩张，负值向内收缩，默认 0 |
| include/pagx/nodes/InnerShadowStyle.h | pagx::InnerShadowStyle | 局部坐标长度；正值扩大向内阴影覆盖，负值缩小覆盖，默认 0 |

公开头文件注释用英语。保留原文件版权年份。Default<T>() 继续从节点默认初始化获取值，不增加重复默认值表。

### 4.2 属性约定

| 项目 | 约定 |
| --- | --- |
| XML 属性名／通道名 | spread |
| 节点类型 | 仅 DropShadowStyle、InnerShadowStyle |
| C++ 类型／动画通道 | float / TypedChannel<float> |
| 默认值 | 0 |
| 单位 | 图层局部坐标长度，与 offsetX、offsetY 一致 |
| 数值语义 | 支持正、负和零；不额外进行符号或幅度钳位，沿用既有数值解析机制 |
| 导出默认省略 | 为 0 时按既有 XMLBuilder 默认值机制省略 |
| 属性注册 | FIELD_FLOAT(Type, "spread", spread, Anim) |
| 布局标记 | 不添加 Layout 标记；RequiresLayout(type, "spread") 为 false |
| 缩放 | 输出缩放交给 TGFX 渲染管线，不在 importer／exporter／LayerBuilder 重复缩放 |

显式 0 导出时可以省略，验收要求数值语义保留，不要求 XML 文本原样保留。

### 4.3 正负值与近似边界

- 外阴影：正值扩张阴影形状，负值收缩阴影形状。
- 内阴影：正值扩大内容内部的阴影覆盖，负值缩小覆盖。TGFX 对遮挡形状内部采用反向 spread；PAGX 不自行取反。
- spread 为 0 时沿用当前 TGFX 的原内容栅格路径，不为了统一实现强制改走非零扩散路径。
- 不规则路径可能使用轮廓包围盒；文本、多个独立几何和只有子图层的组等无单一矢量轮廓内容可能使用内容图像包围盒。
- 圆角矩形、椭圆、描边轮廓、过量收缩的行为以实际 TGFX 版本和测试为准；不承诺任意路径的精确形态学运算。

### 4.4 XML 示例

以下是实施后必须能够解析的自包含示例，也可作为新增测试资源的起点：

~~~xml
<pagx width="240" height="160">
  <Layer id="panel" left="60" top="40" width="120" height="80">
    <Rectangle width="120" height="80" roundness="16"/>
    <Fill color="#E6E6E6"/>
    <DropShadowStyle id="drop" offsetY="4" blurX="3" blurY="3"
                     spread="2" color="#00000040"/>
    <InnerShadowStyle id="inner" offsetY="8" blurX="4" blurY="4"
                      spread="-6" color="#282828"/>
  </Layer>
  <Animations>
    <Animation id="spread-animation" duration="120" frameRate="60" loop="once">
      <Object target="inner">
        <Channel name="spread" type="float">
          <Key time="0" value="0" interpolation="linear"/>
          <Key time="30" value="8" interpolation="linear"/>
          <Key time="60" value="-8" interpolation="linear"/>
          <Key time="90" value="0"/>
        </Channel>
      </Object>
    </Animation>
  </Animations>
</pagx>
~~~

动画 Key.time 的单位是帧；PAGAnimation::setCurrentTime() 的单位是微秒。此示例需要调用方驱动顶层动画，不宣称只加载 XML 就会自动播放。

## 5. 修改文件清单

| 文件 | 必须执行的修改 |
| --- | --- |
| include/pagx/nodes/InnerShadowStyle.h | 新增字段及内阴影语义注释 |
| include/pagx/nodes/DropShadowStyle.h | 新增字段及外阴影语义注释 |
| src/pagx/PAGXImporter.cpp | 在两类 Style 解析入口单独读取 spread |
| src/pagx/PAGXExporter.cpp | 在 WriteLayerStyle 对应分支写出 spread，默认 0 省略 |
| src/renderer/LayerBuilder.cpp | 首次构建 setSpread；分别新增 writer；注册 reader／writer |
| src/pagx/PAGXNodeChannel.cpp | 两类节点字段表注册 spread，标记 Anim |
| spec/pagx.xsd | 两类 Style 类型新增有符号 xs:decimal 属性，默认 0 |
| spec/pagx_spec.md | 5.3.1、5.3.3、D.8、负值示例、单位及兼容／近似边界 |
| spec/pagx_spec.zh_CN.md | 与英文规范对应同步 |
| test/src/PAGXTest.cpp | 读写、场景构建、属性、动画、像素与回归测试 |
| resources/apitest/shadow_spread.pagx | 新增自包含的读写／渲染测试资源，无外部字体或图片依赖 |

核心生产代码为六个文件，XSD、规范、测试和测试资源属于同一完整交付。若需额外修改生产文件，先说明检索证据与必要性；不顺手重构其他格式或相邻功能。

## 6. 核心逻辑与伪代码

以下片段说明准确的实现入口和调用方式，不是已提交的生产实现。

### 6.1 XML 导入

分别在 ParseInnerShadowStyle 和 ParseDropShadowStyle 中、现有阴影属性解析附近增加：

~~~cpp
style->spread = GetFloatAttribute(node, "spread", Default<InnerShadowStyle>().spread, doc);
~~~

~~~cpp
style->spread = GetFloatAttribute(node, "spread", Default<DropShadowStyle>().spread, doc);
~~~

不改变 ParseShadowAttributes 的参数列表，因为该公共函数也被两类阴影 Filter 使用。非法数值沿用 GetFloatAttribute 的既有错误报告机制，不新增另一套 float 解析或钳位规则。

### 6.2 XML 导出

在 WriteLayerStyle 的对应分支、WriteShadowAttributes 调用附近增加：

~~~cpp
xml.addAttribute("spread", style->spread, Default<InnerShadowStyle>().spread);
~~~

~~~cpp
xml.addAttribute("spread", style->spread, Default<DropShadowStyle>().spread);
~~~

不修改公共 WriteShadowAttributes，也不向 Filter 写入属性。测试数值而非要求小数的原始字符表示；采用现有序列化精度。

### 6.3 首次构建

在 convertLayerStyle 的两个阴影分支中，TGFX Make() 后、注册 binding 和返回之前增加：

~~~cpp
tgfxStyle->setSpread(style->spread);
~~~

原样传值，不取绝对值、不取反、不乘倍率。blendMode、excludeChildEffects 和 showBehindLayer 保持既有处理，不借本次任务改变相邻语义。

### 6.4 动态 writer

与现有 BlurX／BlurY writer 同样组织，分别新增：

~~~cpp
static void WriteInnerShadowStyleSpread(void* object, const KeyValue& value, float mix) {
  auto* v = std::get_if<float>(&value);
  if (v == nullptr) {
    return;
  }
  auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
  style->setSpread(MixFloat(style->spread(), *v, mix));
}

static void WriteDropShadowStyleSpread(void* object, const KeyValue& value, float mix) {
  auto* v = std::get_if<float>(&value);
  if (v == nullptr) {
    return;
  }
  auto* style = static_cast<tgfx::DropShadowStyle*>(object);
  style->setSpread(MixFloat(style->spread(), *v, mix));
}
~~~

使用现有绑定保证对象类型，不使用 dynamic_cast，不引入异常、lambda 或为两个短函数建立通用抽象。通过 setter 触发 TGFX 重绘，不直接修改 TGFX 私有状态。

### 6.5 双向运行时 accessor

分别在 bindInnerShadowStyleChannels 和 bindDropShadowStyleChannels 注册：

~~~cpp
_result.binding.setAccessor(
    node, "spread", WriteInnerShadowStyleSpread,
    ReadScalar<tgfx::InnerShadowStyle, &tgfx::InnerShadowStyle::spread>);
~~~

~~~cpp
_result.binding.setAccessor(
    node, "spread", WriteDropShadowStyleSpread,
    ReadScalar<tgfx::DropShadowStyle, &tgfx::DropShadowStyle::spread>);
~~~

reader 从 TGFX 对象读取当前 spread，使现有运行时反向读取链路可用；本次不新增 ViewModel 或 DataBind API。

### 6.6 属性注册

在两个字段表的 blurY 后增加：

~~~cpp
FIELD_FLOAT(InnerShadowStyle, "spread", spread, Anim),
FIELD_FLOAT(DropShadowStyle, "spread", spread, Anim),
~~~

以上两行分别属于对应类型的字段表，不放在同一表中。复用注册机制提供发现、Get／Set／Reset 和动画标记，不扩展其他节点类型。

### 6.7 文档编辑与动画测试调用

文档编辑路径：

~~~cpp
pagx::SetNodeChannel(style, "spread", pagx::KeyValue(-8.0f));
bool layoutChanged = pagx::RequiresLayout(style->nodeType(), "spread");
doc->notifyChange({layer}, layoutChanged);
~~~

测试必须检查 SetNodeChannel 成功、layoutChanged 为 false，并在刷新后重新从 mutableBinding() 获取 TGFX Style；不能依赖刷新前的运行时指针仍代表当前场景。

动画路径：

~~~cpp
auto timeline = scene->getAnimation("spread-animation");
timeline->setCurrentTime(500000);
timeline->apply(1.0f);
~~~

60 fps 下 500000 微秒对应第 30 帧。测试先检查 scene 和 timeline 非空；不能误把 apply(1.0f) 当成设置播放进度，参数是混合权重。

## 7. 分阶段执行步骤

### 阶段 A：建立测试场景与节点／XML 支持

- [ ] 准备测试资源 shadow_spread.pagx，确认根节点、Style ID、通道目标和数值合法。
- [ ] 为两个节点新增字段与公开注释。
- [ ] 实现两类 Style 的 XML 读写，不改变公共 Filter 辅助函数。
- [ ] 增加默认值与 XML 往返测试，两类节点保留缺省 0、正负值和小数值。
- [ ] 将字段注册留到阶段 B，与对应 runtime accessor 同步增加，避免先声明 Anim 能力但没有 writer。
- [ ] 核对 Default<T>()、显式复制／比较／缓存路径，没有遗漏的新状态。

完成条件：两类节点和 XML 都能保值，Filter 表与基类未被扩展。此时尚未注册新通道，不能宣称属性 API、渲染或动画支持完成。

### 阶段 B：接通原生构建与动态通道

- [ ] 两类 TGFX 初始对象都调用 setSpread。
- [ ] 两类 writer 遵循现有类型检查及 MixFloat 规则。
- [ ] 两类 accessor 同时注册 runtime reader 和 writer。
- [ ] 同一实现步骤中为两类字段表注册 spread、标记 Anim，不添加布局标记；不要先单独提交或验证缺失 writer 的中间状态。
- [ ] 增加通用属性测试，包括发现、Get／Set／Reset、错误 KeyValue 类型及 Filter 不支持 spread 的负向测试。
- [ ] 增加内存文档、XML 重载文档到 TGFX 的构建测试。
- [ ] 增加动画读写、插值、混合以及跨零更新测试。
- [ ] 增加 SetNodeChannel 加 notifyChange 后场景刷新的测试。
- [ ] 运行现有 AnimatableChannelsHaveWriters，验证字段注册没有引入缺失 writer。

完成条件：首帧值、运行时值及动态重绘均正确；不在 PAGX 层建立新的扩散或缓存算法。

### 阶段 C：同步规范和 XSD

- [ ] 两份 Markdown 的 5.3.1／5.3.3 属性表增加 spread、默认值、局部长度单位和正负语义。
- [ ] 渲染步骤说明 spread 先影响阴影几何，再参与偏移、模糊与最终裁剪；不把 TGFX 内部取反暴露成调用方要求。
- [ ] D.8 增加 float 通道，只适用于两类 ShadowStyle；D.9 Filter 不增加 spread。
- [ ] 添加负值 XML 示例，说明零值路径、形状近似和旧版本兼容边界。
- [ ] 在 XSD 的 InnerShadowStyleType 和 DropShadowStyleType 中分别新增 xs:decimal、default="0" 的 spread 属性。
- [ ] 解析 XSD 文件并检查新增属性的归属、类型和默认值；如环境已有 XSD 校验器，额外验证自包含示例。
- [ ] 保留现有 layer_styles.pagx 的零扩散示例不变，避免无必要改变已有截图。

XSD 的标量类型沿用已有 xs:decimal 惯例，不引入非负 restriction。若完整 XSD 校验发现既有、不相关的问题，记录并停止扩大修复范围，不以跳过 spread 声明作为解决方式。

### 阶段 D：像素与组合回归

- [ ] 建立与 PAGX 转换独立的 TGFX 参考场景，直接配置相同几何、阴影和背景。
- [ ] 执行第 8 节的基本形状、极值、描边、位置、缩放、近似与三层内阴影对照。
- [ ] 在同一 live scene 和渲染目标上执行连续更新，与每个值的新建参考场景比较，捕获残留帧／旧边界裁剪。
- [ ] 保存关键截图，通过 Baseline::Compare 接入截图回归。
- [ ] 向用户展示新截图；需要接受时等待用户主动执行 /accept-baseline。

完成条件：参数和像素均符合 TGFX 参考，不以截图变亮或固定单 GPU 的 RGB 点值代替正确性证据。

### 阶段 E：构建、全量回归与交付

- [ ] 按第 9 节先格式化，再开启所有模块构建 PAGFullTest。
- [ ] 运行新增用例和已有相关用例。
- [ ] 运行 PAGXTest 全套。
- [ ] 运行 PAGFullTest 全量并记录结果；检查未预期的其他模块回归。
- [ ] 检查文件 diff 和默认零扩散兼容性。
- [ ] 更新本文执行状态／实际测试记录，不将未运行项目写成通过。
- [ ] 按仓库规则仅提交本次会话变更，不自动推送。

## 8. 测试设计与验收矩阵

### 8.1 命名及组织

优先在 test/src/PAGXTest.cpp 使用现有 PAGX_TEST(PAGXTest, Name)，新增测试统一包含 ShadowSpread，方便筛选。下表是计划名称，不代表当前已有测试。

| 建议测试名 | 场景与断言 |
| --- | --- |
| ShadowSpreadDefaults | 两类新建节点、XML 缺省和显式 0 都为 0；运行时 getter 为 0；零扩散图像保持原样 |
| ShadowSpreadXMLRoundTrip | 两类型覆盖 8、-8、2.25、-19.587、0 和缺省；反复导出／导入至少两轮，检查值和符号；非零属性存在、默认属性省略 |
| ShadowSpreadNodeChannels | ChannelExists、ListChannels、Get／Set／Reset、IsAnimatableChannel 和 RequiresLayout；float 可写，错误 KeyValue 类型被拒绝且值不变；Reset 回到 0；Filter 和无关 Style 不存在该通道 |
| ShadowSpreadSceneBuild | 直接内存文档及 XML 重载后构建 PAGScene；分别获取两个 TGFX Style，检查初始 spread 原样传入 |
| ShadowSpreadAnimationRoundTrip | 两类节点均有目标为 Style ID 的 float spread 通道，正负关键帧及插值类型重复读写不丢失；重载后实际求值 |
| ShadowSpreadRuntimeAccessors | RuntimeBinding.hasWriter、apply、read；全量写入和当前值读取都正确，reader 不返回文档旧值 |
| ShadowSpreadAnimationMix | 0 → 8 → -8 → 0；线性跨零及既有插值方式；mix=1、0、0.5；叠加时间线从当前运行时值混合 |
| ShadowSpreadNotifyChange | 文档 SetNodeChannel 后通知 owning Layer；无需布局；重取 TGFX 对象，检查值及像素刷新；Reset 后同样通知 |
| ShadowSpreadBasicRendering | 圆角矩形和椭圆，两类阴影 × 负／零／正；内阴影最终仍在内容轮廓内，外阴影正扩散不被旧边界裁掉；外阴影覆盖 showBehindLayer 两种值 |
| ShadowSpreadCollapsedGeometry | 从正常值到过量收缩再恢复；外阴影消失／内阴影对应结果与 TGFX 一致，无崩溃、异常矩形或残留帧，原内容和背景不被错误清除 |
| ShadowSpreadPositionAndScale | 非零位置及适当的父级变换；1 倍和 2 倍输出分别与同配置 TGFX 参考对照，确认没有重复缩放 |
| ShadowSpreadStrokeAndApproximation | 带描边的基本轮廓、不规则闭合路径、无单一轮廓的多几何／子图层内容；与当前 TGFX 一致，不自己实现精确扩散参考 |
| ShadowSpreadThreeInnerShadows | 实际三层参数，白底、无玻璃；对照直接 TGFX；另渲染 spread 全为 0 的对照，证明用例能检出参数丢失 |
| ShadowSpreadThreeInnerShadowsWithGlass | 同样场景加入现有 GlassStyle，仅做组合回归，参数、样式顺序、excludeChildEffects 与参考一致，不修改玻璃算法 |
| ShadowSpreadDynamicRedraw | 同一运行时对象和渲染目标循环 0 → 正 → 负 → 0；每帧与新建 TGFX 参考一致，末帧与初始零值图一致 |

已有回归入口至少包含：ChannelDropShadow、GlassStyleRoundTripAndRuntime、AnimationAllTypesRoundTrip、AnimatableChannelsHaveWriters、NodeChannelTableConsistency、NotifyChangeFromSetNodeChannel。

### 8.2 数值与混合断言

- 普通测试的字号、几何、位置、spread 和缩放尽量使用整数，例如尺寸 120 × 80、圆角 16、spread ±8。
- 小数只用于 XML 精度测试和原始实际回归，不为了凑整数改掉复现参数。
- 数值比较使用 EXPECT_FLOAT_EQ 或针对既有序列化精度的明确比较，不要求原始小数字符串逐字节相同。
- 部分混合例：当前 8，目标 -8，mix=0.5，结果应为 0；mix=0 不写入。
- 重复混合会基于上次运行时值继续计算。需要独立期望时重设状态或新建场景，不误用初始文档值。
- 整数帧对应 setCurrentTime 的微秒值要明确换算，避开 loop 终点造成回到起点的假失败。

### 8.3 像素参考的建立原则

1. PAGX 路径通过真实 LayerBuilder／PAGScene；参考路径直接创建 TGFX 场景并调用 setSpread，不反向复用待验证的转换函数。
2. 两条路径使用同一个 GPU context，独立渲染目标，相同内容、位置、样式顺序、背景、颜色、透明度、裁剪和缩放。
3. 静态图比较不能代替连续更新测试。动态测试保留同一 live 场景和渲染目标，使用正常自动清理／局部重绘语义，不人为重建场景掩盖缓存问题。
4. 通过现有 Surface::readPixels 或项目已有像素比较能力读取同格式数据。优先复用测试设施，不建立通用图像差分框架。
5. 同 context、同 TGFX 配置应先要求逐像素一致。出现差异先检查坐标系、参考搭建、预乘 alpha、清理和颜色空间；仍有差异则定位根因，不能静默放宽容差。
6. 为验证非零值确实生效，另选不会被裁剪或完全收缩的清晰场景，证明非零图与零值图不同；该断言只是敏感性检查，不是正确性参考。
7. 输出 2 倍时分别比较两条路径的 2 倍图，不要求 1 倍图简单放大后与 2 倍抗锯齿图逐像素一致。
8. 整个画布预留足够空间观察外扩；不要使用本来就裁掉阴影的父级作为外扩边界测试。

跨平台持久截图沿用项目 Baseline 机制；不同 GPU 的差异不以一组固定 RGB 点值裁决。若既有测试设施无法表达同 GPU 参考对照，先说明缺口并确认最小测试补充方案。

### 8.4 实际三层内阴影参数

保留顺序：浅色、上移深色、下移深色。

| 参数 | 浅色 | 上移深色 | 下移深色 |
| --- | --- | --- | --- |
| offsetX | 0 | 0 | 0 |
| offsetY | 19.587 | -19.587 | 19.587 |
| blurX／blurY | 7.34513 | 2.44838 | 2.44838 |
| spread | -19.587 | -19.587 | -19.587 |
| color | #E6E6E6 | #282828 | #282828 |

- 底板尺寸：23.5044 × 102.832；圆角属性：489.676，交由现有形状语义处理，不自行换算。
- 底板填充：黑色，alpha 约 0.004；背景：白色。
- 阴影和玻璃均配置 excludeChildEffects=true；PAGX 与 TGFX 的样式插入顺序完全一致。
- 组合玻璃参数：refraction=70、depth=30、frost=6、dispersion=20、splay=20、lightAngle=0、lightIntensity=25。
- 先无玻璃，再有玻璃；两者分别对照直接 TGFX，确保不将玻璃差异误判为 spread 问题。

若发现现有 LayerBuilder 的其他参数映射与参考不一致，先隔离并报告，重新确认是否单独处理，不能偷偷调整 spread 让图像看起来接近。

### 8.5 测试资源与截图规则

- shadow_spread.pagx 必须自包含，既包含正外扩也包含负内扩，默认展示帧有明确的非零扩散效果。
- 核对现有资源扫描是否自动纳入该文件，避免新增资源意外引入未登记的截图 key。
- 普通像素用例优先程序化构造，实际小数回归也可程序化构造，避免大量重复 XML。
- 新截图 key 建议为 PAGXTest/ShadowSpread...，遵循现有 Baseline::Compare 形式。
- 图片输出位于 test/out/PAGXTest/，基准是对应的 ..._base.webp。
- 基准缺失导致失败不代表算法一定错误，但也不能写成测试已通过；先完成参考对照并向用户展示截图。
- 不直接执行 accept_baseline.sh、UpdateBaseline target，不手改／覆盖 version.json。接受截图基准的唯一方式是用户主动执行 /accept-baseline。
- 未接受的新基准必须在交付记录中标为待确认，不绕过截图测试，不通过改版本号使比较被跳过。

## 9. 构建与验证命令

以下均在 libpag 根目录执行。本文编写阶段不运行功能构建；以下用于实施阶段。

### 9.1 格式化及所有模块构建

~~~bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest
~~~

- 必须先运行格式化并忽略其报错；该脚本会全目录就地格式化，执行前记录用户已有变更，执行后检查 diff，不覆盖用户工作。
- 必须使用 PAG_BUILD_TESTS=ON，不能只编一个禁用了其他模块的小目标代替编译验证。
- CMake 或编译失败时定位根因，不用强转、额外默认参数、注释代码或绕过检查来“通过”。

### 9.2 新增和相关测试

~~~bash
./cmake-build-debug/PAGFullTest --gtest_filter="PAGXTest.*ShadowSpread*:PAGXTest.ChannelDropShadow:PAGXTest.GlassStyleRoundTripAndRuntime:PAGXTest.AnimationAllTypesRoundTrip:PAGXTest.AnimatableChannelsHaveWriters:PAGXTest.NodeChannelTableConsistency:PAGXTest.NotifyChangeFromSetNodeChannel"
~~~

先核对列表中实际存在新增测试，防止过滤条件匹配 0 个用例形成假通过。非零退出码表示失败，应分类调查，不在状态未改变时重复执行同一失败命令。

### 9.3 PAGX 全套与项目全量

~~~bash
./cmake-build-debug/PAGFullTest --gtest_filter="PAGXTest.*"
./cmake-build-debug/PAGFullTest
~~~

按顺序推进；前一步未解决的生产故障先修复，截图基准等待用户确认时记录阻塞状态。记录测试统计、失败名称、构建配置和 GPU 环境，不只记录“跑过”。本任务不需要运行会清理其他构建／结果目录的 autotest.sh。

### 9.4 XSD 与变更检查

~~~bash
git diff --check
git diff --stat
~~~

用环境已有的 XML 工具或 Python 标准库检查 XSD 的 XML 结构及新增属性归属；如已有 xmllint，可执行：

~~~bash
xmllint --noout --schema spec/pagx.xsd resources/apitest/shadow_spread.pagx
~~~

不为一次 XSD 检查擅自安装新依赖。结构检查和完整 XSD 校验要在执行记录中区分；未做完整校验不能宣称已通过。

### 9.5 仅在确有 TGFX 层问题时使用本地源码调试

如同级 ../tgfx 存在且问题定位到 TGFX，先说明需要偏离“不改底层算法”的前提并确认方向，再按仓库规则使用独立目录：

~~~bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DTGFX_DIR=../tgfx -B cmake-build-debuglocal
cmake --build cmake-build-debuglocal --target PAGFullTest
~~~

本地 TGFX 与正式依赖版本必须记录。使用修改后的本地引擎通过测试，不等于正式依赖已通过验收。

## 10. 兼容性、风险与处理原则

| 项目 | 处理原则 |
| --- | --- |
| 旧 PAGX 由新库读取 | 缺省 spread=0，验证原有外观和默认省略行为保持不变 |
| 新 PAGX 由旧库读取 | 不能显示扩散效果；未知属性是否忽略／报错需验证具体旧解析器，不作未经验证的承诺 |
| 旧 XSD 校验新 XML | 旧 XSD 没有该属性，不能承诺通过；本次必须同步新 XSD |
| C++ ABI | 两个公开节点增加成员会改变布局；下游头文件、库和二进制必须配套重编，用户已确认接受 |
| 参数丢失路径 | XML、内存构建、字段复制及动态绑定都需覆盖，不只修 XML |
| 动画残留 | 使用 TGFX setter 和连续帧参考对照，不能重建 scene 掩盖问题 |
| 形状近似 | 写明当前 TGFX 边界，不擅自增加精确扩散算法 |
| 外扩裁剪 | 在充分预留画布的用例中验证 TGFX 边界生效，不通过扩大所有图层边界绕过根因 |
| 缩放重复 | 传原值，分别对照 1 倍／2 倍场景；不得在两层同时乘倍率 |
| 未预期实现障碍 | 停止、说明证据并重新确认，不静默缩减完整交付范围 |
| 已有工作区变更 | 不恢复、不覆盖、不自动提交；全目录格式化造成交叉变更时先检查并确认处理方式 |

旧版本兼容性可先只读核对原需求中 7cb58c1 的解析函数；如该对象本地不可用，不擅自切分支或联网拉取。没有实际执行旧解析器的证据时，交付说明保持“未验证”，且不阻塞新库读取旧文件的本次核心验收。

## 11. 交付与提交

### 11.1 完成检查表

- [ ] 两类公开节点字段及注释完整，默认 0。
- [ ] 两类 XML 导入／导出保留非零 spread，负值和小数不被截断。
- [ ] 内存文档和 XML 文档的初次构建正确调用 TGFX setSpread。
- [ ] 通用属性发现、读写、重置和通知刷新可用。
- [ ] runtime reader／writer 完整，动画 float 通道读写和混合正确。
- [ ] 连续跨零更新重绘正确，无裁剪或残留帧。
- [ ] 中英文规范、D.8、示例和 XSD 同步，Filter 未扩展。
- [ ] 基本形状、缩放、极值、近似和三层实际参数均有 TGFX 对照证据。
- [ ] 已完成全模块构建、新增／相关用例、PAGX 全套和项目完整回归，或准确记录未完成与阻塞项。
- [ ] 新截图基准已由用户通过 /accept-baseline 确认；未确认时不宣称整个验收已完成。
- [ ] diff 没有其他格式功能修改、无关重构或用户原有变更。

### 11.2 交付记录模板

| 项目 | 实施后填写 |
| --- | --- |
| libpag 分支／commit | 待实施 |
| 实际 TGFX 版本／构建目录 | 待实施 |
| 修改文件清单 | 待实施 |
| 格式化／所有模块构建 | 未执行 |
| 新增与相关测试统计 | 未执行 |
| PAGX 全套统计 | 未执行 |
| 全量测试统计 | 未执行 |
| 同 GPU 像素对照 | 未执行 |
| XSD 结构检查／完整校验 | 未执行 |
| 截图路径与基准确认 | 未生成 |
| 旧解析器行为验证 | 未验证 |
| 未解决问题／阻塞项 | 实施后记录，不默认为无 |

### 11.3 提交规则

- 完成后按仓库规则自动 commit，不自动 push、不创建或发布 PR。
- 不在 main 自动提交，不 amend 已有 commit。
- 使用 git commit --only 并明确列出本次会话变更文件；不得用 git add . 将其他变更混入。
- 提交主题遵循仓库英语、120 字符内、末尾英文句号且中间无其他标点的规则。建议：Support shadow spread in PAGX rendering and animation.
- 截图基准变更只有在用户执行 /accept-baseline 后才可纳入，且需确认属于本次任务。
- 交付说明明确：仅 PAGX 原生支持；其他格式与 CoCraft 尚未接入。实际编辑器主 Tab 的最终 PNG 验收属于后续 CoCraft 接入，不作为本次 libpag 可直接完成的承诺。

## 12. 执行顺序摘要

~~~text
确认工作区及 TGFX 能力
  → 两类公开字段
  → XML 读写
  → 初次 TGFX 构建
  → runtime reader／writer 与 Anim 字段注册同步接入
  → 动画读写与混合
  → 文档编辑通知刷新
  → 中英文规范和 XSD
  → 基本／动态／实际组合像素对照
  → 格式化与所有模块构建
  → 新增和相关测试
  → PAGX 全套与项目全量
  → 用户确认截图基准
  → 核对 diff 与交付记录
  → 仅提交本次变更，不推送
~~~

本方案的最小完整交付是“字段、协议、原生渲染、属性与动画、规范和测试”同时具备，而不是将任务降级为新增一个能被 XML 保存的字段。
