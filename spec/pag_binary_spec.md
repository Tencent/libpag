# PAG 二进制格式规范（PAG v4）

> 版本 v1.0-draft1 ｜ 日期 2026-10-22 ｜ 状态：M0 编写中（§1–§11 已定稿，含全部一、二期 Tag）
>
> 依据：`docs/pagx_to_pag_design.md`（v1.0-rc）§3–§5、附录 A/B；旧格式实现 `src/codec/`。
> 本文是 PAG v4 的字节级规范。设计文档描述"为什么"，本文描述"是什么"：每个 Tag 的编号、
> 字段顺序、类型、默认值、存在位，以及基础类型的字节布局。一、二期的 Tag 在本文全部定稿；
> 三期能力只划定编号区间与结构框架，字段在三期详细设计时补全（发版前均可修订，§12 设计文档）。

## 0. 术语与总则

- **字节序**：多字节标量一律小端（Little-Endian），与旧格式一致。
- **位序**：位打包值在字节内从最低位开始填充（LSB-first），与旧格式 `EncodeStream::writeUBits` 一致。
- **varint**：本文写作 `varuint`/`varint`，指旧格式 `EncodedUint`/`EncodedInt` 系列的可变长编码（§3.2）。
- **存在位**（exist bit）：属性块中每个可选字段占 1 位，表示该字段的值是否出现在值区。
- **默认值**：字段等于默认值时存在位为 0、不写值区；浮点可选字段的 NaN 视为"未设置"，与默认值等价。
- **Tag**：一段带编号与长度前缀的数据。编号即 `TagCode`（附录 A）。
- **Block**：内容为子 Tag 流的容器 Tag，以 `End`（编号 0）结束。
- **文档**：根文档或某个 SubDocument，各自拥有独立的节点序号空间（§5.1）。
- **确定性**：同一输入、同一工具版本导出的文件必须逐字节相同（设计文档 §4.1）；本文所有
  "按声明顺序""按首次引用顺序"的规定均为此服务。
- 本规范使用的读写原语（varint、位打包、列表打包、颜色、时间）与旧格式 `EncodeStream`/`DecodeStream`
  的语义逐位一致；新格式的读写函数按 PAGX 类型重新实现（设计文档 §2），但字节布局不变。

## 1. 文件布局

### 1.1 文件头（16 字节内完成识别）

与旧格式逐字节一致（设计文档 §3.3）：

```
偏移  大小  字段          说明
0     3    magic        'P' 'A' 'G'
3     1    version      u8，新格式固定为 4
4     4    bodyLength   u32 LE，主体字节数
8     1    compression  i8，新格式固定为 'U'（78，不压缩）
9     7    （保留）      全 0；读取时忽略
```

- 主体长度上限 4 GB（u32）。编码器在写文件头前算出主体长度，超出即报错；
  解码器读文件头后检查 `bodyLength` 不超过实际数据长度，否则按"文件被截断"报错。
- `version ≥ 5` 拒绝加载，返回"需要更高版本 SDK"。
- 识别规则（设计文档 §3.6）：前 3 字节为 `'PAG'` 按第 4 字节分流（≤3 旧 codec，4 新解码器）；
  跳过可选 UTF-8 BOM 与空白后首字符为 `'<'` 判定为 PAGX；其它为未知格式。按路径加载只读前 16 字节。

### 1.2 主体结构

```
Body := FileAttributes  StringTable  ResourceBlock  DocumentBlock  [SubDocumentTable]  ImageTable  End
```

- 顶层 Tag 顺序固定如上；`SubDocumentTable` 无外部子文档时省略。
- `End`（TagCode 0）表示主体结束。
- 主文档与全部子文档共用一个 `StringTable`、一个 `ResourceBlock`、一个 `ImageTable`（设计文档 §5.1）。
- 不写任何时间戳、机器标识等非确定内容。

### 1.3 顶层各 Tag 一览

| TagCode | 名称 | 内容 |
|---|---|---|
| 256 | FileAttributes | 文件级元数据（当前无必填字段，为后续扩展锚点） |
| 257 | StringTable | 字符串表（§6.2） |
| 258 | ResourceBlock | 资源表：子 Tag 流，每项一个资源 Tag（§6.3） |
| 259 | DocumentBlock | 根文档块（§6.4） |
| 260 | SubDocumentTable | 子文档表：子 Tag 流，每项一个 SubDocument（§6.5） |
| 261 | SubDocument | 单个外部子 PAGX 打包后的完整文档块（§6.5） |
| 262 | ImageTable | 图片表（§6.6） |
| 263 | DocumentAttributes | 根文档/子文档的属性块（width/height/data-*，附录 A 的文件级区间内新增） |
| 264 | RetainedIdTable | 保留 id 表（§9.0）：nodeIndex → id（StringRef），每个文档一张 |
| 0 | End | 结束标志，内容长度 0 |

## 2. TagHeader

与旧格式完全一致（`src/codec/TagHeader.cpp`）：

```
uint16 LE：高 10 位为 code，低 6 位为 length
length == 63 时，紧随一个 u32 LE 表示真实长度（扩展长度）
```

- code 上限 1023；0 保留给 End，1–255 留给旧格式，256–1022 为新格式，1023 保留。
- 解码器每读到一个 TagHeader 就更新 `tagLevel = max(tagLevel, code)`；
  `tagLevel > MaxSupportedTagLevel` 表示文件含有当前 SDK 不认识的 Tag（已被跳过）。
- 编码器保证单个 Tag 内容长度 ≥ 0；空内容 Tag（如无字段的 FileAttributes）长度为 0。

## 3. 基础类型

### 3.1 varuint（无符号可变长整数）

LEB128 风格，每字节低 7 位为有效位，最高位为续位（1 表示还有后续字节），小端逐字节排列：

```
value 的第 0–6 位放在第 1 个字节，第 7–13 位放在第 2 个字节，……最高有效字节续位为 0。
例：300 = 0b100101100 → 字节序列 { 0xAC, 0x02 }
```

### 3.2 varint（有符号可变长整数）

`varint(v) = varuint(|v| << 1 | (v < 0 ? 1 : 0))`，符号位在最低位（与旧 `EncodedInt` 一致，非 zigzag）。
例：`-5` → `|−5|<<1|1 = 11 = 0x0B` → 字节 `{ 0x0B }`。

### 3.3 位打包值（UBits / Bits）

- `UBits(value, n)`：把 value 的低 n 位按 LSB-first 写入当前位流。
- `Bits(value, n)`：有符号 n 位二进制补码，最高位为符号位。
- 跨字节拆分：从当前字节的当前位继续写，写满进下一字节。
- 进入/离开位流区域时调用 `alignWithBytes()`：位指针进位到字节边界，未满的低位补 0。

