# GL 转 Metal 的 Shader 语义差异

## 版本信息

- 日期：2026-08-28
- 分支：`feature/thunderllei_gpu_backend`
- 背景：libpag 的滤镜（`src/rendering/filters/*`）用 GLSL 编写，通过 tgfx 的 `RuntimeEffect` 提交；tgfx 负责把 GLSL 编译到 GL（直接）或 Metal（GLSL → SPIR-V → MSL）。排查 Metal 后端渲染差异时，发现两类 GL 与 Metal 的 shader 语义差异被直接暴露给了使用方。

---

## 一、varying 声明顺序（名字匹配 vs location 匹配）

### 差异

| 后端 | varying 匹配机制 |
|---|---|
| GL | **按名字**匹配，声明顺序无关 |
| Metal | **按 location**匹配，location 由声明顺序决定 |

### 根因

Metal 后端编译走 `PreprocessGLSL`（`tgfx/src/gpu/ShaderCompiler.cpp`），其中 `assignInputLocationQualifiers` / `assignOutputLocationQualifiers` 会自动给每个 `in`/`out` 变量加 `layout(location=N)`，**N 按声明顺序从 0 递增**（SPIR-V 强制要求 interface 变量带 location）。

一旦加了 location，SPIR-V → MSL 就按 location 匹配，**名字不再参与匹配**。若 vertex shader 的 `out` 顺序与 fragment shader 的 `in` 顺序不一致，location 就对不上，导致 varying 错位。

### 实例（`MotionBlurFilter`）

```glsl
// vertex shader 的 out 顺序
out vec2 vertexColor;      // location 0
out vec2 vCurrPosition;    // location 1
out vec2 vPrevPosition;    // location 2

// fragment shader 的 in 顺序（错误，与 vertex 不一致）
in vec2 vCurrPosition;     // location 0 ← 错位！
in vec2 vPrevPosition;     // location 1
in vec2 vertexColor;       // location 2
```

GL 按名字匹配正常，Metal 按 location 匹配导致三个 varying 全部错位。

### 修复

fragment shader 的 `in` 声明顺序与 vertex shader 的 `out` 对齐。

---

## 二、UBO binding（全局编号 vs per-stage 编号）

### 差异

| 后端 | uniform buffer 的 binding 语义 |
|---|---|
| GL | **全局**（program 级，vertex/fragment 共享一个编号空间） |
| Metal / D3D12 | **per-stage**（vertex/fragment 各自从 0 重新编号） |
| Vulkan | **全局**（descriptor set 内 binding 全局唯一） |
| WebGPU | **全局**（bind group 内 binding 全局唯一） |

### 根因：`ShaderCompiler` 里两套编号规则打架

tgfx 的 `ShaderCompiler.cpp` 给 UBO 分配 binding 用了**两套不一致的规则**：

```cpp
// 规则1：内部 UBO（tgfx 自己渲染用）→ 固定全局编号
assignInternalUBOBindings:  VertexUniformBlock → binding 0
                            FragmentUniformBlock → binding 1

// 规则2：自定义 UBO（RuntimeEffect 用户用）→ per-stage 从 0 编号
assignCustomUBOBindings:    每个 shader 单独从 0 开始 counter++
```

而 `uniformBlocks()` 声明的是**全局编号**。于是：

| UBO | 声明 binding（全局） | Metal MSL 实际 buffer index |
|---|---|---|
| VertexArgs | 0 | 0 ✓ |
| FragmentArgs | 1 | **0** ❌（fragment 里从 0 重编） |

GL 后端通过 `glUniformBlockBinding` 按声明**显式设置全局 binding**，所以正确；Metal 后端把全局 binding 直接当 per-stage buffer index 用，导致自定义 UBO 错位。

### 实例（`MotionBlurFilter`）

```cpp
std::vector<tgfx::BindingEntry> uniformBlocks() const {
  return {{"VertexArgs", 0}, {"FragmentArgs", 1}};  // 两个 stage 各一个 UBO
}
```

在 Metal 下，fragment 的 `FragmentArgs` 实际被编成 buffer 0，与 CPU 写的 binding 1 错位，导致 `maxDistance`/`uVelCenter` 读到 vertex 矩阵的垃圾数据，运动模糊失效。

### 修复（libpag 侧，已做）

把 vertex 专用和 fragment 专用的两个 UBO **合并为一个**（`VertexArgs` 同时装矩阵 + 模糊参数），两个 stage 共享同一个 binding 0，消除多 stage UBO 的编号歧义。这是 Metal per-stage 语义下的正确写法。

---

## 三、各后端现状对照（系统性结论）

上述两类差异本质是 **GL 与 Metal 的底层 shader 语义不一致**，tgfx 目前只屏蔽了 NDC Y 方向一类：

