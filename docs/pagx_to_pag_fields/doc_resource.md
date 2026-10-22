# PAGX 节点字段提取：文档级与资源

> M0-1 产出。依据头文件实码提取，作为 spec/pag_binary_spec.md §9（节点 Tag 定义）的编写依据。

# PAGX 节点模型字段清单 —— 文档级与资源（依据头文件实码提取）

来源文件：include/pagx/PAGXDocument.h、include/pagx/nodes/{Node,Composition,Image,PathData,SolidColor,ColorSource,Gradient,ColorStop,LinearGradient,RadialGradient,ConicGradient,DiamondGradient,ImagePattern,Font,GlyphRun}.h、include/pagx/types/*.h、include/pagx/nodes/KeyValue.h、src/pagx/PAGXNodeChannel.cpp、src/pagx/PAGXChannelTable.h。

**NodeType 枚举**（include/pagx/nodes/Node.h，无显式赋值，按声明顺序隐式取值 0..54）：Document=0, PathData=1, Image=2, Composition=3, SolidColor=4, LinearGradient=5, RadialGradient=6, ConicGradient=7, DiamondGradient=8, ImagePattern=9, ColorStop=10, Font=11, Glyph=12, Animation=13, AnimationObject=14, Channel=15, StateMachine=16, StateRegion=17, State=18, StateTransition=19, TransitionCondition=20, StateMachineInput=21, Layer=22, DropShadowStyle=23, InnerShadowStyle=24, BackgroundBlurStyle=25, NoiseStyle=26, GlassStyle=27, BlurFilter=28, DropShadowFilter=29, InnerShadowFilter=30, BlendFilter=31, ColorMatrixFilter=32, NoiseFilter=33, Rectangle=34, Ellipse=35, Polystar=36, Path=37, Text=38, Fill=39, Stroke=40, TrimPath=41, RoundCorner=42, MergePath=43, TextModifier=44, TextPath=45, TextBox=46, Group=47, Repeater=48, RangeSelector=49, GlyphRun=50, ViewModel=51, ViewModelProperty=52, DataBind=53, DataConverter=54。

**通道机制说明**（src/pagx/PAGXChannelTable.h + PAGXNodeChannel.cpp）：
- ChannelFlags 位掩码：None=0、Animatable=1<<0（有运行时 writer，可被动画/DataBind 原地驱动）、RequiresLayout=1<<1（文档上编辑后需重跑 layout 才生效）。组合记 AnimLayout。
- 通道值类型 ChannelValueType（include/pagx/nodes/KeyValue.h）：Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7。KeyValue = std::variant 按此顺序。
- 通道名与 PAGX XML 属性同名；Point/Size 按分量寻址（"position.x"、"size.width"）；枚举以字符串名传递；Padding 四分量通道 "padding.left/top/right/bottom"。
- ChannelsFor(type) 未注册的类型返回空表 → 无通道（不可动画/DataBind 写入）。

---

### Node（基类，抽象，无自身 NodeType 值）
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| id | std::string | {} | 节点唯一标识，用于 "@id" 引用 | 无（id 不走通道） |
| customData | std::unordered_map<std::string, std::string> | {} | XML data-* 自定义属性（key 不含 "data-" 前缀；key 仅小写字母/数字/连字符、不以连字符结尾；值任意字符串；运行时不处理，仅工具间传递元数据） | 无 |
| sourceLine | int | -1 | 源 XML 定义行号；-1 表示无源行信息（程序化创建）；由 PAGXImporter 写入 | 无 |

子节点：无。所有具体节点继承此三类字段。
[设计文档 §4.6 提示] 二进制剥离：其余节点的 data-*、sourceLine 被删除；仅 ViewModelProperty、文档根、ImagePattern 的 data-* 保留（C27）。id 仅保留 Layer id 与按字符串查找的节点 id（动画目标、DataBind 目标）、Animation/StateMachine id。

### PAGXDocument (NodeType::Document=0)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| width | float | 0 | 画布宽度（像素） | 无通道 |
| height | float | 0 | 画布高度（像素） | 无通道 |
| layers | std::vector<Layer*> | {} | 顶层图层列表（裸指针，节点持有所有权） | 无通道 |
| animations | std::vector<Node*> | {} | 文档 <Animations> 块声明的顶层动画/状态机，按声明序；元素为 Animation 或 StateMachine，按 nodeType() 分派 | 无通道 |
| viewModel | ViewModel* | nullptr | 绑定到文档的 ViewModel schema（裸指针，节点持有） | 无通道 |
| dataBinds | std::vector<DataBind*> | {} | 将 ViewModel 属性绑定到图层的 DataBind 节点 | 无通道 |
| nodes | std::vector<std::unique_ptr<Node>> | {} | 文档持有的全部节点（所有权容器） | 无通道 |
| errors | std::vector<std::string> | {} | 解析错误收集；非空表示源文档结构问题但不阻止返回，出错处内容可能不完整 | 无通道 |

方法（公开 API，非数据字段）：Make(width,height)；makeNode<T>(id)/findNode(id)（id 索引，重复 id 报错并替换索引项）；hasUnresolvedImports()（存在未解析的内联 <svg> 或 import 属性，需 pagx resolve）；getExternalFilePaths()（Image 节点或外部合成图层未内嵌数据的全部外部路径，排除 "data:" URI）；getExternalImagePaths()（仅本地可读路径，排除 data:/URL 型（含 "://"）及外部合成图层路径）；loadFileData(path, Data)/loadFileDataMap(map)（内嵌图片数据 / 解析外部 PAGX）；loadFileData(path, shared_ptr<PAGImage>)（宿主解码图，仅运行时、永不序列化，优先级最高；仅影响 ImagePattern 渲染，不影响 ViewModel 图片属性）；fontConfig()（返回 FontConfig 运行时字体注册表，导入器填充 fallback 字体，调用方可在 applyLayout 前注册字体）；applyLayout(fontConfig)/isLayoutApplied()；embed()（收集 Text 布局 GlyphRun 内嵌字体，需先 applyLayout）/clearEmbed()；getRequiredFonts()（去重排序的 PAGFont 列表）；notifyChange(dirtyNodes, layoutChanged)（广播编辑到 liveScenes；GlyphRun/Font/Glyph 不支持 notify，改 Text.layoutChanged）；ownsNode(node)。
私有运行时状态（不序列化）：_fontConfig、layoutApplied、nodeMap/nodeSet（id 索引与归属集合）、liveScenes（weak_ptr<PAGScene>）、layersByImageFilePath(+Built)（惰性 Image 路径→Layer 索引）。
[设计文档 §4.6 提示] 文档根的 data-* 在二进制中保留。

### Composition (NodeType::Composition=3)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| width | float | 0.0f | 合成宽度（像素） | 无通道 |
| height | float | 0.0f | 合成高度（像素） | 无通道 |
| layers | std::vector<Layer*> | {} | 合成包含的图层 | 无通道 |
| animations | std::vector<Node*> | {} | 合成 <Animations> 块声明的动画/状态机（Animation 或 StateMachine，按声明序） | 无通道 |
| viewModel | ViewModel* | nullptr | 绑定到合成的 ViewModel schema | 无通道 |
| dataBinds | std::vector<DataBind*> | {} | 绑定 ViewModel 属性到合成内图层的 DataBind | 无通道 |

子节点：- layers: Layer*；- animations: Node*（Animation|StateMachine）；- viewModel: ViewModel；- dataBinds: DataBind*。
Composition 是可复用资源，被 Layer 的 composition 属性引用创建实例；构造私有，friend PAGXDocument。

### Image (NodeType::Image=2)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| data | std::shared_ptr<Data> | nullptr | 图片二进制数据（base64 解码或 loadFileData 载入）；与 filePath 互斥，data 优先 | 无通道 |
| filePath | std::string | {} | 外部文件路径（与 data 互斥）；可为文件路径、URL 或 data: URI | 无通道 |
| (私有) runtimeImage | std::shared_ptr<PAGImage> | nullptr | 宿主提供的已解码图（loadFileData(path,PAGImage) 设置）；仅运行时状态，永不序列化；优先级高于 data/filePath 解码 | 无通道（不序列化） |

子节点：无。
**【二进制新增】** 依据 docs/pagx_to_pag_design.md §4.5：导出二进制时 Image 节点增加"逻辑尺寸、scaleFactor、内容偏移"三个派生字段（编码细节，不进 PAGX 规范）。导出管线沿用旧 PAG 流程：最大显示缩放（累计每处 ImagePattern 引用的祖先变换 + ImagePattern.matrix + scaleMode 适配缩放，取所有引用与关键帧最大值）→ factor = min(1, 最大显示缩放 × 2.0)（只缩小不放大）→ 透明边裁剪（仅所有引用均 decal 平铺时）并记录偏移 → 质量 80 WebP（已是 WebP 且无需缩小裁边则原样写入不重编码）→ 记录逻辑尺寸与 scaleFactor。运行时 LayerBuilder 按逻辑尺寸适配，以 1/scaleFactor 缩放并平移绘制。图片去重按内容哈希 + 字节比较，ImageTable 序号按首次引用顺序。异常：外部图找不到/解码失败写 warnings 且不写数据（引用填充不绘制）；动图只取首帧；导出后 getExternalFilePaths() 为空（全部内嵌）。

### PathData (NodeType::PathData=1)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| (私有) _verbs | std::vector<PathVerb> | {} | 路径命令序列（对外经 verbs() 只读访问） | 无通道 |
| (私有) _points | std::vector<Point> | {} | 点序列（对外经 points()/countPoints() 访问；按 verb 递增消费：Move=1 点、Line=1 点、Quad=2 点、Cubic=3 点、Close=0 点） | 无通道 |
| (私有) _cachedBounds | Rect | {} | 控制点包围盒缓存（运行时） | 不序列化 |
| (私有) _boundsDirty | bool | true | 缓存失效标记（运行时） | 不序列化 |

API：moveTo/lineTo/quadTo/cubicTo/close 构建路径；forEach(verb, pts) 遍历；getBounds() 返回控制点包围盒（含全部 on/off-curve 点，可能大于几何紧界）；isEmpty()；transform(Matrix) 仿射变换所有点；operator= 仅拷贝 verbs/points 不影响 Node 字段；PointsPerVerb(verb) 静态查询。
子节点：无。可作为资源存于 defs 按 "@id" 引用（如 Path 元素的 data 引用、Glyph 轮廓引用）。

### ColorSource（基类，抽象，无自身 NodeType 值）
字段：无（仅继承 Node）。SolidColor/各渐变/ImagePattern 的公共基类；可内联于 painter 或作为 defs 资源。构造 protected，friend PAGXDocument。无通道注册。

### SolidColor (NodeType::SolidColor=4)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| color | Color | {}（red=0,green=0,blue=0,alpha=1,SRGB 即不透明黑） | RGBA 颜色 + 色彩空间 | "color"（Color 类型，Anim） |

子节点：无。

### Gradient（基类，抽象）
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| matrix | Matrix | {}（identity） | 渐变变换矩阵，在所选坐标系（见 fitsToGeometry）内复合 | 无通道（matrix 未注册） |
| colorStops | std::vector<ColorStop*> | {} | 渐变色标序列（颜色与位置） | 无通道（容器未注册；ColorStop 自身有通道） |
| fitsToGeometry | bool | true | true：渐变位于归一化 (0,0)-(1,1) 空间并映射到每个几何的包围盒，逐几何自适应填充；false：位于父容器（Layer/Group）坐标系（原点 (0,0)），多几何共享一个连续填充 | 各具体渐变子类注册为 "fitsToGeometry"（Bool 类型，NoFlags=不可动画） |

子节点：- colorStops: ColorStop*。

### ColorStop (NodeType::ColorStop=10)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| offset | float | 0.0f | 色标在渐变上的位置，范围 [0,1] | "offset"（Float，Anim） |
| color | Color | {} | 该位置的颜色 | "color"（Color，Anim） |

子节点：无。[设计文档 §4.7 提示] 渐变色标位置编码精度 GRADIENT_PRECISION = 0.00002。

### LinearGradient (NodeType::LinearGradient=5)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| startPoint | Point | {0.0f, 0.0f} | 渐变线起点；fitsToGeometry=true 时为归一化包围盒左边缘 | "startPoint.x"/"startPoint.y"（Float，Anim） |
| endPoint | Point | {1.0f, 0.0f} | 渐变线终点；默认 {1,0} 产生水平从左到右渐变 | "endPoint.x"/"endPoint.y"（Float，Anim） |
| (继承) fitsToGeometry | bool | true | 见 Gradient | "fitsToGeometry"（Bool，NoFlags） |

子节点：继承 colorStops: ColorStop*。

### RadialGradient (NodeType::RadialGradient=6)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| center | Point | {0.5f, 0.5f} | 渐变圆心；默认归一化包围盒中心 | "center.x"/"center.y"（Float，Anim） |
| radius | float | 0.5f | 渐变圆半径；默认 0.5 到达包围盒各轴边缘 | "radius"（Float，Anim） |
| (继承) fitsToGeometry | bool | true | 见 Gradient | "fitsToGeometry"（Bool，NoFlags） |

子节点：继承 colorStops: ColorStop*。

### ConicGradient (NodeType::ConicGradient=7)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| center | Point | {0.5f, 0.5f} | 扫描中心；默认归一化包围盒中心 | "center.x"/"center.y"（Float，Anim） |
| startAngle | float | 0.0f | 扫描起始角（度） | "startAngle"（Float，Anim） |
| endAngle | float | 360.0f | 扫描结束角（度） | "endAngle"（Float，Anim） |
| (继承) fitsToGeometry | bool | true | 见 Gradient | "fitsToGeometry"（Bool，NoFlags） |

子节点：继承 colorStops: ColorStop*。

### DiamondGradient (NodeType::DiamondGradient=8)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| center | Point | {0.5f, 0.5f} | 菱形渐变中心 | "center.x"/"center.y"（Float，Anim） |
| radius | float | 0.5f | 菱形对角线长度的一半；默认 0.5 到达包围盒各轴边缘 | "radius"（Float，Anim） |
| (继承) fitsToGeometry | bool | true | 见 Gradient | "fitsToGeometry"（Bool，NoFlags） |

子节点：继承 colorStops: ColorStop*。

### ImagePattern (NodeType::ImagePattern=9)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| image | Image* | nullptr | 图片资源引用 | 无通道（指针未注册） |
| tileModeX | TileMode | TileMode::Decal | 水平平铺模式 | "tileModeX"（String 枚举名，NoFlags） |
| tileModeY | TileMode | TileMode::Decal | 垂直平铺模式 | "tileModeY"（String 枚举名，NoFlags） |
| filterMode | FilterMode | FilterMode::Linear | 纹理采样过滤模式 | "filterMode"（String 枚举名，NoFlags） |
| mipmapMode | MipmapMode | MipmapMode::Linear | 纹理采样 mipmap 模式 | "mipmapMode"（String 枚举名，NoFlags） |
| matrix | Matrix | {}（identity） | 作用于图片本地坐标系（原始图片矩形、原点左上）的变换矩阵 | 无通道（matrix 未注册） |
| scaleMode | ScaleMode | ScaleMode::LetterBox | 变换后图片放入几何的规则；默认 LetterBox 适配各几何包围盒（逐几何自适应）；None 时不适配、放父容器（Layer/Group）坐标系（原点 (0,0)），多几何共享连续填充 | "scaleMode"（String 枚举名，NoFlags） |

渲染顺序注释：图片先经 matrix 在本地空间变换，再按 scaleMode 放入几何。
子节点：- image: Image*（引用，非容器）。
[设计文档 §4.6 提示] ImagePattern 的 data-* 在二进制中保留（C27）；旧 PAG 图片的缩放与裁边改用 ImagePattern 矩阵表达（§4.5/§7.1）。

### Font (NodeType::Font=11)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| unitsPerEm | int | 1000 | 字体设计空间每 em 单位数；渲染缩放 = fontSize / unitsPerEm | 无通道 |
| file | std::string | {} | 外部字体文件路径；设置时本节点作为字体源声明（pagx embed 加载注册做 shaping，提取的字形数据存于独立 Font 节点，源节点 glyphs 保持为空）；路径相对 PAGX 文件目录解析；空串 = 无外部引用。导入器 FromFile() 运行时把相对路径解析为绝对路径 | 无通道 |
| fileOriginal | std::string | {} | 源 XML 中 file 属性的逐字原值（未做路径解析）；PAGXExporter 非空时写回此值，保证相对路径/URL round-trip 不变 | 无通道 |
| glyphs | std::vector<Glyph*> | {} | 字形列表；GlyphID = index + 1（GlyphID 0 保留给 missing glyph）。应在文档构建期一次性填充（FontEmbedder 或加载 PAGX）；首次渲染后修改为未定义行为（GlyphRunRenderer 缓存已构建 typeface） | 无通道 |
| (私有) renderCache | std::unique_ptr<FontRenderCache> | — | 惰性构建渲染缓存（typeface + 失败哨兵），PIMPL 隐藏 tgfx 类型；clearEmbed 经 resetRenderCache 清空；线程共享需外部串行化 | 不序列化（运行时） |

子节点：- glyphs: Glyph*。
[设计文档 §4.6 提示] 二进制剥离 Font 的 file 外部路径；作者已嵌入的 Font 与 GlyphRun 保留。

### Glyph (NodeType::Glyph=12)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| path | PathData* | nullptr | 矢量字形轮廓数据；与 image 互斥 | 无通道 |
| image | Image* | nullptr | 位图字形图片资源；与 path 互斥 | 无通道 |
| offset | Point | {}（(0,0)） | 设计空间中的字形偏移；通常用于位图字形 | 无通道 |
| advance | float | 0.0f | 设计空间（unitsPerEm 坐标）水平步进宽度；Default 定位模式下决定到下一字形的间距 | 无通道 |

子节点：- path: PathData* / - image: Image*（二选一引用）。
[设计文档 §4.7 提示] 字形轮廓编码沿用旧 WritePath 思路（退化记录 + 点列表按精度打包；记录位宽 3→4 位加入 Quad；不把 Quad 升阶为 Cubic）。

### GlyphRun (NodeType::GlyphRun=50)
| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| font | Font* | nullptr | 引用的 Font 资源 | 无通道 |
| fontSize | float | 12.0f | 渲染字号；实际缩放 = fontSize / font.unitsPerEm | 无通道 |
| glyphs | std::vector<uint16_t> | {} | GlyphID 序列；GlyphID 0 = missing glyph（不渲染） | 无通道 |
| x | float | 0.0f | 整体 X 偏移 | 无通道 |
| y | float | 0.0f | 整体 Y 偏移 | 无通道 |
| xOffsets | std::vector<float> | {} | 逐字形 X 偏移；最终 X = x + xOffsets[i] + positions[i].x | 无通道 |
| positions | std::vector<Point> | {} | 逐字形 (x,y) 偏移；最终 Y = y + positions[i].y | 无通道 |
| anchors | std::vector<Point> | {} | 逐字形锚点偏移，相对默认锚点 (advance × 0.5, 0)；锚点是 scale/rotation/skew 变换的中心 | 无通道 |
| scales | std::vector<Point> | {} | 逐字形缩放 (scaleX, scaleY)，默认 (1,1)；绕锚点应用 | 无通道 |
| rotations | std::vector<float> | {} | 逐字形旋转角（度），默认 0；绕锚点应用 | 无通道 |
| skews | std::vector<float> | {} | 逐字形斜切角（度，沿垂直轴），默认 0；绕锚点应用 | 无通道 |
| bounds | Rect | {} | 本文本块在父坐标系的包围矩形；TextBox 子项时为 TextBox 坐标系，独立 Text 时为 Text 本地坐标系；每个 Text 元素仅第一个 GlyphRun 携带整块 bounds | 无通道 |

子节点：- font: Font*（引用）。GlyphRun 是预 shape 的排版输出（FontEmbedder/embed() 产出），无通道、不可动画；notifyChange 不支持 GlyphRun（应改 Text + layoutChanged）。
[设计文档 §4.7 提示] GlyphRun 位置/偏移/锚点点列表编码用 writeFloatList(..., SPATIAL_PRECISION)。

---

## 基础类型（include/pagx/types/）

### Color（struct）
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| red | float | 0 | 红分量 [0,1] |
| green | float | 0 | 绿分量 [0,1] |
| blue | float | 0 | 蓝分量 [0,1] |
| alpha | float | 1 | 透明度 [0,1]，默认不透明 |
| colorSpace | ColorSpace | ColorSpace::SRGB | 色彩空间 |

[设计文档 §4.7 提示] 颜色编码：sRGB 写 RGBA 各 8 位；广色域（P3）或超 [0,1] 时写色彩空间 + float。

### 枚举汇总（本次范围）
- ColorSpace: SRGB=0（标准 RGB，分量典型 [0,1]）, DisplayP3=1（Apple 广色域，分量可超 [0,1]）
- ScaleMode: None=0（不适配包围盒；置于父容器坐标系并按平铺模式延伸到图外，用于多几何共享连续图像布局）, Stretch=1（拉伸填满，忽略宽高比）, LetterBox=2（等比缩小放入并居中，某轴留空；默认值）, Zoom=3（等比放大覆盖并居中，某轴裁切）
- TileMode: Clamp=0（边缘像素外扩）, Repeat=1（平铺重复）, Mirror=2（边缘镜像重复）, Decal=3（界外透明）
- FilterMode: Nearest=0（单采样点最近邻）, Linear=1（2x2 双线性插值）
- MipmapMode: None=0（忽略 mipmap，取 base 层）, Nearest=1（最近层级）, Linear=2（两最近层级间插值）
- PathVerb: Move=0（1 点：目标点）, Line=1（1 点：终点）, Quad=2（2 点：控制点+终点）, Cubic=3（3 点：控制点1+控制点2+终点）, Close=4（0 点）

### Data（class）
不可变字节缓冲：MakeWithCopy(data,length)/MakeAdopt(uint8_t*,length)（接管 new[] 缓冲）；data()/bytes()/size()/empty()；不可拷贝/移动。经 shared_ptr<Data> 持有（Image.data 等使用）。

### Padding（struct）
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| top | float | 0 | 上内边距 |
| right | float | 0 | 下内边距 |
| bottom | float | 0 | 下内边距 |
| left | float | 0 | 左内边距 |

XML 支持三种简写：单值（四边同值）、两值（"纵,横"）、四值（"上,右,下,左"）。isZero() 判全零。

### Point（struct）
x: float = 0、y: float = 0。

### Size（struct）
width: float = 0、height: float = 0。

### Rect（struct）
x: float = 0、y: float = 0（左上角）、width: float = 0、height: float = 0。MakeLTRB(l,t,r,b)/MakeXYWH(x,y,w,h)；isEmpty() = width<=0 || height<=0。

### Matrix（struct，2D 仿射）
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| a | float | 1 | 水平缩放（scaleX） |
| b | float | 0 | 垂直斜切（skewY） |
| c | float | 0 | 水平斜切（skewX） |
| d | float | 1 | 垂直缩放（scaleY） |
| tx | float | 0 | 水平平移（transX） |
| ty | float | 0 | 垂直平移（transY） |

矩阵形式 |a c tx; b d ty; 0 0 1|；Identity()/Translate(x,y)/Scale(sx,sy)/Rotate(degrees)；operator*（this 连乘 other）；mapPoint。

### Matrix3D（struct，4x4）
values: float[16]，列主序，默认 identity（values = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}）。getRowColumn(r,c)=values[c*4+r]、setRowColumn、isIdentity。

### 通道基础设施补充
- ChannelFlags（src/pagx/PAGXChannelTable.h）: None=0, Animatable=1<<0, RequiresLayout=1<<1。
- ChannelValueType（include/pagx/nodes/KeyValue.h）: Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7（与 KeyValue variant 顺序对齐）。
- 本范围无通道注册的节点：PAGXDocument、Composition、Image、PathData、Font、Glyph、GlyphRun（ChannelsFor 返回空表）。
- 注册但不可动画（NoFlags）的本范围通道：各渐变 fitsToGeometry、ImagePattern 全部枚举通道。
- Fill/Stroke 的 "color" 通道经 AccessFillStrokeColor 读写底层 SolidColor 子节点的 color（此机制属 extract-layer-style 范围，此处仅备注关联）。

## 待确认项
- 无重大待确认项。NodeType 数值为声明顺序隐式值（代码未显式赋值），若二进制格式直接复用需固定此顺序。
- PAGXDocument.fontConfig() 为运行时注册表（FontConfig：registerFont(path/memory)、fallback 查询），非文档数据，不进二进制（设计文档 §4.3 已明确字体注册为运行时接口）。
