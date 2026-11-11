# PAGX MP4 PR #3830 评审问题修复清单

- PR：https://github.com/Tencent/libpag/pull/3830
- 评审提交：`a5a565495bdfbca82fcff598bdb8dd6f533de9c9`
- 整理日期：2026-11-11
- 评论统计：50 条行级评论，50 个独立问题，无作者回复；另有 1 条 Codecov 会话评论，不属于评审问题。
- 编号构成：严重 M1–M4、M5-1–M5-5、M6-1–M6-2，共 11 条；一般 1–39，共 39 条。
- 本文基于静态代码分析整理。
- 2026-11-11 实现进展：M1、M2、M3、M5-1–M5-5、M6-1、M6-2 已修复，用户已确认测试通过。M4 已按文档与提示修正方案修改，待验证。
- 批次 1 剩余项 19、21–26、28、30、31、36 已修复，用户已确认测试通过。
- 批次 2 的一般项 1–6、32–34 已修复，用户已确认测试通过；M5-4 已在上一批完成。
- 批次 4 的 8–11、18、27 已实现，25 已由前序修改满足并补充防回归断言；统一参数校验、错误前缀、显式输出路径和原生 ffmpeg 定位，JS 不再预判或注入 ffmpeg。待用户编译与测试验证后勾选。

## 处理原则

1. 先修复播放正确性、进程生命周期和下载供应链问题，再进行结构性重构。
2. 相互关联的评论合并为一个修复单元，避免重复改动同一逻辑。
3. “部分采纳”表示问题或部分证据成立，但不完全采用 reviewer 的建议。
4. “不采纳”表示无需修改生产代码，但应在 PR 中回复理由。
5. 完成每项后勾选，并补充对应测试或验证记录。

## 一、严重问题

### M1：嵌套动画与 StateMachine 的默认播放行为错误

- [x] **结论：核心问题成立，P0。**
- 问题：默认时长只枚举顶层 Animation，嵌套 Composition 动画会退化为一帧；静态帧复用却扫描全部节点，两个谓词不一致。典型 StateMachine 文档通常同时包含 Animation，当前实现还会绕过 StateMachine 直接播放顶层 Animation。
- 修复：建立内部 `ExportPlaybackPlan`，按实际挂载关系收集顶层及嵌套时间线；顶层 Animation 使用 seek，StateMachine 按帧 delta 调用 `advanceAndApply()`；静态复用、默认帧率和默认时长统一从播放计划派生。
- 边界：普通 Animation 可推导有效播放周期并考虑挂载偏移；StateMachine 没有天然有限时长，未显式提供 `duration` 时应报错。
- 位置：`src/pagx/VideoExporter.cpp`、`src/pagx/PAGScene.cpp`。
- 测试：嵌套动画默认帧数、StateMachine 初始应用与逐帧推进、混合顶层时间线、静态文档复用。

### M2：无 ffmpeg 时无法使用 dry-run

- [x] **结论：成立，P1。**
- 问题：`FindFFmpeg()` 在 dry-run 分支之前执行，导致最需要预览命令的环境反而无法使用。
- 修复：dry-run 跳过可执行性探测，按 `--ffmpeg`、非空 `FFMPEG_BIN`、字面量 `ffmpeg` 生成预览；真实执行仍严格验证。npm wrapper 同步删除提前失败逻辑。
- 测试：清空 PATH 和 `FFMPEG_BIN` 后 dry-run 返回成功、打印命令且不创建输出或临时文件。

### M3：postinstall 下载失败阻断整个 npm install

- [x] **结论：成立，属于安装可用性与产品策略问题，P1。**
- 问题：仅 MP4 需要的可选依赖失败，会阻断全部非 MP4 功能安装；README 的 optional 表述容易误导。
- 修复：postinstall 改为 best-effort，失败时警告并正常退出；显式 `download:ffmpeg` 保持失败退出。提供直接重试下载的恢复命令，运行时保持离线。
- 测试：postinstall 失败状态为 0 且有警告；手动下载失败状态非 0。

### M4：下载器不支持常规代理配置