### 3.4 整型列表（Int32List / Uint32List）

`count == 0` 时只写 `UBits(0, 5)`。否则：

```
[5 位] numBits - 1        （numBits ∈ [1, 32]，为列表中所有值所需的最大位宽）
[count × numBits 位] 每个值依次 Bits/UBits 打包
```

- 有符号列表的位宽计算含符号位（`GetBitLength(int32)`：`min(31, bit_length(|v|)) + 1`）。

### 3.5 浮点列表（FloatList）

`FloatList(values, precision)`：`round(v / precision)` 转为 int32 列表后按 §3.4 打包。
`Point2DList` 等价于把各分量展开成一维后调用 `FloatList`。

### 3.6 标量

| 类型 | 编码 |
|---|---|
| float32 | 4 字节 IEEE 754 小端 |
| u8 / i8 | 1 字节 |
| bool（值区中的显式布尔） | 1 字节（0/1） |
| 枚举 | u8（全部 PAGX 枚举值域 < 256；未知值取默认值并告警） |

### 3.7 颜色（Color）

前导 1 字节类型：

```
type 0（sRGB）  : RGBA 各 1 字节，分量 = round(clamp(v, 0, 1) × 255)
type 1（扩展）  : colorSpace u8（0 = sRGB，1 = DisplayP3）+ RGBA 各 float32
```

- 颜色空间为 P3，或任一分量超出 [0,1] 时必须用 type 1；否则必须用 type 0（结果确定）。
- 解码侧 type 0 恢复为 sRGB 浮点颜色。

### 3.8 矩阵

| 类型 | 编码 |
|---|---|
| Matrix（2D） | 6 × float32，顺序 a, b, c, d, tx, ty（`include/pagx/types/Matrix.h`） |
| Matrix3D | 16 × float32，列主序 `values[16]`（`getRowColumn(r,c) = values[c*4+r]`，`include/pagx/types/Matrix3D.h`） |

单位矩阵作为默认值用存在位省略。

### 3.9 帧时间（Frame）

非负 int64 帧号，编码为 `varuint`（与旧 `WriteTime` 一致）。PAGX 的关键帧时间、动画时长均以帧为单位；
换算真实时间用所属 Animation 的 `frameRate`（float32，帧/秒）。时间锁定挂载的 `startTime`/`timeScale`
字段（§6.3 规范扩展，三期）倾向用秒（float32），在三期详细设计时定稿。

### 3.10 字符串

- 文件中所有字符串值一律存入 `StringTable`，使用处写 `varuint` 序号（0 基）。
- `StringTable` 条目：`varuint` 字节长度 + UTF-8 字节（无终结符）。
- 空 Dokument 共享空表；序号在编码时按首次出现顺序分配（确定性，§12）。

### 3.11 Padding

4 × float32，顺序 top、right、bottom、left（`include/pagx/types/Padding.h`）。

## 4. 属性块布局

节点属性 Tag 与 Block 的属性区采用统一布局（沿用旧 `AttributeHelper` 的设计，设计文档 §3.5）：

```
内容 := [flags 位流（逐字段 1 位存在位，声明顺序，LSB-first）] [值区（仅存在位为 1 的字段，按声明顺序）]
```

- flags 位流写完后 `alignWithBytes()`，值区从字节边界开始。
- **新格式每个字段只有 1 个存在位**：动画不进属性块（关键帧在 Channel Tag 中，二期），
  不存在旧格式的 animatable/hasSpatial 位。静态文件不为可动画性付出任何字节（设计文档 §5.4）。
- 布尔字段用 **BitFlag** 语义：存在位本身即值（1 = true，0 = false 且不写值区），要求默认值为 false。
  默认值为 true 的布尔改为显式 bool 字段（1 字节）。
- **固定字段（Fixed）**：无存在位、总是写值区（路径记录、计数字段、关键帧块等），字段表中标注 Fixed。
- 可选浮点字段的 NaN 视为默认值（存在位 0）；Inf 等非有限值导出时报错（设计文档 §5.6）。
- 枚举、u8、float32、颜色、矩阵、字符串引用、引用 varuint 等"简单字段"直接按 §3 编码进值区。
- **复合字段**（可选值列表、嵌套结构）用 Custom 语义：存在位之后在值区按本规范定义的子布局展开。
- **内联资源子 Tag 的位置**：属性 Tag 写在值区末尾（多个内联按其引用字段的出现顺序）；
  Block 写在属性子 Tag 之后、内容子 Tag 之前（§9.0）。
- 位打包不跨 Tag：每个 Tag 的内容以字节对齐开始与结束。

### 4.1 Block 的子 Tag 流

容器 Tag（LayerBlock、GroupBlock、TextBoxBlock、CompositionBlock、DocumentBlock、AnimationBlock 等）：

```
Block 内容 := [属性区（如 §4，可选，由该 Block 的定义规定）] [子 Tag 流] End
```

- 子 Tag 流中每个子 Tag 带完整 TagHeader；`End` 终止。
- 子 Tag 的顺序：先属性子 Tag（固定顺序），再内容子 Tag（按声明顺序），与各 Tag 定义一致。
- 解码器遇到不认识的 TagCode 按长度跳过（§4.2），遇到不认识的子 Tag 同样跳过。

### 4.2 未知 Tag 的跳过与健壮性

- 解码器维护 `MaxSupportedTagLevel`；不认识的 TagCode 按 TagHeader 长度跳过内容，继续读后续 Tag。
- 未知枚举值取默认值并告警；通道值类型与通道定义不符丢弃该通道并告警（二期）。
- 关键帧时间非递减校验、引用环校验、计数上限校验见 §11 与设计文档 §5.5。
- 空文档、零尺寸画布合法。

## 5. 引用模型（设计文档 §5.2 的字节级定义）

### 5.1 节点序号

- 每个文档独立编号。序号按**文件发射顺序**从 0 分配（先序遍历的确定性等价形式）：
  ① ResourceBlock 内的资源按表序先编号（表序 = 文档树先序遍历中的首次引用顺序），每个资源的子树随资源一起编完；
  ② 之后 DocumentBlock 的节点按先序编号，内联资源子树在其引用位置编号，已编号的共享资源不重复编号。
- 编码分两趟、无循环依赖：先定资源表与序号，再生成引用字节；解码时按读取顺序重建"序号 → 节点"表，
  全部读完后统一回填引用与 RetainedIdTable。

### 5.2 引用编码

