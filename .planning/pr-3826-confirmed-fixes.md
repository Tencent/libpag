# PAGX PR 3826 已确认修复方案

初始来源：[PAGX PR 3826 已确认修复方案](https://chatgpt.com/space/page_10f412c402d08191aed705349af2aac1)。后续确认的方案以本仓库文档为执行和追加入口。

本文档用于在另一个会话中实施 [PR 3826](https://github.com/Tencent/libpag/pull/3826) 已讨论并确认的修复。当前记录命中测试一致性、目标绘制失败传播、稳定复用输出视图状态三项；后两项需要合并实施。后续评论的方案经用户确认后再追加。

## 当前执行状态

核对日期：2026 年 11 月 13 日。工作目录：`/Users/codywwang/Desktop/PAGX/libpag`。分支：`feature/codywwang_glass`。本次更新前的核对基点为 `4ece29518`。执行前重新检查实际工作区与提交，保留其他会话产生的改动。

| 项目 | 状态 | 执行动作 |
| --- | --- | --- |
| 命中测试一致性 | 本地已提交 `ba4a8a179` | 复核实现并回归，不重复实现 |
| 目标绘制失败传播 | 方案已确认，尚未实施 | 与稳定输出视图状态合并实施 |
| 稳定复用输出视图状态 | 方案已确认，尚未实施 | 仅修改 libpag，不修改 TGFX |

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

3. 其余情况通过共享的 `OutputTargetState` 准备 wrapper；父层缺失或准备失败返回 false。该状态同时用于 scene 的持续复用和 CLI 的单次绘制。

4. 检查 `replaceChild()` 和 `addChild()` 的结果，只有重挂全部完成后才进入有效态。若修改树后失败，恢复已修改的父子关系和 mask，再返回失败；回滚信息不能因清空 wrapper 而丢失。

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

## 稳定复用输出视图状态

对应 [P2 每帧绘制改树造成额外分配和失效](https://github.com/Tencent/libpag/pull/3826#discussion_r4535358918)。当前 `FindMaskOwner()` 遍历树，wrapper 换父会递归更新 target 子树的 root 归属，并使祖先的边界和相关状态失效。此项在 libpag 内通过调整生命周期解决，继续使用 TGFX 现有接口和合成路径。

### 共享状态与所有权

在 `src/pagx/utils/OutputView.h` 和 `.cpp` 中将现有临时父层逻辑收敛为内部 `OutputTargetState`。不新增发布 SDK 接口，不复制一套手工绘制 mask、alpha 或背景效果的实现。接口轮廓为：

```cpp
class OutputTargetState {
 public:
  bool prepare(const std::shared_ptr<tgfx::Layer>& root,
               const std::shared_ptr<tgfx::Layer>& target);
  bool draw(tgfx::Canvas* canvas);
  void reset();
};
```

1. 状态保存 root、target、wrapper、原 parent 和位置，以及外部 mask owner 的可靠引用。恢复过程不能依赖已被旧 runtime 树释放的裸指针。析构调用幂等的 `reset()`，禁止复制导致重复恢复。
2. `prepare()` 校验所属树后，查找外部 mask owner、插入 identity wrapper，并解除外部 ownership。target 自身的 mask 以及 target 子树内部的 mask 关系保持原有规则。准备失败按已完成的步骤回滚。
3. 外部 mask ownership 在 scene 准备态期间保持解除，只在 `reset()` 时恢复。此处修改的是 runtime 层关系，PAGXDocument 中的源节点和 mask 声明保持原样。
4. `draw()` 检查准备态和挂接关系仍有效，再按 wrapper 到 root 的实时相对矩阵绘制；仅做 canvas 的作用域保存和恢复。热路径不调用 `FindMaskOwner()`、`Layer::Make()`、换父或 `setMask()`。
5. 保留原树中的祖先和兄弟，继续让 TGFX 采样原有背景。target 的 transform、alpha、visibility、mask、scrollRect 和混合行为仍由同一 wrapper 合成路径应用一次。
6. CLI 的 `DrawOutputTarget()` 创建局部状态，依次 prepare、draw，在作用域结束时 reset。scene 持有同一类状态，避免形成两套准备与恢复逻辑。

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

第一版对所有节点编辑通知保守地复位，不把属性分类优化混入此项。`refreshNodes()` 与 `syncChildren()` 会按源树顺序重排、挂接 slot，因此必须先恢复原树；不能等增量刷新或全量重建结束后才清理 wrapper。完成刷新与输出声明校验后，下一次有效绘制再懒准备。

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

## 执行和验证

先读取仓库 `AGENTS.md` 与 `.codebuddy/rules/`、`.claude/rules/` 中的适用规则。代码与注释使用英语，避免 lambda，保持 C++17 和现有风格。只实施本文档确认的项目，保留其他会话的改动。失败传播与稳定状态共同修改 OutputView 和 scene，应作为一组实现并验证，避免先保留每帧恢复、再局部缓存 wrapper 的冲突实现。

在工作目录中先执行格式化、全模块构建和定向测试：

```bash
./codeformat.sh 2>/dev/null || true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL \
  --gtest_filter='PAGXRuntimeTest.*TargetView*:PAGXRuntimeTest.*OutputView*:PAGXTest.HitTest*:PAGXGlassRenderTest.TargetView*:PAGXCliTest.Render_TargetView*'
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

按仓库规则仅提交本次修改，不自动推送。建议合并实现的提交信息为 `Reuse target view rendering state and report setup failures.`。最终报告列出修改文件、实际测试结果、未解决的环境或基线问题和 commit ID。本文档后续只追加用户确认后的方案。

```codex-prompt
请在 /Users/codywwang/Desktop/PAGX/libpag 按本文档已确认的方案执行。先检查当前分支、提交和工作区；命中测试修复已有本地提交 ba4a8a179，复核并回归，避免重复实现。将 DrawOutputTarget 失败传播与稳定输出视图状态合并实现：scene 跨帧复用，编辑刷新前恢复，CLI 单次调用后恢复。只修改 libpag，验证 scene、Record、CLI、命中和生命周期行为。遵守仓库规则，保留其他会话改动，仅提交本次修改，不推送。不要扩展到尚未讨论确认的 PR 评论。完成后报告变更、测试、阻塞和 commit ID。
```
