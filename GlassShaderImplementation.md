# GlassStyle Shader 实现文档（基于 wasm 逆向更新）

> 本文档基于从编译产物中提取的实际 GLSL shader 源码编写，替代之前基于代码阅读推断的版本。所有变量名、常量、算法流程均来自 shader 原文（经混淆，已还原语义）。

---

## 1. 概述

GlassStyle 模拟光线穿过玻璃的物理行为：折射、色散、磨砂模糊、边缘斜面高光。捕获图层下方背景，通过高度图梯度计算位移方向进行光学扭曲渲染。

**两种 shader pipeline**：

| Pipeline 标识 | 用途 | 形状来源 |
|---|---|---|
| `GLASS_FRAGMENT` / `GLASS_VERTEX` | 圆角矩形/椭圆（解析 SDF） | 参数推导 |
| `GLASS_ARBITRARY_SHAPE_FRAGMENT` / `GLASS_ARBITRARY_SHAPE_VERTEX` | 任意形状（AlphaMask） | 纹理高度图 |

> **注意**：从 wasm 提取的两个 shader blob 完全一致（MD5 相同），表明当前版本两种 pipeline 可能共用同一份 shader 代码，或解析 SDF 路径的 shader 在另一处存储。以下分析基于提取到的 shader 源码。

---

## 2. 参数说明

### 2.1 用户参数

| 参数 | uniform 名 | 类型 | 语义 |
|---|---|---|---|
| 折射强度 | `refractionIntensity` | float | 折射偏移的最终缩放系数 |
| 折射距离 | `refractionDistance` | float | 梯度采样步长 + 偏移 clamp 上限 |
| 光照强度 | `lightIntensity` | float | 边缘高光亮度（**实际被使用**，非死参数） |
| 光照角度 | `lightAngle` | float | 光源方向角度（弧度），叠加 `frameRotation` 偏移 |
| 斜面大小 | `bevelSize` | float | 斜面/边缘带宽度，影响高光范围 |
| 色散 | `chromaticAberration` | float | 色散强度，控制 8 点卷积采样范围 |
| 方向扩散 | `splay` | float | 梯度方向与向心方向的混合权重 [0,1] |

### 2.2 内部 uniform

| uniform 名 | 类型 | 语义 |
|---|---|---|
| `tileExpansionScale` | float | 背景纹理 UV 缩放（纹理边界扩展） |
| `heightExpansionScale` | float | 高度图 UV 缩放 |
| `bevelExpansionScale` | float | 斜面纹理 UV 缩放 |
| `heightMapTexelSize` | float | 高度图单像素尺寸（已声明） |
| `nodeToTileSpace` | mat3 | 节点空间 → 纹理 UV 变换矩阵 |
| `tileToNodeSpace` | mat3 | 纹理 UV → 节点空间逆变换 |
| `frameRotation` | float | 帧旋转角度，叠加到光照角度 |
| `nodeSpaceCenterPoint` | vec2 | 玻璃中心点（节点空间），用于向心方向 |

### 2.3 常量

| 常量名 | 值 | 语义 |
|---|---|---|
| `cg` | `1.57079632679` (π/2) | 角度偏移基准 |
| `db` | `0.2` | （已声明，当前 shader 未直接引用） |
| `dc` | `0.35` | 高光 smoothstep 下限 |
| `dd` | `0.6` | 背面高光强度系数 |
| `de` | `8` | 色散卷积采样点数 |
| `df` | `6.0` | 梯度采样最大步长 |
| `dg` | `0.1` | （已声明，当前 shader 未直接引用） |

---

## 3. 纹理绑定

| 绑定槽 | uniform 名 | 内容 | 采样方式 |
|---|---|---|---|
| 0 | `texture0` | 背景（可能已 Frost 模糊） | ClampToEdge + Linear |
| 1 | `texture1` | RGBA8 打包的高度图 | ClampToEdge + Linear |
| 2 | `texture2` | 斜面（bevel）遮罩，alpha 通道 [0,1] | ClampToEdge + Linear |

> **与旧文档差异**：旧文档只有 2 个纹理（uSource + uMask），实际有 3 个，多出 `texture2`（bevel 斜面纹理）。

---

## 4. Shader 函数详解

### 4.1 RGBA8 打包/解包（精度扩展）

```glsl
// 打包：float → vec2（两通道）
vec2 a(float b) {
    float c = floor(b * 255.0) * (1.0 / 255.0);
    float d = (b - c) * 255.0;
    return vec2(c, d);
}

// 打包：float → vec4（四通道，32 位精度）
vec4 e(float b) {
    vec2 f = a(b);
    float g = f.x + f.y * (1.0 / 255.0);
    float h = (b - g) * 65025.0;
    vec2 i = a(h);
    return vec4(f, i);
}

// 解包：vec4 → float
const vec4 j = vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0);
float k(vec4 l) {
    return dot(l, j);
}
```