| 引用 | 字段类型 | 编码 |
|---|---|---|
| 资源（颜色源/路径/图片/字体/内部 Composition） | ResourceRef | `varuint`：0 = 内联子 Tag；k ≥ 1 = ResourceBlock 第 k−1 项 |
| 图层引用（遮罩） | NodeRef | `varuint`：0 = 无；k ≥ 1 = 本文档节点序号 k−1 |
| 外部 Composition | SubDocRef | `varuint`：SubDocumentTable 序号 |
| 图片数据 | ImageRef | `varuint`：k ≥ 1 = ImageTable 第 k−1 项；0 = 无图（缺失告警后的空 Image） |
| 字符串（动画目标、通道名、id、data-* 等） | StringRef | `varuint`：StringTable 序号 |
| 关键帧图片值（ImageRef 值类型） | ImageRef | 同"图片数据" |

### 5.3 内联与共享

- 被两处及以上引用的颜色源、路径、图片、字体、内部 Composition 进 ResourceBlock；只用一处内联。
- 规则只看引用次数，与原文件是否写 id 无关；统计在剥离后进行，结果确定。
- 内联资源写在引用者 Tag 的固定位置（各 Tag 定义中标注"内联子 Tag"）；共享资源在 ResourceBlock 中
  按**首次引用顺序**排列。
- ResourceBlock 内的 CompositionBlock 是完整块（含图层子 Tag 流）。

### 5.4 作用域

- 资源引用、遮罩、动画目标、DataBind 目标只在所在文档内解析；跨文档引用只有
  Layer → SubDocument 一种（SubDocRef）。
- 引用回填时用 `nodeType()` 校验引用类型；检测引用环、遮罩环、合成环；嵌套深度上限见 §11。

## 6. 顶层 Tag 定义

### 6.1 FileAttributes（256）

属性块，当前版本无字段（flags 为空，内容长度 0）。为后续文件级扩展（加密、性能提示等）保留锚点。

### 6.2 StringTable（257）

```
varuint count
重复 count 次： varuint byteLength  +  UTF-8 字节
```

条目按首次引用顺序排列；主文档与子文档共用。

### 6.3 ResourceBlock（258）

Block：子 Tag 流，每项一个资源 Tag（288 Image、289 PathData、290 SolidColor、291 LinearGradient、
292 RadialGradient、293 ConicGradient、294 DiamondGradient、295 ImagePattern、296 Font、
297 CompositionBlock），以 End 结束。资源顺序 = 首次引用顺序。

### 6.4 DocumentBlock（259）与 DocumentAttributes（263）

```
DocumentBlock 内容 := DocumentAttributes(263)  [RetainedIdTable(264)]  [根图层 Tag…]
                      [根作用域 Animation/StateMachine（二期）] [ViewModel/DataBind（二期）]  End
DocumentAttributes 属性块字段（声明顺序）：
  | 字段 | 类型 | 默认值 | 说明 |
  | width | float32 | 0 | 画布宽（PAGXDocument.width 为 float，保持往返一致） |
  | height | float32 | 0 | 画布高 |
  | dataKeys | StringRef[] | 空 | 文档根 data-* 的键（C27 保留），与 values 一一对应 |
  | dataValues | StringRef[] | 空 | 文档根 data-* 的值 |
```

- 根图层 Tag 顺序 = 声明顺序（Layer/Group/TextBox 混排，按文档 children 顺序）。
- data-* 键值各自成 varuint 数组（先 count 后元素），保持声明顺序。

### 6.5 SubDocumentTable（260）与 SubDocument（261）

```
SubDocumentTable 内容 := SubDocument(261)…  End          （每项一个，顺序 = 首次引用顺序）
SubDocument 内容 := varuint diagnosticName（StringRef，原始相对路径，仅诊断用）
                    DocumentAttributes(263)  [RetainedIdTable(264)]  [子文档根图层…]  [时间线/VM（二期）]  End
```

- 同一外部文件只打包一份；引用 Layer 记 SubDocRef（§5.2）。
- 解码时每个引用 Layer 各自解码一份独立 PAGXDocument（设计文档 §4.8），展开上限见 §11。

### 6.6 ImageTable（262）

```
varuint count
重复 count 次（条目顺序 = 首次引用顺序）：
  u8 mediaType          （0 = 静态 WebP；1 = 位图字形原图；2+ 视频序列/位图序列，三期 §6.6）
  varuint byteLength
  byte[byteLength] data 图片/字形字节（一期不再重新编码已是 WebP 且无需缩放的图，§4.5）
  u32 logicalWidth      逻辑宽
  u32 logicalHeight     逻辑高
  float32 scaleFactor   缩放因子（≤ 1；内容被缩小的比例）
  float32 offsetX       内容偏移 X（裁掉透明边后的偏移）
  float32 offsetY       内容偏移 Y
```

- 位图字形条目不做缩放/裁边/转码，`scaleFactor = 1`、`offset = 0`，逻辑尺寸 = 原图尺寸。
- Image 节点（Tag 288）通过 ImageRef 指向条目；渲染时按逻辑尺寸适配、以 1/scaleFactor 缩放并平移。
- 媒体类型与各期实现范围：一期只产生 0/1；解码器遇到未知 mediaType 跳过该条目并告警，引用它的
  Image 按空图处理。

## 7. 路径编码（PathData）

编码 `pagx::PathData`（verbs + points）。沿用旧 `WritePath` 的"退化记录 + 打包点列"思路
（`src/codec/DataTypes.cpp`），两处变化：记录位宽 3 → 4 位（新增 Quad），量化步长自适应
（步长指数，设计文档 §4.7）。

### 7.1 记录类型（4 位）

| 值 | 记录 | 坐标数 | 语义 |
|---|---|---|---|
| 0 | Close | 0 | 闭合 |
| 1 | Move | 2 | 起点 (x, y) |
| 2 | Line | 2 | 线到 (x, y) |
| 3 | HLine | 1 | 水平线到 x（y 沿用） |
| 4 | VLine | 1 | 垂直线到 y（x 沿用） |
| 5 | Quad | 4 | 二次贝塞尔：控制点 (x, y) + 终点 (x, y) |
| 6 | Curve01 | 4 | 三次贝塞尔，control1 = 当前点：control2 + 终点 |
| 7 | Curve10 | 4 | 三次贝塞尔，control2 = 终点：control1 + 终点 |
| 8 | Curve11 | 6 | 三次贝塞尔：control1 + control2 + 终点 |
| 9–15 | 保留 | — | 解码器按未知记录处理：丢弃该路径并告警 |

### 7.2 布局

```
varuint numVerbs               动词数（0 = 空路径，到此结束）
u8 stepExponent                量化步长指数 e，步长 = 0.05 × 2^(−e)（见下）
[numVerbs × 4 位] 记录类型，按动词顺序 LSB-first 打包
alignWithBytes
[5 位] numBits − 1              坐标列表的打包位宽（Int32List 语义，§3.4）
[N × numBits 位] 坐标，Bits 打包，值 = round(坐标 / 步长)
```

