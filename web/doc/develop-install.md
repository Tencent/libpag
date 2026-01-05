# 开发安装

## 目录结构

```bash
├─ lib                        
│   ├─ libpag.cjs.js        ## CommonJs
│   ├─ libpag.cjs.js.map
│   ├─ libpag.esm.js        ## ESModule
│   ├─ libpag.esm.js.map
│   ├─ libpag.min.js        ## umd-min
│   ├─ libpag.min.js.map
│   ├─ libpag.umd.js        ## umd
│   ├─ libpag.umd.js.map
│   └─ libpag.wasm          ## WebAssembly
│
├─ types
│   ├─ core
│   ├─ h264
│   ├─ utils
│   └─ pag.d.ts
│
├─ CHANGELOG.md
├─ package.json
└─ README.md
```

## 产物说明

### lib

- lib 文件夹中打包了 `esm`、 `cjs` 和 `umd` 三种导出方式版本的 `Js` 文件， `min` 文件为 `umd` 版本的压缩文件
- `libpag.esm.js`  采用 ES Modules 标准 构建，可以使用 `import` 或者使用 `<script type="module">` 加载
- `libpag.cjs.js` 采用 CommonJs Modules 标准 构建，可以使用 `require` 加载
- `libpag.umd.js` 和 `libpag.min.js` 采用 UMD 模块标准 构建，可以直接在 Browser 环境中使用 `<script>` 加载
- `libpag.wasm` WebAssembly 二进制文件

### types

- 包含类型声明文件 `\*.d.ts`

## 安装

默认组件包的 main 入口指向 lib/libpag.cjs.js，esm 入口指向 libpag.esm.js

默认加载 `Js` 文件同目录下的 `libpag.wasm` 文件，可以用 `PAGInit ` 的参数中的 `locateFile` 方法重新指定 `wasm` 文件的地址