### 4.2 坐标变换

```glsl
// 节点空间 → 纹理 UV，带缩放
vec2 dh(vec2 di, float dj) {
    vec2 dk = (nodeToTileSpace * vec3(di, 1.0)).xy;
    vec2 dl = (dk - 0.5) * dj + 0.5;
    return dl;
}
```

### 4.3 纹理采样

```glsl
// 采样背景纹理
vec4 dm(vec2 p) {
    vec2 dn = dh(p, tileExpansionScale);
    return texture2D(texture0, dn);
}

// 采样高度图（RGBA8 解包）
float ed(vec2 ec) {
    return k(texture2D(texture1, ec));
}

// 采样斜面纹理，[0,1] → [-1,1]
float eb(vec2 ec) {
    return texture2D(texture2, dh(ec, bevelExpansionScale)).a * 2.0 - 1.0;
}
```

### 4.4 梯度计算（中心差分）

```glsl
vec2 ee(vec2 di, float ef) {
    float step = clamp(ef, 1.0, df);  // df = 6.0，步长 clamp 到 [1, 6]
    
    // 在 heightExpansionScale 空间下采样 4 邻居
    float eg = ed(dh(di - vec2(step, 0.0), heightExpansionScale));  // left
    float eh = ed(dh(di + vec2(step, 0.0), heightExpansionScale));  // right
    float ei = ed(dh(di - vec2(0.0, step), heightExpansionScale));  // bottom
    float ej = ed(dh(di + vec2(0.0, step), heightExpansionScale));  // top
    
    return vec2(eh - eg, ej - ei) / (2.0 * step);  // 中心差分梯度
}
```

> **与旧文档差异**：步长 `step = clamp(refractionDistance, 1.0, 6.0)`，不是 `minHalf / 20 + 1`。步长由 `refractionDistance` uniform 决定，最大 6.0。

### 4.5 归一化（带 epsilon 保护）

```glsl
// 标准归一化
vec2 m(vec2 b) {
    float n = dot(b, b);
    return n > 1.0e-10 ? b / sqrt(n) : vec2(0.0, 0.0);
}

// 另一个归一化变体（用于 main 中）
vec2 ek(vec2 el) {
    float em = max(dot(el, el), 1e-12);
    return el * inversesqrt(em);
}
```

> **注意**：`ek` 用 `max(., 1e-12)` 保护，不会产生 NaN。但 4.6 节 `en` 函数中的 `normalize(ec - nodeSpaceCenterPoint)` 未加 epsilon 保护——当 `ec == nodeSpaceCenterPoint` 时 `normalize(vec2(0))` 在 GLSL 中是未定义行为，仍可能产生 NaN。旧文档担心的"中心点 NaN"仅在 `ek` 路径已处理，`en` 路径仍有风险。

### 4.6 Splay 方向混合

```glsl
vec2 en(vec2 eo, vec2 ec) {
    float ep = length(eo);                                    // 保留原长度
    vec2 eq = ep > 0.0 ? normalize(eo) : vec2(0.0);          // 梯度方向
    vec2 er = -normalize(ec - nodeSpaceCenterPoint);          // 向心方向
    vec2 es = mix(eq, er, splay);                             // 按 splay 混合
    return es * ep;                                            // 乘回原长度
}
```

> **与旧文档差异**：混合后**乘回原长度** `ep`，不是返回纯方向。这意味着 splay 只影响方向，不影响折射距离。

### 4.7 色散卷积采样（8 点）

```glsl
vec4 dp(vec2 p, vec2 dq, float dr) {
    vec4 ds = vec4(0.0);
    float dt = 1.0 / float(de - 1);  // de = 8, dt = 1/7
    float du = 0.0;
    
    for (int bf = 0; bf < de; bf++) {  // 8 次循环
        float dv = float(bf) * dt - 0.5;           // [-0.5, +0.5]
        float dw = 1.0 - abs(dv * 2.0);             // 三角权重 [0, 1]
        
        // 三个偏移位置，分别取 R/G/B 通道
        vec4 dx = dm(p + (dv + 0.5) * dr * dq);    // R 通道位置
        vec4 dy = dm(p + dv * dr * dq);             // G 通道位置
        vec4 dz = dm(p + (dv - 0.5) * dr * dq);    // B 通道位置
        
        float ea = max(max(dx.a, dy.a), dz.a);      // alpha 取三者最大
        ds += vec4(dx.r, dy.g, dz.b, ea) * dw;      // 加权累积
        du += dw;
    }
    return ds / du;  // 归一化
}
```