- [ ] **结论：部分成立，P2；若企业代理是发布要求则提升为 P1。**
- 问题：Node 原生 `http/https.get` 不读取常见代理环境变量，文档却笼统建议配置代理。
- 修复：短期澄清文档，只推荐镜像或已有 ffmpeg；完整实现时必须支持 `HTTPS_PROXY`、CONNECT、`NO_PROXY`、代理认证和重定向后的代理选择，且日志不得泄露代理凭据。

### M5-1：`VideoExporter::Make()` 职责过多

- [x] **结论：成立，可维护性问题，随 M1 一起修复。**
- 修复：拆为 `ValidateOptions`、`ResolvePlaybackPlan/ResolveSampling`、`CreateResources` 和对象初始化，用内部解析结果传递 scene、时间线、帧率、帧数和尺寸；不要只做机械分段而保留两套播放谓词。

### M5-2：`ParseOptions()` 职责过多

- [x] **结论：成立，可维护性问题。**
- 修复：抽视频参数解析、交叉校验和类型化数值解析 helper；helper 统一检查 `errno`、完整消费、有限浮点数和范围。保留“视频参数不能用于其他格式”的校验，并与 M6-1、31 共用实现和常量。

### M5-3：`ExportToMP4()` 阶段混杂

- [x] **结论：成立，与 M2、11、18、27 一起修复。**
- 修复：按文档准备、ffmpeg 解析、exporter 准备、命令构造、编码循环拆阶段。拆分后必须保证 dry-run 不探测 ffmpeg，命令构造显式接收实际临时输出路径。

### M5-4：`spawnChild()` 混合两套平台实现

- [x] **结论：成立，与一般 1–5、32 一起修复。**
- 修复：拆分 Windows 和 POSIX 子进程创建，公共入口只负责分发与所有权转移；保留句柄/FD 清理、CLOEXEC、信号状态、exec 失败和临时文件所有权语义。

### M5-5：`PrintUsage()` 过长

- [x] **结论：部分采纳，最低优先级。**
- 修复：如需调整，将 common、SVG、PPT、MP4 和 examples 抽成分段文本常量；不要为了缩短行数制造大量只有一次输出的函数。

### M6-1：数值参数解析重复

- [x] **结论：成立，与 M5-2、31 合并修复。**
- 修复：在 CLI 内部提供类型化整数/浮点解析 helper，覆盖 fps、duration、scale、crf，并可顺带收敛 quality、svg-indent。helper 不直接打印错误，由各命令保留自己的错误前缀和文案。

### M6-2：文档准备、帧循环和进度输出重复

- [x] **结论：成立，与 M5-3 合并修复。**
- 修复：在 CLI 层抽 `VideoFrameSource` 和统一帧遍历，公共部分负责加载、字体、布局、Make、帧索引、像素生命周期和进度；MP4 与 PNG 序列只保留各自输出逻辑。
- 边界：不要把阻塞式帧循环加入公共 `VideoExporter` API；PNG 序列不能继承 MP4 的偶数尺寸和 GPU 降采样策略。

## 二、一般问题

### 1：进程级永久忽略 SIGPIPE，子进程继承

- [x] **结论：成立，P1。**
- 修复：在明确的 CLI 进程初始化中设置一次 SIGPIPE 忽略，子进程 exec 前恢复 SIG_DFL；优先考虑 `posix_spawn` signal defaults。测试入口和直接调用 `VideoEncoder::Start()` 的路径也必须有明确策略，不能只改 `main()`。

### 2：写失败后先无限等待再 kill

- [x] **结论：成立，P0/P1。**
- 修复：关闭写端后先做非阻塞或短时有界等待，未退出则 kill/TerminateProcess，最后回收已终止子进程。非 broken-pipe 错误不得先执行无界等待；EPIPE 也不能假设进程一定立即退出。

### 3：execv 失败缺少诊断

- [x] **结论：问题成立，但 reviewer 的 `strerror()` 建议不完全安全。**
- 修复：简单方案是在 fork 子进程中用 async-signal-safe 的 `write()` 输出固定阶段诊断；完整方案是用 CLOEXEC 错误管道把 `errno` 传给父进程，由父进程格式化。不要在 fork 后使用 iostream、`std::string` 或不可移植的 async-signal-safe 假设。

