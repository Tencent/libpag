# PAG 三期迁移详细设计（P3-0）

依据：设计文档 §6/§7/§8.5（C26/C28/C34/C35）、字节级规范 576–767 预留区间、一期/二期已落地能力。
本文是三期全部编码任务（P3-1 ~ P3-15）的前置设计：关键接口、核心伪代码、语义边界与需澄清问题。

---

## 1. 总体架构

### 1.1 迁移管线（设计文档 §7.1，内存直转）

```
旧 .pag ─Codec::Decode─> pag::File ─LegacyPAGImporter（src/pagx/pag/importer/）─> PAGXDocument
                                                                    ├─ PAGExporter ─> 新 .pag   （pagx export）
                                                                    └─ PAGXExporter ─> .pagx    （pagx import）
```

- 导入器**不生成、不重新解析 XML 文本**，直接构建 PAGXDocument 节点树。
- 迁移后的文档走与 PAGX 完全相同的发布流水线（扁平化 → 剥离 → 图片处理 → 编码），
  因此二期已有的全部质量保障（RetainedIdTable、按引用计数、关键帧回调）自动生效。

### 1.2 导入器模块划分（src/pagx/pag/importer/）

| 文件 | 职责 | 对应旧代码 |
|---|---|---|
| LegacyPAGImporter.cpp | 入口：pag::File → PAGXDocument，遍历合成树 | — |
| LayerConverter.cpp | 图层树、Transform2D、运动路径、图层属性 | src/base/Layer.cpp、Transform2D.cpp |
| StyleConverter.cpp | 投影/描边/渐变叠加/外发光 | src/base/LayerStyles |
| EffectConverter.cpp | 11 种特效 → 新滤镜节点 | src/rendering/filters/ |
| TextConverter.cpp | TextDocument → Text/TextBox/TextModifier | src/base/TextLayer.cpp |
| SequenceConverter.cpp | 视频/位图序列 → ImageTable 序列条目 | src/base/sequences |
| AudioConverter.cpp | AudioBytes 原样携带 | src/base/AudioReader |
| KeyframeConverter.cpp | 旧关键帧 → 通道关键帧 | src/base/Property.cpp |

### 1.3 核心入口接口

```cpp
// include/pagx/LegacyPAGImporter.h
class LegacyPAGImporter {
 public:
  // 将旧 pag::File 内存树转换为 PAGXDocument。maxSupportedTagLevel 之上的旧文件返回
  // nullptr 并填充 issues（--strict 语义）。转换过程不修改 file。
  static std::shared_ptr<PAGXDocument> Convert(
      const std::shared_ptr<pag::File>& file,
      std::vector<PAGExportIssue>* issues = nullptr);
};
```

核心伪代码：

```
Convert(file):
  doc = PAGXDocument::Make(file->width, file->height)
  convertComposition(file->rootComposition, doc->rootLayer, parentFrameRate=file->frameRate)
  collectAudio(file)          -> doc->audioBytes
  collectFonts(file)          -> Font 节点 + 字符串表
  ensureRootAnimation(doc)    # §7.3：无关键帧也生成同时长同帧率根 Animation
  return doc

convertComposition(comp, layer, parentFrameRate):
  layer->composition = makeNode<Composition>(comp->width/height)
  for each oldLayer in comp->layers:
    child = makeNode<Layer>()
    LayerConverter::convert(oldLayer, child, parentFrameRate, comp->frameRate)
    layer->children.push(child)
  for each animation in comp->animations:   # 旧 PAG 无；保持对称
    ...
```

---

## 2. 时间锁定语义（§6.3，P3-2；对齐旧预合成）

### 2.1 旧运行时语义（代码核实，src/base/PreComposeLayer.cpp:78）

```
compositionFrame = round((layerFrame − compositionStartTime) × timeScale)
timeScale = 子合成帧率 / 父合成帧率
```

- **无 clamp**：越界帧自然外推；子内容动画在越界帧按关键帧两端保持（旧
  `Property::valueAtTime` 首尾帧外为常量）。