> **与旧文档差异**：色散不是简单的三通道分别偏移采样，而是沿偏移方向 **8 点卷积**，每点取不同通道（R/G/B），用三角权重加权平均。alpha 取三通道最大值。

### 4.8 解析 SDF 函数

shader 包含完整的解析 SDF 实现，用于圆角矩形和椭圆路径：

```glsl
// 矩形 SDF
float o(vec2 p, vec2 q) {
    vec2 r = abs(p) - q;
    return length(max(r, vec2(0.0))) + min(max(r.x, r.y), 0.0);
}

// 圆角矩形 SDF（单一圆角）
float s(vec2 p, vec2 q, float t) {
    float u = min(t, min(q.x, q.y));
    return o(p, q - u) - u;
}

// 圆角矩形 SDF（四角不同圆角）
float v(vec2 p, vec2 q, vec4 w) { ... }

// 超椭圆 SDF（连续曲率圆角）
float cc(vec2 p, float cd, float ce) { ... }

// 带超椭圆圆角的 SDF
float cs(vec2 p, vec2 q, float cd, float ce) { ... }
```

此外还包含 **三次贝塞尔曲线距离计算**（Cardano 公式解三次方程 + Newton-Raphson 迭代），用于复杂形状边缘的精确距离。

---

## 5. main 函数完整流程

```glsl
void main() {
    // ========== Step 1: 坐标变换 ==========
    vec2 ec = (tileToNodeSpace * vec3(_coord2, 1.0)).xy;  // UV → 节点空间坐标
    
    // ========== Step 2: 梯度方向 ==========
    vec2 eo = ee(ec, refractionDistance);  // 中心差分梯度（步长 = refractionDistance）
    eo = en(eo, ec);                        // splay 混合（梯度方向 ↔ 向心方向）
    
    // ========== Step 3: 折射偏移计算 ==========
    vec2 eu = ek(eo);                       // 归一化方向（用于高光计算）
    
    // 非线性缩放
    eo = eo * ((refractionDistance - 1.0) * 0.5);   // 线性缩放
    eo = sign(eo) * pow(abs(eo), vec2(1.4));         // 非线性曲线（幂 1.4）
    
    // bevel 权重
    float ev = smoothstep(0.0, 0.5, 1.0 - eb(ec));   // 斜面纹理 → 权重 [0,1]
    
    // 两条折射路径混合
    // 注：提取的 shader 中有 vec2 ew = vec2(0.999) * refractionDistance 声明（疑似 clamp 上限），
    //     但在可见代码片段中未被引用，可能是死代码或 clamp 调用在另一未提取的分支中。
    vec2 an = refractionIntensity * eo * refractionDistance;       // 全强度折射
    vec2 ex = refractionIntensity * eo * 0.2 * refractionDistance; // 20% 强度折射
    vec2 dq = mix(an, ex, ev);                         // 按 bevel 权重混合
    
    // ========== Step 4: 背景采样 ==========
    vec2 p = ec + dq;  // 位移后位置
    
    vec4 ey;
    if (chromaticAberration < 0.001) {
        ey = dm(p);                         // 无色散：单次采样
    } else {
        ey = dp(p, dq, chromaticAberration); // 有色散：8 点卷积
    }
    
    // ========== Step 5: 边缘高光 ==========
    float ez = lightIntensity;                                    // 高光亮度
    float fa = bevelSize * 0.5;                                  // 斜面半宽
    float fb = lightAngle - frameRotation + cg;                  // 有效光照角度
    vec2 fc = -vec2(cos(fb), sin(fb));                           // 光源方向
    
    float fd = smoothstep(dc, 1.0, dot(-eu, -fc)) * ev * ez;   // 正面高光
    float fe = smoothstep(dc, 1.0, dot(-eu, fc)) * ev * ez * dd; // 背面高光 (dd=0.6)
    ey.xyz += fd + fe;                                           // 叠加到颜色
    
    // ========== Step 6: 输出 ==========
    gl_FragColor = ey;
}
```

---

## 6. 与旧文档的差异总结