- 坐标为**绝对坐标**，一维展开顺序 = 各记录按 §7.1 的坐标顺序串联。
- **步长规则**（设计文档 §4.7）：`e = max(0, ceil(log2(该节点的最大显示缩放)))`，上限 255。
  最大显示缩放 ≤ 1 时 e = 0，步长 0.05，与旧 PAG 完全一致；缩放 3 → e = 2 → 步长 0.0125。
  最大显示缩放含全部引用实例与关键帧（与图片缩放同一套计算）。
- 字形轮廓（Font 内）按字体单位，固定 e = 0。
- 一条路径内所有坐标共用一个 numBits；round 为四舍五入（`roundf`）。

## 8. 关键帧编码（二期 Tag 使用，布局在此定稿）

沿用旧格式列式布局（设计文档 §5.3），按 PAGX 点式关键帧实现：

```
varuint numKeys                    关键帧数（上限见 §11）
[numKeys × 2 位] 插值类型           None=0 / Linear=1 / Bezier=2 / Hold=3
[numKeys × varuint] 时间            帧号，必须非递减，否则丢弃该通道并告警
[numKeys × 值] 值列表               按 ChannelValueType 列式编码（§8.1）
[FloatList(0.005)] 贝塞尔控制点     每个 Bezier 关键帧 4 个 float：bezierOut.x, bezierOut.y,
                                   bezierIn.x, bezierIn.y（按关键帧时间顺序）
```

插值类型位流写完后 `alignWithBytes()` 再写时间列表；贝塞尔列表以 5 位 numBits 前缀开始（§3.4），
Tag 内容在字节边界结束。

### 8.1 值类型的列式编码（ChannelValueType）

| 值类型 | 值列表编码 |
|---|---|
| Float | numKeys × float32 |
| Int | Int32List（§3.4） |
| Bool | numKeys × 1 位 |
| String | numKeys × varuint（StringRef） |
| Color | numKeys × 颜色（§3.7） |
| Matrix | numKeys × 6 float32 |
| ImageRef | numKeys × varuint（ImageTable 引用，0 = 无） |
| Path（§6.1 规范扩展） | numKeys × PathData（§7，各自带完整头） |

- PAGImage 值只在运行时出现，不写入文件。
- 贝塞尔控制点描述时间缓动（时间 → 进度），与值类型无关；颜色、矩阵等多分量值按同一进度逐分量插值。
- 通道名（含 `point{i}.x/y` 动态名）原样存 StringTable。

## 9. 节点 Tag 定义

字段清单依据：`docs/pagx_to_pag_fields/` 五份提取报告（逐字段核对过头文件与通道表）。
一、二期 Tag 在此全部定稿（§14 终审条件 3）；三期能力只占用 576–767 编号区间（设计文档附录 A）。

### 9.0 通用规则

- 字段表按**声明顺序**排列 = 存在位顺序（flags 内 LSB-first）。
- 布尔：默认 false → BitFlag（存在位即值）；默认 true → 显式 bool（1 字节）。
- **固定字段（Fixed）**：无存在位、总是写值区（路径记录、计数字段、关键帧块等），表中标注 Fixed。
- 可空指针（ColorSource*/Image*/Font*/DataConverter*/ViewModel*）：存在位表示"非空"，随后写引用。
- **内联资源子 Tag 位置**：属性 Tag 写在值区末尾（多个内联按引用字段出现顺序）；Block 写在属性子 Tag 之后、内容子 Tag 之前。
- **保留 id 表（RetainedIdTable，TagCode 264）**：节点 Tag 不携带 id 字段。剥离后仍需按字符串查找的节点（Layer id、动画/DataBind 目标、Animation/StateMachine id，§5.2）由该表统一记录。每个文档（根文档与每个 SubDocument）各一张：`varuint count` + count × { `varuint nodeIndex`, `varuint id`(StringRef) }，按 nodeIndex 升序。解码器据此恢复 findNode 语义。AnimationObject.target、DataBind.target、Timeline.animationId、State.animationId 等按字符串原样存储（StringRef），运行时查这张表。
- 引用编码汇总：ResourceRef（varuint，0 = 内联，k = 资源表第 k−1 项）、NodeRef（varuint，0 = 无，k = 本文档节点序号 k−1）、SubDocRef（varuint，SubDocumentTable 序号，0 基）、StringRef、ImageTableRef（varuint，0 = 无数据）。
- PAGX 头文件枚举为隐式赋值（声明顺序）；§9.10 的数值是**二进制格式的固定映射**，与后续头文件改动解耦。
- NodeRef 使用处：Layer.mask、ViewModelProperty.dataConverter、ViewModelProperty.viewModelRef。

### 9.1 资源 Tag（ResourceBlock 内或内联）

**288 Image**（属性 Tag）：imageTableRef（varuint，默认 0；ImageTable 序号，0 = 无数据，导出时图片缺失 §4.5）。

**289 PathData**（属性 Tag）：path（§7 路径布局，Fixed 无存在位；numVerbs=0 即空路径）。

**290 SolidColor**：color（§3.7 颜色，默认不透明黑）。

**291–294 渐变**（LinearGradient / RadialGradient / ConicGradient / DiamondGradient）

各自身有字段（存在位）：
- 291 Linear：startPoint（2×f32，默认 {0,0}）、endPoint（2×f32，默认 {1,0}）
- 292 Radial：center（2×f32，{0.5,0.5}）、radius（f32，0.5）
- 293 Conic：center（2×f32，{0.5,0.5}）、startAngle（f32，0）、endAngle（f32，360）
- 294 Diamond：center（2×f32，{0.5,0.5}）、radius（f32，0.5）

公共字段（跟在各自身有字段之后）：
- matrix：存在位 + 6×f32（默认单位阵）
- fitsToGeometry：显式 bool（默认 true）
- stopCount：varuint（Fixed，0 合法）
- stopOffsets：FloatList(GRADIENT_PRECISION 0.00002)，count = stopCount（列式，§4.7）
- stopColors：stopCount × §3.7 颜色（列式，不用存在位）

**295 ImagePattern**

| # | 字段 | 类型 | 默认 |
|---|---|---|---|
| 1 | image | 存在位 + ResourceRef | null（0 = 内联 Image 子 Tag） |
| 2 | tileModeX | u8 | Decal(3) |
| 3 | tileModeY | u8 | Decal(3) |
| 4 | filterMode | u8 | Linear(1) |
| 5 | mipmapMode | u8 | Linear(2) |
| 6 | matrix | 存在位 + 6×f32 | 单位阵 |
| 7 | scaleMode | u8 | LetterBox(2) |
| 8 | dataKeys | 存在位 + varuint count + StringRef×count | 空（C27 保留） |
| 9 | dataValues | 同上，与 keys 一一对应、同序 | 空 |

