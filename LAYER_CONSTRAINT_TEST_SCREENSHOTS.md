# Layer 约束布局测试 - 预期截图描述

## 概述

为 Layer 约束布局功能新增的 3 个 .pagx 示例文件，每个文件的预期渲染结果如下：

---

## 1. `layer_constraint_absolute.pagx` (600×400)

### 文件说明
演示 Layer 约束在**绝对定位模式**下的功能，包含 7 个彩色方块展示不同的约束类型。

### 预期布局内容

| 位置 | 组件 | 约束 | 预期位置 | 颜色 |
|------|------|------|---------|------|
| 左上角 | Box 1 | `left=20, top=20` | 距离左边 20px，距离顶部 20px | #FF6B6B 红色 |
| 右上角 | Box 2 | `right=20, top=20` | 距离右边 20px，距离顶部 20px | #4ECDC4 青色 |
| 左下角 | Box 3 | `left=20, bottom=20` | 距离左边 20px，距离底部 20px | #45B7D1 蓝色 |
| 右下角 | Box 4 | `right=20, bottom=20` | 距离右边 20px，距离底部 20px | #FFA07A 橙色 |
| 中心 | Box 5 | `centerX=0, centerY=0` | 水平中心，垂直中心 (300, 160) | #95E1D3 薄荷绿 |
| 中上部 | Box 6 | `left=30, right=30, top=260` | 宽度 = 600-30-30=540，在 y=260 处 | #F38181 粉红 |
| 右边 | Box 7 | `top=50, bottom=50, right=20` | 高度 = 400-50-50=300，在 x=550 处 | #AA96DA 紫色 |

### 截图验证点

✓ 四个角的方块分别对齐到四个角（各距边 20px）
✓ 中心方块准确居中显示
✓ 水平拉伸条从 x=30 到 x=570，高度 30px，位于 y=260
✓ 竖直拉伸条宽度 30px，从 y=50 到 y=350，位于 x=550
✓ 所有元素的 roundness="4" 显示圆角

### 尺寸验证
- 文档尺寸：600×400
- Box 1-5 固定尺寸（80×60, 80×60, 80×60, 80×60, 100×80）
- Box 6 宽度派生：540 (600-30-30)
- Box 7 高度派生：300 (400-50-50)

---

## 2. `layer_constraint_with_overlay.pagx` (500×350)

### 文件说明
演示 Layer 约束与容器布局的结合，展示 `includeInLayout=false` 的效果。

### 预期布局内容

| 组件 | 特性 | 预期位置 | 尺寸 | 颜色 |
|------|------|---------|------|------|
| 背景 | 浅灰色背景 | (0,0) | 500×350 | #F8FAFC |
| Child 1 | Flex 参与者 | x=0, y=0 | 180×350 | #4ECDC4 青色 |
| Child 2 | Flex 参与者 | x=190, y=0 | 180×350 | #45B7D1 蓝色 |
| Overlay | 约束定位（独立） | x=410, y=260 | 80×80 | #FF6B6B 红色 |

### 关键特性验证

✓ **容器布局活跃**：Horizontal 模式，gap=10
  - Child 1 和 Child 2 由容器 Flex 布局安排
  - 两个 Child 每个占用 (500-10)/2 ≈ 245px? **不对，应该是 180 + 10 + 180 = 370 < 500**
  - 实际：Child 1 在 x=0，Child 2 在 x=180+10=190

✓ **Overlay 被排除**：`includeInLayout=false`
  - 不参与容器布局的空间计算
  - 其约束 `right=10, bottom=10` 生效
  - 位置 = (500-10-80, 350-10-80) = (410, 260)
  - 呈现为红色圆形（roundness="40"）

✓ **渲染顺序**：Overlay 应该在 Child 1 和 Child 2 上面（后加入）

### 尺寸验证
- 文档尺寸：500×350
- Child 1、2 各 180×350
- Overlay 80×80（固定）
- Overlay 位置：(410, 260)

---

## 3. `layer_constraint_priority.pagx` (600×400)

### 文件说明
演示**容器布局优先于约束**的优先级规则。三个方块有冲突的约束，但都被容器布局所覆盖。

### 预期布局内容

| Box | 设置的约束 | 容器决策 | 预期位置 | 颜色 |
|-----|----------|---------|---------|------|
| Box 1 | `left=999` | Ignored | 由 Flex 布局确定 | #FF6B6B 红色 |
| Box 2 | `centerX=0, centerY=0` | Ignored | 由 Flex 布局确定 | #4ECDC4 青色 |
| Box 3 | `right=999, bottom=999` | Ignored | 由 Flex 布局确定 | #45B7D1 蓝色 |

### 容器布局计算

```
父容器：600×400, layout=Horizontal, gap=15, alignment=Center

三个 Box 宽度各 120，垂直居中于 400 高度

总主轴宽度 = 120 + 15 + 120 + 15 + 120 = 390
容器主轴宽度 = 600

Arrangement = Start (默认)
垂直位置 = (400 - 120) / 2 = 140 (因为 alignment=Center)

预期 x 坐标：
- Box 1: x = 0
- Box 2: x = 120 + 15 = 135
- Box 3: x = 135 + 120 + 15 = 270

预期 y 坐标（全部）：y = 140
```

### 截图验证点

✓ 三个 120×120 的方块按水平 Flex 排列
✓ 方块间间距为 15px
✓ 所有方块在容器内垂直**居中**（不是顶部对齐）
✓ 三个方块的约束 `left=999, centerX=0, right=999` 都被**忽略**
✓ 容器布局的 Horizontal + Center alignment 优先级更高

### 尺寸验证
- 文档尺寸：600×400
- 三个 Box 各 120×120
- Box1 位置：(0, 140)
- Box2 位置：(135, 140)
- Box3 位置：(270, 140)
- 总占用宽度：390px（居左对齐）

---

## 单元测试与截图测试的关系

| 单元测试 | 对应的 .pagx 文件 | 验证内容 |
|---------|-----------------|--------|
| LayerConstraintLeft / Right / Top / Bottom | layer_constraint_absolute.pagx | 四个角的方块位置正确性 |
| LayerConstraintCenterX / CenterY | layer_constraint_absolute.pagx | 中心方块准确居中 |
| LayerConstraintLeftRightDeriveWidth | layer_constraint_absolute.pagx | 水平拉伸条宽度派生 |
| LayerConstraintTopBottomDeriveHeight | layer_constraint_absolute.pagx | 竖直拉伸条高度派生 |
| LayerConstraintWithIncludeInLayoutFalse | layer_constraint_with_overlay.pagx | Overlay 在底右角 |
| LayerConstraintIgnoredInContainerLayout | layer_constraint_priority.pagx | 三个方块水平排列（约束被忽略） |

---

## 截图生成和对比流程

1. **加载 .pagx**：PAGXImporter 解析 XML
2. **构建层树**：LayerBuilder 转换为 tgfx::Layer
3. **创建 Surface**：600×400（或对应尺寸）的渲染缓冲
4. **渲染**：DisplayList::render() 光栅化所有图形
5. **保存截图**：输出 webp 到 `test/out/PAGXTest/`
6. **对比基准**：与 `test/baseline/.cache/PAGXTest/` 中的基准对比

---

## 期望结果

✅ 所有 3 个 .pagx 文件都应该正常加载和渲染
✅ 屏幕截图应该与描述相符
✅ 单元测试验证的逻辑应该在渲染结果中可视化体现
