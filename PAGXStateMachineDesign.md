# PAGX State Machine 设计调研

本文档为 libpag PAGX 体系引入 state machine 能力的前期调研，基于 rive-runtime 仓库的实现与 libpag PAGX 现状摸底整理，供方案讨论使用。

---

## 一、Rive 状态机设计调研

调研对象：`/Users/partyhuang/Documents/UGit/rive-runtime`

### 1.1 整体架构：静态/实例彻底分离

Rive 严格区分**静态定义层**（从 `.riv` 文件反序列化得到的共享只读数据）与**运行时实例层**（每次播放创建、互不污染的状态）。

| 层次 | 静态类（定义） | 实例类（运行时） |
|---|---|---|
| 状态机 | `StateMachine` | `StateMachineInstance` |
| 层 | `StateMachineLayer` | `StateMachineLayerInstance`（匿名类，定义在 cpp 内） |
| 状态 | `LayerState` 及其子类 | `StateInstance` 及其子类 |
| 转换 | `StateTransition` | 无独立实例类，状态由 `StateMachineLayerInstance` 跟踪 |
| 条件 | `TransitionCondition` 子类 | 无独立实例类 |
| 输入 | `StateMachineInput` 子类 | `SMIInput` 子类 |

**Ownership 关系**：
- `StateMachine` 持有 `vector<unique_ptr<StateMachineLayer>>`、`vector<unique_ptr<StateMachineInput>>`、`vector<unique_ptr<StateMachineListener>>`、`vector<unique_ptr<DataBind>>`
- `StateMachineLayer` 持有 `vector<LayerState*>`（裸指针，析构时 delete），以及 `AnyState*`/`EntryState*`/`ExitState*` 三个特殊状态指针
- `LayerState` 持有 `vector<StateTransition*>`
- `StateTransition` 持有 `vector<TransitionCondition*>`
- `StateMachineInstance` 持有 `vector<SMIInput*>`、`StateMachineLayerInstance* m_layers`（new[] 数组）

**封装要点**：`StateMachineLayerInstance` 没有独立头文件，直接定义在 `state_machine_instance.cpp` 的匿名命名空间里，对外只暴露 `StateMachineInstance` 一个入口类。这是值得借鉴的封装思路。

### 1.2 状态类型

继承层次（`LayerState` 为根，通过 `coreType()` 区分）：

- **`AnyState`**：伪状态，代表"任意状态"，其上的 transition 可从任何状态触发。空类，仅作类型标记。
- **`EntryState`**：层入口伪状态，状态机启动时从此处开始。空类。
- **`ExitState`**：层出口伪状态，到达后表示该层结束。空类。
- **`AnimationState`**：播放一个 `LinearAnimation`，`makeInstance` 返回 `AnimationStateInstance`。
- **`BlendState`** 系列（`BlendState1D` / `BlendStateDirect`）：混合多个动画的树节点。

三种系统状态（Any/Entry/Exit）共用 `SystemStateInstance`，其 `advance` / `apply` 均为空操作，`keepGoing()` 返回 false——它们只承担"控制流跳板"的语义，不产生动画输出。

`LayerState::makeInstance` 是工厂方法模式：默认返回 `SystemStateInstance`，子类按需 override（如 `AnimationState` override 返回 `AnimationStateInstance`），使得状态机主循环无需关心具体子类。

### 1.3 转换（Transition）数据结构

`StateTransition` 字段：
- `m_StateToId`：目标状态 id（运行时 resolve 成 `LayerState*`）
- `m_Flags`：`StateTransitionFlags` 位域（`Disabled`、`DurationIsPercentage`、`EnableExitTime`、`ExitTimeIsPercentage`、`PauseOnExit`、`EnableEarlyExit`）
- `m_Duration`：mix 时长（毫秒或百分比）
- `m_ExitTime`：退出时间
- `m_InterpolationType` / `m_InterpolatorId`：mix 时的插值器
- `m_RandomWeight`：随机选择权重

### 1.4 触发条件

`TransitionCondition` 是抽象基类，核心虚方法：

```cpp
virtual bool evaluate(const StateMachineInstance* stateMachineInstance,
                      StateMachineLayerInstance* layerInstance) const;
```

具体子类：
- `TransitionBoolCondition`：支持 `equal` / `notEqual`
- `TransitionNumberCondition`：支持 `equal`/`notEqual`/`lessThanOrEqual`/`lessThan`/`greaterThanOrEqual`/`greaterThan`
- `TransitionTriggerCondition`：检查 trigger 是否被 fire 且未被当前 layer 消费过

条件之间是 **AND 关系**——`StateTransition::allowed` 遍历所有 condition，任一为 false 即返回 `AllowTransition::no`。

### 1.5 Input 系统

三种 Input 类型：

| 类型 | 静态类 | 实例类 | 值字段 |
|---|---|---|---|
| Bool | `StateMachineBool` | `SMIBool` | `bool m_Value` |
| Number | `StateMachineNumber` | `SMINumber` | `float m_Value` |
| Trigger | `StateMachineTrigger` | `SMITrigger` | `bool m_fired` |

**Trigger 的"每层单次消费"机制**（值得特别注意）：
- `SMITrigger` 维护 `m_usedLayers` 列表
- `TransitionTriggerCondition::evaluate` 检查 `triggerInput->m_fired && !triggerInput->isUsedInLayer(layerInstance)`——只有被 fire 且当前 layer 还没消费过才为 true
- transition 被选中后调用 `useLayerInConditions` 标记该 trigger 在此 layer 已消费
- `SMITrigger::advanced()` 在每帧结束时重置 `m_fired = false` 并清空 `m_usedLayers`

这样保证多 layer 场景下，同一个 trigger fire 后每个 layer 最多响应一次。

**Input 变更通知**：`SMIInput::valueChanged()` 调用 `m_machineInstance->markNeedsAdvance()`，即外部设 input 后会标记状态机需要再次 advance，这是触发下一帧推进的信号。

### 1.6 转换执行流程

核心流程在 `StateMachineLayerInstance::tryChangeState`：

1. `findAllowedTransition(stateFromInstance)` 遍历当前 state 的所有 transition，调用 `transition->allowed()` 找第一个可用的。若 state 标记了 `Random` flag，则走 `findRandomTransition` 按权重随机选。
2. `changeState(transition->stateTo())`：销毁旧 `m_currentState`，创建新 StateInstance；触发 atEnd / atStart 事件。
3. 把旧 state 存入 `m_stateFrom`，记录 `m_transition`，`m_mix = 0`。
4. 若 `pauseOnExit` 且有 exitTime，保存 `m_holdAnimation` + `m_holdTime` 以便 apply 时强制固定时间。

**ExitTime 机制**：当 `enableExitTime` 为 true 时，即使所有 condition 都满足，也要等到动画时间越过 `exitTimeSeconds` 才允许转换，此时返回 `AllowTransition::waitingForExit`（既不立即转，也不算失败）。

### 1.7 Mix / Crossfade 实现

在 `updateMix` 中推进 `m_mix` 从 0 趋向 1：

```cpp
m_mix = std::min(1.0f, std::max(0.0f, (m_mix + seconds / mixTime)));
```

`apply` 做实际混合：

```cpp
if (m_stateFrom != nullptr && m_mix < 1.0f) {
    m_stateFrom->apply(artboard, interpolator->transform(m_mixFrom));  // outgoing, 权重递减
}
if (m_currentState != nullptr) {
    m_currentState->apply(artboard, interpolator->transform(m_mix));   // incoming, 权重递增
}
```

`AnimationStateInstance::apply` 调用 `m_AnimationInstance.apply(mix)` 把动画按 mix 权重叠加到 artboard 上。状态机本身不直接驱动属性，而是通过"激活某个 AnimationState → 该 state 的 LinearAnimationInstance 把值写到 artboard"间接驱动。Mix 期间的混合是在 `StateMachineLayerInstance::apply` 里通过两次 `StateInstance::apply(mix)` 完成的——outgoing 写 `mixFrom` 权重，incoming 写 `mix` 权重，artboard 属性做的是后写覆盖前写的叠加。

### 1.8 advance / apply 调用流程

`StateMachineInstance::advance(seconds, newFrame)`：

```
advance(seconds, newFrame)
  ├─ if (newFrame): processFocusEvents / processSemanticEvents / applyEvents / m_needsAdvance=false
  ├─ updateDataBinds(false)
  ├─ for each layer: m_layers[i].advance(seconds, newFrame)   ← 核心
  ├─ advanceDataBinds(seconds)
  └─ for each input: inst->advanced()   ← 重置 trigger 的 m_fired
```

`StateMachineLayerInstance::advance`：

```
advance(seconds, newFrame)
  ├─ m_currentState->advance(seconds, smi)        ← 推进当前 state 的时间
  ├─ updateMix(seconds)                           ← 推进 m_mix
  ├─ if (mix未完 && !holdAnimationFrom): m_stateFrom->advance(seconds, smi)
  ├─ apply()                                      ← 把 stateFrom/currentState 按 mix 作用于 artboard
  └─ for (i=0; updateState(); i++)                ← 状态变更循环，上限 maxIterations=100
       └─ apply()
  → 返回 changedState || m_mix!=1 || m_waitingForExit || m_currentState->keepGoing()
```

`updateState`：若正在 mix 且未开 `enableEarlyExit`，禁止切状态；否则先尝试 `tryChangeState(m_anyStateInstance)`（any state 的 transition 全局生效），再尝试 `tryChangeState(m_currentState)`。

**单帧内可连续切换多次**（最多 100 次），这处理了 Entry → A → B 这种链式跳转，避免了一帧只切一次导致的延迟。

### 1.9 advanceAndApply 的 fix-point 循环

`StateMachineInstance::advanceAndApply` 把状态机推进与 artboard 更新交织：

```
advanceAndApply(seconds)
  ├─ advance(seconds, true)
  ├─ artboard->advanceInternal(seconds, Animate|AdvanceNested|NewFrame)
  └─ for (outerOptionC in 0..5):    ← 最多 5 轮 fix-point
       ├─ artboard->updatePass(true)
       ├─ if (tryChangeState()): advance(0.0f, false)   ← 状态变化后再推进 0 秒
       ├─ artboard->advanceInternal(0.0f, Animate|AdvanceNested)
       ├─ reset()
       └─ if (!artboard->hasDirt(Components)): break
```

这处理了"状态机改动 artboard → artboard 触发新事件 → 事件又改 input → 状态机又要切状态"的反馈循环。

### 1.10 对外暴露的 API

入口在 `ArtboardInstance`：

```cpp
std::unique_ptr<StateMachineInstance> stateMachineAt(size_t index);
std::unique_ptr<StateMachineInstance> stateMachineNamed(const std::string& name);
std::unique_ptr<StateMachineInstance> defaultStateMachine();
```

`StateMachineInstance` 继承自 `Scene`，对外接口：

- **设 input**：`getBool(name)->value(x)` / `getNumber(name)->value(x)` / `getTrigger(name)->fire()`
- **推进**：`advance(seconds)` / `advanceAndApply(seconds)`
- **查询状态变化**：`stateChangedCount()` / `stateChangedByIndex(i)` 返回上一帧发生变化的 LayerState
- **是否需要继续推进**：`needsAdvance()` / `advance` 返回值
- **重置**：`resetState()` 把每个 layer 切回 EntryState
- **指针交互**：`pointerDown/Move/Up/Exit` 等，用于 hit-test 触发 listener

典型用法：

```cpp
auto abi = artboard->instance();
rive::StateMachineInstance* smi = new rive::StateMachineInstance(stateMachine, abi.get());
smi->advance(0.0f);            // 启动
while (smi->needsAdvance()) {  // 主循环
    smi->advanceAndApply(dt);  // 驱动一帧
    // 渲染 artboard...
}
```

### 1.11 Rive 设计评价

**值得借鉴的优点**：
1. 静态/实例彻底分离，同一份定义可被多个 instance 并发引用
2. `StateInstance` 工厂方法 + 多态，新增 state 类型对主循环零侵入
3. `StateMachineLayerInstance` 隐藏在 cpp 匿名命名空间，对外只暴露一个入口类
4. 单帧内状态链式跳转（`maxIterations=100`）
5. Trigger 的 per-layer 单次消费
6. advanceAndApply 的 fix-point 循环处理反馈
7. AnyState 作为伪状态统一进 transition 查找逻辑
8. PauseOnExit + ExitTime 支持常见交互需求
9. 代码生成：字段、反序列化、clone 由 schema 生成

**可能存在的局限性**：
1. `StateTransition::m_EvaluatedRandomWeight` 是 mutable 共享字段，多 instance 并发时存在数据竞争
2. transition 查找是线性遍历，O(n)
3. 没有 transition 的优先级概念，只能按声明顺序取第一个
4. mix 是线性权重叠加，不是真正的动画混合（后写覆盖前写）
5. layer 之间无同步机制，跨 layer 协作得通过 input 间接传递
6. 不支持热替换状态，状态图在 import 后固定
7. 事件/listener/focus/semantic 体系非常庞大（单文件近 2700 行）

### 1.12 Rive 的 ViewModel 与 StateMachine 关系

**核心发现：rive 有两套独立的 input 体系，平行共存，不强制互通**。

#### 体系 A：SMIInput（传统 input，宿主代码直接 set）

- 类层级：`StateMachineInput` → `StateMachineBool/Number/Trigger`
- 运行时实例：`SMIBool/SMINumber/SMITrigger`
- input 是 `StateMachine` 的直接子节点（`m_Inputs`）
- 值由**宿主代码直接 set**，不经过 ViewModel：`machine->getBool("hover")->value(true)`
- `TransitionCondition` 直接读 SMIInput 实例的值

#### 体系 B：BindableProperty + DataBind（VM-driven）

- `BindableProperty` 挂在 StateMachine 上（与 SMIInput 平级）
- `DataBind` 的 target 是 `BindableProperty`，source 是 `ViewModelInstanceValue`
- DataBind 也挂在 StateMachine 上（`m_dataBinds`）
- Transition 条件通过 `TransitionViewModelCondition` 间接读 BindableProperty 的值

**关键事实：两套体系不互通**。在 `DataBind::import`（`src/data_bind/data_bind.cpp:101-127`）的 target 类型分发表里，列出所有可绑定的 target：

```
BindablePropertyNumber / String / Boolean / Enum / Color / Trigger / ...
StateTransition
Component (artboard 属性)
```

**`StateMachineBool`/`StateMachineNumber`/`StateMachineTrigger` 不在这个列表里**。也就是说：
- 想用 VM 驱动状态机 → 必须用 BindableProperty（体系 B），不能用 SMIInput
- 想用宿主代码直接驱动状态机 → 必须用 SMIInput（体系 A），不能假装是 VM