### 4：临时文件重开存在 TOCTOU，最终权限为 0600

- [x] **结论：成立，但不采用 `pipe:1` 主方案，P1。**
- 修复：在输出父目录创建随机、独占、仅当前用户可访问的 staging 子目录，ffmpeg 在其中写普通可 seek 文件；成功后同文件系统原子发布并清理目录。可改用 `-n` 避免覆盖已有文件。
- 原因：`pipe:1` 不等价于可 seek 文件输出，不能保留当前 `+faststart` 语义，也不具备跨平台一致性。
- 权限：明确产品约定；若遵循普通 CLI 输出权限，发布前按 umask 调整；若 0600 是有意安全默认，则保留并写入文档。

### 5：waitpid 失败后可能 kill 已复用 PID

- [x] **结论：成立，P1。**
- 修复：`ECHILD` 时标记已回收、退出状态无效并清除可发送信号的 PID，使后续 kill 为 no-op；不得伪造退出码，其他错误不能一概视为已回收。区分“是否仍可发送信号”和“是否有退出状态”。

### 6：encoder-args 可破坏固定管线和原子发布

- [x] **结论：成立，属于输出契约问题，不是 shell RCE。**
- 修复：将其限定为“当前唯一视频输出的编码参数”。优先白名单允许 `-b:v`、`-maxrate`、`-bufsize`、`-g` 等；或改成结构化重复参数。禁止新增输入、filter graph、map、muxer、输出数量和输出路径。
- 边界：不能简单拒绝所有非 `-` token，否则会误伤合法参数值；验证器必须理解允许选项的参数个数。

### 7：frames-dir 旧帧残留、长序列排序错误、非原子写入

- [ ] **结论：成立。**
- 修复：已存在目录要求为空并明确报错，不默认递归清空用户目录；帧号宽度至少 6 位，最好根据 `frameCount - 1` 动态计算；单帧先写同目录临时文件，检查写入和关闭结果后 rename，失败时清理本次新建帧。

### 8：firstVideoOnlyOption 实际记录最后一个参数

- [ ] **结论：成立。**
- 修复：仅在该字段为空时记录，确保报告第一个无效参数；补充多个视频专属参数顺序的测试。

### 9：font/fallback 对非视频格式静默忽略

- [ ] **结论：部分成立。background 已经会报错，font/fallback 确实静默忽略。**
- 修复：将 `--font`、`--fallback` 纳入视频专属参数校验；若希望矢量导出也支持外部字体，应接入对应导出和布局流程，而不是仅修改提示。补 SVG/PPTX/HTML 下三个参数的行为测试。

### 10：render 错误前缀不一致

- [ ] **结论：成立。**
- 修复：统一真正的失败路径为 `pagx render: error: ...`，在 CliUtils 提供错误打印入口，同时覆盖 document/font/sequence 错误；warning、progress 和 wrote 不变。

### 11：arguments.back() 隐式位置契约

- [ ] **结论：部分采纳。**
- 修复：让 `BuildFFmpegCommandLine()` 显式接收本次输出路径，删除 `arguments.back()` 覆盖。
- 不采纳部分：dry-run 的固定 `.part` 是预览占位符，不应生成随机文件名或创建临时文件；保留并加强文档说明。

### 12：校验和与归档同源，没有独立信任根

- [x] **结论：成立，P0。**
- 修复：`prepack` 从官方 Release 生成 `ffmpeg-manifest.json` 并随 npm 包发布（生成物不提交 Git），固定当前 release、target、资产名、SHA-256 和大小；镜像只能提供字节一致的官方制品。按已确认的产品边界，不支持通过 `PAGX_FFMPEG_RELEASE` 自动下载其他版本，其他构建通过 `FFMPEG_BIN` 或 `--ffmpeg` 指定本地可执行文件。

### 13：允许明文 HTTP 与 HTTPS 降级重定向

