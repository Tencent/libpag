# Polystar Git 历史记录分析

## 时间线概览

最近关于 Polystar 的主要改动集中在 2026 年 3 月，特别是关于 position 默认值动态计算的重构。

### 关键 Commit 时间线（从新到旧）

1. **2026-03-30** - `2208a0c60` - **Revert** Polystar frame-aligned bounds change
2. **2026-03-29** - `3e2276112` - Change Polystar from pixel-aligned to frame-aligned bounds
3. **2026-03-27** - `06326c595` - Add preferredX/Y to LayoutNode for bounds origin tracking
4. **2026-03-27** - `eb6b8b814` - Remove unused attributes from XSD and PAGXImporter validation
5. **2026-03-26** - `bdcd3abd9` - Fix Polystar computeBounds to exclude position offset
6. **2026-03-26** - `3f4a95c21` - Fix HTMLExporter DisplayP3 SVG color (含 Polystar 变更)
7. **2026-03-13** - `c3d40c62b` - Rename center attribute to position
8. **2026-02-13** - `9b69f0d07` - Improve PAGX spec clarity on Polystar roundness range
9. **2026-01-21** - `b756a676f` - Fix Polystar attribute name: polystarType -> type

---

## 核心改动分析

### 1. Rename center → position (c3d40c62b - 2026-03-13)

**提交信息**: "Rename center attribute to position for Rectangle, Ellipse, and Polystar. (#3319)"

**主要改动**:
- 将 Polystar 的 `center` 属性重命名为 `position`
- 这是大规模重构，涉及 Rectangle 和 Ellipse 的统一 API 变更
- 文件较大（207.2KB），涉及多个文件的更新

**影响范围**: 
- 所有使用 Polystar 的代码都需要更新属性名
- 向后兼容性考虑需要处理

---

### 2. Fix Polystar computeBounds (bdcd3abd9 - 2026-03-26)

**提交信息**: "Fix Polystar computeBounds to exclude position offset and preserve float precision."

**关键改动**:
```cpp
// 修改前
return Rect::MakeXYWH(position.x + minX, position.y + minY, maxX - minX, maxY - minY);

// 修改后
return Rect::MakeXYWH(minX, minY, maxX - minX, maxY - minY);
```

**影响**:
- `computeBounds()` 返回的边界框不再包含 position 偏移
- 边界框只包含相对于坐标系原点的实际图形尺寸
- 这是为后续的框对齐机制做准备

---

### 3. Add preferredX/Y to LayoutNode (06326c595 - 2026-03-27)

**提交信息**: "Add preferredX/Y to LayoutNode for bounds origin tracking."

**关键改动**:
- 在 `LayoutNode` 中添加 `preferredX` 和 `preferredY` 私有属性
- Polystar、Path、Text 等在 `onMeasure` 方法中设置这些值
- 在布局测量阶段记录边界框的原点位置

**Polystar 中的实现**:
```cpp
void Polystar::onMeasure(const LayoutContext&) {
  auto bounds = computeBounds();
  preferredX = bounds.x;  // 新增：记录边界框原点 x
  preferredY = bounds.y;  // 新增：记录边界框原点 y
  preferredWidth = bounds.width;
  preferredHeight = bounds.height;
}
```

**目的**: 
- 为 MeasureChildNodes 提供精确的原点位置
- 支持像素对齐节点的正确约束计算

---

### 4. Change Polystar to frame-aligned bounds (3e2276112 - 2026-03-29)

**提交信息**: "Change Polystar from pixel-aligned to frame-aligned bounds using outerRadius*2 as logical frame size."

**这是核心重构，改变了 Polystar 的定位模型！**

#### 核心变更：Position 默认值从动态计算改为固定值

**文档更新**:
```markdown
旧：position 默认值 = bounding box 的中心 (动态计算)
新：position 默认值 = (outerRadius, outerRadius) (固定值)
```

#### Polystar 分类变更

从"**像素对齐**"改为"**框对齐**":

**旧分类（像素对齐）**:
- Bounds = 实际渲染像素边界（由 pointCount、rotation、innerRadius 计算）
- Position 默认值 = bounding box 中心（需要动态计算）

**新分类（框对齐）**:
- Bounds = 逻辑框 `[0, outerRadius×2] × [0, outerRadius×2]`
- Position 默认值 = `(outerRadius, outerRadius)`（固定计算）

#### 代码改动（Polystar.cpp）

```cpp
// 旧代码（像素对齐）- bdcd3abd9 之后
void Polystar::onMeasure(const LayoutContext&) {
  auto bounds = computeBounds();  // 动态计算精确边界
  preferredX = bounds.x;
  preferredY = bounds.y;
  preferredWidth = bounds.width;
  preferredHeight = bounds.height;
}

// 新代码（框对齐）- 3e2276112 
void Polystar::onMeasure(const LayoutContext&) {
  preferredWidth = outerRadius * 2;    // 简化为固定逻辑尺寸
  preferredHeight = outerRadius * 2;
}

void Polystar::setLayoutPosition(const LayoutContext&, float x, float y) {
  // 旧方式：从约束位置减去边界框偏移
  // position.x = x - bounds.x;
  
  // 新方式：从约束位置加上半宽度
  if (!std::isnan(x)) {
    position.x = x + actualWidth * 0.5f;  // (x + outerRadius)
  }
  if (!std::isnan(y)) {
    position.y = y + actualHeight * 0.5f;  // (y + outerRadius)
  }
}
```