两套是平行通道，设计上故意不互通。

#### Rive 的三条数据流路径

**路径 1：VM → Artboard 属性（不经状态机）**

```
ViewModelInstanceValue
   ↓ DataBind (toTarget, target = Component*)
Rectangle.width / Shape.rotation / SolidColor.color / TextRun.text
```

ViewModel 可以完全独立于状态机使用，直接驱动 artboard 属性。

**路径 2：VM → BindableProperty → TransitionViewModelCondition（VM 驱动状态机）**

```
ViewModelInstanceValue
   ↓ DataBind (toTarget, target = BindableProperty*)
BindableProperty (状态机的子节点)
   ↑
TransitionViewModelCondition::evaluate 读取
```

VM 改值 → `advanceAndApply` → `updateDataBinds` 把 VM 写到 BindableProperty → 状态机评估 transition 时读 BindableProperty。

**路径 3：宿主代码 → SMIInput（直接 set，不经 VM）**

```
machine->getBool("hover")->value(true)
   ↓
SMIBool.m_Value
   ↑
TransitionBoolCondition::evaluate 直接读
```

完全独立于 ViewModel，交互式场景（鼠标 hover、点击）走这条路径。

#### Rive 的执行顺序

`StateMachineInstance::advanceAndApply` 按以下顺序：

```cpp
1. SM advance (含 listener 触发)
2. Artboard advance (含 updateDataBinds，执行路径 1 和 2)
3. fix-point 循环 (最多 5 轮):
   a. updateDataBinds (VM → target)
   b. tryChangeState (重新评估 transition，读 BindableProperty 和 SMIInput)
   c. SM advance(0) (状态变化后再推进)
   d. Artboard advance(0)
   e. reset (清理 trigger)
   f. 若无 dirty 则 break
```

#### Rive 设计意图总结

**为什么有两套 input**：
- SMIInput 解决交互式输入（鼠标 hover、点击），宿主代码直接设值，快速、无绑定开销
- BindableProperty + DataBind 解决声明式数据绑定，VM 改了状态机自动响应，宿主代码不用手写同步

两套不互通是有意的解耦：
- SMIInput 不需要 VM 也能工作（满足"无 VM 场景"）
- BindableProperty 不需要宿主代码手设（满足"声明式数据驱动"场景）
- 同一个状态机可以同时用两套：比如 transition A 用 SMIInput（hover），transition B 用 BindableProperty（VM 数据状态）

**BindableProperty 是"VM 端口"**：rive 不让 transition 直接读 VM，而是要求在状态机上显式声明一个 BindableProperty 作为端口，再用 DataBind 把 VM 绑到这个端口。好处：
1. 状态机 schema 自包含，不依赖 VM 路径（编译时就能检查 BindableProperty 是否存在）
2. 同一个状态机可被不同 VM 实例驱动（端口固定，绑定的 VM 可换）
3. transition 条件求值时不走 VM 路径解析，直接读 BindableProperty 值，性能稳定

---

## 二、libpag PAGX 现状摸底

### 2.1 没有存量 state machine 实现

对 `src/`、`include/`、`test/src/` 全量搜索 `StateMachine|stateMachine|skeleton|Skeleton|bone|Bone`：
- `src/pagx/utils/RasterUtils.cpp:55` 仅有注释 "Common skeleton shared by every off-screen GPU render path"，与骨骼动画无关
- `test/src/PAGXTest.cpp:5664` 测试用例注释把 PAGTimeline 的 play/pause/stop 行为称为 "state machine"，是描述性称呼，不是独立类
- 没有任何 `stateMachineTimeline`、`Skeleton`、`Bone` 类型存在

**结论**：state machine 能力是从零开始新增，没有可冲突的存量同名实现。

### 2.2 PAGTimeline 已有一个最小的"隐式状态机"

`include/pagx/PAGTimeline.h:51-167` 与 `src/pagx/PAGTimeline.cpp:104-176`：

```cpp
// include/pagx/PAGTimeline.h:162-163
int64_t currentTimeUs = 0;
bool playing = true;
```

状态字段只有 `playing`（bool）+ `currentTimeUs`（int64_t）。状态转移方法：
- `play()` — `playing = true`
- `pause()` — `playing = false`，时间保留
- `stop()` — `playing = false` + `currentTimeUs = 0`
- `setCurrentTime(us)` — 直接设置时间
- `advance(deltaUs)` — 按 `LoopMode` 推进时间，可能触发 `playing = false`（Once 模式到尾）

这是一个"隐式状态机"：状态空间是 `{playing, paused, stopped}` + 连续时间值，但没有命名状态、转移条件、守卫的概念。state machine timeline 不是替代它，而是并列——一个 Layer 可同时挂 `AnimationTimeline`（驱动连续动画）和 `StateMachineTimeline`（驱动离散状态切换），两者按 mix 叠加到同一 channel。

### 2.3 PAGX 的数据/运行时分离结构

**数据层**（`include/pagx/`）：
- `pagx::PAGXDocument`（`include/pagx/PAGXDocument.h:48`）— 数据根，继承 `Node`，`NodeType::Document`。拥有 `layers`、`animations`、`viewModel`、`dataBinds`、`nodes`（owning unique_ptr vector）、`nodeMap`（id 索引）
- `pagx::Node`（`include/pagx/nodes/Node.h:247`）— 所有数据节点基类，仅含 `id`、`customData`、`sourceLine`、`virtual nodeType()`
- `pagx::Layer`（`include/pagx/nodes/Layer.h:51`）— 层节点，含 `visible`、`alpha`、`matrix`、`composition`、`timelines`（vector<unique_ptr<Timeline>>）、`contents`、`styles`、`filters`、`children`、布局字段
- `pagx::Composition`（`include/pagx/nodes/Composition.h:35`）— 可复用合成资源，含 `layers`、`animations`、`viewModel`、`dataBinds`
- `pagx::Animation`（`include/pagx/nodes/Animation.h:52`）— timeline 数据，含 `duration`、`frameRate`、`loop`（LoopMode）、`objects`
- `pagx::AnimationObject`（`include/pagx/nodes/AnimationObject.h:34`）— 单个 target 的动画对象，含 `target`（Node id）、`channels`
- `pagx::Channel`（`include/pagx/nodes/Channel.h:63`）— 抽象 channel 基类，`TypedChannel<T>` 是具体特化
- `pagx::Keyframe<T>`（`include/pagx/nodes/Keyframe.h:86`）— 单个关键帧

**运行时层**（`include/pagx/` + `src/pagx/runtime/`）：
- `pagx::PAGScene`（`include/pagx/PAGScene.h:64`）— **runtime root**，继承 `enable_shared_from_this<PAGScene>`（不继承 PAGComposition）。拥有 `_rootComposition`（shared_ptr<PAGComposition>）、`displayList`、`timelinesByAnimation`（Animation* → PAGTimeline 缓存）、`layerRegistry`。提供 `getTimeline(id)` / `getDefaultTimeline()` / `advanceAndApply(delta)` / `draw(surface)`
- `pagx::PAGComposition`（`include/pagx/PAGComposition.h:45`）— 继承 `PAGLayer`，增加 `binding`（unique_ptr<RuntimeBinding>）、`timelines`（vector<shared_ptr<PAGTimeline>>）、`compositionViewModel`、`dataBindRuntime`、`dataContext`、`document`（PAGXDocument* 用于 channel target 解析）
- `pagx::PAGLayer`（`include/pagx/PAGLayer.h:52`）— 运行时层基类，含 `node`（源 Layer*）、`runtimeLayer`（shared_ptr<tgfx::Layer>）、`children`、`rootScene`（weak_ptr<PAGScene>）。`advance(delta)` / `apply(mix)` 是 virtual
- `pagx::PAGTimeline`（`include/pagx/PAGTimeline.h:51`）— 单 Animation 控制器，含 `owner`（weak_ptr<PAGScene>）、`animation`、`binding`、`contextDoc`、`resolvedTargets`（缓存）、`currentTimeUs`、`playing`

### 2.4 Timeline 体系已为 state machine 预留扩展点

`include/pagx/nodes/Timeline.h:25-32`：

```cpp
/**
 * Discriminator for Timeline subclasses. Used by importers, exporters, and runtime layout to
 * dispatch on the concrete timeline kind without dynamic_cast. Future timeline kinds (e.g. time
 * machines, state machines) extend this enum.
 */
enum class TimelineType : uint8_t {
  Animation = 0,
};
```

`Timeline` 抽象基类：

```cpp
class Timeline {
 public:
  virtual ~Timeline() = default;
  virtual TimelineType timelineType() const = 0;
protected:
  Timeline() = default;
};
```

`AnimationTimeline`（`include/pagx/nodes/AnimationTimeline.h:31`）是当前唯一的具体子类，含 `animationId`（引用 Animation 的 id）和 `playing`（初始播放状态）。

**结论**：state machine 的数据层应新增 `pagx::StateMachineTimeline : public Timeline`（与 `AnimationTimeline` 并列），并在 `TimelineType` 加 `StateMachine` 枚举值。这是设计上预留的扩展点，注释明确点名了 "state machines"。

### 2.5 没有 skeleton / bone 预留

`NodeType` 枚举（`include/pagx/nodes/Node.h:30-240`）列出所有节点类型，分九大类：Document、Resources、Animation（Animation/AnimationObject/Channel）、Layer、Layer Styles、Layer Filters、Elements、Text Selectors、ViewModel/DataBind/DataConverter。**没有 Skeleton、Bone、StateMachine、State、Transition 任何相关类型**。`TimelineType` 是唯一明确提到 state machine 的预留点。

### 2.6 动画播放体系

**PAGX 播放循环**（`src/pagx/PAGScene.cpp:442-447`）：

```cpp
void PAGScene::advanceAndApply(int64_t deltaMicroseconds) {
  if (_rootComposition != nullptr) {
    _rootComposition->advance(deltaMicroseconds);
    _rootComposition->apply();
  }
}
```

调用链：
1. 业务侧调用 `scene->advanceAndApply(deltaUs)` 或 `scene->getTimeline(id)->advanceAndApply(deltaUs, mix)`
2. `PAGComposition::advance(delta)`（`src/pagx/runtime/PAGComposition.cpp:53-58`）：先迭代 `timelines` 调 `timeline->advance(delta)`，再调 `PAGLayer::advance(delta)` 递归到 children
3. `PAGComposition::apply(mix)`（`src/pagx/runtime/PAGComposition.cpp:60-65`）：先迭代 `timelines` 调 `timeline->apply(mix)`，再调 `PAGLayer::apply(mix)` 递归到 children
4. `PAGLayer::advance/apply`（`src/pagx/PAGLayer.cpp:99-111`）是基础情况，只递归 children
5. 业务侧单独调 `scene->draw(surface)`，draw 内部先 `flushDataBinds()` 再 `displayList->render()`

**关键设计意图**：advance（推进时间，无副作用）与 apply（把时间映射到 channel 值并写入 binding）严格分离。这是 state machine 接入的天然缝隙——state machine 的"推进状态"对应 advance，"应用状态输出到 channel"对应 apply。

**Per-composition 实例化**：每次 `Layer` 节点的 `composition` 字段引用一个 `Composition`，运行时 `PAGComposition::MakeChild()` 为每次引用构造独立的 `PAGComposition`，各自持有独立的 binding、timelines、ViewModel 实例。state machine 应该跟着这个模型——每个 composition 引用都创建独立的 state machine 实例（独立当前态、独立转移进度）。

### 2.7 可复用的基础设施

#### Keyframe 系统
- `Keyframe<T>` 模板：`time`（Frame）、`value`（T）、`interpolation`（KeyframeInterpolationType）、`bezierOut` / `bezierIn`（Point）。可直接复用做 state 间过渡的 easing 描述
- `KeyframeInterpolationType`：`None/Linear/Bezier/Hold`。Hold 模式天然适合"状态切换时立刻跳到新值"，Linear/Bezier 适合"状态间平滑过渡"
- `KeyframeEvaluator`：运行时求值器，处理 bezier easing 与 Color/float lerp
- `KeyValue` variant：`std::variant<float, bool, int, std::string, ImageRef, Color, Matrix, shared_ptr<PAGImage>>`——state machine 输出值类型可以复用
- `TypedChannel<T>::onEvaluateAtMicros(microseconds, frameRate)`：把微秒时间映射到帧位置并求值，state machine 的"过渡进度 → 输出值"可以复用这个求值机制

#### RuntimeBinding / RuntimeTarget
`src/renderer/LayerBuilder.h:54-126` 定义了 channel 写入的抽象：

```cpp
using RuntimeWriter = void (*)(void* object, const KeyValue& value, float mix);
using RuntimeReader = bool (*)(const void* object, KeyValue* out);

struct RuntimeTarget {
  virtual bool apply(const std::string& channel, const KeyValue& value, float mix);
  virtual bool read(const std::string& channel, KeyValue* out) const;
};

struct RuntimeBinding {
  bool apply(const Node* node, const std::string& channel, const KeyValue& value, float mix) const;
  // ...
};
```

`RuntimeBinding` 是"PAGX 节点 → tgfx 运行时对象 + channel 写入器"的映射表。所有 timeline（包括未来的 state machine timeline）都通过 `binding->apply(node, channelName, value, mix)` 写入 channel，机制与 timeline 类型无关。

PAGTimeline 通过 `ApplyResolved()` 调用：

```cpp
binding->apply(targetNode, channel->name,
               channel->evaluateAt(microseconds, animation->frameRate), mix);
```

**Mix 叠加规则**（`include/pagx/PAGTimeline.h:113-125`）：
- 连续 channel（float / Color）：`result = lerp(current, evaluated, mix)`
- 离散 channel（bool / int / string / ImageRef）：mix > 0 时直接覆盖
- 多个 timeline 按调用顺序叠加，后写入的 mix 与前一次写入的结果混合

**结论**：state machine timeline 输出 channel 时无需新建写入路径，直接复用 `RuntimeBinding::apply()`，并自动遵循 mix 叠加规则。

#### ObserverHandle
`include/pagx/ObserverHandle.h:32-56` + `include/pagx/PAGViewModelValue.h:45-154`：

```cpp
class PAGViewModelValue : public std::enable_shared_from_this<PAGViewModelValue> {
 public:
  using Observer = std::function<void()>;
  ObserverHandle addObserver(Observer observer);
  bool hasChanged() const { return dirty; }
 protected:
  void notifyValueChanged();
  void notifyChanged(bool fromVM);
  void notifyObservers();
  void clearDirty();
};

class ObserverHandle {
 public:
  void detach();
  // RAII: 析构自动移除 observer
};
```