**296 Font**：unitsPerEm（varuint，默认 1000）、glyphCount（varuint，Fixed）、glyphCount 个字形条目。
字形条目（GlyphID = 序号 + 1，0 保留给缺字形）：
`u8 flags（bit0 hasPath，bit1 hasImage，其余 0）`、`f32 advance`、`f32 offsetX`、`f32 offsetY`、
`[hasImage] varuint imageTableRef`、`[hasPath] §7 路径布局（stepExponent 固定 0，字体单位，步长 0.05）`。
指向共享 PathData 资源的字形轮廓：导出器解析为其内容并内联（病态场景接受少量重复，避免字形条目引入第二种引用编码）。

**297 CompositionBlock + 298 CompositionAttributes**（内部 Composition；文档节点空间的一部分，非独立文档）
298 字段：width（f32，0）、height（f32，0）。
297 内容：`[298] [图层 Tag…] [Animation/StateMachine（二期）] [ViewModelBlock / DataBind（二期）] End`。

**299 ViewModelBlock**（二期）：内容 = `[ViewModelProperty 300…] End`。无属性。

**300 ViewModelProperty**（二期）：name（StringRef，""）、propertyType（u8，Number=0）、dataConverter（存在位+NodeRef，0）、viewModelRef（存在位+NodeRef，0）、defaultNumber（f32，0）、minValue（存在位+f32）、maxValue（存在位+f32）、defaultString（StringRef，""）、defaultBoolean（BitFlag，false）、defaultColor（存在位+颜色，默认不透明黑）、defaultImage（存在位+ResourceRef，null；0 = 内联 Image 子 Tag）、enumOptions（存在位 + varuint count + StringRef×count）、defaultEnum（varint，0）、dataKeys/dataValues（存在位，同 ImagePattern，C27 保留）。

**301 DataConverter**（二期）：converterType（StringRef，""）、params（存在位 + varuint count + count × {StringRef key, StringRef value}，按键名升序 = std::map 迭代序，确定性）。

### 9.2 图层与容器

**352 LayerBlock + 353 LayerAttributes + 354 LayerTransform**

353 字段（声明顺序）：
| # | 字段 | 类型 | 默认 | 备注 |
|---|---|---|---|---|
| 1 | name | StringRef | "" | |
| 2 | x | f32 | 0 | 渲染值（扁平化写回） |
| 3 | y | f32 | 0 | 渲染值 |
| 4 | visible | bool 显式 | true | |
| 5 | alpha | u8 | 255 | 0–255（§4.7） |
| 6 | blendMode | u8 | Normal(0) | |
| 7 | matrix | 存在位 + 6×f32 | 单位阵 | |
| 8 | matrix3D | 存在位 + 16×f32（列主序） | 单位阵 | |
| 9 | preserve3D | BitFlag | false | |
| 10 | antiAlias | bool 显式 | true | |
| 11 | groupOpacity | bool 显式 | true | |
| 12 | passThroughBackground | bool 显式 | true | |
| 13 | width | 存在位 + f32 | NaN | 渲染需要时写回的布局尺寸 |
| 14 | height | 存在位 + f32 | NaN | |
| 15 | scrollRect | 存在位 + 4×f32 | 无 | x,y,w,h；存在即应用 |
| 16 | mask | varuint NodeRef | 0 | 0 = 无蒙版 |
| 17 | maskType | u8 | Alpha(0) | |
| 18 | hasComposition | BitFlag | false | 内部合成 |
| 19 | compositionRef | ResourceRef（hasComposition 时） | — | 0 = 内联 CompositionBlock |
| 20 | hasSubDocument | BitFlag | false | 外部子文档 |
| 21 | subDocRef | varuint（hasSubDocument 时） | — | SubDocumentTable 序号，0 基 |
| 22 | vmContext | StringRef | "" | |

Layer 的 id 不在 Tag 中，见 RetainedIdTable（§9.0）。

354 LayerTransform（可选属性子 Tag，任一字段非默认才写）：
anchor（2×f32，{0,0}）、rotation（f32，0）、scale（2×f32，{1,1}）、skew（f32，0）、skewAxis（f32，0）、
startTime（f32，0；§6.3 时间锁定，三期才写）、timeScale（f32，1；同前）。
优先级 matrix3D > matrix > 本 Tag（§6.2）。

352 内容：`[353] [354?] [内联 CompositionBlock?] [contents…] [styles…] [filters…] [children…] [timelines…（二期）] End`。
同类内按声明顺序，解码器按 TagCode 归类。

**433 GroupBlock + 434 GroupAttributes**

434 字段：anchor（2×f32，{0,0}）、position（2×f32，{0,0}，渲染值）、rotation（f32，0）、scale（2×f32，{1,1}）、
skew（f32，0）、skewAxis（f32，0）、alpha（u8，255）、width（存在位+f32，NaN）、height（存在位+f32，NaN）。
433 内容：`[434] [elements…] End`。

**431 TextBoxBlock + 432 TextBoxAttributes**

432 字段 = 434 全部 + padding（存在位 + 4×f32，全 0 省略；排版输入，保留）+ textAlign（u8，Start=0）、
paragraphAlign（u8，Near=0）、writingMode（u8，Horizontal=0）、lineHeight（f32，0）、wordWrap（bool 显式，true）、
overflow（u8，Visible=0）。
431 内容：`[432] [elements…] End`。

**420 TextBlock + 421 TextAttributes + 422 GlyphRun**

421 字段：position（2×f32，{0,0}；渲染值，按 `Text::renderPosition` 反解写回，§4.2）、width（存在位+f32，NaN；
布局适配缩放写回）、height（存在位+f32，NaN）、text（StringRef，""）、fontFamily（StringRef，""）、
fontStyle（StringRef，""）、fontSize（f32，12）、letterSpacing（f32，0）、fauxBold（BitFlag，false）、
fauxItalic（BitFlag，false）、textAnchor（u8，Start=0）、baseline（u8，LineBox=0）。
420 内容：`[421] [GlyphRun 422…（仅源文件已嵌入字形时）] End`。