- [ ] **结论：成立，P0。**
- 修复：初始 URL 和每一跳重定向都执行相同策略；非 loopback 仅允许 HTTPS，禁止 HTTPS→HTTP；本地 HTTP 例外仅用于明确的 loopback 测试或本地镜像场景。

### 14：归档链接、特殊文件与权限未受控

- [ ] **结论：核心风险成立，换行绕过证据不足，P0。**
- 修复：使用能读取真实条目元数据的解压方案，只允许目录和普通文件；拒绝 symlink、hardlink、设备、FIFO、控制字符和越界路径；限制条目数、单项与总展开大小；显式设置权限，不保留 setuid、特殊位或所有者信息。
- 边界：解压后 realpath 只能作为纵深防御，不能替代解压前拒绝链接，因为越界写入可能已经发生。

### 15：安装锁 stale 与等待超时耦合，释放不校验归属

- [ ] **结论：成立，P1。**
- 修复：拆分等待超时与 stale 阈值；锁写随机 owner token，持锁期间心跳更新，发布前重新确认所有权，finally 仅释放仍属于自己的锁；stale 接管需避免检查后误删新锁的竞态。

### 16：下载缺少整体超时、大小上限和流式哈希

- [ ] **结论：成立，P0/P1。**
- 修复：整体 deadline 覆盖所有重定向，保留空闲超时作为第二层保护；检查 Content-Length 和实际字节数，超限 abort 并清理临时文件；SHA-256 改为流式计算；下载上限不能替代解压上限。

### 17：release 未校验即参与缓存路径拼接

- [x] **结论：成立，但默认属于低严重度健壮性问题。**
- 修复：下载和缓存路径只使用包内 manifest 的固定 release；`PAGX_FFMPEG_RELEASE` 请求其他版本时在路径计算和网络请求前直接报错，因此不再能将该环境变量用于目录穿越。

### 18：JS 与原生参数文法漂移，FFMPEG_BIN 行为不一致

- [ ] **结论：问题成立，但不默认采用静默回退。**
- 修复：JS 不再预判 MP4 参数，由原生统一解析和报错；等号形式要么双方都支持，要么双方都拒绝。
- 策略：失效的显式 `FFMPEG_BIN` 默认 fail-fast，避免意外执行另一个 ffmpeg；如产品要求缓存回退，必须明确警告并可选择严格模式。

### 19：背景字段冗余，偶数尺寸命名不准确

- [x] **结论：部分采纳，公开 API 定型前处理。**
- 修复：使用单一背景 Color，并显式默认全透明 `{0, 0, 0, 0}`；`alignToEvenSize` 改名为 `padToEvenSize`。是否要求 opaque/even 仍由编码 host 决定。
- 注意：`Color{}` 默认 alpha 为 1，不能直接依赖默认构造表达透明。

### 20：Options ABI 文档不一致

- [ ] **结论：发现不一致成立，但不采用 reviewer 的修法。**
- 修复：保留 VideoExporter“不承诺字段布局 ABI，调用方和库使用匹配头文件”的说明；另行修正 HTMLExporter 的尾部追加兼容性措辞。
- 原因：旧调用方只分配旧版 `sizeof(Options)`，新库读取新增字段仍可能越界。真正 ABI 稳定需要 `structSize/version`、opaque options 或稳定 C API。

### 21：整数帧率无法表达 NTSC 分数帧率

- [x] **结论：成立，公开 API 定型前处理。**
- 修复：优先使用有理数帧率，如 `30000/1001`、`24000/1001`，用整数或宽整数计算帧时间，ffmpeg 参数输出分数。若本期明确不支持，应记录限制，不能声称覆盖常见视频帧率。

### 22：私有成员使用尾下划线

- [x] **结论：成立，低优先级风格问题。**
- 修复：按项目命名规范改为小驼峰语义名称，如 `outputWidth`、`samplingFrameRate`、`nextFrameIndex`。

### 23：VideoExporter 未禁拷贝

- [x] **结论：成立，公开 API 定型前处理。**
- 修复：显式删除拷贝构造和拷贝赋值；不支持移动时也删除移动。增加 `std::is_copy_constructible_v` 和 `std::is_copy_assignable_v` 编译期断言。

