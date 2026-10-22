# PAGX 节点字段提取：ViewModel 与 DataBind

> M0-1 产出。来源：include/pagx/nodes/{ViewModel,ViewModelProperty,DataBind,DataConverter}.h、include/pagx/PAGViewModelValue*.h、include/pagx/PropertyData.h、src/pagx/DataBindRuntime.cpp、src/pagx/DataContext.cpp、src/pagx/PAGXNodeChannel.cpp。

## NodeType 数值（Node.h 顺序枚举 0..54，已核对）
ViewModel=51, ViewModelProperty=52, DataBind=53, DataConverter=54。四类节点在通道注册表中均**无任何通道**（不可动画、不可被 DataBind 写入）。

## Node 基类公共字段（同其余报告）：id（string，默认空）、customData（unordered_map，data-* 元数据）、sourceLine（int，-1）。

### ViewModel (NodeType::ViewModel = 51)
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| properties | vector<ViewModelProperty*> | {} | 本 schema 声明的属性，按声明顺序 |

子节点：properties: ViewModelProperty*。挂在 Composition（或根文档）上，可被其他组件按 id 引用。
XML 序列化（PAGXExporter.cpp:1609）：`<ViewModel id>`，属性写成 `<Property id name type default [min max] [options] [viewModelRef] [dataConverter]>`。

### ViewModelProperty (NodeType::ViewModelProperty = 52)
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| name | string | {} | 属性名，供编程访问 |
| propertyType | ViewModelPropertyType | Number | 属性值类型 |
| dataConverter | DataConverter* | nullptr | 可选转换器，应用前先变换值 |
| viewModelRef | ViewModel* | nullptr | propertyType==ViewModel 时指向目标 schema |
| defaultNumber | float | 0.0f | Number 默认值 |
| minValue | optional<float> | nullopt | Number 下界 |
| maxValue | optional<float> | nullopt | Number 上界 |
| defaultString | string | {} | String 默认值 |
| defaultBoolean | bool | false | Boolean 默认值 |
| defaultColor | Color | {}（黑，a=1，SRGB） | Color 默认值 |
| defaultImage | Image* | nullptr | Image 默认值，引用 <Image> 资源节点；null = 无默认图 |
| enumOptions | vector<string> | {} | Enum 允许的字符串值，映射到 0 起索引 |
| defaultEnum | int | 0 | Enum 默认选项索引 |

子节点（单指针引用）：dataConverter、viewModelRef、defaultImage。
枚举 ViewModelPropertyType（ViewModelProperty.h:29-38）: Number=0, String=1, Boolean=2, Color=3, Image=4, ViewModel=5, Enum=6, Trigger=7。XML 字符串名与枚举名一致。

### DataBind (NodeType::DataBind = 53)
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| source | string | {} | 源路径 "$vm.propertyName" / "$vm.parent.child"，相对所属 ViewModel 及父级解析 |
| target | string | {} | 目标渲染节点 id（如 "@layerId"），须在同一 Composition 内 |
| channel | string | {} | 目标通道名（如 "alpha"、"text"） |
| direction | DataBindDirection | ToTarget | 绑定方向 |

枚举 DataBindDirection（DataBind.h:25-50，uint8_t）: ToTarget=0（VM→target 只读，默认；一个 VM 属性绑多个 target 无歧义）, ToSource=1（target→VM 写回）, TwoWay=2, Once=3（初始化应用一次）。
导出时 direction 仅在 != ToTarget 时写为属性（PAGXExporter.cpp:1679-1681）。
若同一通道既被 DataBind 绑定又被动画驱动，绑定值优先于动画。

### DataConverter (NodeType::DataConverter = 54)
| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| converterType | string | {} | 转换器类型标识，运行时按名查找转换函数 |
| params | map<string,string> | {} | 参数键值对；序列化为子元素 `<Param name value>`（PAGXExporter.cpp:1692-1700）；std::map 按键升序迭代（确定性） |

## ChannelValueType（KeyValue.h:44-53）
Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7（PAGImage 仅运行时，不序列化）。

## PropertyData 反射元数据（PropertyData.h）
基类：propertyType、propertyName、customDataMap（属性节点 data-*）。
- Number: defaultValue/min/max (optional<float>)；String/Boolean/Color: defaultValue (optional)；Image: 无额外；ViewModel: viewModelId (string)；Enum: options (vector<string>) + defaultValue (int)；Trigger: 无额外。

## 运行时值类型（PAGViewModelValue.h，非 Node 不进文档树）
- Number: float 0.0f；String: string {}；Boolean: bool false；Color: Color {}；Enum: int 0
- Image: shared_ptr<PAGImage> nullptr + sourceImage: Image*（构建时解码自的资源节点）+ userAssigned: bool（业务显式赋值后取得所有权）
- Trigger: 无存储状态，fire() 一次性事件，SuppressDelegation 内合并
- ViewModel: referenceInstance: shared_ptr<PAGViewModel>（嵌套实例）

## DataBind 运行时语义（DataBindRuntime.cpp + DataContext.cpp）
- 方向：toTarget() = direction != ToSource；toSource() = ToSource || TwoWay。
- source 解析（DataContext.cpp:33-93）：路径按 '.' 分段，首段 "$vm" 跳过；本地 VM 逐段 findProperty；遇 ViewModel 值下降；失败回退 parentContext 链；仅叶子值可绑定。
- target 解析（DataBindRuntime.cpp:96-117）：必须以 '@' 开头，取其后 id 做 doc->findNode 全文档查找；不在所属 composition 的 RuntimeBinding 作用域内则跳过（镜像 PAGTimeline::resolveTargets）。
- 应用：dirty 绑定经 DataConverterRegistry::apply 转换后 binding->apply 写入目标通道。Once 首次应用后永久跳过。
- syncBack：仅 ToSource/TwoWay；无 reader 的只写通道跳过；Color 在 sRGB 域比较并重表达为 VM 当前色彩空间写回，防 P3 退化；写回经 applyInverse 逆转换，setValueInternal(v, false) 不标脏。
- 绑定成功且 toTarget() 时立即 markDirty，首次 update() 把默认值应用到目标。
- 导入仅校验 source/target/channel 非空（PAGXImporter.cpp:3187-3203），未强制 Animatable。