- compositionStartTime 可为负（等价新 startTime 为负）。
- timeScale 始终 > 0（两帧率均为正）。

### 2.2 新格式映射

- LayerTransform(354)：`startTime`（秒）= compositionStartTime / 父帧率；
  `timeScale`（无量纲）= 子帧率 / 父帧率。
- 子时间（秒）=（父时钟 − startTime）× timeScale。父时钟 = 文档默认时间线
  （Animation）的当前时间；嵌套锁定 = 父实例的子时间。
- **越界/负值**：不 clamp，与旧一致外推；动画通道 evaluate 在首尾帧外保持常量
  （二期通道实现已满足，需补回归用例）。
- **timeScale ≤ 0**：旧格式不可能出现；新格式 verify 告警并按 1 处理。
- 默认时间线是 StateMachine 或缺失：锁定退回自行推进 + verify 告警。

### 2.3 运行时接口（PAGComposition）

```cpp
// include/pagx/runtime/PAGComposition.h 扩展
class PAGComposition {
  // 时间锁定模式（§6.3）：由 LayerTransform 声明触发。
  // advance() 跳过锁定实例的时间线；apply() 按父时钟计算并定位：
  //   childTime = (parentClock − startTime) × timeScale
  // setCurrentTime() 之后下一次 apply() 将全部锁定后代级联定位。
  bool isTimeLocked() const;
  void setTimeLock(float startTimeSeconds, float timeScale);
};
```

伪代码：

```
PAGComposition::advance(delta):
  for child in children:
    if child.isTimeLocked: continue        # 时间由父时钟决定
    child.advance(delta)

PAGComposition::apply():
  parentClock = owningScene.defaultTimeline.currentTime()   # 或父实例 childTime
  for child in children:
    if child.isTimeLocked:
      childTime = (parentClock − child.startTime) × child.timeScale
      child.seekAllTimelinesTo(childTime)   # 定位该实例内全部时间线
    child.apply()
```

**帧步进确定性**（迁移对比用）：`pagx render --time t` 的 SM 步进循环同样适用于
锁定实例——默认时间线定位到 t 后 apply 级联，无需逐帧推进即可得到与旧 PAG
任意帧定位一致的结果（旧 PAG 本身就是纯函数式按帧求值）。

---

## 3. 关键难点设计

### 3.1 运动模糊（P3-9）——无状态按帧查询

**关键事实（代码核实，src/rendering/filters/MotionBlurFilter.cpp:101-106）**：旧实现
的"前一帧变换"是`layerCache->getTransform(contentFrame − 1)`——**纯函数式按帧号查询，
无跨帧运行时状态**。

新实现直接复用通道求值：

```
MotionBlurApply(layer, frame):
  cur  = evaluateTransformChannels(layer, frame)
  prev = evaluateTransformChannels(layer, frame > 0 ? frame − 1 : 0)
  if cur == prev: skip
  return MotionBlurFilter(cur, prev).apply(layerContent)
```

tgfx 侧 MotionBlurFilter 着色器直接移植（顶点插值两个矩阵）。
**不引入任何运行时缓存**，保证迁移对比的逐帧确定性。

### 3.2 3D 与相机（P3-9）——逐帧矩阵采样

旧 3D（Transform3D 90、CameraLayer 91）在新格式中表达为 `matrix3D` 通道的关键帧：

```
for frame in 0..duration:
  matrix3D[frame] = composeCamera(camera->transformAt(frame),
                                  layer->transform3DAt(frame))
  channel.keyframes.push(frame, matrix3D[frame])
```

- 采样密度 = 目标合成帧率（整数帧对齐迁移标准）；采样在**导入期**完成，
  运行时零 3D 计算。
- 投影与景深公式对齐旧运行时（src/base/CameraLayer.cpp），保留全部中间精度
  （double 计算，写出的关键帧为 float32——与旧渲染路径的 float32 输出一致）。
- 样本分布：仓库 166 个旧样本 3D/相机为 0，需在 P3-0 验收前用业务素材构造样例。