### 24：pixels() const 使用可写接口且存在不可达空检查

- [x] **结论：成立。**
- 修复：改用 `Pixmap::pixels() const`，nextFrame 写回仍使用 `writablePixels()`；删除与类不变量矛盾的空检查，无需缓存像素指针。

### 25：格式策略分散

- [x] **结论：部分采纳。**
- 修复：给 `VideoFormatSpec` 增加 `requiresEvenDimensions`，由 exporter 配置和命令校验共同使用。单元素格式表本身不是缺陷，第二个格式出现前不必再引入同义 `ResolveVideoOutput()` 层。

### 26：Make 后 document 可修改性的契约缺失

- [x] **结论：成立，公开 API 定型前处理。**
- 修复：明确 exporter 存活期间源 document 及可达外部 Composition 文档必须不可变，需要新内容则重新 Make。若要支持动态修改，必须引入 revision、播放计划重建和资源/时长失效机制。

### 27：ffmpeg 定位逻辑重复且已漂移

- [ ] **结论：成立，与 M2、18 一起修复。**
- 修复：原生统一负责 `--ffmpeg`、显式 `FFMPEG_BIN`、pagx 管理缓存、PATH 的优先级和可执行性验证；缓存路径、release 和 target 与 Node 共用 manifest。JS wrapper 只启动原生程序，不再扫描 PATH、预解析 export 参数或注入伪用户配置。

### 28：新增 lambda 违反项目规范

- [x] **结论：成立。**
- 修复：按项目规范改为命名谓词；若 M1 的统一播放计划删除 `HasTimelineDefinitions()`，则直接随重构移除，无需单独保留 helper。

### 29：SetError 在 core 与 CLI 重复

- [ ] **结论：不采纳生产代码修改，需在 PR 回复说明。**
- 理由：两个数行局部 helper 分属不同层，保留比增加公共依赖更清晰；不能让 `src/pagx` 反向依赖 `CliUtils`。CLI 错误打印可因问题 10 统一，但不要借此共享 core 的 SetError。

### 30：输出尺寸计算重复

- [x] **结论：部分采纳。**
- 修复：共享带 double 乘法、有限性、正值和整数溢出防护的尺寸计算函数，供纹理限制、提示和 VideoExporter 使用。真正的偶数取整目前只有一处；GPU 最大纹理尺寸降级仍留在 CLI/host，单帧 render 路径不强行合并。

### 31：帧率和 CRF 限制分散

- [x] **结论：成立，与 M5-2、M6-1 一起修复。**
- 修复：VideoExporter 暴露帧率上下限，VideoEncoder 暴露 CRF 上限；parser、校验和错误文本共同使用这些常量。

### 32：writeAll 两平台失败清理重复

- [x] **结论：成立，与一般 1–5、M5-4 一起修复。**
- 修复：平台分支只负责 write 和保存原始错误码，公共失败处理统一关闭、有界等待/终止、回收和临时文件清理；保留 Windows broken-pipe 与 POSIX EPIPE/EINTR 差异。

### 33：编码失败测试断言过弱

- [x] **结论：成立。**
- 修复：分别断言 write/finish 错误及输出未发布；优先用受控 fake ffmpeg 模拟提前关闭 stdin、消费后非零退出和成功，避免依赖本机 ffmpeg 对未知编码器的退出时序。

### 34：exec 失败测试覆盖不严格

- [x] **结论：部分采纳。**
- 修复：POSIX 强制 Start 成功并最终报告 exit code 127；Windows 允许 Start 直接失败并检查平台错误。保留 write 已失败后的分支处理，因为这是合理竞态。
- 测试文件：固定 `.part` 内容不变用于验证不误删无关文件，应保留；另外扫描本次 `.part.*`，断言真实拥有的临时文件全部清理。

### 35：真实解压和安全边界缺少测试

- [ ] **结论：部分采纳。**
- 修复：补本地恶意 tar/ZIP、绝对路径、`..`、链接、特殊文件、权限、HTTPS 降级、非 loopback HTTP、整体 deadline、慢速滴流、超大响应、stale 锁和 release 穿越测试；测试不依赖公网。
- 说明：现有下载器并非零覆盖，已有 checksum、重定向、并发和 postinstall 测试，但 fakeInstaller 绕过了真实解压。

