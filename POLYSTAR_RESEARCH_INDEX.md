# Polystar Git 历史研究 - 文档索引

本索引总结了对 libpag 仓库中 Polystar 相关改动的完整分析。

## 生成时间

2026 年 3 月 31 日

## 搜索结果概览

**找到的相关 Commit 数**: 13 个
**核心改动 Commit 数**: 7 个
**关键时间范围**: 2026-01-21 至 2026-03-30

## 文档说明

### 1. POLYSTAR_HISTORY_ANALYSIS.md (8.5 KB, 266 行)

**内容**: 高层次的历史分析和概念梳理

包含以下内容:
- 时间线概览（表格形式）
- 7 个核心改动的简明分析
- 关键设计决策对比
- 受影响文件的分类列表
- 总结性的演变脉络说明

**适合**: 快速了解改动脉络、掌握整体思路

**快速导航**:
- 时间线概览: 第 1-10 行
- 核心改动分析: 第 16-160 行
- 关键设计决策: 第 163-190 行
- 受影响文件: 第 194-216 行

---

### 2. POLYSTAR_COMMITS_DETAILED.md (12 KB, 417 行)

**内容**: 每个 commit 的详细改动分析

包含以下内容:
- 7 个核心 commit 的逐个深入分析
- 完整的代码对比和改动说明
- 设计意图和影响分析
- 测试用例变更详解
- 概念对比表
- 当前状态总结

**适合**: 深入理解具体的代码改动、学习设计思路

**快速导航**:
- Commit 1 (属性命名): 第 7-25 行
- Commit 2 (API 统一): 第 27-49 行
- Commit 3 (computeBounds 修复): 第 51-85 行
- Commit 4 (preferredX/Y): 第 87-145 行
- Commit 5 (移除无用属性): 第 147-198 行
- Commit 6 (框对齐重构): 第 200-351 行 ⭐ 核心改动
- Commit 7 (回滚): 第 353-406 行 ⭐ 核心反转

---

## 核心发现

### 三个关键时刻

#### 1. 属性 API 标准化（2026-01-21 至 2026-03-13）

```
polystarType → type         (属性命名规范)
center → position           (统一 API 命名)
```

#### 2. 布局基础设施完善（2026-03-26 至 2026-03-27）

```
computeBounds() 分离改动    (geometry-only)
preferredX/Y 添加            (bounds origin tracking)
无用属性清理                 (spec compliance)
```

#### 3. 定位模型试验与回滚（2026-03-29 至 2026-03-30）

```
像素对齐 → 框对齐 (3e2276112)     ← 核心改动
框对齐 → 像素对齐 (2208a0c60)    ← 完全回滚
```

### Position 默认值的两种方案

**方案 A: 像素对齐（当前采用）**
- Position 默认值 = `computeBounds().center()` 动态计算
- 边界框基于实际顶点位置
- 更准确但更复杂

**方案 B: 框对齐（已回滚）**
- Position 默认值 = `(outerRadius, outerRadius)` 固定规则
- 边界框为逻辑框 `outerRadius×2 × outerRadius×2`
- 更简洁但不够灵活

## 关键文件变更

### 代码文件
- `include/pagx/nodes/Polystar.h` - Polystar 类定义
- `src/pagx/nodes/Polystar.cpp` - 关键实现变更
- `include/pagx/layout/LayoutNode.h` - 布局基础设施
- `src/pagx/LayoutNode.cpp` - 测量逻辑

### 测试文件
- `test/src/PAGXTest.cpp` - 约束缩放测试用例变更

### 文档文件
- `spec/pagx_spec.md` / `.zh_CN.md`
- `.codebuddy/skills/pagx/references/`

## 当前状态（2026-03-30 之后）

✅ **已确定**:
- Polystar 分类为像素对齐节点
- Position 默认值为 bounding box 中心
- preferredX/Y 基础设施已就位
- computeBounds() 专注于几何计算

⚠️ **可优化**:
- Position 默认值的动态计算算法
- 框对齐与像素对齐的平衡方案
- 约束缩放的精度问题

## 使用建议

### 如果你想...

**了解总体改动脉络**
→ 阅读 `POLYSTAR_HISTORY_ANALYSIS.md` 前 50 行

**理解框对齐改动的细节**
→ 查看 `POLYSTAR_COMMITS_DETAILED.md` 第 200-351 行

**学习 preferredX/Y 的设计**
→ 查看 `POLYSTAR_COMMITS_DETAILED.md` 第 87-145 行

**了解为什么被回滚**
→ 比较 `POLYSTAR_COMMITS_DETAILED.md` 中的 Commit 6 和 7

**查看所有影响的文件**
→ 阅读 `POLYSTAR_HISTORY_ANALYSIS.md` 的"受影响的文件"段落

## 技术细节提取

### Position 默认值的计算

当前（像素对齐）:
```cpp
position 默认值 = computeBounds().center()
```

被回滚的方案（框对齐）:
```cpp
position 默认值 = (outerRadius, outerRadius)
```

### 约束缩放的差异

以 5-point star, outerRadius=30, innerRadius=15 为例:

| 场景 | 像素对齐 | 框对齐 |
|-----|---------|--------|
| 水平 left=50, right=50, 可用宽度 300 | scale ≈ 5.172 | scale = 5.0 |
| 双轴 上下左右各 20, 可用 360×180 | scale ≈ 4.865 | scale = 4.5 |

### 代码结构变更

**onMeasure 的两种实现**:

像素对齐:
```cpp
auto bounds = computeBounds();  // 精确计算
preferredX = bounds.x;
preferredY = bounds.y;
preferredWidth = bounds.width;
preferredHeight = bounds.height;
```

框对齐（已回滚）:
```cpp
preferredWidth = outerRadius * 2;    // 固定规则
preferredHeight = outerRadius * 2;
```

## 参考资源

### 规范文档
- `spec/pagx_spec.md` - PAGX 格式完整规范
- `spec/pagx_spec.zh_CN.md` - 中文版本

### Git 命令参考

查看相关 commit:
```bash
git log --all --oneline --grep="polystar" -i

# 查看具体 commit
git show 3e2276112    # 框对齐改动
git show 2208a0c60    # 回滚改动
```

### Commit 哈希值

| 用途 | Hash |
|-----|------|
| 属性命名 | b756a676f |
| API 统一 | c3d40c62b |
| computeBounds 修复 | bdcd3abd9 |
| preferredX/Y | 06326c595 |
| 移除无用属性 | eb6b8b814 |
| 框对齐改动 | 3e2276112 |
| 回滚改动 | 2208a0c60 |

## 问题与思考

### 为什么回滚？

基于改动的脉络，可能的原因包括:

1. **兼容性问题** - 框对齐方案与现有应用不兼容
2. **精度问题** - 固定逻辑框不适合所有 Polystar 配置
3. **性能问题** - 某些场景下性能下降
4. **规范冲突** - 与 PAGX 规范的其他部分冲突

### 未来可能的方向

1. 改进 position 默认值的动态计算算法
2. 为框对齐方案添加配置选项
3. 实现混合模型：某些约束下框对齐，其他时候像素对齐
4. 优化性能和精度的平衡

## 更新日志

- 2026-03-31: 初始分析完成，生成两份详细文档

---

**文档生成命令**:
```bash
git log --all --oneline --grep="polystar" -i
git show <commit-hash>  # 查看具体改动
```

**更新此索引**:
如有新的 Polystar 改动，请按照同样的格式扩展两份主文档。