机制要点：
- `Observer` 是 `std::function<void()>` 回调
- `ObserverHandle` 是 RAII 句柄，析构自动 detach
- `dirty` 标志 + `clearDirty()` 在帧末重置

**结论**：这套 observer 模式可直接复用做 state change 通知。state machine 的"当前态改变"事件可以照搬：`PAGStateMachine::addStateChangeListener(callback)` 返回 `ObserverHandle`。

### 2.8 现有 ViewModel 与 DataBind 系统

**ViewModel**（`include/pagx/nodes/ViewModel.h:34`）放在 `<Resources>` 里，是数据绑定源。支持 8 种 property type：`Number`、`String`、`Boolean`、`Color`、`Image`、`ViewModel`（嵌套）、`Enum`、`Trigger`。

**DataBind**（`include/pagx/nodes/DataBind.h:63`）字段：`source`（`$vm.xxx` 路径）、`target`（`@layerId`）、`channel`、`direction`（`ToTarget`/`ToSource`/`TwoWay`/`Once`）。

**runtime 层**：
- `PAGViewModel`（`include/pagx/PAGViewModel.h:52`）持有 `propertyMap` + `propertyList`
- `PAGViewModelValue`（`include/pagx/PAGViewModelValue.h:45`）是 typed value 基类，提供 `addObserver` + dirty 机制
- `DataBindRuntime`（`src/pagx/DataBindRuntime.h:38`）每 composition 一个，负责把 VM 值同步到 channel
- `DataContext::resolve(path)`（`src/pagx/DataContext.cpp:37`）解析 `$vm.xxx` 路径到 `PAGViewModelValue*`

**关键观察**：ViewModel 的 `Boolean`/`Number`/`Trigger` 三种类型，与 state machine input 的 `bool`/`number`/`trigger` 类型天然对齐。`DataContext::resolve()` 是现成的 VM 值查找入口。

---

## 三、XML Schema 设计

### 3.1 放置位置

`<StateMachine>` 与 `<Animation>` 并列，都放在 `<Animations>` 容器里。语义上两者都是时序驱动器（Animation 是单段时间轴，StateMachine 是状态图驱动的时序），同类。`<Resources>` 仅保留被动资源（Image/PathData/Font/Composition/ViewModel/ColorSource 等）。

### 3.2 设计原则：状态机不感知 ViewModel

**借鉴 rive 的双体系设计**，state machine 设计为自包含的逻辑单元，不感知 ViewModel 的存在：

- **StateMachine 层**：只关心状态、转移、input 端口，自包含。可独立设计、独立测试、独立复用
- **DataBind 层**：负责把 VM 值同步到 input 端口（或直接到 Layer channel），是 VM 与 SM 的桥梁
- **ViewModel 层**：纯数据模型，不感知消费方

三个层次解耦。状态机可以在没有 ViewModel 的场景下独立工作（宿主代码直接设 input），也可以在需要声明式数据驱动时通过 DataBind 接入 VM。

### 3.3 完整示例

以一个按钮为例，它有两个正交维度：视觉状态（normal/hover/pressed）和启用状态（enabled/disabled）。两个维度用两个并行 StateRegion 表达，避免 3×2=6 个状态的组合爆炸。这里 `btnSM` 是**顶层 SM**——定义在 `<Animations>` 里，业务通过 `getStateMachineTimeline("btnSM")` 拿来自己驱动，**不需要**被 Layer 的 `<Timelines>` 引用（对齐顶层 Animation 通过 `getTimeline` 拿取的模型）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Layer id="btn">
    <Rectangle width="100" height="40"/>
    <Fill color="#0099FF"/>
  </Layer>

  <Animations>
    <!-- State 绑定的动画，与 StateMachine 同容器 -->
    <Animation id="normalAnim" duration="30" frameRate="60" loop="loop">
      <Object target="btn">
        <Channel name="scaleX" type="float">
          <Key time="0" value="1"/>
          <Key time="30" value="1"/>
        </Channel>
      </Object>
    </Animation>
    <Animation id="hoverAnim" duration="15" frameRate="60" loop="loop">
      <Object target="btn">
        <Channel name="scaleX" type="float">
          <Key time="0" value="1"/>
          <Key time="15" value="1.1"/>
        </Channel>
      </Object>
    </Animation>
    <Animation id="pressedAnim" duration="10" frameRate="60" loop="loop">
      <Object target="btn">
        <Channel name="scaleX" type="float">
          <Key time="0" value="0.95"/>
          <Key time="10" value="0.95"/>
        </Channel>
      </Object>
    </Animation>
    <Animation id="enabledAnim" duration="1" frameRate="60" loop="loop">
      <Object target="btn">
        <Channel name="alpha" type="float"><Key time="0" value="1"/></Channel>
      </Object>
    </Animation>
    <Animation id="disabledAnim" duration="1" frameRate="60" loop="loop">
      <Object target="btn">
        <Channel name="alpha" type="float"><Key time="0" value="0.4"/></Channel>
      </Object>
    </Animation>

    <!-- 状态机定义，与 Animation 并列 -->
    <StateMachine id="btnSM">
      <Inputs>
        <Input name="isHover" type="bool" default="false"/>
        <Input name="isPressed" type="bool" default="false"/>
        <Input name="isEnabled" type="bool" default="true"/>
        <Input name="click" type="trigger"/>
      </Inputs>

      <!-- 视觉维度 -->
      <StateRegion name="visual" initialState="normal">
        <States>
          <State name="normal" animation="@normalAnim"/>
          <State name="hover" animation="@hoverAnim"/>
          <State name="pressed" animation="@pressedAnim"/>
        </States>
        <Transitions>
          <Transition from="normal" to="hover" duration="10"
                      interpolation="bezier" bezier-out="0.42,0" bezier-in="0.58,1">
            <Condition input="isHover" op="equal" value="true"/>
          </Transition>
          <Transition from="hover" to="normal" duration="10">
            <Condition input="isHover" op="equal" value="false"/>
          </Transition>
          <Transition from="hover" to="pressed" duration="5">
            <Condition input="isPressed" op="equal" value="true"/>
          </Transition>
          <Transition from="pressed" to="normal" duration="10">
            <Condition input="isPressed" op="equal" value="false"/>
          </Transition>
          <!-- from="any" 表示本 region 内任意状态可触发 -->
          <Transition from="any" to="normal" duration="5">
            <Condition input="click" op="trigger"/>
          </Transition>
        </Transitions>
      </StateRegion>

      <!-- 启用维度，与 visual 并行运行 -->
      <StateRegion name="enable" initialState="enabled">
        <States>
          <State name="enabled" animation="@enabledAnim"/>
          <State name="disabled" animation="@disabledAnim"/>
        </States>
        <Transitions>
          <Transition from="enabled" to="disabled" duration="5">
            <Condition input="isEnabled" op="equal" value="false"/>
          </Transition>
          <Transition from="disabled" to="enabled" duration="5">
            <Condition input="isEnabled" op="equal" value="true"/>
          </Transition>
        </Transitions>
      </StateRegion>
    </StateMachine>
  </Animations>
</pagx>
```

### 3.4 节点含义详解

#### 3.4.1 为什么需要 StateMachine 节点

**现有体系的两个局限**：
- `<Animation>` 只能做"连续时间轴"，无法表达"根据是否 hover 切换两段动画"这种离散逻辑
- `PAGTimeline` 的 play/pause/stop 是隐式状态机，没有命名状态、转移条件、过渡曲线的概念

**StateMachine 的定位**：与 `<Animation>` 并列的时序驱动器。Animation 是单段时间轴按时间推进插值 channel，StateMachine 是状态图驱动器按状态转移切换绑定的 Animation 并在转移期间做 mix 过渡。

#### 3.4.2 `<StateMachine>` — 状态机定义

```xml
<StateMachine id="btnSM">
  <Inputs>...</Inputs>
  <StateRegion name="visual" initialState="normal">...</StateRegion>
  <StateRegion name="enable" initialState="enabled">...</StateRegion>
</StateMachine>
```

**为什么独立成节点**：与 `<Animation>` 同构——Animation 是可被多个 Layer 引用的可复用资源，StateMachine 也是。同一个按钮 SM 定义一次，多个按钮实例各自挂引用，各自独立运行。

| 属性 | 必填 | 说明 |
|---|---|---|
| `id` | 是 | 唯一标识，被 `<StateMachine ref="@id"/>` 引用 |

子标签结构：先声明 `<Inputs>`（全 region 共享的输入端口），再声明一个或多个 `<StateRegion>`。

#### 3.4.3 `<StateRegion>` — 并行状态区域

```xml
<StateRegion name="visual" initialState="normal">
  <States>...</States>
  <Transitions>...</Transitions>
</StateRegion>
```

**为什么需要 StateRegion**：概念来自 UML State Machine 的 orthogonal region（正交区域）。一个状态机可以拆成多个独立并行的状态区域，每个 region 有自己的当前状态和转移逻辑，互不阻塞。

**核心价值：维度正交，避免组合爆炸**。以按钮为例，视觉维度（normal/hover/pressed，3 态）和启用维度（enabled/disabled，2 态）是两个独立维度：
- 单 region 方案：必须枚举 3×2=6 个组合状态（normal_enabled、normal_disabled…），加维度就指数爆炸
- 多 region 方案：视觉 3 态 + 启用 2 态 = 5 个状态，两个 region 并行组合出 6 种表现

每个 region 独立响应 input——visual region 的 hover 转移不需要关心 enable region 是 enabled 还是 disabled。

**运行时**：一个 StateMachine 实例持有多个 region 实例，每个 region 有独立当前状态。每帧所有 region 并行 advance/apply，各自输出叠加到 channel。

| 属性 | 必填 | 说明 |
|---|---|---|
| `name` | 是 | region 名（机内唯一），用于 runtime 查询该 region 当前状态 |
| `initialState` | 是 | 该 region 的初始状态 name，状态机启动时进入 |

子标签顺序固定 `States → Transitions`。

**为什么 `initialState` 放在 StateRegion 上**：每个 region 有自己独立的初始状态。`<StateMachine ref>` 引用处不再需要 `initialState`——初始状态是 region 的固有属性，由 StateMachine 定义决定，不随引用位置变化。

#### 3.4.4 `<Inputs>` — 输入声明

```xml
<Inputs>
  <Input name="isHover" type="bool" default="false"/>
  <Input name="click" type="trigger"/>
</Inputs>
```

**为什么需要 Input**：状态机的转移需要由外部信号触发。Input 是状态机与外部世界的接口——宿主代码或 ViewModel（通过 DataBind）通过设置 input 的值来驱动状态切换。

**作用域**：Inputs 声明在 StateMachine 层级，被所有 StateRegion 共享。多个 region 可以监听同一个 input（如 `click` trigger 同时驱动 visual 和 enable region）。

**与 ViewModel 的关系**：状态机不感知 ViewModel。Input 是状态机自己的端口，由谁设值（宿主代码 or DataBind）是外部决定的事。详见 3.5 节。

| 属性 | 必填 | 说明 |
|---|---|---|
| `name` | 是 | input 名，`<Condition>` 用此名引用 |
| `type` | 是 | `bool` / `number` / `trigger` |
| `default` | 否 | 初始值。bool 为 `true`/`false`，number 为数字字面量。trigger 不需要 default |

**三种 type 的语义**：
- `bool`：持续状态量。如 `isHover` 表示"鼠标是否悬停"，保持 true 期间一直满足条件
- `number`：数值量。如 `health` 表示血量，可做 `lessThan 0` 判断
- `trigger`：一次性事件。`fireTrigger` 只置标志（不立即评估），下一次 `advance` 才评估消费，被每个 region 各消费一次后在该次 advance 末尾清空。fire 后若未 advance，标志保留到下次 advance。如 `click` 表示"点击事件发生了"，消费后不持续保持

#### 3.4.5 `<States>` — 状态定义

```xml
<States>
  <State name="normal" animation="@normalAnim"/>
  <State name="idle"/>  <!-- 空状态 -->
</States>
```

**为什么 State 绑定 Animation 而不是直接内联关键帧**：复用已有 Animation 体系。一个 State 对应一段视觉表现（一段动画），Animation 节点已经在做这件事。直接引用即可，不重复造轮子。

**空状态语义**：`<State name="idle"/>` 不绑定动画，`advance`/`apply` 是空操作。用于纯逻辑跳板——比如"loading 状态不显示任何东西，等条件满足后转 next 状态"。对应 rive 的 SystemStateInstance。

**作用域**：State name 在所属 StateRegion 内唯一即可，不同 region 可以有同名 state。

| 属性 | 必填 | 说明 |
|---|---|---|
| `name` | 是 | 状态标识（region 内唯一），`<Transition from/to>` 用此名引用 |
| `animation` | 否 | 该状态绑定的 Animation 节点的 `@id` 引用。省略则为空状态 |

#### 3.4.6 `<Transitions>` — 转移定义

```xml
<Transitions>
  <Transition from="normal" to="hover" duration="10"
              interpolation="bezier" bezier-out="0.42,0" bezier-in="0.58,1"
              exitTime="15">
    <Condition input="isHover" op="equal" value="true"/>
  </Transition>
  <Transition from="any" to="normal" duration="5">
    <Condition input="click" op="trigger"/>
  </Transition>