### 3.3 视频与位图序列（P3-10，C28）

**结构**：
- ImageTable 条目媒体类型扩展（规范 §6.6 已预留 2+）：`videoSequence`、`bitmapSequence`。
- 一个条目 = 一份序列（多分辨率 = 多条目，全部保留）；帧数据原样存储不重编码。
- XML：序列数据外置（`pagx import` 写旁置 .mp4/.webp 文件，Image 节点引用路径）。
- 二进制：ImageTable 内原样携带。

**解码栈对接（复用，不重写）**：

```
新 PAGX 运行时
  └─ SequenceReader（新薄封装）
      ├─ VideoSequenceDemuxer + VideoDecoderFactory（平台硬解）
      ├─ SoftAVCDecoder（软解兜底）
      ├─ SequenceImageQueue（预读）
      └─ 静态时间段跳过（SequenceInfo）
```

对接点（P3-10 详细化时的接口草案）：

```cpp
// src/pagx/runtime/SequencePlayer.h
class SequencePlayer {
  // 显示帧由所在合成实例的时间决定（与时间锁定共用 §6.3 的时间计算）。
  std::shared_ptr<tgfx::Image> frameAt(const Image* sequenceImage, double compositionTime);
  // PAGScene::advanceAndApply 驱动预读队列；seek（--time 定位）走 flush + 重定位。
  void advance(int64_t deltaUs);
};
```

**渲染对比的平台差异**：视频解码结果因平台硬解实现而异。验收规则（C34 的落地）：
视频合成样本的对比使用**软解路径**（SoftAVCDecoder）在单一平台跑，硬解仅验证
结构往返不参与像素门禁。

### 3.4 时间伸缩（P3-11）

`setDuration` + TimeStretchMode（缩放/重复/反向重复）为**文档级运行时接口**，
不进二进制（规范已定）：

```cpp
// 文档级：修改默认时间线的推进方式
scene->setTimeStretchMode(TimeStretchMode::Scale);   // 时长改变 = duration / rate
scene->setDuration(seconds);                          // 按 mode 重算 rate / 循环窗口
```

迁移映射：旧 TimeStretchMode(32) 标量写入文档 customData（诊断用），语义由
导入器换算为默认时间线的 duration 语义。

---

## 4. 能力块设计要点（摘要，逐项展开随各任务提交）

| 块 | 关键决策 |
|---|---|
| P3-1 Layer 变换属性 | anchor/rotation/scale/skew/skewAxis 与 Group 语义一致；运行时优先级 matrix3D > matrix > 分解属性；LayerRuntimeTarget 扩展种子自分解属性 |
| P3-1 Path 值类型 | 变体新增 PathData；命令序列相同 → 逐点插值（对齐旧 PathData::interpolate），不同 → Hold + verify 告警；列式：每帧一个完整 PathData |
| P3-3 文本通道 | TextHolder 的 TextLayoutParams 改为可写成员（通道写入后 flush 重排）；tracking 单位 1/1000em，有效字距 = letterSpacing + tracking × fontSize / 1000 |
| P3-6 样式 | 描边（沿轮廓描边，tgfx StrokeStyle）、渐变叠加（GradientOverlayStyle：gradient + angle + scale + offset）、外发光（GlowStyle：颜色/模糊/强度）；投影补 spread（扩展模糊量）与 blendMode |
| P3-7 特效 8 项 | 全部移植旧 GLSL（src/rendering/filters/）；节点模型：每特效一个 LayerFilter 子类 + 参数通道（Channel 名 = 参数名，Float/Color/Point） |
| P3-8 遮罩路径列表 | Layer.masks: vector<MaskPath>（path + mode + inverted + feather + opacity + expand）；tgfx MaskContext 逐条生成；路径动画直接走 §6.1 |
| P3-12 可编辑 | 旧 editableText/editableImage 索引 → ViewModel 属性 text<i>/image<i>（§7.2 映射表已定）；DataBind 指向具体 Text/Fill |