| 语义差异 | GL | Metal | Vulkan | D3D12 | WebGPU | tgfx 现状 |
|---|---|---|---|---|---|---|
| varying 匹配 | 按名字 | 按 location | 按 location | 按 location | 按 location | **未屏蔽** |
| UBO binding | 全局 | per-stage | 全局 | per-stage | 全局 | **未屏蔽** |
| NDC Y 方向 | 向上 | 向下 | 向上 | 向上 | 向上 | 已用 `flip_vert_y` 处理 |

### 关键发现：D3D12 后端已经正确处理了

`D3D12RenderPipeline::createRootSignature`（`tgfx/src/gpu/d3d12/D3D12RenderPipeline.cpp`）里有一段 pre-scan 逻辑，根据 `entry.visibility` 为 Vertex 和 Fragment **各自独立**分配 register index（都从 0 递增），并做了"VertexFragment 可见的 UBO 若两个 stage register 不同则报错"的校验。

**这正是 Metal 后端缺失的逻辑。**

### Metal 后端该怎么改（参考 D3D12）

`MetalRenderPipeline` 目前只有 `uniformBlockVisibility`（binding → visibility），缺 `binding → per-stage index` 映射。需补上：

1. **构造时**（参考 D3D12 的 pre-scan）：
   ```cpp
   uint32_t nextVertexIndex = 0, nextFragmentIndex = 0;
   for (auto& entry : descriptor.layout.uniformBlocks) {
     if (entry.visibility & ShaderVisibility::Vertex)
       uniformVertexIndices[entry.binding] = nextVertexIndex++;
     if (entry.visibility & ShaderVisibility::Fragment)
       uniformFragmentIndices[entry.binding] = nextFragmentIndex++;
   }
   ```

2. **`setUniformBuffer` 时**：用映射出的 index，而不是直接用 binding：
   ```cpp
   if (visibility & Vertex)   setVertexBuffer(atIndex: uniformVertexIndices[binding]);
   if (visibility & Fragment) setFragmentBuffer(atIndex: uniformFragmentIndices[binding]);
   ```

这样 `VertexArgs`（全局 0）→ vertex index 0，`FragmentArgs`（全局 1）→ fragment index 0，与 MSL 的 per-stage 从 0 编号对齐。

### 更深的坑：内部 UBO 与自定义 UBO 编号规则不一致

即使 Metal 补上 per-stage 映射，仍有一个隐患：内部 UBO 的 `FragmentUniformBlock` 由 `assignInternalUBOBindings` 固定编号为 **binding 1**（MSL buffer 1），而自定义 UBO 的 `FragmentArgs` 由 `assignCustomUBOBindings` 编号为 **binding 0**（MSL buffer 0）。两者的 fragment stage 编号规则不同。

因此**彻底的修复是统一 `assignInternalUBOBindings` / `assignCustomUBOBindings` 的编号规则**（统一为 per-stage 语义），并让各后端（Metal/Vulkan/D3D12/GL）按 visibility 做 per-stage 映射。这属于 tgfx 层的接口级重构，正是"分开设置"（binding 显式区分 stage）的方向。

### 建议改进方向

1. **文档化约束**：明确 `RuntimeEffect` 的 varying 声明顺序必须一致、多 stage UBO 必须同名同 binding。
2. **运行时诊断**：pipeline 创建时做 reflection 校验，顺序/binding 不一致直接报错，而非静默渲染错。
3. **接口级重构**：统一 UBO 编号规则为 per-stage 语义，binding 显式区分 stage，参考 WebGPU 官方 `GPUBindGroupLayoutEntry` 的 `binding + visibility` 设计（tgfx 的 `BindingEntry` 已具雏形，但各后端实现未对齐）。

> 注：Metal/SPIR-V 机制决定了 varying 无法回到"按名字匹配"，只能文档化 + 诊断；UBO 的 per-stage 编号在 Metal/D3D12 是硬性机制，Vulkan/WebGPU/GL 是全局编号，需要 tgfx 层统一映射。

---

## 四、WebGPU 官方接口设计（参考）

`GPUBindGroupLayoutEntry` 的核心设计：

```
binding   —— 资源编号（同一 bind group 内全局唯一）
visibility —— 该绑定对哪些 shader stage 可见（VERTEX / FRAGMENT 位标志）
```

**关键**：同一 bind group 内 binding 编号共享、不按 stage 分开；`visibility` 是"过滤"作用（决定可见 stage），不是"划分编号空间"。

tgfx 的 `BindingEntry{binding, visibility}` 已经参考了这个设计，但各后端的实现没有把"共享编号 + visibility"模型正确落地——Metal 缺 per-stage 映射，Vulkan 的 stageFlags 硬编码为 `VERTEX | FRAGMENT`（未按 visibility 区分）。统一修复时应对照此官方设计。