| # | 方面 | 旧文档 | 实际实现 |
|---|---|---|---|
| 1 | **lightAngle / lightIntensity** | "shader 未使用，死参数" | **被使用**：计算正面/背面边缘高光，叠加到最终颜色 |
| 2 | **纹理数量** | 2 个（uSource + uMask） | **3 个**：背景 + 高度图 + **bevel 斜面纹理** |
| 3 | **梯度步长** | `minHalf / 20 + 1` | `clamp(refractionDistance, 1.0, 6.0)` |
| 4 | **折射距离** | 线性 `refDist × (1 - height)` | 非线性：`pow(abs(eo), 1.4)` + bevel 权重双路径混合 |
| 5 | **色散** | 三通道分别偏移采样（3 次 texture） | **8 点卷积**，每点取不同通道（8 次 texture） |
| 6 | **splay 混合** | `mix(gradDir, centerDir, splay)` 返回纯方向 | 同上但**乘回原长度** `ep`，保留距离信息 |
| 7 | **中心点 NaN** | 担心 `normalize(vec2(0))` 产生 NaN | 实际用 `max(dot(.,.), 1e-12)` + `inversesqrt` 保护 |
| 8 | **bevel 斜面** | 未提及 | 有独立纹理 + `bevelSize` uniform，控制高光范围和折射混合权重 |
| 9 | **高光计算** | 无 | `smoothstep(0.35, 1.0, dot(refractDir, lightDir))` × bevel 权重 × lightIntensity |
| 10 | **折射双路径** | 单一路径 | `mix(fullRefraction, reducedRefraction, bevelWeight)` — bevel 区折射减弱到 20% |

---

## 7. 参数交互关系（更新后）

```
refractionDistance ──→ 梯度采样步长 (clamp [1, 6])
                    └→ 折射偏移 clamp 上限
                    └→ 非线性缩放系数 (refractionDistance - 1) × 0.5

refractionIntensity ──→ 折射偏移最终缩放

splay ──→ 梯度方向 ↔ 向心方向混合权重（不影响距离）

chromaticAberration ──→ 8 点卷积采样范围（0 时单次采样）

bevelSize ──→ 高光范围（bevelSize × 0.5）
           └→ bevel 权重 smoothstep → 折射混合（全强度 ↔ 20% 强度）

lightAngle ──→ 光源方向（叠加 frameRotation）
            └→ 正面/背面高光方向判定

lightIntensity ──→ 高光亮度系数

height map (texture1) ──→ 梯度方向（中心差分）
                      └→ 折射方向来源

bevel map (texture2) ──→ bevel 权重（1 - bevel → smoothstep）
                     └→ 折射强度混合 + 高光触发
```

---

## 8. 潜在问题（基于实际实现重新评估）

### 8.1 已解决的问题
- **中心点 NaN（部分）**：`ek()` 用 `max(., 1e-12)` 保护，不会 NaN。但 `en()` 中的 `normalize(ec - nodeSpaceCenterPoint)` 仍无保护，中心点处仍有 NaN 风险（见 4.6）。
- **lightAngle/lightIntensity 死参数**：实际被使用，不是死参数。

### 8.2 仍存在的问题

**P1：refractionDistance 同时控制步长和偏移**
- 步长 `step = clamp(refractionDistance, 1, 6)` 决定梯度采样精度
- 偏移 `eo * refractionDistance` 决定折射距离
- 同一个参数同时影响精度和视觉强度，调整时可能互相干扰

**P2：pow(1.4) 非线性曲线的边界行为**
- `eo = sign(eo) * pow(abs(eo), vec2(1.4))`
- 当 `eo` 接近 0 时（平坦区域），pow(0, 1.4) = 0，折射消失——这是期望行为
- 但 pow 对负值的 sign 处理在某些 GPU 上可能不一致（虽然用了 sign + abs 保护）

**P3：bevel 权重与高度图的关系**
- `ev = smoothstep(0, 0.5, 1 - eb(ec))`
- 如果 bevel 纹理（texture2）未正确生成或为空，`eb()` 返回 -1（alpha=0 → 0*2-1=-1），`1-(-1)=2`，`smoothstep(0,0.5,2)=1`——bevel 权重恒为 1，折射恒为 20% 强度
- 需要 CPU 侧确保 bevel 纹理正确生成

**P4：色散 8 点卷积性能**
- 每像素 8 次 `dm()` 调用 = 8 次背景纹理采样
- 加上梯度 4 次高度图采样 + 1 次 bevel 采样 = **13 次纹理采样/像素**
- 在移动端可能有性能压力

**P5：高光依赖 bevel 权重**
- `fd = smoothstep(...) * ev * ez` — 高光乘以 `ev`（bevel 权重）
- 如果 bevel 纹理异常（如 P3），高光也会异常
- lightIntensity=0 时高光为零（符合预期），但 refraction 不受影响（与旧文档的"lightIntensity=0 禁用折射"不同）