---

## 5. 旧接口对照表（§8.5，按 include/pag/pag.h 的类）

| 旧类/方法组 | 新对应 | 状态 |
|---|---|---|
| PAGFile::Load / LoadFromBytes | PAGImporter::LoadFromPAG（v4）+ LegacyPAGImporter（旧→新内存转换后再走 v4 路径）；或 PAGXImporter::FromFile（.pagx） | 已有（P3-13 接 CLI） |
| PAGFile::numTexts / getText / replaceText | ViewModel 属性 text<i>（读 + 赋值 + 观察） | P3-12 |
| PAGFile::numImages / replaceImage | ViewModel 属性 image<i> | P3-12 |
| PAGFile::getBounds / width / height | PAGXDocument::width / height / 根 layer layoutBounds | 已有 |
| PAGComposition::getLayersBy / layer(±i) / containsPoint / hitTestPoint | 新运行时树遍历（PAGComposition::layers / findNode）+ LayerBuilder hitTest | 遍历已有；hitTest P3-14 |
| PAGComposition::setCurrentTime / getProgress（播放控制） | PAGScene::getDefaultTimeline()->setCurrentTime + 进度换算 | 已有（C23 统一） |
| PAGLayer::visible / alpha / matrix / matrix3D / excludedTargets | 通道直写（viewModel 或动画通道 apply）+ PAGLayer 等价访问器 | P3-14 访问器 |
| PAGLayer::startTime / duration / timeStretch（运行时改） | 时间锁定属性运行时写接口 | P3-2/P3-14 |
| PAGLayer::markerList | 不支持（C35），迁移时丢弃并记录 | 关闭 |
| PAGImage（路径/像素/URI 构造） | ViewModel image 属性赋值（Image 节点 data/filePath） | P3-12 |
| PAGImageLayer / PAGImageScaleModes | ImagePattern + scaleMode（映射表 §7.4） | 已有 |
| PAGTextLayer::text / fillColor / ... | Text/TextBox 通道（§6.5 表） | P3-3 |
| PAGFont::RegisterFont / SetFont | FontConfig::registerFont（全局，§8.1） | 已有 |
| PAGSurface（离屏/纹理） | PAGSurface::MakeOffscreen / MakeFrom（已有） | 已有 |
| PAGPlayer（progress/play/stop/surface/flush） | PAGScene + 宿主时钟（advanceAndApply）；PAGPlayer 等价外观类在四期平台层提供 | 核心 P3-14；视图四期 |
| PAGVideoRange（视频区间查询） | SequencePlayer::visibleRanges | P3-10 |
| 音频（PAGFile 音频轨道播放） | AudioBytes 携带 + AudioReader 读取接口 | P3-11 |

---

## 6. 设计决策（2026-10-24 已确认）

1. **timeScale 与子合成实际帧率不一致**：verify 告警（非错误），运行时以 timeScale
   为准（与旧 PAG 一致）。
2. **视频对比门禁**：先按严格像素对齐（软解单平台路径）；硬解容差门禁发版前评估。
3. **PAGPlayer 播放外观类**：放四期。三期聚焦 PAGX → PAG 转换本身；PAGViewer 相关
   改造作为单独需求另行排期。
4. **3D/相机/运动模糊测试素材**：暂缓。先实现代码，素材回头补充（P3-9 的验收用例
   以构造样本先行，业务素材复核后置）。
5. **视频的 XML 外置方案（.pagx 输出旁置 .mp4）**：暂不需要。三期只要求
   .pag → 新 .pag 的转换（视频原样进二进制 ImageTable）；`pagx import` 的
   .pagx 输出若含视频，序列条目仅保结构、数据缺省为空并告警。

---

## 7. 验收门禁（引用）

- P3-15：166 个旧样本逐整数帧新旧渲染一致（C34，不接受近似）；可编辑内容同等
  操作后对比；视频走 §3.3 规则。
- 每个 P3-n 任务附带：往返/幂等回归 + 新增样本 + PAGFullTest 全量绿。