</Transitions>
```

**作用域**：transition 只在所属 StateRegion 内生效，`from`/`to` 引用的是本 region 内的 state。

**为什么是扁平列表而不是嵌套在 State 里**：两个原因：
1. `from="any"` 可以统一表达"本 region 内任意状态可触发的转移"，不用在每个 State 里重复声明
2. 转移是"从 A 到 B"的关系，逻辑上属于 A 和 B 两者，强行归属任一方都不对称。扁平列表更清晰

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| `from` | 是 | — | 起始状态 id。`any` 表示本 region 内任意状态（对应 rive 的 AnyState 伪状态） |
| `to` | 是 | — | 目标状态 id（本 region 内） |
| `duration` | 是 | — | 过渡时长，单位帧（与 `Keyframe.time` 一致） |
| `interpolation` | 否 | `linear` | `none`/`linear`/`bezier`/`hold`，复用 Keyframe 系统 |
| `bezier-out` | 否 | `0,0` | 贝塞尔出向控制点，仅 `bezier` 有效 |
| `bezier-in` | 否 | `0,0` | 贝塞尔入向控制点，仅 `bezier` 有效 |
| `exitTime` | 否 | 无 | 动画播到该帧后才允许转移 |
| `earlyExit` | 否 | `false` | 是否允许在过渡未完成时打断当前过渡触发本转移 |
| `pauseOnExit` | 否 | `false` | 过渡期间是否把来源状态冻结在 exitTime 帧（仅在设了 exitTime 时有意义）|

**`interpolation` 各值含义**：
- `none`：不过渡，立即跳变
- `linear`：线性插值
- `bezier`：贝塞尔曲线插值（用 `bezier-out`/`bezier-in`）
- `hold`：保持起始值直到过渡结束再跳变

**`exitTime` 语义**：即使所有 Condition 都满足，也要等到当前 Animation 播到 `exitTime` 帧才允许转换。典型场景：loading 动画 60 帧，`exitTime="60"` 表示必须播完才能转 success 状态。

**`earlyExit` 语义**：默认 `false`——转移只在本 region 没有正在进行的过渡时才被评估。设为 `true` 时，可在过渡进行中打断：当前 outgoing 状态继续淡出，新目标状态淡入（多个 outgoing 状态可叠加淡出）。对应 rive 的 `EnableEarlyExit` flag。

**`pauseOnExit` 语义**：默认 `false`——过渡期间来源状态继续播放。设为 `true` 时（需配合 `exitTime`），来源状态在过渡期间冻结在 exitTime 帧不动，只做混合不继续播。用于让淡出的姿势保持稳定（如走路→跑步时走路停在某个稳定姿势再混合，避免腿部继续摆动的抖动）。对应 rive 的 `PauseOnExit` flag。

#### 3.4.7 `<Condition>` — 转移条件

```xml
<Condition input="isHover" op="equal" value="true"/>
<Condition input="health" op="lessThan" value="0"/>
<Condition input="click" op="trigger"/>
```

**多个 Condition 是 AND 关系**：所有都为 true 时 transition 才被允许。OR 可通过"多个 transition 同一个 to"模拟（任一触发就转），所以初期不支持显式 OR。

| 属性 | 必填 | 说明 |
|---|---|---|
| `input` | 是 | 引用 StateMachine 的 `<Input name="..."/>` |
| `op` | 是 | 运算符，见下表 |
| `value` | op != `trigger` 时必填 | 比较值 |

**op 与 type 对应关系**：
| input type | 可用 op | value 格式 |
|---|---|---|
| `bool` | `equal` / `notEqual` | `true`/`false` |
| `number` | `equal`/`notEqual`/`lessThan`/`lessThanOrEqual`/`greaterThan`/`greaterThanOrEqual` | 数字 |
| `trigger` | `trigger` | 无需 value |

**trigger 的 per-region 单次消费**：trigger 类型的 input 是一次性事件，`op="trigger"` 检查"是否被 fire 且当前 region 还没消费过"。一个 trigger fire 后，每个 region 各判断一次自己的 transition，触发后标记该 region 已消费，帧末统一清空。这保证多 region 场景下，同一个 trigger fire 后每个 region 最多响应一次（借鉴 rive 的 per-layer 消费机制）。

#### 3.4.8 `<Timelines>` 里的引用（仅嵌套场景）

`<Timelines><StateMachine ref>` **只用于嵌套场景**：让子 composition 的某个 Layer 挂一个 SM，由 `scene->advanceAndApply` 递归时自动带跑（类比子 Layer 挂 `<Animation ref>`）。**顶层 SM 不用这个**——顶层 SM 直接定义在 `<Animations>` 里，业务通过 `getStateMachineTimeline(id)` 拿来自己驱动（见 8.7）。

```xml
<!-- 子 composition 内的 Layer，挂 SM 让 scene 自动驱动 -->
<Timelines>
  <StateMachine ref="@btnSM"/>
</Timelines>
```

| 属性 | 必填 | 说明 |
|---|---|---|
| `ref` | 是 | 引用 `<StateMachine id="btnSM"/>` |

**为什么引用处不需要 initialState**：每个 StateRegion 自带 `initialState`，初始状态是 region 的固有属性。引用处只需指定用哪个状态机即可。

### 3.5 StateMachine 与 ViewModel 的联动方式

**核心原则：input 是 SM 的唯一对外接口，VM 只是"喂值的来源"之一，行为完全等同宿主直接设置。**

SM 不感知 VM。SM 只暴露 input（bool/number/trigger），任何来源喂值都归一到同一个入口：
- **宿主代码**：`sm->setBool("isHover", true)` / `sm->fireTrigger("click")`
- **ViewModel**：VM 值变化时，直接把值设到对应 SM input——bool/number 同步当前值，trigger 触发时调 fire

**关键：VM 驱动 SM = VM 直接修改 SM input 的值**，最终行为与宿主直接 `setBool/setNumber/fireTrigger` **完全一致**。SM 内部只有一套 input 语义，不区分值从哪来。

**不采用 rive 的 DataBind+BindableProperty 弯路**。rive 把 VM→SM 塞进"通用值同步管道"（DataBind），为此在 SM 上加 BindableProperty 端口、用 uint32 计数器把 trigger 伪装成值、加特殊比较器——非常复杂。libpag 的直觉模型更简单：VM 值变化 → 直接设 SM input。SM 的 input 本身就是端口，不需要额外的 BindableProperty；trigger 直接调 fire，不需要计数器伪装。

**不用 ViewModel 时**（初版）：状态机独立工作，input 由宿主代码直接设值（顶层 SM，业务自驱动）。

```cpp
auto scene = PAGScene::Make(doc);
auto sm = scene->getStateMachineTimeline("btnSM");   // 顶层 SM
sm->setBool("isHover", true);
sm->fireTrigger("click");
while (running) {
  sm->advanceAndApply(deltaUs);   // 业务自己驱动顶层 SM
  scene->draw(surface);
}
```

**用 ViewModel 时**（后续扩展）：`<DataBind>` 的 target 指向 SM、`input` 指定要喂的 input 名，VM 值变化时直接设到该 input：

```xml
<DataBind source="$vm.hovering" target="@btnSM" input="isHover"/>
<DataBind source="$vm.clicked"  target="@btnSM" input="click"/>
```

运行时：VM value 变化 → 经现有 `PAGViewModelValue::addObserver` 回调 → 回调里 `sm->setBool("isHover", newValue)`（trigger 则 `sm->fireTrigger("click")`）。bool/number 喂当前值，trigger 喂 fire。多个 SM 绑同一 VM 值时各自注册 observer、各自被喂值，天然"每个 SM 独立响应"。

这条不在初版范围，但方向明确：**VM 直接喂 SM input，行为等同宿主直接设置**，不做 rive 的 BindableProperty 端口层。

### 3.6 待确认的 XML 设计点

1. **Transition 容器方案**：当前用扁平 `<Transitions>` 列表，而不是嵌套在 State 里。好处是 `from="any"` 统一表达 AnyState。已确认采用。

2. **EntryState 伪状态**：用每个 StateRegion 的 `initialState` 属性替代 EntryState。已确认不做 EntryState。

3. **ExitTime**：已确认做。

4. **空状态**：已确认支持 `<State name="idle"/>` 空状态。

5. **Condition OR 关系**：不支持显式 OR，用多个 transition 同 to 模拟。已确认。

---

## 四、设计决策点（已对齐）

### 决策点 1：数据 schema 的解耦方向 ✅

**采用方向 B**：state machine 作为独立 Node 类型，`StateMachineTimeline` 只引用其 id。与 `AnimationTimeline.animationId → Animation` 的解耦模式一致。状态机数据可被多个 timeline 引用、可独立编辑/序列化、复用 `PAGXDocument::findNode<StateMachine>(id)` 现有 id 索引。

### 决策点 2：与 ViewModel 的关系 ✅

**input 是 SM 的唯一对外接口，VM 只是喂值来源之一，行为等同宿主直接设置。** 状态机不感知 VM。VM 驱动 SM = VM 值变化时直接设 SM input（bool/number 同步值，trigger 调 fire），最终归一到宿主也在用的 `setBool/setNumber/fireTrigger` 同一入口。**不采用 rive 的 DataBind+BindableProperty 端口层弯路**（详见 3.5）。初版只做宿主直接设 input，VM→SM 作为后续扩展。

### 决策点 3：触发机制 ✅

- trigger input 必做，业务侧 `sm->getTrigger("click")->fire()` 触发
- AnyState 保留（`from="any"`），任意状态可触发的转移
- EntryState/ExitState 伪状态不做，用 `initialState` 属性替代 EntryState

**使用场景**：
- **trigger**：一次性事件驱动。如按钮点击 `click`、播放请求 `play`、重置 `reset`。区别于 bool——bool 表达"持续处于某状态"（如 `isHover`），trigger 表达"某事件此刻发生了"（如 `clicked`），响应后即失效不会重复触发。
- **AnyState (`from="any"`)**：无论当前在哪个状态都要响应的转移。如：任意状态下收到 `error` trigger 都跳到 error 状态；任意状态下 `reset` 都回到 idle。省去为每个状态各写一条相同转移。
- **不做 EntryState**：EntryState 是 rive 用来"进入状态机时先过一个空节点再自动流转到首个真实状态"的机制。libpag 用 `initialState` 直接指定首状态更直白，不需要这个中间跳板。

### 决策点 4：状态间过渡方式 ✅

- **mix 权重叠加**（rive 模式）：outgoing/incoming 按各自权重 `apply(mix)` 叠加，复用现有 `RuntimeBinding::apply` 的 mix 机制，采用 rive 的独立 `mixFrom` 权重模型（非 `1-mix`）
- **Keyframe 的 `interpolation`/`bezier-out`/`bezier-in`** 用在 mix 权重本身的曲线上（不是 keyframe 序列）
- **ExitTime 做**：transition 的 allowed 检查里加 Animation 时间 >= exitTime 判断
- **earlyExit 做**：per-transition flag（默认 false），控制过渡进行中能否被本转移打断
- **pauseOnExit 做**：per-transition flag（默认 false），配合 exitTime——过渡期间把来源状态冻结在 exitTime 帧做混合，避免来源动画继续播产生抖动
- **waitingForExit 三态不做**：libpag 宿主时钟持续驱动，exitTime 靠外部时钟自然到达，`allowed()` 返回 bool 即可（详见 8.6）

**使用场景**：
- **mix 过渡 (`duration`)**：状态切换时的视觉平滑。如 normal→hover 用 `duration=10` 做 10 帧淡入淡出，避免瞬间跳变。`duration=0` 则硬切。
- **interpolation 曲线**：过渡的缓动手感。`bezier` 做 ease-in-out 让过渡更自然；`hold` 做"保持旧值直到过渡末尾突然切新值"（适合离散内容如换图不希望半透明叠影）。
- **exitTime**：等当前动画播到某帧才允许转移。如：loading 动画 60 帧，`exitTime=60` 表示 loading 必须播完整才能转 success，即使 success 条件提前满足；又如攻击动作必须打完才能转 idle。
- **earlyExit**：过渡进行中能否被打断。如：`idle→walk` 过渡到一半用户松开方向键，若 `walk→idle` 设 `earlyExit=true` 就能立刻反向过渡（响应灵敏）；反之攻击动作 `combo1→combo2` 不设 earlyExit（默认 false），保证连招过渡不被中途打断。
- **pauseOnExit**：过渡期间来源是否冻结。如：走路→跑步，`pauseOnExit=true` + `exitTime` 让走路停在某个稳定姿势（如双脚着地帧）再与跑步混合，避免混合期间走路动画继续摆动造成的腿部抖动。UI 场景通常不需要（默认 false）。

### 决策点 5：多 timeline 共存的 apply 顺序 ✅

**业务侧决定**，不强制。文档给推荐顺序（如"先 advance 所有 timeline，再 flushDataBinds，最后 draw"），但实际顺序由调用方控制。

**使用场景**：一个 Layer 同时挂 AnimationTimeline（如持续的 idle 呼吸动画）+ StateMachineTimeline（如交互态切换），两者都写同一批 channel。谁后 apply 谁的权重叠加在上层。由业务按需求决定顺序——比如希望交互态覆盖 idle，就让 StateMachine 后 apply。

### 决策点 6：状态类型范围 ✅

- 初期只做 `AnimationState`（每个状态绑定一个 PAGX Animation）+ AnyState 伪状态
- **预留升级扣子**：`State` 基类 + `StateType` 枚举（类似 `TimelineType` 设计），`AnimationState : public State`。后续加 `BlendState : public State` 时，主循环的 `switch (state->stateType())` 加 case 即可，对现有代码零侵入

**使用场景**：
- **AnimationState**：绝大多数状态——每个状态对应一段动画表现（normal 态的呼吸、hover 态的放大）。
- **空状态**（AnimationState 但 animationId 为空）：纯逻辑跳板，不输出画面。如 loading 逻辑态：进入后不显示任何东西，等数据就绪（condition 满足）自动转到真正的内容态。
- **BlendState（未来）**：按 number input 混合多段动画。如角色移动速度 0→1 在 idle/walk/run 之间连续混合。初版不做，但基类扣子留好。

### 决策点 7：嵌套 ✅

**不做声明式联动**。父级代码拿到子 SM 手动设 input。现有 PAGComposition 嵌套机制已提供"子 composition 独立 SM 实例"的能力，父级通过 `PAGScene` 访问子 SM 即可。

rive 的 NestedStateMachine 是为跨文件 NestedArtboard 引用设计，libpag 的 Composition 引用已覆盖此场景，不需要专门的 NestedStateMachine 类。

**使用场景**：一个卡片组件（Composition）内部有自己的状态机（如展开/收起）。列表里放多个卡片实例，每个实例的状态机独立运行互不干扰（per-composition 实例天然保证）。父级若要控制某张卡片，用 `scene->getStateMachineTimeline(...)` 拿到那个实例的 SM 直接设 input，不需要在 XML 里声明父子状态机的绑定关系。

### 决策点 8：Input 系统与多 StateRegion ✅

**支持多 StateRegion 并行**（借鉴 UML State Machine 的 orthogonal region）。

- 一个 StateMachine 由一个或多个 `<StateRegion>` 组成，每个 region 有独立当前状态和转移逻辑，并行运行
- **命名**：用 `StateRegion` 而非 rive 的 `Layer`（避免与 PAGX 渲染 Layer 冲突）或 Track（避免与 TrackMatte/AudioTrack 冲突）
- 多 region 解决"维度正交避免组合爆炸"问题（如按钮的视觉维度 × 启用维度）
- `<Inputs>` 声明在 StateMachine 层级，被所有 region 共享
- 只支持 bool/number/trigger 三种 input
- **trigger 的 per-region 单次消费**：一个 trigger fire 后，每个 region 各判断一次自己的 transition，触发后标记该 region 已消费，帧末统一清空。保证多 region 场景下每个 region 最多响应一次

**使用场景**：
- **多 region**：一个对象有多个正交维度各自独立变化。如角色：移动层（idle/walk/run）× 情绪层（happy/angry）× 装备层（sword/bow）三个 region 并行，互不阻塞——走路时可以切换情绪，切武器不打断走路。若用单一状态会爆炸成 3×2×2=12 个组合状态。
- **input 全 region 共享**：一个 `speed` number 可同时被移动层（切 walk/run）和情绪层（速度快时切紧张表情）读取。
- **trigger per-region 消费**：一次 `hit` trigger，移动层切到受击、情绪层切到痛苦、各消费一次，不会因为移动层先消费导致情绪层收不到。

**Why**：多 region 是这次引入状态机的核心价值。相比"多个独立 SM 实例 + VM 协调"，把并行维度收进一个 StateMachine 内部，schema 更内聚、input 共享更自然，且符合 UML 标准模型。

---

## 五、改造影响面预估

### 5.1 必须新增的文件

| 文件 | 内容 |
|---|---|
| `include/pagx/nodes/StateMachineTimeline.h` | Timeline 子类，只持有 `stateMachineId`（initialState 在 StateRegion 里，不在引用处） |
| `include/pagx/nodes/StateMachine.h` | 状态机数据节点，持有 `inputs` + `regions` |
| `include/pagx/nodes/StateRegion.h` | 并行状态区域，持有 `initialState` + `states` + `transitions` |
| `include/pagx/nodes/State.h` | 状态节点（含 `animationId` 引用），带 StateType 枚举扣子 |
| `include/pagx/nodes/StateTransition.h` | 转移定义 |
| `include/pagx/nodes/TransitionCondition.h` | 条件基类 + 子类 |
| `include/pagx/nodes/StateMachineInput.h` | input 定义（bool/number/trigger） |
| `include/pagx/PAGStateMachine.h` | runtime 控制器（持有多 region 实例） |

### 5.2 必须修改的文件

| 文件 | 改动 |
|---|---|
| `include/pagx/nodes/Timeline.h:30` | `TimelineType` 加 `StateMachine` 枚举值 |
| `include/pagx/nodes/Node.h:30` | `NodeType` 加 StateMachine/StateRegion/State/Transition/Input 类型 |
| `include/pagx/PAGScene.h` | 加 `getStateMachineTimeline(id)` 访问器 |
| `src/pagx/runtime/PAGComposition.cpp:112-134` | `spawnTimelines` 加 `case TimelineType::StateMachine` 分支 |
| `src/pagx/PAGXImporter.cpp:673-699` | `ParseLayerTimelines` 加 `<StateMachine ref="@id">` 分支 |
| `src/pagx/PAGXImporter.cpp:1855` | `ParseAnimations` 加 `<StateMachine>` 子标签分支（含 Inputs/StateRegion 解析） |
| `src/pagx/PAGXExporter.cpp:302` | `WriteAnimations` 加 `case` 写出 `<StateMachine>`（含 Inputs/StateRegion） |
| `src/pagx/PAGXExporter.cpp:1494-1510` | Timelines switch 加 `case TimelineType::StateMachine` |

**无需改动**：
- `PreRegisterResource` / `IsExportableResource`（StateMachine 不在 `<Resources>` 里，不需要预注册）
- StateMachine 节点通过 `makeNodeFromXML` 进 `doc->nodeMap`，`<StateMachine ref="@btnSM"/>` 的 id 解析走 `doc->findNode<StateMachine>("btnSM")` 现有机制

### 5.3 runtime 控制器 API 初步草图（仅供讨论，未编码）

参照 `PAGTimeline` 的设计，`PAGStateMachine` 持有多个 region 实例，每帧并行 advance/apply：

```cpp
class PAGStateMachine {
 public:
  ~PAGStateMachine() = default;