### 36：公开 API 行为与失败路径覆盖不全

- [x] **结论：部分采纳，优先覆盖真实合同缺口。**
- 修复：补嵌套默认时长、StateMachine 推进、背景透明/不透明、padToEvenSize、分数帧率 PTS、不可拷贝 traits、Make 后不可变合同，以及 pixels 首帧前/EOS 后行为。
- 后续：资源创建、draw/readPixels 失败可在增加可注入测试 seam 后覆盖，不为补测试破坏生产分层。

### 37：运行时 CI 与下载器 CI 覆盖不足

- [ ] **结论：部分采纳。**
- 修复：增加 macOS、Linux、Windows matrix，运行 Node 下载器测试、真实解压和原生进程测试；使用本地 HTTP/HTTPS 服务与固定归档，不依赖公网 Release。
- 说明：当前是 autotest 只 macOS，Linux/Windows 只有构建，并非整个 CI 只有 macOS。`test:ffmpeg` 未接入 workflow 属实。

### 38：PAGXCliTest 文件过大

- [ ] **结论：成立，行为修复完成后处理。**
- 修复：新建 `PAGXVideoCliTest.cpp` 承载视频 CLI 测试，按需提取 CliTestUtils；核心采样测试继续留在 `PAGXVideoExporterTest.cpp`。
- 构建：同步修改 `web/test/CMakeLists.txt` 排除新 native-only 测试及 helper，避免 web glob 引入未编译的生产依赖。

### 39：wasm 支持边界不清

- [ ] **结论：部分采纳，先明确支持矩阵。**
- 修复：若本期只支持 native，在文档和构建开关中一致表达；若浏览器 wasm 是目标，保留平台无关渲染器，明确 WebCodecs 编码由 host 提供，再决定是否增加 embind/TS 包装。
- 说明：无 JS binding 不等于 C++ wasm 调用方无法使用，也不等于最终 wasm 一定保留该翻译单元；微信小游戏与浏览器 WebCodecs 应分别决策。

## 三、推荐修复批次

### 批次 1：播放正确性与公开 API

- M1、M5-1、19、21–26、28、30、31、36。
- 先确定 `ExportPlaybackPlan` 和公开选项，再修改调用方及测试，避免反复破坏字段布局。

### 批次 2：子进程与输出安全

- 一般 1–6、32–34、M5-4。
- 统一进程状态机、有界终止、exec 诊断、staging 发布和 encoder 参数边界；保留 MP4 faststart。

### 批次 3：下载供应链与安装并发

- 12–17、35。
- 按“固定 manifest → 安全传输 → 安全解压 → 有界资源消耗 → 锁所有权”实施，并为每项补恶意输入测试。

### 批次 4：CLI 统一与 dry-run

- M2、M5-2、M5-3、M6-1、M6-2、8–11、18、25、27。
- 原生拥有参数和 ffmpeg 定位语义，JS 只负责进程启动；dry-run 保持无执行、无临时文件副作用。

### 批次 5：安装策略、CI 与收尾

- M3、M4、M5-5、7、37–39。
- 29 不修改生产代码，直接在 PR 回复分层理由。

## 四、合并前最低验收要求

- 嵌套动画、StateMachine、静态文档的默认帧数和实际像素变化符合播放合同。
- 无 ffmpeg 环境下 dry-run 可用，真实导出仍严格失败并提供恢复提示。
- POSIX/Windows 写失败不挂起，exec 失败有诊断，不会终止无关 PID。
- MP4 保留 faststart，临时文件和额外输出均受控，成功才发布最终文件。
- 默认下载具备包内固定信任根，非 loopback 全链路 HTTPS，恶意归档无法越界或引入链接/特殊文件。
- 下载、解压和安装锁均有明确资源上限与并发所有权保护。
- Node 下载器和原生进程测试在 macOS、Linux、Windows 实际执行。
- 完成公开 API、native/wasm 支持矩阵及文档不可变契约的最终确认。
