# PAGX PR 3826 已确认修复方案

初始来源：[PAGX PR 3826 已确认修复方案](https://chatgpt.com/space/page_10f412c402d08191aed705349af2aac1)。后续确认的方案以本仓库文档为执行和追加入口。

本文档用于在另一个会话中实施 [PR 3826](https://github.com/Tencent/libpag/pull/3826) 已讨论并确认的修复。当前记录命中测试一致性、目标绘制失败传播、复用已有栅格化挂接工具、稳定复用输出视图状态、纯渲染变更的声明校验快路径、输出窗口数值校验复用六项。先扩展已有工具，再接入输出视图；失败传播与稳定状态需要合并实施，校验快路径沿用同一 runtime 恢复时序。后续评论的方案经用户确认后再追加。

## 当前执行状态

核对日期：2026 年 11 月 13 日。工作目录：`/Users/codywwang/Desktop/PAGX/libpag`。分支：`feature/codywwang_glass`。本次更新前的核对基点为 `399aef760`。执行前重新检查实际工作区与提交，保留其他会话产生的改动。

| 项目 | 状态 | 执行动作 |
| --- | --- | --- |
| 命中测试一致性 | 本地已提交 `ba4a8a179` | 复核实现并回归，不重复实现 |
| 目标绘制失败传播 | 方案已确认，尚未实施 | 与稳定输出视图状态合并实施 |
| 复用已有栅格化挂接工具 | 方案已确认，尚未实施 | 扩展 RasterLayerParent，输出视图接入已有实现 |
| 稳定复用输出视图状态 | 方案已确认，尚未实施 | 仅修改 libpag，不修改 TGFX |
| 纯渲染变更的声明校验快路径 | 方案已确认，尚未实施 | 只跳过能够证明不影响声明语义的重校验 |
| 输出窗口数值校验复用 | 方案已确认，尚未实施 | 提取现有规则，导入与程序化校验共用 |

## 命中测试一致性

对应 [P2 命中测试两套 API 结论矛盾](https://github.com/Tencent/libpag/pull/3826#discussion_r4535356311)。原先 `getLayersUnderPoint()` 在 target view 下直接返回空，而 `PAGLayer::hitTestPoint()` 已应用 output origin 并可以命中。修复统一两套 API 的输出范围，保留包围盒与实际形状测试的精度差异。

本地实现提交：`ba4a8a179 Align target view hit testing with the output window.`。该提交存在于当前分支，后续执行会话应先确认它仍在提交历史中。

### 行为契约

| 输出状态 | 枚举 API | 单层命中 API |
| --- | --- | --- |
| None | 保持原有全场景行为 | 保持原有行为 |
| Valid | 查询 target 自身及后代 | 仅允许测试 target 自身及后代 |
| Invalid | 返回空数组 | 返回 false |

有效 target view 还要求：输出窗口存在、runtime target 存在、zoom 大于零，且 surface 点转换后的 root 点位于 `outputBounds` 内。窗口采用 `ToTGFX(bounds).contains()` 的左上包含、右下排除规则。沿用 TGFX 的既有命中语义，包括 alpha 为零仍可命中。

### 实现复核

涉及文件：`include/pagx/PAGScene.h`、`src/pagx/PAGScene.cpp`、`include/pagx/PAGLayer.h`、`src/pagx/PAGLayer.cpp`、`test/src/PAGXRuntimeTest.cpp`。

1. `PAGScene::prepareHitTest()` 集中检查状态、窗口和坐标转换，并从当前 binding 获取 runtime target；通过 `getOutputBounds()` 覆盖直接清空字段但未调用 `notifyChange()` 的情况。

2. 枚举从 target 开始递归，保留 TGFX 的结果顺序和 registry 映射；进入递归前补齐 target 自身的可见性、mask、scrollRect 检查。

3. `PAGLayer::hitTestPoint()` 委托 scene，按 runtime 子树归属和 target 到当前层的路径检查限制，再执行原有包围盒或形状测试。

4. target 外祖先仍贡献坐标变换，但不参与可见性、mask、scrollRect 的拒绝判断。这与当前 `DrawOutputTarget()` 的输出路径一致。

5. `surfaceToRoot()` 和 `rootToSurfaceMatrix()` 保持坐标转换职责；命中查询保持只读。

### 验收覆盖

现有新增用例以 `PAGXRuntimeTest.TargetViewHitTest` 开头，覆盖 origin 与显示变换、子树隔离、窗口边界、无效声明和零 zoom、target 及内部路径限制、外部祖先约束、target 被外部层引用为 mask、查询状态不变，以及形状精度。普通文档的 `PAGXTest.HitTest*` 继续回归。`notifyChange()` 导致 runtime 树重建后，有效查询使用重新取得的 handle。

## 目标绘制失败传播

对应 [P2 DrawOutputTarget 返回 void 导致失败被当作成功](https://github.com/Tencent/libpag/pull/3826#discussion_r4535357551)。`OutputTargetParent` 无效时，辅助函数直接退出；`PAGScene::renderTo()` 随后仍清除 dirty 并返回成功，CLI 也继续回读和编码图片。`pagx::Record()` 通过 `renderTo()` 间接受影响。

### 返回值含义

将 `DrawOutputTarget()` 改为返回 `bool`，传播能够检测到的参数、层级关系和重挂准备失败。TGFX 的 `Layer::draw()` 本身返回 `void`，因此该返回值表示准备完成并执行了绘制调用，不能代表所有 GPU 工作均执行成功。目标没有可绘制内容、不可见、透明或完全被裁剪时，正常完成仍返回 true。

### 修改辅助函数和共享状态

修改 `src/pagx/utils/OutputView.h` 与 `src/pagx/utils/OutputView.cpp`，签名调整为：

```cpp
bool DrawOutputTarget(tgfx::Canvas* canvas,
                      const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer);
```

按顺序处理：

1. canvas、root、target 为空时返回 false；target 不属于 root 时返回 false。复用 runtime 层级检查，避免按源节点 ID 判断实例归属。

2. `targetLayer == root` 时调用 `root->draw(canvas)`，正常返回 true。

3. 其余情况由 `OutputTargetState` 委托扩展后的 `RasterLayerParent` 准备 wrapper；输出视图中非 root 的 target 缺失父层或准备失败时返回 false。保留栅格化工具原有的无 parent 支持，不把输出视图的入口约束强加给已有导出路径。

4. 挂接和恢复只在共享的 `RasterLayerParent` 中实现。检查 `replaceChild()` 和 `addChild()` 的结果，只有重挂全部完成后才进入有效态。若修改树后失败，恢复已修改的父子关系和 mask，再返回失败；回滚信息不能因清空 wrapper 而丢失。

5. 正常路径继续复用原有坐标变换、背景采样和 mask 处理。每次绘制都恢复 canvas 状态；树与 mask 的恢复时机由状态生命周期决定：scene 失效、退出输出模式或销毁时恢复，CLI 单次调用结束时恢复。具体生命周期见下一节。

6. 更新函数注释，准确说明 bool 的含义。

### 接入 scene 和 Record

修改 `src/pagx/PAGScene.cpp` 的有效 target view 分支，通过私有方法 `ensureOutputTargetState()` 懒准备并复用状态；准备或绘制入口检查失败时立即返回 false。仅在成功路径调用 `clearAllViewModelsDirty()` 并将 `outputViewDirty` 置为 false：

```cpp
if (!ensureOutputTargetState() || !outputTargetState->draw(canvas)) {
  return false;
}
clearAllViewModelsDirty();
outputViewDirty = false;
return true;
```

有效声明下的绘制准备失败不能消费待绘制状态。无效声明以及窗口被清空时，先释放输出状态，保留现有的 dirty 消费策略。`PAGScene::draw()` 已处理 `renderTo()` 失败并释放 context；`pagx::Record()` 已在 `renderTo()` 失败时返回 nullptr，复用这些返回路径即可。scene 不在每帧调用会在返回时恢复树的单次 `DrawOutputTarget()` 封装。

### 接入 CLI

修改 `src/cli/CommandRender.cpp` 的目标层绘制分支。辅助函数失败后释放 context 锁，输出明确错误并返回空 bitmap；现有 `RunRender()` 会将空 bitmap 转为退出码 1：

```cpp
if (!pagx::DrawOutputTarget(canvas, rootLayer, targetTgfxLayer)) {
  device->unlock();
  std::cerr << "pagx render: failed to prepare target layer for rendering\n";
  return {};
}
```

### 回归用例

在 `test/src/PAGXRuntimeTest.cpp` 补充行为测试，命名使用 `TargetView` 前缀，覆盖：

- 辅助函数拒绝空参数、脱离 root 的 target 和属于其他 root 的 target，失败时不改变原有树关系。

- 创建有效 scene 后，从 runtime 树摘除 target，但保留 binding；`draw()` 返回 false。分别覆盖首次准备前以及已有准备态后的损坏；断言 dirty 保持 true 时，先通过初始待绘制状态或显示参数变化建立 dirty 为 true 的前置条件。

- 同类故障通过 `pagx::Record()` 返回 nullptr，待绘制状态未被清除。使用独立 fixture 或恢复树后再测试，避免前一条用例状态影响结论。

- 修复 runtime 父子关系后再次绘制成功，并正常消费 dirty。

- 单次辅助函数返回后，父子关系、兄弟顺序和 mask 完整恢复。scene 则在帧间保持同一准备态，失效或退出输出模式后完整恢复。两种入口每次绘制都恢复 canvas 状态。

- 没有可绘制内容的目标或不可见目标正常完成时返回 true。

上述摘除 runtime target 是用于覆盖错误路径的测试构造，不代表已经找到正常文档触发该故障的输入。

CLI 保留现有 `PAGXCliTest.Render_TargetView*` 回归，检查正常输出、显式选择、crop 和非法声明处理；检查新的 helper 失败分支确实在回读和编码之前释放锁并返回空结果。

## 复用已有栅格化挂接工具

对应 [P2 OutputTargetParent 与 RasterLayerParent 重复](https://github.com/Tencent/libpag/pull/3826#discussion_r4535361783)。按 CR 建议，以 `src/pagx/utils/RasterUtils.cpp` 中已有的 `RasterLayerParent` 为基础提取并扩展共享内部工具，补充外部 mask ownership 的释放与恢复；两种背景输出策略在注释中说明取舍。

### 复用方向和职责

1. 将已有 `RasterLayerParent` 从匿名命名空间提取至 utils 下的共享内部文件，例如 `RasterLayerParent.h` 与 `.cpp`，保留原有挂接和恢复机制。不让 `RasterUtils` 改为依赖仅为本 PR 新建的输出视图绘制框架。
2. 在原类上补齐有效态检查、部分准备失败的回滚、显式且幂等的 reset，以及非复制的生命周期管理。原 parent、位置、target、wrapper、已释放的 mask owner 等挂接恢复数据只由这一处维护。
3. 增加显式释放外部 mask owner 的能力：沿用 `FindMaskOwner` 对 target 子树内 owner 的排除规则，记录被解除的关系，并在 reset 或析构时恢复。已有 Raster 调用默认保留原 mask 处理方式；输出视图按自身语义显式启用释放，避免合并时改变既有导出行为。
4. 删除 `OutputTargetParent`。`OutputTargetState` 只持有输出视图所需的 root、target 绑定信息并委托共享工具，负责 scene 的准备态复用和失效时机，不再实现另一套换父、mask 查找或恢复逻辑。
5. `RenderMaskedLayer()` 继续使用原有 `MaskedLayerDrawer` 和共享的 `RasterLayerParent`，采用单次调用的作用域生命周期。scene 则让持有该工具的状态跨帧存在；CLI 通过局部输出状态完成单次调用。

### 保留现有栅格化契约

- 坐标与尺寸继续按既有入口处理。scene、CLI 和普通 PPT 栅格化使用 root；SVG 传入 target 原 parent，保留 `wrapper->getRelativeMatrix(coordinateSpace)`、bounds、pixelScale 与 PNG 放置规则，避免增加二维变换拼接或重复应用父变换。
- `root == target` 保留现有直接绘制分支。低层 `RasterLayerParent` 保留无 parent target 的支持，并在结束时将其从 wrapper 摘除；scene、CLI 的 root 归属校验由各自入口承担，不反向收紧已有栅格化能力。
- 已有 alpha、mask、scrollRect、混合和 perspective、preserve3D 的处理仍经过原有父层合成路径。共享重构只增加所需能力与失败检查，不重写正确的绘制实现。
- 保留 GPU context 锁、像素尺寸上限、PNG 编码和 SVG/PPT 的失败降级规则。PPT 完全裁剪或空 bounds 的“成功但不输出”语义保持。

### 背景输出策略说明

| 入口 | 交付内容和用途 |
| --- | --- |
| target view 与 RenderMaskedLayer | 交付 target，原文档祖先和前序兄弟为 Glass、BackgroundBlur 等提供采样上下文 |
| RenderLayerCompositeWithBackdrop | 将前序背景和 target 一起烘焙，供 PPT 不支持的混合模式等场景使用 |

保留 `BackdropCompositeDrawer` 与 `BackdropSiblingMask` 的 root 绘制和后序兄弟隐藏逻辑，说明它们如何避免后序内容被提前烘焙、再被导出器重复输出。它们不改为 target-only 入口。临时可见性在所有返回路径恢复，原本隐藏的层仍保持隐藏。HTML 未直接使用这些栅格化入口，不扩大修改范围。

### 准备失败与验收

在 `MaskedLayerDrawer` 中接入共享工具的有效态检查，将可检测的准备失败传回 `RenderToPNG()`，返回 nullptr，不能编码成有效的透明 PNG。可将内部 drawer 约定统一为 bool，让 `RenderToPNG()` 在 false 时结束；其他既有 drawer 正常完成时返回 true，保留其绘制步骤。所有失败路径仍保证 GPU unlock 和作用域状态恢复，不把 TGFX 的 void 绘制调用包装为任意 GPU 错误检测。

复用现有回归并补充共享能力测试：

- `PAGXUtilsTest.RasterLayerUsesParentCompositing`、`RasterLayerPreservesMaskScrollRectAndAlpha`，以及 SVG parent-space、3D 和 scrollRect 用例。
- 共享工具在普通调用与显式 mask 释放两种用法下的正确行为，以及原 parent、位置、mask 和无 parent target 的恢复。
- root 与 target 相等、prepare 部分失败、单次调用结束与 scene reset 的恢复；失败返回 nullptr，正常透明内容和既有空 bounds 规则保持。
- PPT 的非 Normal blend、Glass 背景烘焙、祖先链各层后序兄弟排除与可见性恢复。

## 稳定复用输出视图状态

对应 [P2 每帧绘制改树造成额外分配和失效](https://github.com/Tencent/libpag/pull/3826#discussion_r4535358918)。当前 `FindMaskOwner()` 遍历树，wrapper 换父会递归更新 target 子树的 root 归属，并使祖先的边界和相关状态失效。此项在 libpag 内通过调整生命周期解决，继续使用 TGFX 现有接口和合成路径。

### 共享状态与所有权

在 `src/pagx/utils/OutputView.h` 和 `.cpp` 中用内部 `OutputTargetState` 管理输出视图生命周期，内部持有扩展后的 `RasterLayerParent`。挂接、mask 释放和恢复复用已有工具；输出状态只负责校验、持有和委托，不新增发布 SDK 接口或第二套底层绘制框架。生命周期入口轮廓为：

```cpp
class OutputTargetState {
 public:
  bool prepare(const std::shared_ptr<tgfx::Layer>& root,
               const std::shared_ptr<tgfx::Layer>& target);
  bool draw(tgfx::Canvas* canvas);
  void reset();
};
```

1. 输出状态持有 root、target 绑定信息与共享挂接工具；wrapper、原 parent 和位置、外部 mask owner 等恢复数据由 `RasterLayerParent` 唯一维护，并保存可靠引用。析构通过幂等的 `reset()` 释放工具，避免依赖已被释放的 runtime 裸指针。
2. `prepare()` 校验所属树后，使用已有挂接工具准备 identity wrapper，再显式启用外部 mask owner 的释放能力。target 自身的 mask 以及 target 子树内部的关系保持原有规则。准备失败由工具回滚，不在输出状态里复制恢复算法。
3. 外部 mask ownership 在 scene 准备态期间保持解除，工具随 `reset()` 释放时恢复。此处修改的是 runtime 层关系，PAGXDocument 中的源节点和 mask 声明保持原样。
4. `draw()` 委托共享工具检查准备态和挂接关系，再沿用 wrapper 到 root 的实时相对矩阵绘制；仅做 canvas 的作用域保存和恢复。热路径不调用 `FindMaskOwner()`、`Layer::Make()`、换父或 `setMask()`。
5. 保留原树中的祖先和兄弟，继续让 TGFX 采样原有背景。target 的 transform、alpha、visibility、mask、scrollRect 和混合行为仍由同一 wrapper 合成路径应用一次。
6. CLI 的 `DrawOutputTarget()` 创建局部输出状态，依次 prepare、draw，在作用域结束时 reset；`RenderMaskedLayer()` 继续使用单次作用域的共享挂接工具。scene 只延长该工具的生命周期，不形成另一套准备与恢复逻辑。

### Scene 生命周期

在 `include/pagx/PAGScene.h` 中前置声明内部状态类，并添加私有状态指针和 `ensureOutputTargetState()`。在 `src/pagx/PAGScene.cpp` 中实现生命周期：

| 时机 | 状态处理 |
| --- | --- |
| 首次有效绘制 | 从当前 binding 获取 target，准备一次状态 |
| 重复绘制或有效 zoom、offset、background 改变 | 直接复用状态，仅更新绘制参数 |
| `onNodesChanged()` 入口 | 先 reset，再进入 foreign、Document 或增量刷新分支 |
| `buildRuntimeTree()` 入口 | 在移除旧 root、替换 binding 前再次保证 reset |
| 图片资源通知 | 保守地使准备态失效，并保留原有图片刷新和 dirty 通知 |
| 声明变为 None 或 Invalid，或者窗口缺失 | 恢复原树和 mask，释放准备态 |
| scene 析构 | 在相关 runtime 对象释放前 reset |

第一版对所有节点编辑通知保守地复位。后述声明校验快路径不改变这一要求。`refreshNodes()` 与 `syncChildren()` 会按源树顺序重排、挂接 slot，因此必须先恢复原树；不能等增量刷新或全量重建结束后才清理 wrapper。完成刷新与必要的输出声明校验后，下一次有效绘制再懒准备。

`ensureOutputTargetState()` 区分未准备和准备关系损坏。未准备时可初始化；已准备但 target、root 或挂接关系不符时返回失败，不能偷偷重挂并将错误当作成功。只有文档变更等明确的失效路径才丢弃旧状态、允许按新树重新准备。

命中测试、全局矩阵和 bounds 查询保持只读。identity wrapper 不应改变坐标结果，子树归属继续按实际 runtime 层级判定，查询结果不暴露内部 wrapper。保留 `outputViewDirty` 对文档、资源和显示参数的跟踪，并将注释从每帧临时换父的说明更新为直接渲染模式下的变更跟踪。

聚合 bounds 需要单独验证：target 原先作为 mask 时会被祖先聚合边界排除，长驻准备态解除 ownership 后，root 或外部祖先的 `getBounds()`、`getGlobalBounds()` 结果可能改变。identity wrapper 不能单独保证这些查询兼容。实施时先补 prepare 前后的对比用例，保留原有查询语义；若需要调整已确认的缓存或恢复策略，先向用户说明并确认，不能静默改变 API 行为。

### 验收与性能对比

- 重复绘制、平移、缩放和背景色变化复用同一准备态；帧间父子关系和已解除的外部 ownership 稳定，canvas 状态每帧恢复。
- 普通、半透明、不可见、mask、scrollRect、Glass 和 BackgroundBlur 的输出保持一致；祖先和前序兄弟只提供采样，后序兄弟不混入结果。
- target 切换、子层增删和重排、Document 或外部文档导致的全量重建，以及图片替换之后，使用新树和新资源绘制。
- Valid 到 None、Valid 到 Invalid、直接清空 bounds 未通知、准备失败和 scene 销毁时，原父子关系、兄弟顺序和 mask 正确恢复，不残留旧 runtime 引用。
- 多个 scene 使用同一文档时，各自的输出状态互不影响；首次准备前、稳定绘制后和失效重建后的命中与坐标查询一致。单独覆盖 root、外部祖先和 mask owner 的局部及全局 bounds，保留聚合查询语义。
- 对相同的大文档和 target 子树连续平移、缩放，比较修改前后的 CPU profile 与耗时。验收重点是准备后的交互帧移除查找 owner、分配 wrapper、换父和 mask 切换的工作；性能测试不使用依赖机器速度的单元测试阈值。

target view 目前绕过 DisplayList，仍不启用 tile 或 DisplayList 子树缓存。本项减少结构操作和相关失效开销，不扩展渲染模式支持，也不引入整页位图或 Picture 缓存。

## 纯渲染变更的声明校验快路径

对应 [P2 每次属性编辑都做全量重校验](https://github.com/Tencent/libpag/pull/3826#discussion_r4535360351)。当前增量刷新结束后无条件调用 `resolveOutputTarget()`，对有效 target view 再扫描动态内容、目标 ID 唯一性和完整 Layer 树可达性。此项减少附加的语义校验扫描，保留既有 runtime 刷新、布局和资源通知行为。

### 判定原则

仅在能证明输出声明语义未改变时复用上次 Valid 结果。不能仅按 dirty 是否位于 target 子树决定，也不能仅凭 `layoutChanged == false`、节点类型或登记版本跳过。target 外兄弟、资源 ID、祖先 children、动态定义和 externalDoc 都可能改变合法性；前序兄弟的颜色、图片也可能改变背景采样输出。

`registerNode()` 的 `errors` 是历史诊断，修复冲突后不会自动清除，不能用它替代当前重复 ID 判断。全量路径继续按当前数据调用 `ResolveOutputView()`。

### 修改位置和内部数据流

涉及 `include/pagx/PAGXDocument.h`、`src/pagx/PAGXDocument.cpp`、`include/pagx/PAGScene.h`、`src/pagx/PAGScene.cpp`，必要的内部辅助类型放在 `src/pagx/utils/OutputView.h` 和 `.cpp`。测试放在 `test/src/PAGXRuntimeTest.cpp`。不新增要求调用方选择变更类型的公共 SDK 接口。

1. 在 `PAGXDocument::notifyChange()` 将原始 dirty 节点折叠为 owning Layer 之前，保留原始 owned dirty 集合、布局标记和保守的变更说明，通过私有通路交给 scene。快路径信息与原有 runtime dirty 集合分开，不能用折叠后的 Layer 反推原始改动。
2. 私有通知缺少证据时标为 Unknown；Unknown、Document、Layer、Composition、动态定义以及外部文档变更均走全量路径。原始 owned dirty 非空时，即使折叠后没有 owning Layer，也保留一次语义通知。可扩展私有 `onNodesChanged(collapsedDirty, changeInfo)`：集合为空时跳过 runtime refresh，仅处理 dirty 与声明校验；`onImageResourcesChanged()` 继续负责图片资源刷新，避免重复执行同一批校验。
3. 在文档维护私有节点登记版本，覆盖节点创建、实际删除和 ID 索引变更。可在 `registerNode()` 的空 ID 提前返回之前递增，使无 ID 的 `makeNode()` 也被捕捉；`removeNodes()` 实际删除时递增，`setNodeId()` 通过登记路径使版本变化。现有节点的 `Node::id` 可直接写入，因此还必须检查实际 dirty 节点和已校验 target 的当前 ID，版本号不能作为唯一证据。
4. 每个 scene 在全量校验得到 Valid 后建立自己的语义快照，至少记录声明字段和原始声明属性、文档顶层 layers、animations、dataBinds 列表、登记版本和已校验的源 target。比较列表的内容与顺序，不能只比较长度。先检查登记版本，再检查源 target 仍归文档持有、其当前 ID 未变且 `findNode(targetId)` 仍指向它，避免读取已经失效的节点。进入 None 或 Invalid 时清除可复用的 Valid 快照。
5. scene 使用该快照和原始通知决定是否重校验。多个 scene 分别维护自己的校验结果，不允许先处理的 scene 消费掉其他 scene 所需的变更信息。foreign 通知和完整 runtime 重建保留全量校验。

### 首批快路径范围

首版白名单固定为 `SolidColor`、`Rectangle`、`Image`：原始 dirty 必须全部为这些归文档持有的叶子节点，且 `layoutChanged == false`。同时要求声明和顶层列表快照、登记版本未变，已校验的 target 身份和 ID 检查通过，每个 dirty 叶子的当前 ID 均不等于 target ID。任一条件无法证明或混合批次包含其他类型，整个批次全检。

Valid target 必须是 Layer，以上白名单节点不可能是 target。上次 Valid 且登记版本未变说明本批没有新增或删除节点；检查本批叶子的当前 ID 不等于 target ID，可以排除它们对目标唯一性的新冲突。因此第一版无需另存所有节点的 ID 快照。该结论只适用于已明确报告的叶子变更，不能扩展为任意 Layer 或容器通知。

原始 `notifyChange({layer}, false)` 仍为 Unknown。一个 Layer 通知可以代表 alpha、ID、children、composition、externalDoc，或者其内容节点的 ID 已变，不能仅比较 layer.id 就判定纯渲染。首版不承诺这类通知自动命中快路径；若以后扩大范围，必须覆盖对应内容范围的语义依赖并补充测试。

公共字段修改后仍遵循原有通知约定：修改哪个节点，就将相关节点通知到文档；文档级列表和节点集合的结构修改以 Document 或 Unknown 处理。对于已有通知无法证明安全的情况，保留全检，不引入“调用方未改变其他字段”的隐含快路径假设。

### 与重绘和 runtime 恢复的关系

将 `resolveOutputTarget()` 中的 `outputViewDirty = true` 与语义校验拆开。所有既有相关内容、布局、资源通知仍触发待绘制状态，前序兄弟改色也必须更新 Glass 输出。跳过语义校验不代表跳过重绘、runtime 刷新或输出状态 reset。

处理顺序为：

```text
保留原始变更信息
按既有入口恢复 OutputTargetState 并刷新 runtime
标记输出需要重绘
如果上次结果为 Valid 且快照与本次通知证明语义未变
    复用声明校验结果
否则
    ResolveOutputView 全量校验并更新或清除快照
下一次绘制从当前 binding 取得 runtime target 并重新准备输出状态
```

`Invalid` 状态保守地重新全检，保证任意合法修复可以恢复；`None` 保持现有入口的快速返回。节点登记、target ID 或身份、children、声明或动态定义变化均使快照失效；白名单叶子 ID 与 target 冲突也必须全检。即使复用 Valid，也不能复用旧 runtime target 指针；刷新后的 target 仍从当前 binding 获取。

快路径仍会比较顶层列表，成本随这些列表和原始 dirty 集合的规模变化；省去的是 resolver 对 nodes、外部文档和完整 Layer 树的附加扫描，不能将整个属性更新宣称为 O(1)。

### 验收用例

用例以 `PAGXRuntimeTest.OutputViewValidation` 开头，纳入后述定向命令。

- 白名单叶子的颜色、矩形 roundness 和图片更新在条件满足时复用校验结果，同时正确刷新 runtime、标脏并输出新内容；布局变更或混合 Unknown 批次回到全检。
- 前序兄弟改颜色或图片后，Glass 与 BackgroundBlur 的背景采样正确变化。
- 在白名单资源节点上直接修改 ID 为 target ID，能检出冲突；修复后恢复 Valid。历史 `errors` 中保留的旧诊断不能阻止恢复。
- 新建无 ID 动态节点、普通节点增删、target 外兄弟或资源 ID 冲突、祖先移除 target、重复路径和 target 外循环均按全量路径检出。
- Composition 动态定义、externalDoc、外部文档通知、输出字段修改和清除，以及 Invalid 后修复不被错误缓存；原始 Layer 通知维持保守全检。
- 白名单批次折叠为空、仅有图片资源通知、多个 scene 与完整重建后，校验事件和快照生命周期正确。
- 借助测试中的路径观测或 profiler 确认快路径确实省去 resolver 全量扫描；用性能对比分离校验成本与已有 runtime 刷新成本，不使用机器相关耗时阈值作为单元测试门禁。

## 输出窗口数值校验复用

对应 [P2 bounds 合法性校验重复](https://github.com/Tencent/libpag/pull/3826#discussion_r4535363216)。`OutputView.cpp` 的 `IsFiniteRect()` 与 `PAGXImporter.cpp` 中 `ParseOutputBounds()` 尾部具有相同数值规则。按 CR 提取现有实现，importer 只负责严格的字符串解析，再调用同一个数值校验函数。

### 最小修改范围

1. 将现有 `IsFiniteRect()` 原实现移出 `OutputView.cpp` 的匿名命名空间，命名为 `IsValidOutputBounds()`，在已经存在的内部 `src/pagx/utils/OutputView.h` 声明。importer 已包含该头文件，无需新建校验模块或发布 SDK 接口。
2. `ParseOutputBounds()` 保留四字段、逗号分隔和完整消费字符串的语法检查，删除尾部重复的数值判断。`ParseOutputView()` 将解析结果与共享校验组合；`ResolveOutputView()` 也改用同一个函数。
3. 不修改通用 `Rect`、`ParseRect()`、`ParseFloatList()` 的规则。它们的宽松解析或空矩形语义与 outputBounds 不同，不能为了复用而替换严格解析器。
4. 共享数值规则和核心要求说明。importer 继续补充原始属性值、XML 位置，运行时继续提供程序化赋值的错误上下文；不要求两种入口的整条诊断完全相同。

```cpp
bool IsValidOutputBounds(const Rect& bounds);

// In ParseOutputView:
bool boundsValid =
    ParseOutputBounds(boundsAttr, &bounds) && IsValidOutputBounds(bounds);
```

### 保持现有数值和解析行为

- x、y、width、height 均为有限数，width 和 height 严格大于零。
- 按原来的 float 加法检查 `x + width`、`y + height` 仍为有限数，不能改成扩大到 double 后才检查。
- 保留负 origin、正负号、逗号周围空白、科学计数法和可表示的微小正尺寸。
- 不新增 epsilon 截断、最小像素尺寸或“端点必须严格大于 origin”的条件；大 origin 加极小正 width 被 float 舍入时仍按现有规则处理。
- 不增加 `errno == ERANGE` 的一概拒绝。`strtof` 可能返回正 subnormal 并报告下溢，仍应根据所得 float 值校验；下溢为零时，width/height 拒绝，origin 按原行为允许。
- 语法失败或数值不合法时，保留原始非法声明供导出回写，避免变成没有输出声明的全画布文档。`RectToString()`、`FloatToString()` 和 XSD 不在本项修改范围。

### 验收用例

复用 `PAGXImporterValidationTest.OutputViewInvalidBoundsRejected`、`OutputViewBoundsAcceptWhitespaceAndExponents` 和 `OutputViewTinyBoundsRoundTrip`，再补导入文本与程序化设置 Rect 后 `ResolveOutputView()` 的一致性用例：

- 合法负 origin、普通正尺寸、可表示的微小正尺寸和正 subnormal。
- NaN、正负 Inf、零与负尺寸，包括负零尺寸。
- 所有分量分别有限但 float 端点相加溢出，例如 `FLT_MAX + FLT_MAX`。
- 缺逗号、连续或尾部逗号、字段不足或过多、尾部垃圾字符和不完整指数仍被拒绝。
- 导入错误继续包含原始值和 XML 上下文；程序化非法 bounds 返回 Invalid，修复后的有效数据正常恢复。

程序化用例可使用 `PAGXRuntimeTest.OutputViewBounds` 前缀，导入用例沿用 `PAGXImporterValidationTest.OutputView` 前缀，纳入后述定向命令。先建立有效 target，隔离数值校验失败与目标引用失败，避免测试因另一个错误提前结束。

## 执行和验证

先读取仓库 `AGENTS.md` 与 `.codebuddy/rules/`、`.claude/rules/` 中的适用规则。代码与注释使用英语，避免 lambda，保持 C++17 和现有风格。只实施本文档确认的项目，保留其他会话的改动。先提取扩展已有 RasterLayerParent 并通过既有栅格化回归，再将输出视图失败传播与稳定状态作为一组接入；共享 bounds 规则可独立提取并验证，随后实现纯渲染校验快路径。保留原有 runtime 恢复时序，不为本 PR 反向改写正确的导出实现。

在工作目录中先执行格式化、全模块构建和定向测试：

```bash
./codeformat.sh 2>/dev/null || true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL \
  --gtest_filter='PAGXRuntimeTest.*TargetView*:PAGXRuntimeTest.*OutputView*:PAGXImporterValidationTest.OutputView*:PAGXTest.HitTest*:PAGXGlassRenderTest.TargetView*:PAGXCliTest.Render_TargetView*:PAGXUtilsTest.RasterLayer*:PAGXUtilsTest.SVGExportPreservesDepthWithoutPerspective:PAGXSVGTest.*Bake*:PAGXPPTTest.*Backdrop*:PAGXPPTTest.*ScrollRect*'
```

在基线缓存已同步的环境中执行完整 SwiftShader 回归：

```bash
cmake -G Ninja \
  -DPAG_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPAG_USE_METAL=OFF \
  -DPAG_USE_OPENGL=ON \
  -DPAG_USE_SWIFTSHADER=ON \
  -B cmake-build-hit-test-swiftshader
cmake --build cmake-build-hit-test-swiftshader --target PAGFullTest_OpenGL
./cmake-build-hit-test-swiftshader/PAGFullTest_OpenGL
```

禁止自动接受截图基线或修改 `version.json`。当前 `autotest.sh` 经 `update_baseline.sh` 会执行 `UpdateBaseline_*` 并可能暂存、切换工作区，因此采用上述显式构建与完整测试命令。若基线缓存版本不匹配导致跳过比较，应如实记录，不能计为完整截图回归通过。

此前本会话运行 OpenGL 定向测试时，在 `test/src/base/PAGTest.h:92` 创建 GPU context 失败，未执行到测试正文。执行会话需重新验证；初始化失败应报告为环境阻塞，不能计为功能测试通过。

## 交付

按仓库规则仅提交本次修改，不自动推送。共享工具可先提交为 `Reuse raster layer parenting for output views.`；稳定状态与失败传播建议提交信息为 `Reuse target view rendering state and report setup failures.`；共享 bounds 校验可单独提交为 `Share output bounds validation across import and runtime.`；校验快路径可单独提交为 `Avoid output view revalidation for proven render only changes.`。最终报告列出修改文件、实际测试结果、未解决的环境或基线问题和 commit ID。本文档后续只追加用户确认后的方案。

```codex-prompt
请在 /Users/codywwang/Desktop/PAGX/libpag 按本文档已确认的方案执行。先检查当前分支、提交和工作区；命中测试修复已有本地提交 ba4a8a179，复核并回归，避免重复实现。以已有 RasterLayerParent 为基础提取并扩展共享工具，删除 OutputTargetParent；保留已有 Raster 的坐标、无 parent 支持和两种背景输出策略。OutputTargetState 只委托共享工具管理 scene 的跨帧生命周期；CLI 和 Raster 调用保持单次恢复。接通准备失败传播，提取现有 bounds 数值校验供导入和运行时共用，保留严格解析及浮点行为。再加入可证明语义不变的渲染叶子校验快路径；未知变更与 Invalid 全检，重绘通知、runtime 刷新和 wrapper reset 不得被跳过。只修改 libpag，不反向改写正确的既有导出实现。验证 scene、Record、CLI、SVG/PPT、导入、命中、缓存和生命周期行为。遵守仓库规则，保留其他会话改动，仅提交本次修改，不推送。不要扩展到尚未讨论确认的 PR 评论。完成后报告变更、测试、阻塞和 commit ID。
```