  // identity
  const std::string& getId() const;
  std::vector<std::string> getRegionIds() const;

  // per-region current state query
  std::string getCurrentState(const std::string& regionId) const;

  // inputs (business-driven, shared across regions)
  //   返回 typed value handle，对标 PAGViewModel 的 typed accessor
  bool setBool(const std::string& name, bool value);
  bool setNumber(const std::string& name, float value);
  bool fireTrigger(const std::string& name);

  // time-driven advance + apply, paralleling PAGTimeline
  //   内部对所有 region 并行 advance/apply，trigger 帧末统一清空
  void advance(int64_t deltaMicroseconds);
  void apply(float mix = 1.0f);
  bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f);

  // state change observation (regionId + new state)
  ObserverHandle addStateChangeListener(
      std::function<void(const std::string& regionId, const std::string& newState)> callback);

 private:
  std::weak_ptr<PAGScene> owner;
  StateMachine* stateMachine = nullptr;       // data node
  RuntimeBinding* binding = nullptr;          // null for top-level
  PAGXDocument* contextDoc = nullptr;
  // 每个 region 一个运行时实例（内部类型，持有 currentState / stateFrom / mix 进度）
  std::vector<RegionInstance> regions;
  // input 运行时值（bool/number/trigger），全 region 共享
  InputValues inputs;
};
```

其中 `RegionInstance` 是内部实现细节（类比 rive 把 `StateMachineLayerInstance` 藏在 cpp 匿名命名空间），持有该 region 的 `currentState`、`stateFrom`、`transitionProgress`、以及本帧已消费的 trigger 集合。对外只暴露 `PAGStateMachine` 一个入口类。

`PAGScene` 上的访问器（与 `getTimeline` / `getTimelineIds` 对偶，顶层 only，同 id 返回同一缓存实例；详见 8.7 两条驱动路径）：

```cpp
std::vector<std::string> getStateMachineIds() const;
std::shared_ptr<PAGStateMachine> getStateMachineTimeline(const std::string& id);
```

---

## 六、建议下一步

所有设计决策点已对齐（决策点 1-8）。按 `.codebuddy/rules/Code.md` 第 10 条，改动范围较大，下一步输出关键类和接口的详细定义供第二轮讨论，最后编码。

**初版实现范围总结**：
- 数据层：StateMachine（inputs + regions）/ StateRegion（initialState + states + transitions）/ State (AnimationState) / StateTransition / TransitionCondition / StateMachineInput (bool/number/trigger) / StateMachineTimeline
- 运行时层：PAGStateMachine（多 StateRegion 并行、mix 过渡、ExitTime、trigger per-region 消费帧末清空）
- XML 导入导出：`<StateMachine>` 放 `<Animations>` 容器（含 `<Inputs>` + 多个 `<StateRegion>`），`<StateMachine ref="@id"/>` 挂 Timelines
- 暂不实现：BlendState、声明式 VM 联动、NestedStateMachine、waitingForExit 三态（libpag 驱动模型不需要）

---

## 七、数据层 Node 类详细定义（待审阅）

以下为待编码的数据层类定义草稿，遵循现有 PAGX 约定（重点参照 ViewModel / ViewModelProperty 先例）：Node 子类构造函数 private + `friend class PAGXDocument`（由 document 统一 new/持有），子节点用裸指针存储（ownership 在 document），进 document.nodes；被 `@ref` 引用的节点用 `id`（进 nodeMap），机内/区内标识用 `name`（不进 nodeMap，与 ViewModelProperty.name 一致）；默认值拆成独立字段（与 ViewModelProperty 的 defaultNumber/defaultBoolean 一致，不用 variant）；可选值用 `std::optional`（与 ViewModelProperty 的 minValue/maxValue 一致）；枚举底层类型 `uint8_t`、驼峰值名无 `k` 前缀。为聚焦结构，以下省略版权头/`#pragma once`/`namespace pagx`。

**标识约定**：
- `StateMachine` 用 `id`（被 `<StateMachine ref="@id">` 引用，进 nodeMap，像 Animation/ViewModel）
- `StateRegion` / `State` / `StateMachineInput` 用 `name`（机内标识，不进 nodeMap，像 ViewModelProperty）
- `StateTransition` / `TransitionCondition` 无标识字段（不被引用）

### 7.1 StateMachineTimeline（Timeline 子类）

挂在 Layer 的 `<Timelines>` 里，只引用状态机 id。与 `AnimationTimeline` 对称，但不含 initialState（初始状态在各 StateRegion 上）。

```cpp
// include/pagx/nodes/StateMachineTimeline.h
#include "pagx/nodes/Timeline.h"

/**
 * StateMachineTimeline attaches a referenced StateMachine to the owning Layer's runtime
 * composition. The referenced StateMachine is looked up by stateMachineId against the owning
 * document's id table. Unlike AnimationTimeline, the initial state is not specified here; each
 * StateRegion carries its own initialState.
 */
class StateMachineTimeline : public Timeline {
 public:
  StateMachineTimeline() = default;

  /**
   * The id of the referenced StateMachine. Resolved against the owning document's id table.
   */
  std::string stateMachineId = {};

  TimelineType timelineType() const override {
    return TimelineType::StateMachine;
  }
};
```

对 `Timeline.h` 的改动：`TimelineType` 加 `StateMachine = 1`。

### 7.2 StateMachine（顶层数据节点）

放在 `<Animations>` 容器里，与 `Animation` 并列。持有共享的 inputs 和一个或多个并行 region。

```cpp
// include/pagx/nodes/StateMachine.h
#include <vector>
#include "pagx/nodes/Node.h"

class StateMachineInput;
class StateRegion;

/**
 * StateMachine defines a named state graph composed of one or more parallel StateRegions plus a
 * set of shared inputs. It is identified by Node::id (unique within the owning PAGXDocument) and
 * referenced from Layer.timelines via StateMachineTimeline.stateMachineId.
 *
 * Each StateRegion runs independently and in parallel (borrowing the orthogonal-region concept
 * from UML state machines), letting orthogonal dimensions (e.g. a button's visual state and its
 * enabled state) be modeled without a combinatorial state explosion. All regions share the same
 * inputs declared here.
 */
class StateMachine : public Node {
 public:
  /**
   * The input ports shared by all regions. Set by host code or synced from a ViewModel via
   * DataBind; conditions in any region reference these inputs by name.
   */
  std::vector<StateMachineInput*> inputs = {};

  /**
   * The parallel state regions. At least one is required. Each region owns its states,
   * transitions and initial state, and advances independently.
   */
  std::vector<StateRegion*> regions = {};

  NodeType nodeType() const override {
    return NodeType::StateMachine;
  }

 private:
  StateMachine() = default;

  friend class PAGXDocument;
};
```

### 7.3 StateMachineInput（输入端口）

三种类型合成一个类，用枚举区分，默认值用 union-like 的独立字段（避免 variant 的复杂度，与 ViewModelProperty 的 defaultXxx 风格一致）。

```cpp
// include/pagx/nodes/StateMachineInput.h
#include <string>
#include "pagx/nodes/Node.h"

/**
 * StateMachineInputType selects the value kind carried by a StateMachineInput.
 */
enum class StateMachineInputType : uint8_t {
  /**
   * A persistent boolean value. Used with equal / notEqual conditions.
   */
  Bool = 0,
  /**
   * A persistent numeric value. Used with equal / notEqual / lessThan / lessThanOrEqual /
   * greaterThan / greaterThanOrEqual conditions.
   */
  Number = 1,
  /**
   * A one-shot event. Fired by host code, consumed at most once per region, then cleared at the
   * end of the frame. Used with the trigger condition.
   */
  Trigger = 2,
};

/**
 * StateMachineInput declares a named input port on a StateMachine. Inputs are shared by all
 * StateRegions and referenced by TransitionCondition.inputName.
 */
class StateMachineInput : public Node {
 public:
  /**
   * The input name, referenced by conditions. Unique within the owning StateMachine.
   */
  std::string name = {};

  /**
   * The value kind of this input.
   */
  StateMachineInputType type = StateMachineInputType::Bool;

  /**
   * The initial boolean value when type is Bool. Ignored for other types.
   */
  bool defaultBool = false;

  /**
   * The initial numeric value when type is Number. Ignored for other types.
   */
  float defaultNumber = 0.0f;

  NodeType nodeType() const override {
    return NodeType::StateMachineInput;
  }

 private:
  StateMachineInput() = default;

  friend class PAGXDocument;
};
```

### 7.4 StateRegion（并行状态区域）

```cpp
// include/pagx/nodes/StateRegion.h
#include <string>
#include <vector>
#include "pagx/nodes/Node.h"

class State;
class StateTransition;

/**
 * StateRegion is one orthogonal region of a StateMachine. It owns an independent set of States
 * and StateTransitions and its own initial state, and advances in parallel with sibling regions.
 * Multiple regions let orthogonal dimensions coexist without a combinatorial state explosion.
 */
class StateRegion : public Node {
 public:
  /**
   * The region name, unique within the owning StateMachine. Used by runtime to query this
   * region's current state (e.g. getCurrentState(regionName)).
   */
  std::string name = {};

  /**
   * The name of the state this region enters when the state machine starts. Must match one of the
   * State names in this region.
   */
  std::string initialState = {};

  /**
   * The states belonging to this region. State names are unique within the region (different
   * regions may reuse the same state name).
   */
  std::vector<State*> states = {};

  /**
   * The transitions belonging to this region. Each transition's from/to reference state names
   * within this region; the special from value "any" matches any state in this region.
   */
  std::vector<StateTransition*> transitions = {};

  NodeType nodeType() const override {
    return NodeType::StateRegion;
  }

 private:
  StateRegion() = default;

  friend class PAGXDocument;
};
```

### 7.5 State（状态节点，带 StateType 升级扣子）

```cpp
// include/pagx/nodes/State.h
#include <string>
#include "pagx/nodes/Node.h"

/**
 * StateType discriminates State subclasses for runtime dispatch without dynamic_cast. v1 only
 * ships AnimationState; future kinds (e.g. BlendState) extend this enum.
 */
enum class StateType : uint8_t {
  /**
   * A state bound to a single Animation (or to nothing, an empty control-flow state).
   */
  Animation = 0,
};

/**
 * State is the base for a single state within a StateRegion. Concrete kinds are distinguished by
 * stateType(). A State with no bound animation acts as an empty control-flow state (its advance /
 * apply are no-ops), useful as a logic-only stepping stone.
 */
class State : public Node {
 public:
  /**
   * The state id, unique within the owning StateRegion. Referenced by StateTransition.from/to.
   */
  std::string name = {};

  /**
   * Returns the concrete state kind, used by runtime dispatch.
   */
  virtual StateType stateType() const = 0;

 protected:
  State() = default;
};

/**
 * AnimationState plays a referenced Animation while it is the current state of its region. The
 * animation is optional: when animationId is empty the state produces no output (empty state).
 */
class AnimationState : public State {
 public:
  /**
   * The id of the Animation this state plays, resolved against the owning document's id table.
   * Empty means an empty state that drives no channels.
   */
  std::string animationId = {};

  StateType stateType() const override {
    return StateType::Animation;
  }

  NodeType nodeType() const override {
    return NodeType::State;
  }

 private:
  AnimationState() = default;

  friend class PAGXDocument;
};
```

