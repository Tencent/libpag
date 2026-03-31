# Polystar Commit 详细改动文档

## 按时间顺序详细记录所有改动

---

## 1. Fix Polystar attribute name: polystarType → type (b756a676f)

**日期**: 2026-01-21
**作者**: Dom
**提交信息**: Fix Polystar attribute name: polystarType -> type

**影响范围**: 规范文档和示例更新

**主要改动**:
- PAGX 规范中将 `polystarType` 改为 `type`
- 涉及中文和英文文档

**关键代码变化**:
```markdown
# 旧
<Polystar center="100,100" polystarType="star" pointCount="5" ... />
| `polystarType` | PolystarType | star | 类型（见下方） |

# 新
<Polystar center="100,100" type="star" pointCount="5" ... />
| `type` | PolystarType | star | 类型（见下方） |
```

---

## 2. Rename center attribute to position (c3d40c62b)

**日期**: 2026-03-13
**作者**: Dom Chen <dom@idom.me>
**提交信息**: Rename center attribute to position for Rectangle, Ellipse, and Polystar. (#3319)
**Commit Hash**: c3d40c62ba76b44d691ed5ea184ba35e9999ac66

**影响范围**: 大规模重构，涉及 Rectangle、Ellipse、Polystar 的统一 API 变更
**文件数量**: 207.2 KB（包含 Code Buddy 技能文件更新）

**核心改动**: 
- Polystar 中 `center` 属性 → `position` 属性
- 这是统一的 API 命名规范化

**说明**: 
- 涉及的文件：`.codebuddy/skills/cr/`、代码文件、规范文档
- 这个 commit 很大，包含了广泛的 API 更新

---

## 3. Fix Polystar computeBounds (bdcd3abd9)

**日期**: 2026-03-26
**作者**: domchen <domchen@users.noreply.github.com>
**提交信息**: Fix Polystar computeBounds to exclude position offset and preserve float precision.
**Commit Hash**: bdcd3abd97d6c9b10b6603f1df2cfcd0bc94f719

### 关键改动详析

**include/pagx/nodes/Polystar.h**:
```cpp
// 修改前
return Rect::MakeXYWH(position.x + minX, position.y + minY, maxX - minX, maxY - minY);

// 修改后
return Rect::MakeXYWH(minX, minY, maxX - minX, maxY - minY);
```

**影响**:
- `computeBounds()` 现在返回的是相对于坐标系原点的边界框，而不包含 position 偏移
- 这很关键，因为边界框应该只表示图形的几何范围，而不受 position 属性的影响
- 为后续的 preferredX/Y 机制铺平道路

**设计意图**:
- 分离关切：`computeBounds()` 专注于几何计算，position 处理单独处理
- 为布局系统的精确约束计算做准备

---

## 4. Add preferredX/Y to LayoutNode (06326c595)

**日期**: 2026-03-27
**作者**: domchen <domchen@users.noreply.github.com>
**提交信息**: Add preferredX/Y to LayoutNode for bounds origin tracking.
**Commit Hash**: 06326c59501feef7813b70388779069ee55e32fa

### 关键改动详析

**include/pagx/layout/LayoutNode.h**:
```cpp
// 从公开改为私有
private:
  // Preferred position and size
  float preferredX = 0;        // 新增
  float preferredY = 0;        // 新增
  float preferredWidth = NAN;
  float preferredHeight = NAN;
  
  // Actual layout size
  float actualWidth = NAN;
  float actualHeight = NAN;

  // 添加 friend classes
  friend class Rectangle;
  friend class Ellipse;
  friend class Path;
  friend class Polystar;
  friend class Text;
  friend class TextPath;
  friend class TextBox;
  friend class Group;
  friend class Layer;
```

**src/pagx/nodes/Polystar.cpp**:
```cpp
void Polystar::onMeasure(const LayoutContext&) {
  auto bounds = computeBounds();
  preferredX = bounds.x;    // 新增：记录边界框左上角 X
  preferredY = bounds.y;    // 新增：记录边界框左上角 Y
  preferredWidth = bounds.width;
  preferredHeight = bounds.height;
}
```

**src/pagx/LayoutNode.cpp**:
```cpp
// MeasureChildNodes 中的改动
float extX = node->hasConstraints() ? node->constraintExtentX() : node->preferredX;
float extY = node->hasConstraints() ? node->constraintExtentY() : node->preferredY;
extX += node->preferredWidth;
extY += node->preferredHeight;
```

**设计意图**:
- 在测量阶段记录元素的自然原点位置
- 为无约束节点提供精确的定位信息
- 支持像素对齐节点（Path、Polystar、Text）的正确约束计算

**影响范围**:
- Path、Text、TextPath 也都添加了相同的 preferredX/Y 设置
- 这为统一的约束系统奠定了基础

---

## 5. Remove unused attributes from XSD (eb6b8b814)

**日期**: 2026-03-27
**作者**: domchen <domchen@users.noreply.github.com>
**提交信息**: Remove unused attributes from XSD and PAGXImporter validation.
**Commit Hash**: eb6b8b814717dc0edca8a3f33d7f37159a4c8736

**改动**:
- 移除 Polystar 的未使用属性：`size` 和 `points`
- 移除 Text 的未使用属性：`color`
- 这些属性从未在代码中被解析过

**spec/pagx.xsd**:
```xml
<!-- 移除 -->
<xs:attribute name="size" type="SizeType"/>
<xs:attribute name="points" type="xs:decimal"/>
```

**src/pagx/PAGXImporter.cpp**:
```cpp
// 更新验证列表
// 从
{"id", "position", "type", "pointCount", "outerRadius", "innerRadius",
 "rotation", "outerRoundness", "innerRoundness", "reversed", "size", "points",
 "left", "right", "top", "bottom", "centerX", "centerY"}

// 改为
{"id", "position", "type", "pointCount", "outerRadius", "innerRadius",
 "rotation", "outerRoundness", "innerRoundness", "reversed", "left", "right",
 "top", "bottom", "centerX", "centerY"}
```

**示例更新**:
```xml
<!-- 旧 -->
<Polystar centerX="0" centerY="0" size="120,120" points="5" innerRadius="36"/>

<!-- 新 -->
<Polystar centerX="0" centerY="0" innerRadius="36"/>
```

---

## 6. Change Polystar to frame-aligned bounds (3e2276112) ⭐ 核心重构

**日期**: 2026-03-29
**作者**: domchen <domchen@users.noreply.github.com>
**提交信息**: Change Polystar from pixel-aligned to frame-aligned bounds using outerRadius*2 as logical frame size.
**Commit Hash**: 3e2276112288a278b15bf970c94baa29cef722ea

### 这是本次改动的核心和转折点！

#### 核心概念变更

**从像素对齐改为框对齐**:

```
旧（像素对齐）:
- Polystar 的边界框 = 实际渲染像素的精确边界
- Position 默认值 = bounding box 的中心（动态计算）
- 约束缩放基于精确的顶点位置计算

新（框对齐）:
- Polystar 的边界框 = 逻辑框 [0, outerRadius×2] × [0, outerRadius×2]
- Position 默认值 = (outerRadius, outerRadius)（固定规则）
- 约束缩放基于简化的逻辑框尺寸
```

#### 文档变更

**属性文档** (.codebuddy/skills/pagx/references/attributes.md):
```markdown
旧: | `position` | Point | (center of bounding box) |
新: | `position` | Point | (outerRadius, outerRadius) |
```

**spec/pagx_spec.md** 中的分类变更:
```markdown
旧（像素对齐节点列表）:
Path, Text, Polystar, TextPath

新（框对齐节点列表）:
Rectangle, Ellipse, Polystar, TextBox, Group, Layer

新（像素对齐节点列表）:
Path, Text, TextPath
```

**约束文档说明**:
```markdown
For Polystar, bounds = [0, outerRadius×2] × [0, outerRadius×2]
Polystar uses its frame bounds (outerRadius×2 × outerRadius×2) for scaling calculations
```

#### 代码实现变更

**src/pagx/nodes/Polystar.cpp**:

旧代码（动态计算，基于精确边界）:
```cpp
void Polystar::onMeasure(const LayoutContext&) {
  auto bounds = computeBounds();           // 根据顶点计算
  preferredX = bounds.x;
  preferredY = bounds.y;
  preferredWidth = bounds.width;           // 精确宽度（≈57.06 for 5-point star）
  preferredHeight = bounds.height;         // 精确高度（≈45.23 for 5-point star）
}

void Polystar::setLayoutSize(const LayoutContext&, float width, float height) {
  if (width > 0 && height > 0) {
    float scale = std::min(width / bounds.width, height / bounds.height);
    outerRadius = outerRadius * scale;
    innerRadius = innerRadius * scale;
  }
  auto bounds = computeBounds();           // 重新计算
  actualWidth = bounds.width;
  actualHeight = bounds.height;
}

void Polystar::setLayoutPosition(const LayoutContext&, float x, float y) {
  auto bounds = computeBounds();
  if (!std::isnan(x)) {
    position.x = x - bounds.x;             // 减去边界框偏移
  }
  if (!std::isnan(y)) {
    position.y = y - bounds.y;
  }
}
```

新代码（固定计算，基于简化框）:
```cpp
void Polystar::onMeasure(const LayoutContext&) {
  preferredWidth = outerRadius * 2;        // 简化为 2×radius
  preferredHeight = outerRadius * 2;
}

void Polystar::setLayoutSize(const LayoutContext&, float width, float height) {
  if (width > 0 && height > 0) {
    float scale = std::min(width / (outerRadius * 2), height / (outerRadius * 2));
    outerRadius = outerRadius * scale;
    innerRadius = innerRadius * scale;
  }
  actualWidth = outerRadius * 2;           // 简化为 2×radius
  actualHeight = outerRadius * 2;
}

void Polystar::setLayoutPosition(const LayoutContext&, float x, float y) {
  if (!std::isnan(x)) {
    position.x = x + actualWidth * 0.5f;   // 添加半宽度得到中心
  }
  if (!std::isnan(y)) {
    position.y = y + actualHeight * 0.5f;
  }
}
```

#### 测试用例变更

**test/src/PAGXTest.cpp** - 约束缩放测试:

```cpp
// 水平缩放（LayoutConstraintScalePolystarHorizontal）
旧期望:
  EXPECT_FLOAT_EQ(star->outerRadius, 155.17241f);   // 基于 bounds.width ≈ 57.06
  EXPECT_FLOAT_EQ(star->innerRadius, 77.586205f);
  // 注释：scale = 300 / 58 ≈ 5.172

新期望:
  EXPECT_FLOAT_EQ(star->outerRadius, 150.0f);       // 基于逻辑框 outerRadius*2=60
  EXPECT_FLOAT_EQ(star->innerRadius, 75.0f);
  // 注释：scale = 300 / 60 = 5.0

// 双轴缩放（LayoutConstraintScalePolystarBothAxes）
旧期望:
  EXPECT_FLOAT_EQ(star->outerRadius, 97.297295f);
  EXPECT_FLOAT_EQ(star->innerRadius, 48.648647f);
  // 基于：scaleX ≈ 9.231, scaleY ≈ 4.865

新期望:
  EXPECT_FLOAT_EQ(star->outerRadius, 90.0f);
  EXPECT_FLOAT_EQ(star->innerRadius, 45.0f);
  // 基于：scaleX = 9, scaleY = 4.5 → min(9, 4.5) = 4.5
```

#### 概念对比表

| 方面 | 旧（像素对齐）| 新（框对齐）|
|-----|--------------|-----------|
| 边界框计算 | 基于实际顶点位置 | 基于固定逻辑 |
| preferredWidth | ≈ 2.0 × outerRadius (for star) | = 2.0 × outerRadius |
| preferredHeight | ≈ 1.809 × outerRadius | = 2.0 × outerRadius |
| position 默认值 | center of bounding box | (outerRadius, outerRadius) |
| 缩放因子计算 | 基于精确边界尺寸 | 基于简化逻辑框 |
| 约束定位 | 从约束点减去边界偏移 | 从约束点加上半宽度 |

---

## 7. Revert Polystar frame-aligned bounds change (2208a0c60) ⭐ 完全回滚

**日期**: 2026-03-30
**作者**: domchen <domchen@users.noreply.github.com>
**提交信息**: Revert Polystar frame-aligned bounds change.
**Commit Hash**: 2208a0c60e09a2795d071b4cec6ee8d403d5be1d

### 这个 commit 完全回滚了 commit 3e2276112 的所有改动！

**回滚内容**:
- 所有代码改动完全恢复
- 所有文档改动完全恢复
- 所有测试用例期望值完全恢复

**影响范围**:
- `.codebuddy/skills/pagx/references/`
- `include/pagx/nodes/Polystar.h`
- `spec/pagx_spec.md` 和 `spec/pagx_spec.zh_CN.md`
- `src/pagx/nodes/Polystar.cpp`
- `test/src/PAGXTest.cpp`

**结果状态**:
- 仓库回到 commit 06326c595 之后的状态
- Polystar 重新分类为"像素对齐"
- Position 默认值回到"bounding box 中心"
- 约束缩放回到基于精确顶点位置的计算

**可能原因**:
- 框对齐方案在实际测试中暴露了问题
- 与现有应用程序或规范的兼容性问题
- 性能或精度方面的考量

---

## 时间线总结

```
2026-01-21: Fix polystarType → type  (属性命名)
    ↓
2026-03-13: Rename center → position (API 统一)
    ↓
2026-03-26: Fix computeBounds (分离关切)
    ↓
2026-03-27: Add preferredX/Y (布局系统基础设施)
2026-03-27: Remove unused attrs (清理规范)
    ↓
2026-03-29: Change to frame-aligned (框对齐试验 ← 核心改动)
    ↓
2026-03-30: Revert to pixel-aligned (回到像素对齐)
```

---

## 当前状态（2026-03-30 之后）

**Polystar 的定位模型**:
- 分类：**像素对齐**
- Position 默认值：**bounding box 中心**（动态计算）
- 边界框：**基于实际顶点位置**（由 pointCount、rotation、innerRadius 确定）
- 约束缩放：**基于精确边界尺寸**
- 约束定位：**从约束点减去边界框偏移**

**相关基础设施**:
- ✅ preferredX/Y 已添加到 LayoutNode
- ✅ computeBounds() 不再包含 position 偏移
- ✅ 属性重命名完成（center → position）
- ✅ 无用属性已清理

**未来方向** (基于改动脉络):
- 框对齐方案可能需要进一步优化或改进
- Position 默认值的动态计算机制可能还有进一步的优化空间
- preferredX/Y 的基础设施为未来的改进提供了可能
