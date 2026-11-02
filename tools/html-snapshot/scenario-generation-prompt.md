# Scenario JSON 协议

按照以下 version 1 协议生成 scenario。只输出一个合法 JSON 对象，不包含 Markdown、解释、注释、占位符或额外包装。可选字段省略时使用默认值，不使用 `null`。所有对象只允许列出的字段；数值范围均包含边界，数字必须有限。

## 顶层对象

| 字段 | 必填 | 类型与约束 | 默认值 |
| --- | --- | --- | --- |
| `version` | 是 | 数字，固定为 `1` | — |
| `viewport` | 否 | 对象，只允许 `width`、`height` | 见下文 |
| `timeoutMs` | 否 | 数字，1～600000，场景执行超时，单位毫秒 | 120000 |
| `stepTimeoutMs` | 否 | 数字，1～300000，单步默认超时，单位毫秒 | 15000 |
| `stableMs` | 否 | 数字，0～5000，捕获前画面连续稳定的时间窗口，单位毫秒 | 150 |
| `steps` | 是 | 有序数组，1～256 个步骤，其中必须有 1～32 个 `capture` | — |

`viewport.width`、`viewport.height` 均可省略；提供时必须为 1～16384 的整数，分别默认 1400、900。

## target 对象

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `css` | 非空字符串 | CSS 选择器 |
| `testId` | 非空字符串 | 精确匹配元素的 `data-testid` |
| `role` | 非空字符串 | 可访问性角色，仅 Playwright 引擎支持 |
| `name` | 非空字符串 | 可选，只能与 `role` 一起使用，精确匹配完整可访问名称 |
| `frame` | 非空字符串 | 可选，唯一定位一个已挂载 iframe 的 CSS 选择器；主定位器在该 iframe 内解析 |

`css`、`testId`、`role` 必须且只能提供一个。`frame` 不支持数组或嵌套 frame 路径。

操作目标在执行时须匹配一个元素；等待条件允许暂时匹配零个元素。匹配多个元素会报错，不会自动选择第一个。

## step 对象

每个步骤必须包含 `type`，并可包含 `timeoutMs`。单步 `timeoutMs` 为 1～300000 的数字，单位毫秒；省略时使用顶层 `stepTimeoutMs`，实际仍受场景剩余总时间限制。

除 `type`、`timeoutMs` 外，各类型只允许下表中的字段：

| type | 必填字段 | 可选字段 | 执行语义 |
| --- | --- | --- | --- |
| `click` | `target` | `url` | 点击元素；可同时等待预期 URL |
| `fill` | `target`、`value` | — | 用字符串替换可编辑控件的值；空字符串用于清空 |
| `type` | `target`、`value` | — | 逐字符输入字符串，不预先清空；结果受当前值、光标和选区影响 |
| `press` | `target`、`key` | `url` | 发送按键；`key` 是引擎支持的非空按键字符串 |
| `hover` | `target` | — | 将鼠标移到元素 |
| `focus` | `target` | — | 聚焦元素 |
| `check` | `target`、`value` | — | 设置 checkbox/radio 的选中状态；`value` 必须是布尔值，radio 只支持设为 `true` |
| `select` | `target`、`value` | — | 选择原生 select 中对应的 `option.value`；`value` 为单个字符串，不是标签、索引或数组 |
| `scroll` | `x`、`y` 至少一项 | `target` | 滚动到绝对位置；无 target 时操作窗口，否则操作目标容器 |
| `waitFor` | 见下节 | 见下节 | 等待一个条件成立 |
| `capture` | `name` | `extent` | 捕获当前静态状态，不允许 target |

`fill.value`、`type.value`、`select.value` 均允许空字符串。`scroll.x`、`scroll.y` 是 -10000000～10000000 的数字；省略某一轴时保留该轴位置。坐标是绝对位置，不是增量，也不是点击坐标。

## waitFor 条件

`waitFor` 除公共字段外，只允许 `target`、`state`、`attribute`、`text`、`url`。`state`、`attribute`、`text`、`url` 必须且只能提供一个；多个条件须拆为多个步骤。

| 条件 | target | 类型与匹配规则 |
| --- | --- | --- |
| `state` | 必填 | 字符串，只允许 `visible`、`hidden`、`attached`、`detached` |
| `attribute` | 必填 | 对象，只允许且必须包含 `name`、`value`；name 为非空字符串，value 为字符串，可为空；精确比较 `getAttribute(name) === value` |
| `text` | 必填 | 字符串，可为空；精确比较 `textContent === text`，包含后代文字及空白，不 trim、不做子串或正则匹配 |
| `url` | 可省略 | 非空字符串；等待主页面 URL 精确相等，提供 target 也不改变等待范围 |

状态语义：

- `attached`：元素存在。
- `detached`：元素不存在。
- `visible`：元素存在，display 不为 none、visibility 不为 hidden、opacity 大于 0，且边界框宽高大于 0；不保证在视口内或未被遮挡。
- `hidden`：元素不存在，或不满足上述可见条件。

`text` 比较的是文本内容，不是输入框的实时值；`attribute` 比较的是 HTML 属性，不是 `value`、`checked`、`selected` 等 DOM 属性。

## URL 语义

`url` 仅用于 `click`、`press`、`waitFor`，值必须是非空字符串。支持绝对 URL，以及相对当前主页面地址解析的 URL；解析后与实际页面 URL 精确比较，包括路径、查询参数和 hash。

`click.url`、`press.url` 在输入动作前解析并建立等待；`waitFor.url` 相对执行该等待时的页面地址解析。`url` 只表达预期地址，不执行导航。

## capture 字段

| 字段 | 必填 | 类型与约束 | 默认值 |
| --- | --- | --- | --- |
| `name` | 是 | 字符串，满足 `^[a-zA-Z0-9][a-zA-Z0-9_-]{0,79}$`，在整个 scenario 内唯一 | — |
| `extent` | 否 | 字符串，`viewport` 或 `document` | `viewport` |

- `viewport` 捕获当前视口；`document` 捕获当前已存在的文档布局，不自动加载懒加载内容、展开内部滚动容器或遍历虚拟列表。document 高度上限为 16384 像素。
- 捕获尽力保留可处理的内容。不支持的运行态、资源缺失或静态副本布局差异记入结果的 `warnings`，不因此中止后续检查点；无法获取的内容局部省略，字体可使用回退字体。
- 捕获不提供整页位图兜底，也没有 `unsupported` 策略字段。`warnings` 是执行结果字段，不能写入 scenario。
- 非法协议、交互失败、等待超时和导入失败仍然报错。包含 warning 的执行完成状态不表示画面已被完整还原。

## 执行语义

所有 steps 在新的隔离浏览器上下文中共享同一页面，按数组顺序执行。初始页面、浏览器引擎和外部会话配置由调用方提供，不属于 scenario 字段。

capture 不重载或重置页面，也不主动恢复滚动、焦点、鼠标、表单或弹层状态。检查点输出为独立静态产物，不包含交互状态机或可播放动画。

捕获前会等待图像、字体和观测画面稳定；已明确加载或解码失败的图像记录 warning 并继续。业务就绪由显式 waitFor 表达。stableMs=0 仅关闭连续稳定时间要求，不冻结动画、不指定采样时刻，也不跳过仍在加载的资源等待。

协议只支持上述 11 种步骤，不支持自定义动作、分支、循环、重试、任意脚本、直接导航或中途改变视口。