**说明**：`State` 是抽象基类（`protected` 构造），`AnimationState` 是初版唯一具体子类。所有 State 子类共用 `NodeType::State`（对外只暴露一种 node type），内部靠 `stateType()` 细分——这样后续加 `BlendState` 时不需要动 `NodeType` 枚举，只加 `StateType` 值。是否接受"共用一个 NodeType、用 StateType 细分"？（另一种做法是每个 state 子类一个 NodeType，但会让 NodeType 膨胀。）

### 7.6 StateTransition（转移）

```cpp
// include/pagx/nodes/StateTransition.h
#include <optional>
#include <string>
#include <vector>
#include "pagx/nodes/Keyframe.h"   // KeyframeInterpolationType, Frame
#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

class TransitionCondition;

/**
 * The special from-state name that matches any state within the region (UML AnyState). A
 * transition with from == AnyStateName is evaluated regardless of the region's current state.
 */
constexpr const char* AnyStateName = "any";

/**
 * StateTransition describes a directed edge between two states within a StateRegion, together
 * with the conditions that gate it and the crossfade parameters used while switching. All
 * conditions must hold (AND) for the transition to be allowed.
 */
class StateTransition : public Node {
 public:
  /**
   * The source state name, or "any" to match any state in the region.
   */
  std::string from = {};

  /**
   * The destination state name within the region.
   */
  std::string to = {};

  /**
   * The crossfade duration, in frames, over which the outgoing state's weight fades to 0 and the
   * incoming state's weight rises to 1.
   */
  Frame duration = 0;

  /**
   * The interpolation curve applied to the crossfade weight over duration. Reuses the keyframe
   * interpolation model (None / Linear / Bezier / Hold).
   */
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;

  /**
   * The outgoing bezier handle for the crossfade weight curve, used when interpolation is Bezier.
   */
  Point bezierOut = {};

  /**
   * The incoming bezier handle for the crossfade weight curve, used when interpolation is Bezier.
   */
  Point bezierIn = {};

  /**
   * Optional exit-time gate, in frames. When set, the transition is only allowed after the source
   * state's animation has advanced past this frame, even if all conditions already hold. nullopt
   * means no exit-time gate.
   */
  std::optional<Frame> exitTime = std::nullopt;

  /**
   * Whether this transition may interrupt an in-progress crossfade. When false (default), the
   * transition is only evaluated once the region has no active crossfade; when true, it can be
   * taken mid-crossfade, and the current outgoing state keeps fading out while the new target
   * fades in (multiple outgoing states may overlap).
   */
  bool enableEarlyExit = false;

  /**
   * Whether the source state's animation is held (frozen) at its exitTime frame during the
   * crossfade, instead of continuing to play. Only takes effect together with exitTime (a frozen
   * frame is only meaningful when there is an exit frame to hold). Default false: the source keeps
   * advancing while fading out. Useful to keep the outgoing pose stable during blending.
   */
  bool pauseOnExit = false;

  /**
   * The conditions gating this transition. All must evaluate true (AND). An empty list means the
   * transition is always allowed (subject to exitTime).
   */
  std::vector<TransitionCondition*> conditions = {};

  NodeType nodeType() const override {
    return NodeType::StateTransition;
  }

 private:
  StateTransition() = default;

  friend class PAGXDocument;
};
```

### 7.7 TransitionCondition（条件）

单类 + 枚举 op 表达三种 input 的所有比较。value 拆成 `valueBool` + `valueNumber` 独立字段（与 ViewModelProperty 的 defaultBoolean/defaultNumber 一致，不用 variant）。trigger 类型的 condition 用 `op == Trigger`，忽略 value。

```cpp
// include/pagx/nodes/TransitionCondition.h
#include <string>
#include "pagx/nodes/Node.h"

/**
 * TransitionConditionOp is the comparison operator applied between an input's current value and
 * the condition's value. The set of valid operators depends on the referenced input's type:
 *   Bool   -> Equal / NotEqual                       (compares against valueBool)
 *   Number -> Equal / NotEqual / LessThan / LessThanOrEqual / GreaterThan / GreaterThanOrEqual
 *                                                    (compares against valueNumber)
 *   Trigger-> Trigger (values ignored; checks fired-and-not-yet-consumed-in-this-region)
 */
enum class TransitionConditionOp : uint8_t {
  Equal = 0,
  NotEqual = 1,
  LessThan = 2,
  LessThanOrEqual = 3,
  GreaterThan = 4,
  GreaterThanOrEqual = 5,
  Trigger = 6,
};

/**
 * TransitionCondition tests a single StateMachine input against a value using an operator.
 * Multiple conditions on a transition are combined with AND.
 */
class TransitionCondition : public Node {
 public:
  /**
   * The name of the StateMachine input this condition tests.
   */
  std::string inputName = {};

  /**
   * The comparison operator. Must be compatible with the referenced input's type.
   */
  TransitionConditionOp op = TransitionConditionOp::Equal;

  /**
   * The boolean value compared against a Bool input's current value. Used when the referenced
   * input is Bool. Ignored for Number/Trigger inputs.
   */
  bool valueBool = false;

  /**
   * The numeric value compared against a Number input's current value. Used when the referenced
   * input is Number. Ignored for Bool/Trigger inputs.
   */
  float valueNumber = 0.0f;

  NodeType nodeType() const override {
    return NodeType::TransitionCondition;
  }

 private:
  TransitionCondition() = default;

  friend class PAGXDocument;
};
```

### 7.8 NodeType 枚举新增

在 `Node.h` 的 `NodeType` 里，`Animation` 段落之后新增 state machine 相关类型：

```cpp
  // State Machine
  StateMachine,
  StateRegion,
  State,               // 所有 State 子类共用（AnimationState 等），靠 State::stateType() 细分
  StateTransition,
  TransitionCondition,
  StateMachineInput,
```

### 7.9 ownership 与引用关系图

```
PAGXDocument (拥有所有 Node，unique_ptr in nodes vector)
  ├─ StateMachine
  │    ├─ inputs:  StateMachineInput*  (裸指针，指向 document 拥有的节点)
  │    └─ regions: StateRegion*
  │         ├─ states:      State* (AnimationState*)  ──animationId──> Animation
  │         └─ transitions: StateTransition*
  │              └─ conditions: TransitionCondition*  ──inputName──> StateMachineInput
  └─ Layer.timelines: StateMachineTimeline  ──stateMachineId──> StateMachine
```

- 实线 owning：document 用 unique_ptr 拥有所有 Node，各 Node 用裸指针互相引用
- `animationId` / `stateMachineId` / `inputName` 是字符串 id 引用，运行时 resolve
- `StateMachineTimeline` 是 Layer 直接持有（unique_ptr，与 AnimationTimeline 一致），不进 document 的 nodeMap

### 7.10 已确认的设计细节（参照 ViewModel 先例）

1. **State 的 NodeType 归并** ✅：所有 State 子类共用 `NodeType::State`，靠 `StateType` 细分。避免 NodeType 膨胀，后续加 BlendState 不动 NodeType。
2. **condition value 拆字段** ✅：用 `valueBool` + `valueNumber` 独立字段（对齐 ViewModelProperty.defaultBoolean/defaultNumber），不用 KeyValue variant。
3. **exitTime 用 optional** ✅：`std::optional<Frame>`（对齐 ViewModelProperty.minValue/maxValue 的 optional 用法），比 -1 magic number 清晰。
4. **AnyState 用 "any" 常量** ✅：`constexpr AnyStateName = "any"`，显式，避免空字符串与"未填写"混淆。
5. **Input/Region/State 继承 Node，标识用 name** ✅：参照 ViewModelProperty——继承 Node、进 document.nodes、用 `name` 而非 `id`（不进 nodeMap）。只有 StateMachine 用 `id`（被 `@ref` 引用）。

---

## 八、运行时 advance / apply 核心逻辑（待审阅）

`PAGStateMachine` 每帧驱动。内部为每个 StateRegion 持有一个 `RegionInstance`（藏在 cpp，不对外）。以下伪代码描述核心时序，供审阅逻辑正确性。

### 8.0 驱动模型契约（与 rive 的唯一架构差异）

**libpag 与 rive 的驱动模型不同，但不影响状态逻辑结果，只影响"谁决定要不要 tick"。**

- **rive：按需唤醒**。`advance` 返回 bool 上抛给宿主，宿主 `while(needsAdvance()) advanceAndApply()` 自己决定是否继续；状态机稳定后返回 false，宿主停止 tick（省电睡眠）；改 input 经 `markNeedsAdvance` 唤醒。为此 rive 需要 needsAdvance / markNeedsAdvance / waitingForExit 一整套让外部知情的机制。
- **libpag：时钟持续驱动**。宿主每帧无条件调 `scene->advanceAndApply(delta)`，"要不要干活、干多少"全在 timeline 内部消化——`PAGComposition::advance` 直接调 `timeline->advance(delta)` 并忽略返回值（`runtime/PAGComposition.cpp:55`），与现有 PAGTimeline 一致。

**契约**：`PAGStateMachine` 假设宿主每帧持续驱动，**不做按需唤醒**。因此：
- 不需要 `needsAdvance`（外部不问）
- 不需要 `markNeedsAdvance`（外部本就每帧来，改 input 下一帧自然被评估）
- 不需要 `waitingForExit` 三态（不睡眠，exitTime 靠外部时钟自然累加到达）

**结果一致性**：只要宿主遵守"每帧调 advanceAndApply"契约，所有状态逻辑（转移时机、mix、exitTime 自动转移、trigger 响应）与 rive 结果一致。差异仅在：rive 静止时睡眠省电，libpag 静止时空转——这对 PAG"合成动画持续播放"的既有假设无影响。若将来需要省电模式，可另行补 needsAdvance 机制，不影响状态机语义。

**trigger 延迟消费——两种策略的差异**：trigger 的"fire 只记录、advance 才评估消费"是固有语义（两边都有）。但"延迟多久消费"两种策略不同：

- **rive（睡眠）**：SM 停在静止状态（oneShot 播完 / 空状态，`keepGoing==false`）时会睡眠。fire 靠 `markNeedsAdvance` 唤醒，宿主发现 `needsAdvance` 后才 advance 消费——延迟 0~多帧不定，且依赖唤醒机制不失效（否则 trigger 可能丢）。
- **libpag（每帧 advance）**：永不睡眠，fire 的 trigger **必然在下一次 advance 被评估消费**，延迟恒定 ≈1 帧，可预测、无丢失风险，也**不需要 `markNeedsAdvance` 唤醒**。

**结论**：libpag 每帧持续驱动的策略，恰好让 trigger 行为更简单可预测。要守住的唯一契约是 **`fireTrigger` 只置标志、绝不在 fire 里做转移评估**——评估消费只发生在 `advance` 内（8.3）。这样 fire 与 advance 解耦：fire 时机随意（advance 前多次 fire 去重成一次，对齐 rive `if(m_fired) return`），消费统一在 advance。rive 的 `markNeedsAdvance` / 睡眠唤醒是为省电付出的复杂度，libpag 不追求省电（合成动画本就持续播放），故不引入。

### 8.1 RegionInstance 内部状态

采用 rive 的独立淡出权重模型：每个正在淡出的来源状态持有自己的权重起点 `mixFrom`，不用 `1-mix` 互补式 crossfade。这样支持 early-exit 时多个未完成过渡叠加淡出。

```
RegionInstance {
  StateRegion* region;          // 数据定义
  State* currentState;          // 当前（淡入中或已稳定）状态
  int64_t currentElapsedUs;     // currentState 绑定动画已播放的微秒数
  float mix;                    // currentState 的淡入进度 0..1（1 表示过渡完成）

  // 当前过渡（无过渡时 transition==null，mix==1）
  StateTransition* transition;

  // 正在淡出的来源状态列表（early-exit 时可 >1 个；无过渡时为空）
  //   每项独立记录自己的动画时间与淡出权重起点，互不干扰
  list<FadingState> fadingOut;

  set<string> consumedTriggers; // 本帧已消费的 trigger 名（帧末清空）
}

FadingState {
  State* state;
  int64_t elapsedUs;   // 该来源状态的动画时间
  float mixFrom;       // 淡出权重起点：进入淡出那一刻 currentState 的 mix 值（rive 的 m_mixFrom）
  bool frozen;         // pauseOnExit：true 则 elapsedUs 冻结不再推进（固定在 exitTime 帧）
}
```

### 8.2 每帧总流程

```
PAGStateMachine::advance(deltaUs):
    for region in regions:
        region.advance(deltaUs)         // 各 region 独立推进

PAGStateMachine::apply(smMix):
    for region in regions:
        region.apply(smMix)             // 各 region 依次把输出叠加到 channel
    // 注：多 region apply 顺序 = regions 声明顺序，后写覆盖/lerp 前写（复用 RuntimeBinding mix 规则）

PAGStateMachine::advanceAndApply(deltaUs, smMix):
    advance(deltaUs)
    apply(smMix)
    // 本次 advance 末尾清空 trigger（评估已在上面完成；fire 只置标志、advance 才评估消费）
    for input in triggerInputs: input.fired = false
    for region in regions: region.consumedTriggers.clear()
```

### 8.3 单个 region 的 advance