422 GlyphRun 字段（列顺序固定，§5.6）：
| # | 字段 | 类型 | 默认 | 备注 |
|---|---|---|---|---|
| 1 | font | 存在位 + ResourceRef | null | 0 = 内联 Font 子 Tag（值区末尾） |
| 2 | fontSize | f32 | 12 | |
| 3 | glyphCount | varuint（Fixed） | 0 | |
| 4 | glyphIds | glyphCount × uint16，§3.4 位打包（5 位 numBits 前缀） | — | Fixed |
| 5 | x | f32 | 0 | |
| 6 | y | f32 | 0 | |
| 7 | xOffsets | 存在位 + FloatList(0.05)，count = glyphCount | 空 | 空 = 全 0（§4.7） |
| 8 | positions | 存在位 + Point2DList(0.05) | 空 | 空 = 全 0 |
| 9 | anchors | 存在位 + Point2DList(0.05) | 空 | 空 = 默认锚点 |
| 10 | scales | 存在位 + varuint count(=glyphCount) + 2×count×f32 | 空 | 空 = (1,1)；标量列，§4.7 |
| 11 | rotations | 存在位 + varuint count + count×f32 | 空 | 空 = 0 |
| 12 | skews | 存在位 + varuint count + count×f32 | 空 | 空 = 0 |
| 13 | bounds | 存在位 + 4×f32 | 无 | 仅每 Text 首个 GlyphRun 携带 |

### 9.3 矢量元素与画笔（Layer/Group/TextBox 的内容子 Tag）

- **416 Rectangle**：position.x（存在位+f32，NaN）、position.y（存在位+f32，NaN）、size.width（f32，0）、
  size.height（f32，0）、roundness（f32，0）、reversed（BitFlag，false）。
- **417 Ellipse**：position.x/y（存在位+f32，NaN）、size.width/height（f32，0）、reversed（BitFlag）。
- **418 Polystar**：position.x/y（存在位+f32，NaN）、type（u8，Star=1）、pointCount（f32，5）、
  outerRadius（f32，100）、innerRadius（f32，50）、rotation（f32，0）、outerRoundness（f32，0）、
  innerRoundness（f32，0）、reversed（BitFlag）。
- **419 Path**：data（存在位 + ResourceRef，null；0 = 内联 PathData 子 Tag，值区末尾）、position（2×f32，{0,0}，渲染值）、
  reversed（BitFlag，false）。
- **423 Fill**：color（存在位 + ResourceRef，null）、alpha（u8，255）、blendMode（u8，Normal=0）、
  fillRule（u8，Winding=0）、placement（u8，Background=0）。
- **424 Stroke**：color（存在位 + ResourceRef，null）、width（f32，1）、alpha（u8，255）、blendMode（u8）、
  cap（u8，Butt=0）、join（u8，Miter=0）、miterLimit（f32，4）、dashes（存在位 + varuint count + count×f32；空 = 实线）、
  dashOffset（f32，0）、dashAdaptive（BitFlag，false）、align（u8，Center=0）、placement（u8，Background=0）。
- **425 TrimPath**：start（f32，0）、end（f32，1）、offset（f32，0）、type（u8，Separate=0）。
- **426 RoundCorner**：radius（f32，10）。
- **427 MergePath**：mode（u8，Append=0）。
- **430 TextPath**：path（存在位 + ResourceRef，null）、baselineOrigin（2×f32，{0,0}，渲染值）、baselineAngle（f32，0）、
  firstMargin（f32，0）、lastMargin（f32，0）、perpendicular（bool 显式，true）、reversed（BitFlag，false）、
  forceAlignment（BitFlag，false）。
- **435 Repeater**：copies（f32，3）、offset（f32，0）、order（u8，BelowOriginal=0）、anchor（2×f32，{0,0}）、
  position（2×f32，{100,100}）、rotation（f32，0）、scale（2×f32，{1,1}）、startAlpha（u8，255）、endAlpha（u8，255）。

### 9.4 修改器

- **428 TextModifierBlock + 436 TextModifierAttributes**：436 字段：anchor（2×f32，{0,0}）、position（2×f32，{0,0}）、
  rotation（f32，0）、scale（2×f32，{1,1}）、skew（f32，0）、skewAxis（f32，0）、alpha（u8，255）、
  fillColor（存在位 + 颜色）、strokeColor（存在位 + 颜色）、strokeWidth（存在位 + f32）。
  428 内容：`[436] [RangeSelector 429…] End`。
- **429 RangeSelector**：start（f32，0）、end（f32，1）、offset（f32，0）、unit（u8，Percentage=1）、shape（u8，Square=0）、
  easeIn（f32，0）、easeOut（f32，0）、mode（u8，Add=0）、weight（f32，1）、randomOrder（BitFlag，false）、
  randomSeed（varint，0）。

### 9.5 样式与滤镜（Layer 的 styles/filters 子 Tag）

- **355 DropShadowStyle**：blendMode（u8，Normal=0）、excludeChildEffects（BitFlag，false）、offsetX（f32，0）、
  offsetY（f32，0）、blurX（f32，0）、blurY（f32，0）、color（颜色，默认不透明黑）、showBehindLayer（bool 显式，true）。
- **356 InnerShadowStyle**：blendMode、excludeChildEffects、offsetX/offsetY/blurX/blurY（f32，0）、color。
- **357 BackgroundBlurStyle**：blendMode、excludeChildEffects、blurX/blurY（f32，0）、tileMode（u8，Mirror=2）。
- **358 NoiseStyle**：blendMode、excludeChildEffects、mode（u8，Mono=0）、size（f32，4）、density（f32，0.5）、
  seed（f32，0）、color（不透明黑）、firstColor（不透明黑）、secondColor（不透明白 {1,1,1,1}）、
  opacity（u8，round(0.15×255)=38）。
- **359 GlassStyle**：blendMode、excludeChildEffects、refraction（f32，80）、depth（f32，20）、frost（f32，5）、
  dispersion（f32，50）、splay（f32，0）、lightAngle（f32，45）、lightIntensity（f32，80）。
- **360 BlurFilter**：blurX/blurY（f32，0）、tileMode（u8，Decal=3）。
- **361 DropShadowFilter**：offsetX/offsetY/blurX/blurY（f32，0）、color（不透明黑）、shadowOnly（BitFlag，false）。
- **362 InnerShadowFilter**：同 361。
- **363 BlendFilter**：color（不透明黑）、blendMode（u8，Normal=0）。
- **364 ColorMatrixFilter**：matrix（存在位 + 20×f32；默认单位阵，行主序
  [1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0]；无通道，不可动画）。
- **365 NoiseFilter**：mode（u8，Mono=0）、size（f32，4）、density（f32，0.5）、seed（f32，0）、
  blendMode（u8，Normal=0）、color、firstColor、secondColor、opacity（u8，38）。

### 9.6 时间线（二期，LayerBlock 子 Tag）

- **366 AnimationTimeline**：animationId（StringRef，""）、playing（bool 显式，true）、
  evaluationOffset（varint，0；帧数，可为负，语义同旧 compositionStartTime）。
- **367 StateMachineTimeline**：stateMachineId（StringRef，""）。

### 9.7 动画与状态机（二期）

- **480 AnimationBlock + 489 AnimationAttributes**：489：duration（varuint，0）、frameRate（f32，60）、
  loop（u8，Once=0）。480 内容：`[489] [AnimationObjectBlock 481…] End`。
