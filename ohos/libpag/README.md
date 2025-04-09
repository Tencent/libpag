[![license](https://img.shields.io/badge/license-Apache%202-blue)](https://github.com/Tencent/libpag/blob/master/LICENSE.txt) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/libpag/pulls) 
[![codecov](https://codecov.io/gh/Tencent/libpag/branch/main/graph/badge.svg)](https://codecov.io/gh/Tencent/libpag)
[![Actions Status](https://github.com/Tencent/libpag/workflows/autotest/badge.svg?branch=main)](https://github.com/Tencent/libpag/actions)
[![build](https://github.com/Tencent/libpag/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/Tencent/libpag/actions/workflows/build.yml)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Tencent/libpag)](https://github.com/Tencent/libpag/releases)

[Homepage](https://pag.io)

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files that renders both
vector-based and raster-based animations across most platforms, such as iOS, Android, OpenHarmony, macOS,
Windows, Linux, and Web.

PAG is an open-source file format for recording animations. PAG files can be created and exported
from Adobe After Effects with the PAGExporter plugin and previewed in the PAGViewer app, which you
can get from [pag.io](https://pag.io), and both of them are available on both macOS and Windows.

PAG is now being used by 40+ Tencent apps, such as WeChat, Mobile QQ, Honor of Kings Mobile Game,
Tencent Video, QQ Music, and so on, reaching hundreds of millions of users.

## Advantages

- **Highly efficient file format**

<img src="https://pag.io/pag/1.apng" alt="intro_1" width="282"/>

Benefiting from the highly efficient binary format design, PAG files can be decoded 10x faster than
JSON files but also are about 50% smaller in file size when exporting the same animations. Designers
can also ship beautiful animations with bitmaps or audiovisual media integrated into a single file
easily without other attachments.

- **All AE features supported**

<img src="https://pag.io/pag/2.apng" alt="intro_2" width="282"/>

While other solutions may only support exporting limited vector-based AE features, PAG supports
exporting all AE animations into a single file by combining vector-based exporting with raster-based
exporting techniques. Therefore, third-party plugin effects in AE can be exported as well.

- **Measurable performance**

<img src="https://pag.io/pag/4.apng" alt="intro_4" width="282"/>

PAG provides a monitoring panel in PAGViewer that shows normalized performance data for PAG files,
which helps designers to examine and optimize performance easily without developers. Along with
dozens of automatic optimization techniques from the PAGExporter plugin, animations with cool visual
effects and excellent performance now can be created more effectively.

- **Runtime editable animations**

<img src="https://pag.io/pag/5.apng" alt="intro_5" width="282"/>

With the flexible editing APIs from PAG SDK, developers can easily change the layer structure of a
single PAG file, mix multiple PAG files into one composition, or replace texts and images with all
pre-designed animation effects applied at runtime. It reduces tons of coding work for product
features like video templates.

## System Requirements

- HarmonyOS Next 5.0.0(12) or later.

## Getting Started

You can use the har downloaded from the [release](https://github.com/Tencent/libpag/releases) page, or add libpag to your project by
OHPM:

```
ohpm install @tencent/libpag
```

Or, you can add it to your project manually.
Add the following lines to oh-package.json5 on your app module.
```
"dependencies": {
"@tencent/libpag": "^1.0.1",
}
```
Then run
```
ohpm install
```

## Development

We recommend using CLion IDE on the macOS platform for development.

### Branch Management

- The `main` branch is our active developing branch which contains the latest features and bugfixes. 
- The branches under `release/` are our stable milestone branches which are fully tested. We will 
periodically cut a `release/{version}` branch from the `main` branch. After one `release/{version}` 
branch is cut, only high-priority fixes are checked into it.

**Note: This repository only contains the latest code since PAG 4.0. To use the legacy PAG 3.0 
versions, you can download the precompiled libraries from [here](https://github.com/Tencent/libpag/releases).**

### Build Prerequisites

- Xcode 11.0+
- GCC 9.0+
- Visual Studio 2019+
- NodeJS 14.14.0+
- Ninja 1.9.0+
- CMake 3.13.0+
- QT 6.2.0+
- Emscripten 3.1.58+

### Dependency Management

libpag uses [depsync](https://github.com/domchen/depsync) tool to manage third-party dependencies.

**For macOS platform：**

Run the script in the root of the project:

```
./sync_deps.sh
```

This script will automatically install the necessary tools and synchronize all third-party repositories.

**For other platforms：**

First, make sure you have installed the latest version of node.js (You may need to restart your
computer after this step). And then run the following command to install depsync tool:

```
npm install -g depsync
```

And then run `depsync` in the root directory of the project.

```
depsync
```

Git account and password may be required during synchronizing. Please make sure you have enabled the
`git-credential-store` so that `CMakeList.txt` can trigger synchronizing automatically next time.

### Build

After the synchronization, you can open the project with CLion and build the pag library.

**For macOS platform：**

There are no extra configurations of CLion required.

**For Windows platform：**

Please follow the following steps to configure the CLion environment correctly:

- Make sure you have installed at least the **[Desktop development with C++]** and **[Universal Windows Platform development]** components for VS2019.
- Open the **File->Setting** panel, and go to **Build, Execution, Deployment->ToolChains**, then set the toolchain of CLion to **Visual Studio** with **amd64 (Recommended)** or **x86** architecture.

**Note: If anything goes wrong during cmake building, please update the cmake commandline tool to the latest
version and try again.**

## Support Us

If you find libpag is helpful, please give us a **Star**. We sincerely appreciate your support :)


[![Star History Chart](https://api.star-history.com/svg?repos=Tencent/libpag&type=Date)](https://star-history.com/#Tencent/libpag&Date)


## License

libpag is licensed under the [Apache Version 2.0 License](./LICENSE.txt)

## Privacy Policy

Please comply with [the personal information processing rules of PAG SDK](https://privacy.qq.com/document/preview/01e79d0cc7a2427ba774b88c6beff0fd) while using libpag SDK

## Contribution

If you have any ideas or suggestions to improve libpag, welcome to submit
an [issue](https://github.com/Tencent/libpag/issues/new/choose)
/ [pull request](https://github.com/Tencent/libpag/pulls). Before making a pull request or issue,
please make sure to read [Contributing Guide](./CONTRIBUTING.md).