```
RegionInstance::advance(deltaUs):
    // 1. 推进所有状态的动画时间
    currentElapsedUs += deltaUs
    for f in fadingOut:
        if not f.frozen:                // pauseOnExit 冻结的来源保持在 exitTime 帧不动
            f.elapsedUs += deltaUs

    // 2. 推进当前过渡的淡入进度
    if transition != null:
        mixDurationUs = FramesToMicros(transition.duration, frameRate)
        mix = (mixDurationUs <= 0) ? 1 : clamp(mix + deltaUs / mixDurationUs, 0, 1)
        if mix >= 1:                    // 过渡完成：丢弃所有淡出来源
            fadingOut.clear()
            transition = null

    // 3. 尝试触发新的状态切换（单帧内可链式切换，上限 100 与 rive 一致）
    for i in 0 .. MaxTransitionsPerFrame:   // MaxTransitionsPerFrame = 100
        if not tryChangeState(): break

RegionInstance::tryChangeState():
    inTransition = (transition != null and mix < 1)
    // 先查 from="any"（region 内任意状态可触发），再查当前状态的转移
    t = findAllowedTransition(AnyStateName, inTransition)
    if t == null:
        t = findAllowedTransition(currentState.name, inTransition)
    if t == null:
        return false
    changeState(t)
    return true

RegionInstance::findAllowedTransition(fromName, inTransition):
    for t in region.transitions where t.from == fromName:
        // early-exit 门槛：过渡进行中，只有 enableEarlyExit 的转移可打断
        if inTransition and not t.enableEarlyExit: continue
        if allowed(t): return t
    return null

RegionInstance::allowed(t):
    // exitTime 门槛：当前状态动画的累计播放时间未越过 exitTime 则不允许（即使条件满足）
    if t.exitTime.hasValue():
        anim = document.findAnimation(currentState.animationId)
        if anim != null:
            exitUs = FramesToMicros(t.exitTime, frameRate)
            durationUs = FramesToMicros(anim.duration, frameRate)
            // 对齐 rive：exitTime 若落在单个循环周期内且动画是循环模式，
            // 把 exitTime 抬到当前所在的循环周期，实现"每遍循环相同位置都可退出"
            if exitUs <= durationUs and anim.loop != Once and durationUs > 0:
                exitUs += floor(currentElapsedUs / durationUs) * durationUs
            if currentElapsedUs < exitUs: return false
    // 所有 condition AND
    for c in t.conditions:
        if not evaluateCondition(c): return false
    return true

RegionInstance::evaluateCondition(c):
    input = stateMachine.findInput(c.inputName)
    switch input.type:
        Bool:
            v = inputValues.getBool(input)
            return c.op == Equal ? (v == c.valueBool) : (v != c.valueBool)
        Number:
            v = inputValues.getNumber(input)
            switch c.op: Equal/NotEqual/LessThan/.../GreaterThanOrEqual → 比较 v 与 c.valueNumber
        Trigger:
            // per-region 单次消费：fired 且本 region 未消费过
            return input.fired and c.inputName not in consumedTriggers

RegionInstance::changeState(t):
    // 把当前状态推入淡出列表，记录它此刻的淡入权重作为淡出起点（rive 的 m_mixFrom）
    fadingState = FadingState{
        state:    currentState,
        elapsedUs: currentElapsedUs,
        mixFrom:  mix,                  // 关键：起点是当前 mix，不是 1（支持连续/early-exit 叠加）
        frozen:   false,
    }
    // pauseOnExit：冻结来源在 exitTime 帧（需配合 exitTime；对齐 rive applyExitCondition）
    if t.pauseOnExit and t.exitTime.hasValue():
        fadingState.elapsedUs = FramesToMicros(t.exitTime, frameRate)
        fadingState.frozen = true
    fadingOut.push_back(fadingState)
    currentState = region.findState(t.to)
    currentElapsedUs = 0
    transition = t
    mix = (t.duration <= 0) ? 1 : 0
    if mix >= 1:                        // duration=0：立即完成，无需保留淡出
        fadingOut.clear()
        transition = null
    // 标记本次转移消费的 trigger（per-region）
    for c in t.conditions where c.op == Trigger:
        consumedTriggers.insert(c.inputName)
    notifyStateChange(region.name, currentState.name)   // observer 回调
```

### 8.4 单个 region 的 apply（独立权重叠加）

```
RegionInstance::apply(smMix):     // smMix = 外部传入的整体权重（默认 1）
    // 先 apply 所有淡出来源（各自独立权重），再 apply 当前状态（淡入权重）
    // 顺序：越早进入淡出的越先 apply，currentState 最后 apply（后写权重最高）
    for f in fadingOut:
        w = smMix * curve(f.mixFrom)         // 淡出权重 = 曲线映射后的 mixFrom（rive 行为）
        applyState(f.state, f.elapsedUs, w)
    applyState(currentState, currentElapsedUs, smMix * curve(mix))  // 淡入权重

RegionInstance::curve(t):
    // 用当前 transition 的 interpolation/bezier 曲线映射权重值 t
    if transition == null: return t          // 无过渡，权重直接用（通常是 1）
    switch transition.interpolation:
        None/Hold: return t >= 1 ? 1 : 0     // 无过渡感，直接跳变
        Linear:    return t
        Bezier:    return bezierEval(transition.bezierOut, transition.bezierIn, t)

RegionInstance::applyState(state, elapsedUs, weight):
    if weight <= 0: return
    if state is empty (animationId 为空): return   // 空状态不输出
    animation = document.findAnimation(state.animationId)
    // 按 animation 的 loop 模式把 elapsedUs 映射到采样时间（复用 PAGTimeline 的 loop 逻辑）
    sampleUs = applyLoop(elapsedUs, animation)
    // 复用现有 channel 求值 + RuntimeBinding::apply，weight 即 mix 权重
    for object in animation.objects:
        for channel in object.channels:
            value = channel.evaluateAt(sampleUs, animation.frameRate)
            binding.apply(object.targetNode, channel.name, value, weight)
```

### 8.5 关键点说明

- **独立权重模型（对齐 rive）**：来源状态用 `mixFrom`（进入淡出时保存的淡入权重），**不是 `1-mix`**。这样 early-exit 时多个未完成过渡可以叠加淡出（A 淡出中被打断转 B，A 保留残留权重、B 又淡出、C 淡入）。参见 rive `state_machine_instance.cpp:577,609-640`
- **early-exit**：`findAllowedTransition` 在过渡进行中只接受 `enableEarlyExit==true` 的转移；默认 false 时行为与 rive 一致（不可打断）
- **pauseOnExit**：`changeState` 时若转移设了 `pauseOnExit`（且有 exitTime），把来源 FadingState 的 `elapsedUs` 固定为 exitTime 并标记 `frozen`，advance 不再推进它——过渡期间来源冻结在 exitTime 帧做混合（对齐 rive `applyExitCondition`）
- **mix 曲线复用 Keyframe**：`curve()` 把权重值经 `interpolation`/`bezier` 曲线映射，与 keyframe easing 同一套 bezier 求值
- **过渡叠加**：过渡中同一 channel 被多个 outgoing + 一个 incoming 各写一次，`RuntimeBinding::apply` 的 mix 规则自动做 lerp（连续）或覆盖（离散）；currentState 最后 apply，权重最高
- **exitTime**：`allowed()` 里条件检查前先卡累计时间 `currentElapsedUs >= exitUs`。对 loop 动画，若 exitTime 落在单个周期内则每遍循环对齐重触发（对齐 rive `state_transition.cpp:192-196` 的 `floor(lastTime/duration)*duration` 逻辑）；`currentElapsedUs` 本身累计不回绕（对应 rive `totalTime`），只在 `applyLoop` 采样时回绕。`allowed()` 返回 bool（yes/no），不需要 rive 的 waitingForExit 三态——rive 的三态是为驱动 `needsAdvance` 循环，而 libpag 宿主时钟持续驱动，exitTime 靠外部时钟自然到达（详见 8.6）
- **trigger 时序**：`fireTrigger` 只置 `input.fired=true`（不评估，仅记录，若之后不 advance 则标志保留）→ 下一次 `advance` 内各 region 的 `evaluateCondition` 读到 → 触发的 region 记入 `consumedTriggers` → 本次 advance 末尾（评估之后）统一清 `fired` 和 `consumedTriggers`。即 fire 与消费解耦：fire 记录、advance 评估消费，两者可跨调用。对齐 rive `SMITrigger`（`fire` 置位 + `markNeedsAdvance`，`layer.advance` 评估在前、`input.advanced()` 清空在后，同一次 advance 内）
- **单帧链式切换**：`advance` 里循环 `tryChangeState` 最多 100 次（与 rive `maxIterations=100` 一致），处理 A→B→C 链

### 8.6 与 rive 的一致性对照

| 行为 | 本设计 | rive | 一致 |
|---|---|---|---|
| 过渡中来源状态继续播 | 是（`f.elapsedUs += deltaUs`） | 是（`m_holdAnimationFrom==false` 时） | ✅ |
| early-exit 打断过渡 | 支持，`enableEarlyExit` 默认 false | 支持，`EnableEarlyExit` flag 默认 false | ✅ |
| 单帧链式切换上限 | 100 | 100（`maxIterations`） | ✅ |
| trigger per-region 消费 | 每 region 各消费一次，帧末清空 | 每 layer 各消费一次，帧末清空（`m_usedLayers`） | ✅ |
| trigger 过渡中 fire 但无法消费 | 帧末清空，需重新 fire | 帧末无条件清空（`advanced()` reset） | ✅ |
| exitTime 与 loop 动画 | 累计时间比较；exitTime≤周期时每遍循环对齐重触发 | 同（`totalTime` + `floor(lastTime/duration)*duration`） | ✅ |
| mix 权重模型 | 独立 `mixFrom`，非 `1-mix` | 独立 `m_mixFrom` | ✅ |
| pauseOnExit（过渡冻结来源） | 支持（FadingState.frozen） | 支持（`m_holdAnimationFrom`/`m_holdTime`） | ✅ |
| waitingForExit（三态） | 不需要 | 三态（no/yes/waitingForExit） | ⚠️ 驱动模型不同，见下 |

**两处差异的说明**：

- **pauseOnExit**：已支持。`changeState` 时若转移设了 pauseOnExit + exitTime，来源 FadingState 冻结在 exitTime 帧（`frozen=true`，advance 不推进其 elapsedUs），过渡期间保持稳定姿势做混合，避免走路→跑步时来源动画继续摆动的抖动。对齐 rive `state_transition.cpp:208-221` 的 applyExitCondition。

- **waitingForExit**：rive 的三态（`state_transition.hpp:21-26`）中 `waitingForExit` 的真正作用是驱动 `advance` 返回值（`state_machine_instance.cpp:255`：`return ... || m_waitingForExit || ...`），让宿主的 `while(needsAdvance())` 循环在"条件满足但等 exitTime"时继续 tick，否则时间停止推进、exitTime 永远到不了。**libpag 不需要这个机制**——libpag 是宿主时钟持续驱动模型：`PAGComposition::advance` 直接调 `timeline->advance(delta)` 并忽略返回值（`runtime/PAGComposition.cpp:55`），宿主每帧无条件推进时间。`currentElapsedUs` 每帧照常累加，exitTime 靠外部时钟自然到达。因此 `allowed()` 返回 bool（yes/no 合并）即可，无需三态。

### 8.7 顶层 SM 与嵌套 SM 的驱动归属（完全对齐 Animation / rive）

StateMachine 的驱动模型与现有 Animation/PAGTimeline 一致，也与 rive 的"最外层业务驱动、嵌套自动带跑"一致。**分两套，不是同一实例的两条路径。**

| 维度 | 顶层 SM | 嵌套 SM（子 composition 内） |
|---|---|---|
| **声明位置** | document 顶层 `<Animations>` 里，与 `<Animation>` 并列 | 子 composition 的 Layer `<Timelines>` 里用 `<StateMachine ref>` |
| **谁创建** | 业务调 `getStateMachineTimeline(id)` 现场创建 + 缓存 | 构建运行时树时 `spawnTimelines` 自动创建 |
| **谁驱动** | **业务自己**每帧 `sm->advanceAndApply(dt)` | **scene 递归**：`scene->advanceAndApply` → `PAGComposition::advance` 遍历 timelines 自动带跑 |
| **是否在 composition.timelines 里** | 否（游离对象，`scene->advanceAndApply` 不碰它） | 是 |
| **对齐现有** | = 顶层 Animation（`getTimeline` 现场 new、business drive） | = 嵌套 Animation（`spawnTimelines` 建、scene drive） |

**顶层 SM 用法（业务主控）**：

```cpp
auto sm = scene->getStateMachineTimeline("btnSM");  // 顶层 SM，从 document 顶层 <Animations> 找
sm->setBool("isHover", true);
sm->fireTrigger("click");
sm->advanceAndApply(delta);      // ★ 业务自己驱动，scene->advanceAndApply 不驱动它
scene->draw(surface);
```

**嵌套 SM 用法（scene 自动驱动）**：

```cpp
scene->advanceAndApply(delta);   // ★ 递归驱动所有子 composition 里 <Timelines> 挂的 SM
scene->draw(surface);
// 若要控制某个嵌套 SM，经 rootComposition() 向下找到对应子 PAGComposition 访问
```

**关键对齐点**：
- 顶层 SM 定义在 `<Animations>` 里，**不需要**被任何 Layer 的 `<Timelines>` 引用即可通过 `getStateMachineTimeline(id)` 拿到驱动——与顶层 Animation 通过 `getTimeline(id)` 拿到完全一致（`getStateMachineIds()` 直接列 document 顶层的 StateMachine）。
- `<Timelines><StateMachine ref="@id">` 仅用于**嵌套场景**：子 composition 的 Layer 挂一个 SM，让 scene 递归时自动带跑（类比子 Layer `<Timelines><Animation ref>`）。
- `getStateMachineTimeline` 只返回顶层 SM（对齐 `getTimeline` 只返回 `document->animations`），嵌套 SM 是 per-composition 实例，按 id 全局查有歧义，故走 composition 树访问。

**与 rive 对照**：rive 最外层 `stateMachineNamed()` 业务创建 + `advanceAndApply` 业务驱动；嵌套 `NestedStateMachine` 文件自带、由外层 `advanceInternal(AdvanceNested)` 递归带跑。libpag 顶层 SM ≈ rive 最外层，嵌套 SM ≈ rive NestedStateMachine，驱动归属一一对应。

---

## 九、测试场景设计（待审阅）

测试放在 `test/src/PAGXTest.cpp`，用 `PAGX_TEST(PAGXTest, ...)` 宏，遵循现有风格（数值用整数、round-trip 用 ToXML→FromXML、时间用微秒）。

### 9.1 导入/导出 round-trip

| 用例 | 验证点 |
|---|---|
| `StateMachineRoundTrip` | 完整状态机（inputs + 多 region + states + transitions + conditions）导出再导入，各字段一致 |
| `StateMachineTimelineRoundTrip` | Layer 的 `<StateMachine ref="@id"/>` timeline 引用 round-trip，timelineType 为 StateMachine，stateMachineId 正确 |
| `StateMachineInDataURI` | StateMachine 放在 `<Animations>` 容器里与 `<Animation>` 并列，round-trip 后顺序与归属正确 |
| `InputAllTypesRoundTrip` | 三种 input（bool/number/trigger）+ 各自 default（defaultBool/defaultNumber/trigger 无 default）round-trip |
| `TransitionAllOpsRoundTrip` | 所有 op（equal/notEqual/lessThan/lessThanOrEqual/greaterThan/greaterThanOrEqual/trigger）round-trip，op↔字符串映射正确 |
| `TransitionInterpolationRoundTrip` | transition 的 interpolation（none/linear/bezier/hold）/bezier-out/bezier-in/duration round-trip |
| `TransitionExitTimeRoundTrip` | exitTime 的 nullopt（不写属性）与有值（写属性）两种情况 round-trip，区分清楚 |
| `TransitionEarlyExitRoundTrip` | earlyExit 的 true/false round-trip；false 为默认时不写属性（对齐 playing 默认省略的风格） |
| `TransitionPauseOnExitRoundTrip` | pauseOnExit 的 true/false round-trip；false 为默认时不写属性 |
| `AnyStateTransitionRoundTrip` | `from="any"` 的转移 round-trip，from 值保持 "any" |
| `EmptyStateRoundTrip` | `<State name="idle"/>` 无 animation 的空状态 round-trip，animationId 为空 |
| `MultiRegionRoundTrip` | 多个 StateRegion（各自 name/initialState/states/transitions）round-trip，顺序与内容一致 |
| `MultiConditionRoundTrip` | 一条 transition 挂多个 Condition round-trip，顺序一致 |
| `StateMachineDefaultsOmitted` | 默认值属性（interpolation=linear、earlyExit=false、无 exitTime）导出时省略，再导入仍得默认值 |
| `StateMachineInComposition` | StateMachine 定义在 `<Composition>` 内部（而非顶层）时 round-trip 正确，被组合内 Layer 引用 |