- **481 AnimationObjectBlock + 492 AnimationObjectAttributes**：492：target（StringRef，""；按字符串查找，§5.2）。
  481 内容：`[492] [Channel 482…] End`。
- **482 Channel**：name（StringRef，""）、valueType（u8，ChannelValueType）、keyframes（§8 布局，Fixed 无存在位）。
- **483 StateMachineBlock**：内容：`[StateMachineInput 484…] [StateRegionBlock 485…] End`。无属性。
- **484 StateMachineInput**：name（StringRef，""）、type（u8，Bool=0）、defaultBool（BitFlag，false）、
  defaultNumber（存在位+f32，0）。
- **485 StateRegionBlock + 490 StateRegionAttributes**：490：name（StringRef，""）、initialState（StringRef，""）。
  485 内容：`[490] [State 486…] [StateTransition 487…] End`。
- **486 State**：stateType（u8，Animation=0）、name（StringRef，""）、animationId（StringRef，""；空 = 空状态）。
- **487 StateTransition + 491 StateTransitionAttributes**：491：from（StringRef，""；"any" 匹配任意状态）、
  to（StringRef，""）、duration（varuint，0）、frameRate（f32，60）、interpolation（u8，Linear=1）、
  bezierOut（2×f32，{0,0}）、bezierIn（2×f32，{0,0}）、enableEarlyExit（BitFlag，false）、pauseOnExit（BitFlag，false）。
  487 内容：`[491] [TransitionCondition 488…] End`。
- **488 TransitionCondition**：inputName（StringRef，""）、op（u8，Equal=0）、valueBool（BitFlag，false）、
  valueNumber（存在位+f32，0）。

### 9.8 ViewModel 与数据绑定（二期）

- **544 DataBind**：source（StringRef，""）、target（StringRef，""；按字符串查找）、channel（StringRef，""）、
  direction（u8，ToTarget=0）。声明在 Composition/Document 块内，保持声明顺序（§5.2）。
- 299/300/301 见 §9.1。

### 9.9 新增 TagCode（相对设计文档附录 A）

M0 规范新增（附录 A 以本规范为准，设计文档附录 A 在 M0 收尾时同步）：
263 DocumentAttributes、264 RetainedIdTable、436 TextModifierAttributes、489 AnimationAttributes、
490 StateRegionAttributes、491 StateTransitionAttributes、492 AnimationObjectAttributes。

### 9.10 枚举取值表（u8 编码，二进制固定映射）

| 枚举 | 取值 |
|---|---|
| BlendMode | Normal=0, Multiply=1, Screen=2, Overlay=3, Darken=4, Lighten=5, ColorDodge=6, ColorBurn=7, HardLight=8, SoftLight=9, Difference=10, Exclusion=11, Hue=12, Saturation=13, Color=14, Luminosity=15, PlusLighter=16, PlusDarker=17 |
| MaskType | Alpha=0, Luminance=1, Contour=2（反相扩展 §6.4 三期：3–5 预留 alphaInverted/luminanceInverted/contourInverted） |
| TileMode | Clamp=0, Repeat=1, Mirror=2, Decal=3 |
| FilterMode / MipmapMode | Nearest=0, Linear=1 / None=0, Nearest=1, Linear=2 |
| ScaleMode | None=0, Stretch=1, LetterBox=2, Zoom=3 |
| NoiseMode | Mono=0, Duo=1, Multi=2 |
| FillRule / LayerPlacement | Winding=0, EvenOdd=1 / Background=0, Foreground=1 |
| LineCap / LineJoin / StrokeAlign | Butt=0, Round=1, Square=2 / Miter=0, Round=1, Bevel=2 / Center=0, Inside=1, Outside=2 |
| TrimType / MergePathMode | Separate=0, Continuous=1 / Append=0, Union=1, Intersect=2, Xor=3, Difference=4 |
| RepeaterOrder / PolystarType | BelowOriginal=0, AboveOriginal=1 / Polygon=0, Star=1 |
| TextAlign / TextAnchor / TextBaseline | Start=0, Center=1, End=2, Justify=3 / Start=0, Center=1, End=2 / LineBox=0, Alphabetic=1 |
| WritingMode / ParagraphAlign / Overflow | Horizontal=0, Vertical=1 / Near=0, Middle=1, Far=2 / Visible=0, Hidden=1 |
| SelectorUnit / SelectorShape / SelectorMode | Index=0, Percentage=1 / Square=0, RampUp=1, RampDown=2, Triangle=3, Round=4, Smooth=5 / Add=0, Subtract=1, Intersect=2, Min=3, Max=4, Difference=5 |
| ColorSpace | SRGB=0, DisplayP3=1（§3.7 type 1 的颜色空间字节） |
| LoopMode / KeyframeInterpolationType | Once=0, Loop=1, PingPong=2 / None=0, Linear=1, Bezier=2, Hold=3 |
| ChannelValueType | Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7（新增 path=8，§6.1） |
| TimelineType / StateType | Animation=0, StateMachine=1 / Animation=0 |
| StateMachineInputType | Bool=0, Number=1, Trigger=2 |
| TransitionConditionOp | Equal=0, NotEqual=1, LessThan=2, LessThanOrEqual=3, GreaterThan=4, GreaterThanOrEqual=5, Trigger=6 |
| ViewModelPropertyType | Number=0, String=1, Boolean=2, Color=3, Image=4, ViewModel=5, Enum=6, Trigger=7 |
| DataBindDirection | ToTarget=0, ToSource=1, TwoWay=2, Once=3 |

## 10. 逐字节示例

以下示例覆盖终审条件 1 要求的全部七类结构。所有多字节标量小端；位打包 LSB-first；
每个 Tag 的内容以字节对齐开始与结束（位打包不跨 Tag）。标注"存在位"的位流按声明顺序 LSB-first。

### 10.1 属性块（含可选值）—— Rectangle

Rectangle：position = (50, 40)，size = (100, 80)，roundness = 12，reversed = false。

- 存在位（6 位）：pos.x=1, pos.y=1, size.w=1, size.h=1, roundness=1, reversed=0 → `0b00011111` = `1F`
- 值区（仅存在位为 1 的字段）：50.0f=`00 00 48 42`，40.0f=`00 00 20 42`，100.0f=`00 00 C8 42`，
  80.0f=`00 00 A0 42`，12.0f=`00 00 40 41`
- Tag 内容 = `1F 00 00 48 42 00 00 20 42 00 00 C8 42 00 00 A0 42 00 00 40 41`（21 字节）
- TagHeader：code 416 → 416<<6 = 0x6800，长度 21 < 63 → 0x6800|21 = 0x6815 → 小端 `15 68`

### 10.2 字符串表

字符串 "logo"（4 字节）与 "标题文字"（12 字节 UTF-8）：

