# PAGX PR 3826 已确认修复方案

来源：[PAGX PR 3826 已确认修复方案](https://chatgpt.com/space/page_10f412c402d08191aed705349af2aac1)

本页用于在另一个会话中实施 [PR 3826](https://github.com/Tencent/libpag/pull/3826) 已讨论并确认的修复。当前记录命中测试一致性和目标绘制失败传播两项；后续评论的方案经用户确认后再追加。

## 当前执行状态

核对日期：2026 年 11 月 13 日。工作目录：`/Users/codywwang/Desktop/PAGX/libpag`。分支：`feature/codywwang_glass`。本次核对 HEAD 为 `ba4a8a179`，已跟踪文件无未提交改动，本地领先远端 3 个提交。执行前重新检查实际工作区与提交，保留其他会话产生的改动。

| 项目 | 状态 | 执行动作 |
| --- | --- | --- |
| 命中测试一致性 | 本地已提交 `ba4a8a179` | 复核实现并回归，不重复实现 |
| 目标绘制失败传播 | 方案已确认，尚未实施 | 按下述方案修改并验证 |

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

### 修改辅助函数和临时父层

修改 `src/pagx/utils/OutputView.h` 与 `src/pagx/utils/OutputView.cpp`，签名调整为：

```cpp
bool DrawOutputTarget(tgfx::Canvas* canvas,
                      const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer);
```

按顺序处理：

1. canvas、root、target 为空时返回 false；target 不属于 root 时返回 false。复用 runtime 层级检查，避免按源节点 ID 判断实例归属。

2. `targetLayer == root` 时调用 `root->draw(canvas)`，正常返回 true。

3. 其余情况创建 `OutputTargetParent`；父层缺失或准备失败返回 false。

4. 检查 `replaceChild()` 和 `addChild()` 的结果，只有重挂全部完成后才进入有效态。若修改树后失败，恢复已修改的父子关系和 mask，再返回失败；回滚信息不能因清空 wrapper 而丢失。

5. 正常路径继续复用原有坐标变换、背景采样和 mask 处理，通过 RAII 恢复树关系与 canvas 状态。

6. 更新函数注释，准确说明 bool 的含义。

### 接入 scene 和 Record

修改 `src/pagx/PAGScene.cpp` 的有效 target view 分支，在调用失败时立即返回 false。仅在成功路径调用 `clearAllViewModelsDirty()` 并将 `outputViewDirty` 置为 false：

```cpp
if (!DrawOutputTarget(canvas, _rootComposition->runtimeLayer, target)) {
  return false;
}
clearAllViewModelsDirty();
outputViewDirty = false;
return true;
```

有效声明下的绘制准备失败不能消费待绘制状态。无效声明以及窗口被清空时，保留现有的 dirty 消费策略。`PAGScene::draw()` 已处理 `renderTo()` 失败并释放 context；`pagx::Record()` 已在 `renderTo()` 失败时返回 nullptr，复用这些返回路径即可。

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

- 创建有效 scene 后，从 runtime 树摘除 target，但保留 binding；`draw()` 返回 false，待绘制状态仍为 true。

- 同类故障通过 `pagx::Record()` 返回 nullptr，待绘制状态未被清除。使用独立 fixture 或恢复树后再测试，避免前一条用例状态影响结论。

- 修复 runtime 父子关系后再次绘制成功，并正常消费 dirty。

- 成功绘制后父子关系、兄弟顺序、mask 和 canvas 状态恢复；复用现有 mask、重复绘制用例补充断言。

- 没有可绘制内容的目标或不可见目标正常完成时返回 true。

上述摘除 runtime target 是用于覆盖错误路径的测试构造，不代表已经找到正常文档触发该故障的输入。

CLI 保留现有 `PAGXCliTest.Render_TargetView*` 回归，检查正常输出、显式选择、crop 和非法声明处理；检查新的 helper 失败分支确实在回读和编码之前释放锁并返回空结果。

## 执行和验证

先读取仓库 `AGENTS.md` 与 `.codebuddy/rules/`、`.claude/rules/` 中的适用规则。代码与注释使用英语，避免 lambda，保持 C++17 和现有风格。只实施本页确认的项目，保留其他会话的改动。

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

按仓库规则仅提交本次修改，不自动推送。建议新提交信息为 `Propagate target view rendering setup failures.`。最终报告列出修改文件、实际测试结果、未解决的环境或基线问题和 commit ID。本页后续只追加用户确认后的方案。

```codex-prompt
请在 /Users/codywwang/Desktop/PAGX/libpag 按本页已确认的方案执行。先检查当前分支、提交和工作区；命中测试修复已有本地提交 ba4a8a179，复核并回归，避免重复实现。重点完成 DrawOutputTarget 失败传播及其 scene、Record 和 CLI 路径的验证。遵守仓库规则，保留其他会话改动，仅提交本次修改，不推送。不要扩展到尚未讨论确认的 PR 评论。完成后报告变更、测试、阻塞和 commit ID。
```