### 9.2 导入错误处理

| 用例 | 验证点 |
|---|---|
| `StateMachineMissingRegion` | StateMachine 无任何 StateRegion → 报错进 doc.errors |
| `StateMachineDuplicateRegionName` | 同一 StateMachine 内两个 region 同名 → 报错 |
| `StateRegionMissingInitialState` | region 未写 initialState 或指向不存在的 state → 报错 |
| `StateRegionDuplicateStateName` | 同一 region 内两个 state 同名 → 报错 |
| `TransitionMissingFromOrTo` | transition 缺 from 或 to 属性 → 报错 |
| `TransitionUnknownState` | transition 的 from/to 引用不存在的 state（非 "any"）→ 报错 |
| `StateUnknownAnimation` | State 的 `animation="@nope"` 引用不存在的 Animation → 报错 |
| `ConditionUnknownInput` | condition 的 inputName 引用未声明的 input → 报错 |
| `ConditionMissingOp` | condition 缺 op 属性 → 报错 |
| `ConditionOpTypeMismatch` | number 专用 op（lessThan 等）用在 bool input 上 → 报错；trigger op 用在 bool 上 → 报错 |
| `ConditionMissingValue` | 非 trigger 的 condition 缺 value → 报错；trigger condition 有 value → 忽略（不报错）|
| `InputMissingNameOrType` | Input 缺 name 或 type，或 type 非法（不是 bool/number/trigger）→ 报错 |
| `InputDuplicateName` | 同一 StateMachine 两个 input 同名 → 报错 |
| `TimelineRefUnknownStateMachine` | `<StateMachine ref="@nope"/>` 引用不存在的状态机 → 报错 |
| `TimelineRefMissingAt` | `<StateMachine ref="btnSM"/>` ref 不以 @ 开头 → 报错（对齐 AnimationTimeline 的 ref 校验）|
| `StateMachineErrorsDoNotAbort` | 局部错误（如某 condition 非法）报错后，解析继续，其余节点仍正确加载（对齐现有 ReportError 不中断约定）|

### 9.3 运行时单元测试（不涉及渲染，验证状态逻辑）

**基础转移**

| 用例 | 验证点 |
|---|---|
| `SMInitialState` | 构造后每个 region 的 currentState == 其 initialState |
| `SMBoolTransitionEqual` | bool input == true 满足 `op=equal value=true` → 切换；恢复 false → 反向转移 |
| `SMBoolTransitionNotEqual` | `op=notEqual` 正确触发 |
| `SMNumberTransitionAllOps` | number input 对 6 种比较 op（equal/notEqual/lessThan/lessThanOrEqual/greaterThan/greaterThanOrEqual）在边界值（相等、略大、略小）的触发/不触发 |
| `SMNoTransitionWhenConditionFalse` | 条件不满足时 advance 不切状态，currentState 不变 |
| `SMDefaultInputValue` | input 的 default 值在构造时生效（如 default=true 的 bool 初始就满足条件）|

**trigger**

| 用例 | 验证点 |
|---|---|
| `SMTriggerFireThenAdvanceConsumes` | fire 后调 advance 才评估消费（fire 本身不触发转移）；验证 fire 与消费解耦 |
| `SMTriggerFireWithoutAdvanceKeeps` | fire 后不 advance，fired 标志保留；之后 advance 仍能消费（不因未 advance 而丢失）|
| `SMTriggerConsumedOnce` | fire trigger → advance → 单 region 只切一次；同次 advance 内不重复切 |
| `SMTriggerClearedAfterAdvance` | fire → advance（评估+消费+末尾清空）→ 再 advance 不再触发（同一次 advance 内评估在前、清空在后）|
| `SMTriggerPerRegion` | 两个 region 都监听同一 trigger → 一次 advance 各切一次（per-region 消费），互不影响 |
| `SMTriggerNotFiredNoTransition` | 未 fire 的 trigger condition 不满足，不切换 |
| `SMTriggerFireDuringTransition` | 过渡进行中 fire trigger，若目标转移无 earlyExit 则等过渡完；已确认（见 9.7-4）：本次 advance 末尾清空，需重新 fire |

**AnyState / 条件组合**

| 用例 | 验证点 |
|---|---|
| `SMAnyStateTransition` | `from="any"` 的转移从任意当前状态都能触发 |
| `SMAnyStatePriority` | 同时存在 any 转移和当前状态转移且都满足时，any 优先（对齐伪代码先查 any）|
| `SMConditionAnd` | 多 condition 的 transition：全满足才切，缺一不切 |
| `SMEmptyConditionAlwaysAllowed` | 无 condition 的 transition：进入 from 状态后立即（受 exitTime 约束）自动转移 |

**exitTime**

| 用例 | 验证点 |
|---|---|
| `SMExitTimeGate` | 有 exitTime 的转移：条件满足但动画未播到 exitTime 时不切，播过后才切 |
| `SMExitTimeZero` | exitTime=0：条件满足即可立即转（等价无门槛）|
| `SMExitTimeWithLoop` | 绑定 loop 动画的状态，exitTime≤周期时每遍循环相同位置都可触发（第 1 遍累计 exitTime 触发，未转成则第 2 遍累计 duration+exitTime 再触发）| 
| `SMExitTimeNoConditionAutoAdvance` | 无 condition + 有 exitTime：动画播到 exitTime 帧自动转移（loading→success 场景）|

**earlyExit / 过渡打断**

| 用例 | 验证点 |
|---|---|
| `SMNoEarlyExitBlocks` | 过渡进行中（mix<1），目标转移 earlyExit=false → 新条件满足也不打断，等过渡完成后才切 |
| `SMEarlyExitInterrupts` | 过渡进行中，目标转移 earlyExit=true → 立即打断，旧过渡的来源进入淡出列表 |
| `SMEarlyExitMultiFadeOut` | 连续两次 early-exit（A→B 途中打断转 C）→ fadingOut 列表有 2 个来源（A、B），各自独立 mixFrom |
| `SMTransitionCompleteClearsFading` | 过渡 mix 到 1 后 fadingOut 清空、transition 置 null |

**pauseOnExit**

| 用例 | 验证点 |
|---|---|
| `SMPauseOnExitFreezesSource` | pauseOnExit=true + exitTime：过渡开始后来源 FadingState 的 elapsedUs 固定为 exitTime，多次 advance 不变（frozen=true）|
| `SMPauseOnExitNeedsExitTime` | pauseOnExit=true 但未设 exitTime：不冻结（frozen=false，来源继续推进）|
| `SMPauseOnExitDefault` | 默认 pauseOnExit=false：来源过渡期间继续推进 elapsedUs |

**链式 / 边界**

| 用例 | 验证点 |
|---|---|
| `SMChainedTransition` | 单帧内 A→B→C 链式切换（B 的转移条件立即满足），一次 advance 到 C |
| `SMChainedMaxIterations` | 构造死循环转移（A→B→A 条件恒真）→ 达到 100 次上限后停止，不崩溃、不死循环 |
| `SMDurationZeroHardCut` | duration=0 的转移：无过渡，mix 直接为 1，fadingOut 不保留 |
| `SMZeroDeltaAdvance` | advance(0)：不推进时间但仍评估转移（input 已变时应触发）|

**观察与多 region**

| 用例 | 验证点 |
|---|---|
| `SMStateChangeListener` | 状态切换触发 observer 回调，回调收到正确的 regionName + newState；未切换不回调 |
| `SMListenerDetach` | ObserverHandle 析构/detach 后不再回调 |
| `SMMultiRegionIndependent` | 两 region 各自独立推进，一个切状态不影响另一个 |
| `SMMultiRegionSharedInput` | 一个 input 被多个 region 的 condition 读取，各自独立判断 |
| `SMGetCurrentStatePerRegion` | `getCurrentState(regionName)` 返回对应 region 的当前状态；未知 regionName 的处理 |
| `SMGetRegionIds` | `getRegionIds()` 返回所有 region 名，顺序与声明一致 |

### 9.4 mix 过渡与 apply（截图测试，验证渲染输出）

| 用例 | 验证点 | Baseline key |
|---|---|---|
| `SMCrossfade` | normal→hover 过渡中 mix=0.5，scaleX 在两动画间按 0.5 权重叠加 | `PAGXStateMachine/Crossfade` |
| `SMLinearMixHalfway` | linear interpolation，过渡到时间中点时输出为两状态中值 | `PAGXStateMachine/LinearMixHalfway` |
| `SMBezierMix` | bezier interpolation 的过渡权重曲线正确（同一时间点权重与 linear 不同）| `PAGXStateMachine/BezierMix` |
| `SMHoldInterpolation` | hold interpolation：过渡期间保持来源值，末尾才跳变到目标 | `PAGXStateMachine/Hold` |
| `SMNoneInterpolation` | none interpolation：无过渡感，立即跳变（与 hold 效果对比）| `PAGXStateMachine/None` |
| `SMEarlyExitOverlap` | early-exit 造成两个来源同时淡出 + 一个淡入，三者叠加渲染正确 | `PAGXStateMachine/EarlyExitOverlap` |
| `SMPauseOnExitFrozenFrame` | pauseOnExit 过渡中来源冻结在 exitTime 帧渲染（对比不冻结时来源继续播的不同输出）| `PAGXStateMachine/PauseOnExit` |
| `SMMultiRegionStack` | visual region（scaleX）+ enable region（alpha）同时作用，输出叠加正确 | `PAGXStateMachine/MultiRegionStack` |
| `SMMultiRegionSameChannel` | 两 region 写同一 channel，按声明顺序后者叠加在前者结果上 | `PAGXStateMachine/MultiRegionSameChannel` |
| `SMEmptyStateNoOutput` | 空状态激活时不写 channel，目标保持进入空状态前的值 | `PAGXStateMachine/EmptyState` |
| `SMTransitionToEmptyState` | 从动画状态过渡到空状态：动画权重淡出，空状态无输出，最终回到 channel 基线 | `PAGXStateMachine/ToEmptyState` |
| `SMStableStateOutput` | 无过渡的稳定状态：持续输出当前状态动画（按 loop 推进）| `PAGXStateMachine/StableState` |

### 9.5 生命周期与集成

| 用例 | 验证点 |
|---|---|
| `SMTimelineOutlivesScene` | `getStateMachineTimeline` 返回的 timeline 在 scene 析构后调用 advance/apply 安全（weak_ptr owner，静默 no-op）|
| `SMPerCompositionInstance` | 同一 StateMachine 被两个 Layer 引用 → 两个独立实例，各自当前状态互不干扰 |
| `SMGetSameInstance` | 同一 id 多次 `getStateMachineTimeline` 返回同一实例（缓存）|
| `SMGetUnknownTimeline` | `getStateMachineTimeline("nope")` 返回 nullptr，不崩溃 |
| `SMTwoDrivePathsSameInstance` | 路径A（scene->advanceAndApply 整体驱动）与路径B（getStateMachineTimeline 单独驱动）操作同一实例：单独设 input 后整体驱动能看到、整体驱动的状态变化单独查 getCurrentState 能读到 |
| `SMGetStateMachineIdsTopLevelOnly` | `getStateMachineIds()` 只返回顶层 StateMachine，不含子 composition 内的（对齐 getTimelineIds 约定）|
| `SMNestedComposition` | 子 composition 内的状态机独立运行；顶层 `getStateMachineTimeline` 不返回它，需经 rootComposition 向下访问 |
| `SMCoexistWithAnimation` | 同一 Layer 同时挂 AnimationTimeline + StateMachineTimeline，两者输出按叠加规则共存 |
| `SMScopedInputAcrossCompositions` | 两个 composition 各自的 SM 实例的 input 相互隔离，设一个不影响另一个 |

### 9.6 编辑与重建（对齐现有 notifyChange / timeline 重建机制）

| 用例 | 验证点 |
|---|---|
| `SMNotifyChangeRebuildsOnStateMachineEdit` | 编辑 StateMachine 节点（增删 state/transition）后 notifyChange → runtime SM 实例重建，当前状态回到 initialState |
| `SMNotifyChangeKeepsWhenUnrelated` | 编辑无关节点 notifyChange → SM 实例不重建，当前状态保留 |
| `SMTimelineRefEditRebuild` | 编辑 Layer 的 StateMachineTimeline 引用（改 stateMachineId）→ 重新绑定到新状态机 |

### 9.7 已确认的测试语义

1. **截图测试 baseline 前缀**：用 `PAGXStateMachine/` 目录。（待你最终确认）
2. **单元测试 vs 截图测试分层**：9.3 只查状态字段不 draw；9.4 才 draw 比对截图。（待你最终确认）
3. **多 region 写同一 channel（`SMMultiRegionSameChannel`）**：按声明顺序叠加（后者叠在前者结果上）。建议在文档里作为契约明确。（待你最终确认）
4. **trigger 跨帧行为（`SMTriggerFireDuringTransition`）** ✅：对齐 rive——帧末无条件清空。过渡中 fire 但因 earlyExit=false 无法消费时，该 trigger 当帧末即清空，业务需在过渡完成后重新 fire。
5. **exitTime 与 loop（`SMExitTimeWithLoop`）** ✅：对齐 rive——按累计时间 `currentElapsedUs` 比较，且当 exitTime≤单个循环周期时**每遍循环的相同位置都可触发**（`exitUs += floor(currentElapsedUs/durationUs)*durationUs`）。测试预期：动画 10 帧、exitTime=5、loop 模式，条件恒真 → 第 1 遍累计 5 帧触发、若那次没转成则第 2 遍累计 15 帧再次可触发。exitTime>周期或 Once 模式则只首次到达触发一次。