- count = `02`；条目 1 = `04 6C 6F 67 6F`；条目 2 = `0C E6 A0 87 E9 A2 98 E6 96 87 E5 AD 97`
- 内容 19 字节；TagHeader：257<<6 = 0x4040，|19 → 0x4053 → `53 40`

### 10.3 资源表与内联

Fill（423）引用红色 SolidColor（颜色 = type 0 sRGB，RGBA = FF 00 00 FF）：

**只用一处 → 内联**：Fill 存在位 bit0（hasColor）=1 → `01`；值区 colorRef = 0（内联）→ `00`；
值区末尾内联 SolidColor Tag（其自身：flags `01` + 颜色 `00 FF 00 00 FF`，6 字节内容，头 `86 48`）：
`CA 69  01 00  86 48 01 00 FF 00 00 FF`（Fill 内容 10 字节，头 = 423<<6|10 = 0x69CA）。

**被两处引用 → 资源表**：SolidColor 进 ResourceBlock（一次），两个 Fill 各写 `01 01`
（colorRef = 1 = 资源表第 0 项），头 = 0x69C2 → `C2 69`。

### 10.4 关键帧块 —— Channel

Channel(482)：name = "alpha"（StringRef 5），Float，2 个关键帧：t=0 值 1.0（Linear）、t=30 值 0.5（Linear）。

- flags：bit0 name 存在 = `01`
- 值区：name `05`；valueType `00`（Float）；numKeys `02`；
  插值位 Linear=1 ×2 → 4 位 `01 01` = `05`，`alignWithBytes`；
  时间 `00` `1E`；值 1.0f=`00 00 80 3F`、0.5f=`00 00 00 3F`；
  贝塞尔列表为空 → 5 位 0 + 补齐 = `00`
- 内容 16 字节 = `01 05 00 02 05 00 1E 00 00 80 3F 00 00 00 3F 00`；头 = 482<<6|16 = 0x7890 → `90 78`

### 10.5 路径记录（含 Quad 与步长指数）

路径：Move(0,0) → Quad 控制点(3, 1.5) 终点(6, 0) → Close；节点最大显示缩放 3
→ e = ceil(log2(3)) = 2 → 步长 0.05×2⁻² = 0.0125（§7.2）。

- numVerbs = `03`；stepExponent = `02`
- 记录 4 位/个：Move=1、Quad=5、Close=0 → 位流 `0001` `0101` `0000` → `51` `00`（补齐对齐）
- 坐标 ×80（1/0.0125）：[0, 0, 240, 120, 480, 0]；最大 |480| → 9 位 → numBits=10 → 前缀 `09`
  打包从该字节第 5 位起延续（5 位前缀 + 坐标位流连续），后 9 字节为
  `00 00 E0 C1 03 3C 00 00`（实测编码器输出）
- PathData 内容 14 字节 = `03 02 51 00 09 00 00 E0 C1 03 3C 00 00`；头 = 289<<6|14 = 0x484E → `4E 48`
- 作为 Path(419) 的内联资源：Path flags = `01`（data 存在），值区 dataRef = `00`（内联），
  值区末尾跟 PathData Tag → 内容 18 字节，头 = 419<<6|18 = 0x68D2 → `D2 68`
- 以上字节由 `PAGBinaryFormatTest.PathQuadStepExponent` 的编码器输出固化（T1），手算位流易错，以测试为准

### 10.6 ImageTable 条目

100×60 图片，factor = 0.5（缩小），裁边偏移 (4, 6)，WebP 数据 4200 字节：

`00`（mediaType 静态 WebP）｜`E8 20`（varuint 4200：104|0x80, 32）｜图片字节 4200 个｜
`64 00 00 00`（u32 逻辑宽 100）｜`3C 00 00 00`（u32 逻辑高 60）｜
`00 00 00 3F`（f32 0.5）｜`00 00 80 40`（f32 4）｜`00 00 C0 40`（f32 6）

### 10.7 SubDocument

外部子 PAGX "sub.pagx"（StringRef 3），画布 100×60，无保留 id、一个图层：

`03`（diagnosticName）｜`C9 41 03 00 00 C8 42 00 00 70 42`（DocumentAttributes：flags=03 存在位
width/height，100.0f、60.0f；头 = 263<<6|9 = 0x41C9）｜`01 42 00`（RetainedIdTable：头 = 264<<6|1 =
0x4201，count=0）｜图层 Tag…｜`00 00`（End）

## 11. 资源上限

发版前如需调整，只影响解码器的拒绝策略，不影响已发布文件的兼容性。

| 上限项 | 数值 | 处理 |
|---|---|---|
| 文件主体长度 | 4 GB（u32） | 编码前检查；解码按截断报错 |
| 单个字符串 | 16 MB | 拒绝加载 |
| 字符串表条目数 | 4,194,304 | 拒绝加载 |
| 单个路径：动词数 / 点数 | 各 1,048,576（2²⁰） | 拒绝加载 |
| 单通道关键帧数 | 5,184,000（沿用旧 MAX_KEYFRAMES：60fps × 24h） | 拒绝加载 |
| 图片像素尺寸 | 16,384 × 16,384 | 拒绝加载 |
| 单条图片数据 | 256 MB | 拒绝加载 |
| ImageTable 条目数 | 65,536 | 拒绝加载 |
| 资源表条目数 | 65,536 | 拒绝加载 |
| 单文档节点总数 | 1,048,576（设计文档初定值） | 导出与解码都检查 |
| 子文档展开实例总数 | 1,024（设计文档初定值） | 导出报错；解码拒绝 |
| SubDocumentTable 条目数 | 256 | 拒绝加载 |
| 子文档引用链深度 | 64 | 环检测 + 深度检查 |
| 嵌套深度（Layer/Group/Composition/StateRegion） | 128（设计文档 §5.2） | 拒绝该子树 |
| 渐变色标数 | 1,024 | 拒绝加载 |
| 单 Font 字形数 | 65,536（uint16 GlyphID 上限） | 拒绝加载 |
| 列表计数（动画对象/通道/状态/转移/条件/输入/VM 属性/DataBind/图层子项等） | 各 65,536 | 拒绝加载 |

上限数值用仓库 317 个样本 + `resources/pagx_to_pag/` 校准后再冻结（§13.3 第 9 条）；发版前允许修订。

## 12. 确定性要求

- StringTable、ResourceBlock、ImageTable、SubDocumentTable 的顺序分别按首次引用顺序生成；
  节点序号按剥离后的先序遍历分配；文件不含时间戳与机器标识。
- 内联/共享决策、步长指数、颜色类型选择均为纯函数，不依赖哈希表迭代顺序。
- 同一输入、同一工具版本两次导出逐字节相同（T3 校验）。
