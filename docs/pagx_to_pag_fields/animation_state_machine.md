# PAGX 节点字段提取：动画与状态机

> M0-1 产出。来源：include/pagx/nodes/*.h、src/pagx/PAGXNodeChannel.cpp、PAGAnimation.cpp、PAGStateMachine.cpp。
> 作为 spec/pag_binary_spec.md §9（节点 Tag 定义）的编写依据。二进制新增/规范扩展标注见设计文档。

# PAGX 节点模型字段提取：动画与状态机

来源：include/pagx/nodes/{Animation,AnimationObject,Channel,KeyValue,Keyframe,Timeline,AnimationTimeline,StateMachineTimeline,StateMachine,StateMachineInput,StateRegion,State,StateTransition,TransitionCondition}.h、include/pagx/nodes/Node.h、include/pagx/types/{Point,Color}.h、src/pagx/PAGXNodeChannel.cpp、src/pagx/PAGAnimation.cpp、src/pagx/PAGStateMachine.cpp

## 通道总结（重要）

`src/pagx/PAGXNodeChannel.cpp` 的 `ChannelsFor(NodeType)`（约 :1154）switch 只注册了 Layer/元素/Style/Filter 节点。本报告全部 9 个 Node 派生类型（Animation=13、AnimationObject=14、Channel=15、StateMachine=16、StateRegion=17、State=18、StateTransition=19、TransitionCondition=20、StateMachineInput=21）均落入 `default: return empty`，即**全部无通道**：它们的任何字段都不能被动画/DataBind 写入。这些节点是动画的*驱动方*而非*目标方*（动画通过 Channel.name 寻址目标节点上注册的通道）。

另注：任务里提到的 "InputType / ConditionOperator" 实际枚举名为 `StateMachineInputType` 和 `TransitionConditionOp`。

## 共享基础类型

- `Frame = int64_t`（Keyframe.h:23），`constexpr Frame ZeroFrame = 0`
- `Point { float x=0; float y=0; }`（types/Point.h）
- `Color { float red=0; green=0; blue=0; alpha=1; ColorSpace colorSpace=SRGB; }`（types/Color.h）
- `KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color, Matrix, std::shared_ptr<PAGImage>>`（KeyValue.h:27）— variant 下标与 ChannelValueType 数值一一对应
- Node 基类（所有 Node 派生类继承，下不重复）：`id: std::string = {}`、`customData: std::unordered_map<std::string,std::string> = {}`（key 不含 "data-" 前缀）、`sourceLine: int = -1`

---

### Animation (NodeType::Animation = 13)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| duration | Frame (int64_t) | 0 | 动画总长（帧数） | 无 |
| frameRate | float | 60.0f | 时间↔帧换算速率 | 无 |
| loop | LoopMode | LoopMode::Once | 到达末尾后的循环行为 | 无 |
| objects | std::vector<AnimationObject*> | {} | 动画对象列表，每个驱动一个目标节点 | （子节点容器） |

子节点：
- objects: AnimationObject*

枚举：
- LoopMode（隐式赋值，头文件无显式数值）: Once=0, Loop=1, PingPong=2
  - Once=播放一次停在末帧；Loop=到尾从头重启；PingPong=正向到尾再反向到头往复

运行时（PAGAnimation.cpp）：Once 用 clamp、Loop 用 mod duration、PingPong 在 2×duration 周期上镜像（WrapTime）；playbackPeriod 对 PingPong 返回 duration*2。

---

### AnimationObject (NodeType::AnimationObject = 14)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| target | std::string | {} | 目标节点 id，构建运行时 timeline 时按文档 id 表解析 | 无 |
| channels | std::vector<Channel*> | {} | 施加到目标节点的动画通道集 | （子节点容器） |

子节点：
- channels: Channel*

运行时（PAGAnimation.cpp resolveTargets）：`contextDoc->findNode(object->target)` 为全文档扁平查找；解析到的目标若不在当前 effectiveBinding 范围内（`binding->contains(targetNode)`）则丢弃，避免跨 Composition 边界施加。

---

### Channel (NodeType::Channel = 15) — 抽象基类

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| name | std::string | {} | 本通道驱动的目标节点属性名（运行时按此寻址目标节点注册的通道） | 无 |
| (virtual) valueType() | → ChannelValueType | — | 返回关键帧值类型，供无 RTTI 分发 | — |
| (virtual) evaluateAt(Frame) / evaluateAt(int64_t micros, float frameRate) | → KeyValue | — | 帧离散/微秒连续采样，内部 double 精度插值 | — |

具体子类 `TypedChannel<T> : public Channel`（模板）：

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| keyframes | std::vector<Keyframe<T>> | {} | 按时间排序的关键帧；插值方式由各关键帧的 KeyframeInterpolationType 决定 | 无 |

Channel.cpp 显式实例化 T ∈ {float, bool, int, std::string, ImageRef, Color, Matrix}；PAGImage 变体被排除（不可插值）。[待确认：Matrix 变体如何参与关键帧插值的细节在 src/pagx/animation/Channel.cpp，本次未读]

---

### Keyframe<T>（模板 struct，非 Node）

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| time | Frame (int64_t) | ZeroFrame (0) | 本关键帧时间（帧） |
| value | T | {} (值初始化) | 本关键帧值 |
| interpolation | KeyframeInterpolationType | Linear | 本帧→下一帧的插值模式 |
| bezierOut | Point | {0,0} | 本帧出向贝塞尔控制柄（interpolation==Bezier 时使用） |
| bezierIn | Point | {0,0} | 本帧入向贝塞尔控制柄（interpolation==Bezier 时使用，与下一帧的 bezierOut 配对求缓动） |

### ImageRef（struct，非 Node）

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| id | std::string | {} | 引用的图片资源 id，按文档 id 表解析 |

---

### Timeline（抽象基类，**非 Node**，无 id/customData）

| 成员 | 说明 |
|---|---|
| (virtual) timelineType() → TimelineType | 具体 timeline 种类分发 |

持有者：`Layer.timelines: std::vector<std::unique_ptr<Timeline>>`（Layer.h，注释：Layer 引用 Composition 时激活的时间驱动行为列表，空=静态渲染）。

枚举：
- TimelineType: Animation=0, StateMachine=1（uint8_t，显式赋值）

### AnimationTimeline (: Timeline，非 Node)

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| animationId | std::string | {} | 引用的 Animation id，按文档 id 表解析 |
| playing | bool | true | PAGScene 首次构建运行时树时的初始播放状态；false=初始暂停 |
| evaluationOffset | int64_t | 0 | 求值点偏移，以该 Animation 自身 frameRate 的帧数计；运行时按 (currentFrame - evaluationOffset) 求值：正值延迟播放（冻在首帧）、负值跳前；语义等同 PAG 的 compositionStartTime |

### StateMachineTimeline (: Timeline，非 Node)

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| stateMachineId | std::string | {} | 引用的 StateMachine id，按文档 id 表解析 | 无 |

注：与 AnimationTimeline 不同，无 playing/evaluationOffset；初始态由各 StateRegion.initialState 携带。

---

### StateMachine (NodeType::StateMachine = 16)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| inputs | std::vector<StateMachineInput*> | {} | 所有 region 共享的输入端口声明（名字+类型）；活值由运行时 PAGStateMachine 单独持有 | 无 |
| regions | std::vector<StateRegion*> | {} | 并行正交 region 列表（至少一个），各自独立推进 | 无 |

子节点：
- inputs: StateMachineInput*
- regions: StateRegion*

### StateMachineInput (NodeType::StateMachineInput = 21)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| name | std::string | {} | 输入名，被 TransitionCondition.inputName 引用；State内唯一 | 无 |
| type | StateMachineInputType | Bool | 值种类 | 无 |
| defaultBool | bool | false | type==Bool 时的初始值，其他类型忽略 | 无 |
| defaultNumber | float | 0.0f | type==Number 时的初始值，其他类型忽略 | 无 |

枚举：
- StateMachineInputType: Bool=0, Number=1, Trigger=2（uint8_t，显式）
  - Bool=持久布尔，配 Equal/NotEqual；Number=持久数值，配全部 6 个比较运算符；Trigger=一次性事件，宿主触发、每 region 至多消费一次、每次 advance 结束清除（无论是否消费）

### StateRegion (NodeType::StateRegion = 17)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| name | std::string | {} | region 名，StateMachine 内唯一；运行时按名查询（getCurrentState(regionName)） | 无 |
| initialState | std::string | {} | 启动时进入的状态名；无匹配则运行时回退为 region 内第一个 State | 无 |
| states | std::vector<State*> | {} | 状态集；状态名 region 内唯一（跨 region 可重名） | 无 |
| transitions | std::vector<StateTransition*> | {} | 转移集；from/to 引用本 region 状态名，from="any" 匹配任意状态 | 无 |

### State (NodeType::State = 18) — 抽象基类

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| name | std::string | {} | 状态名，region 内唯一；被 StateTransition.from/to 引用 | 无 |
| (virtual) stateType() → StateType | — | 具体状态种类分发 | — |

AnimationState 的 nodeType() 继承自 State，仍返回 NodeType::State=18（序列化时 State/AnimationState 同用 18，靠 StateType 区分子类）。

### AnimationState (: State)

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| animationId | std::string | {} | 该状态播放的 Animation id；空=空状态（纯控制流节点，不驱动任何通道） | 无 |

枚举：
- StateType: Animation=0（uint8_t，显式；v1 仅此一种，预留 BlendState 等扩展）

运行时（PAGStateMachine.cpp createTimelineForState）：仅处理 StateType::Animation；animationId 引用缺失或类型错误的节点按畸形文档处理——debug 断言、release 回退为空状态。

### StateTransition (NodeType::StateTransition = 19)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| from | std::string | {} | 源状态名，或 "any"（`constexpr const char* AnyStateName = "any"`）匹配 region 内任意状态 | 无 |
| to | std::string | {} | 目标状态名（本 region 内） | 无 |
| duration | Frame (int64_t) | 0 | crossfade 时长（帧）；出态权重降到 0、入态升到 1 的过渡长度 | 无 |
| frameRate | float | 60.0f | 把 duration 帧换算成时间的帧率（crossfade 速度独立于所连接的状态动画） | 无 |
| interpolation | KeyframeInterpolationType | Linear | crossfade 权重曲线插值（复用关键帧插值模型） | 无 |
| bezierOut | Point | {0,0} | crossfade 曲线出向贝塞尔柄（Bezier 时） | 无 |
| bezierIn | Point | {0,0} | crossfade 曲线入向贝塞尔柄（Bezier 时） | 无 |
| enableEarlyExit | bool | false | 是否允许打断进行中的 crossfade；false=仅在无活动 crossfade 时评估；true=可在 crossfade 中途触发，多个出态可叠加淡出 | 无 |
| pauseOnExit | bool | false | 转移开始时是否冻结源状态动画在当前帧（而非边播边淡出） | 无 |
| conditions | std::vector<TransitionCondition*> | {} | 门控条件，全部为真（AND）才允许转移；空=无条件恒允许 | 无 |

运行时：CurveMix 把 interpolation 映射为 None/Hold→阶跃、Bezier→SolveBezierEasing(bezierOut.x, bezierOut.y, bezierIn.x, bezierIn.y, t)、Linear→恒等。crossfade 权重模型为 "new-over-old"：出态保持满权重（mixFrom），入态 0→1 叠加淡入（非双向对淡）。frameRate<=0 时回退 60。每帧每 region 最多 100 次转移（MAX_TRANSITIONS_PER_FRAME）。

### TransitionCondition (NodeType::TransitionCondition = 20)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| inputName | std::string | {} | 被测试的 StateMachine 输入名 | 无 |
| op | TransitionConditionOp | Equal | 比较运算符，须与输入类型兼容 | 无 |
| valueBool | bool | false | Bool 输入的比较值；Number/Trigger 忽略 | 无 |
| valueNumber | float | 0.0f | Number 输入的比较值；Bool/Trigger 忽略 | 无 |

枚举：
- TransitionConditionOp: Equal=0, NotEqual=1, LessThan=2, LessThanOrEqual=3, GreaterThan=4, GreaterThanOrEqual=5, Trigger=6（uint8_t，显式）
  - 合法组合（头文件注释 + PAGStateMachine.cpp EvaluateCondition 双重确认）：Bool→Equal/NotEqual（比 valueBool）；Number→全部 6 个比较（比 valueNumber，Equal/NotEqual 用 FloatNearlyEqual）；Trigger→仅 Trigger（值忽略，检查 fired 且未被本 region 消费）
  - EvaluateCondition 对 Bool 输入遇到比较类运算符返回 false；Trigger 输入在 EvaluateCondition 内直接返回 false，由 IsTransitionAllowed 经 consumedTriggers 集合单独处理；转移成功后对应 trigger 记入 region 的 consumedTriggers，advance 结束统一清除 fired 与 consumedTriggers

---

## 附：ChannelValueType 与 KeyValue variant 对齐表

| ChannelValueType (uint8_t) | 数值 | KeyValue variant alternative | 可插值 |
|---|---|---|---|
| Float | 0 | float | 是 |
| Bool | 1 | bool | 是 |
| Int | 2 | int | 是 |
| String | 3 | std::string | 是 |
| ImageRef | 4 | ImageRef | 是 |
| Color | 5 | Color | 是 |
| Matrix | 6 | Matrix | 是 |
| PAGImage | 7 | std::shared_ptr<PAGImage> | 否（TypedChannel 不实例化） |

## 二进制格式设计相关结论

1. 9 个动画/SM 节点均无通道注册 → 它们在二进制格式里是纯"驱动方"结构，字段全部静态存储（无 per-frame 可写位）。
2. Timeline 体系（AnimationTimeline/StateMachineTimeline）不是 Node：无 id/customData，由 Layer.timelines 以 unique_ptr 持有，二进制里应作为 Layer 的内嵌变体记录（TimelineType 判别）。
3. Keyframe<T>/ImageRef 是值 struct 非 Node；Keyframe 按 T=ChannelValueType 之一实例化 7 种（PAGImage 排除）。
4. State/AnimationState 共享 NodeType::State=18，需 StateType 子判别（类比 Layer 的结构）。
5. LoopMode、NodeType 枚举值是隐式赋值（本文数值按声明顺序推得）；ChannelValueType、KeyframeInterpolationType、TimelineType、StateMachineInputType、StateType、TransitionConditionOp 均显式赋值，可直接作为二进制 tag。
6. AnimationTimeline.evaluationOffset 为 int64_t 帧数（非微秒）；StateTransition.duration 为 int64_t 帧数 + 独立 frameRate。