#### 约束缩放行为变更

**测试案例更新** (test/src/PAGXTest.cpp):

```cpp
// 水平缩放测试变化
// 旧：基于精确顶点位置计算
//   bounds.width ≈ 57.06 → scale ≈ 5.172
//   outerRadius ≈ 155.17, innerRadius ≈ 77.59
// 新：基于简化的逻辑框
//   preferredWidth = 60 → scale = 5.0
//   outerRadius = 150, innerRadius = 75

// 双轴缩放测试变化
// 旧：考虑顶点三角函数计算
//   scaleX ≈ 9.231, scaleY ≈ 4.865 → scale ≈ 4.865
//   outerRadius ≈ 97.30
// 新：使用逻辑框
//   scaleX = 9, scaleY = 4.5 → scale = 4.5
//   outerRadius = 90
```

#### 文档更新内容

1. **属性默认值文档**更新 (attributes.md):
```
position 默认值: (center of bounding box) → (outerRadius, outerRadius)
```

2. **规范文档** (spec/pagx_spec.md, spec/pagx_spec.zh_CN.md):
   - Polystar 从"像素对齐"移入"框对齐"分类
   - 约束文档中明确 Polystar 使用逻辑框 `outerRadius×2 × outerRadius×2`

---

### 5. Revert Polystar frame-aligned bounds change (2208a0c60 - 2026-03-30)

**提交信息**: "Revert Polystar frame-aligned bounds change."

**这是 **完全回滚** commit 3e2276112 的改动！**

**回滚原因**: 
可能在测试或实施过程中发现了问题，导致需要回到像素对齐模型

**结果**:
- Polystar 重新归类为"像素对齐"
- Position 默认值回归为 bounding box 中心（需要动态计算）
- 约束缩放行为回归到基于精确顶点位置的计算方式

---

### 6. 其他相关改动

#### Remove unused attributes (eb6b8b814 - 2026-03-27)
- 移除 Polystar 的无用属性：`size`、`points`
- 这些属性在代码中从未使用过

#### Fix HTMLExporter (3f4a95c21 - 2026-03-26)
- 在 SVG 导出器中修复 Polystar 相关的类型安全问题

---

## 关键设计决策

### Position 默认值的两种设计方案

#### 方案 A: 动态计算（像素对齐）
```cpp
// 根据 pointCount、innerRadius 等参数动态计算边界
position 默认值 = computeBounds().center()
```
**优点**: 自动适应各种 Polystar 配置
**缺点**: 复杂，不稳定，难以预测

#### 方案 B: 固定计算（框对齐）
```cpp
// 基于 outerRadius 的简单固定规则
position 默认值 = (outerRadius, outerRadius)
```
**优点**: 简单、可预测、与约束系统一致
**缺点**: 忽略了 innerRadius 对实际渲染的影响

### 当前状态

根据 commit 时间线，仓库现在处于**方案 A（动态计算）**的状态（因为 2026-03-30 的 revert）。

---

## 受影响的文件

### 核心代码文件
- `include/pagx/nodes/Polystar.h` - Polystar 类定义
- `src/pagx/nodes/Polystar.cpp` - Polystar 实现
- `include/pagx/layout/LayoutNode.h` - 布局节点基类
- `src/pagx/LayoutNode.cpp` - 布局测量逻辑

### 相关类文件
- `src/pagx/nodes/Path.cpp` - 设置 preferredX/Y
- `src/pagx/nodes/Text.cpp` - 设置 preferredX/Y
- `src/pagx/nodes/TextPath.cpp` - 设置 preferredX/Y

### 测试文件
- `test/src/PAGXTest.cpp` - 布局约束测试用例

### 文档文件
- `spec/pagx_spec.md` - 英文规范
- `spec/pagx_spec.zh_CN.md` - 中文规范
- `.codebuddy/skills/pagx/references/` - 技能文档

### 导入导出
- `src/pagx/PAGXImporter.cpp` - PAGX 格式解析和验证

---

## 总结

这一系列改动反映了 Polystar 在约束布局系统中的定位模型演变：

1. **初始阶段**：属性名从 `polystarType` → `type`，`center` → `position`
2. **修复阶段**：修正 `computeBounds()` 方法，不包含 position 偏移
3. **优化准备**：添加 `preferredX/Y` 用于精确的边界原点跟踪
4. **框对齐改造**（试验）：尝试将 Polystar 改为框对齐模型，使用固定的逻辑框尺寸
5. **回滚**：因某些原因恢复到像素对齐模型

当前代码库应该是在 **commit 2208a0c60 之后**，即 **像素对齐 + 动态计算 position 默认值** 的方案。
