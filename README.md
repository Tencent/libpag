<img src="resources/readme/logo.png" alt="PAG Logo" width="474"/>

[![license](https://img.shields.io/badge/license-Apache%202-blue)](https://github.com/Tencent/libpag/blob/master/LICENSE.txt) [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/libpag/pulls) [![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Tencent/libpag)](https://github.com/Tencent/libpag/releases)

English | [简体中文](./README.zh_CN.md) | [Homepage](https://pag.io)

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files that renders both
vector-based and raster-based animations across most platforms, such as iOS, Android, macOS,
Windows, Linux, and Web.

PAG is an open-source file format for recording animations. PAG files can be created and exported
from Adobe After Effects with the PAGExporter plugin and previewed in the PAGViewer app, which you
can get from [pag.io](https://pag.io), and both of them are available on both macOS and Windows.

PAG is now being used by 40+ Tencent apps, such as WeChat, Mobile QQ, Honor of Kings Mobile Game,
Tencent Video, QQ Music, and so on, reaching hundreds of millions of users.

## Advantages

- **Highly efficient file format**

<img src="resources/readme/intro_1.png" alt="intro_1" width="282"/>

Benefiting from the highly efficient binary format design, PAG files can be decoded 10x faster than
JSON files but also are about 50% smaller in file size when exporting the same animations. Designers
can also ship beautiful animations with bitmaps or audiovisual media integrated into a single file
easily without other attachments.

- **All AE features supported**

<img src="resources/readme/intro_2.png" alt="intro_2" width="281"/>

While other solutions may only support exporting limited vector-based AE features, PAG supports
exporting all AE animations into a single file by combining vector-based exporting with raster-based
exporting techniques. Therefore, third-party plugin effects in AE can be exported as well.

- **Measurable performance**

<img src="resources/readme/intro_4.png" alt="intro_4" width="282"/>

PAG provides a monitoring panel in PAGViewer that shows normalized performance data for PAG files,
which helps designers to examine and optimize performance easily without developers. Along with
dozens of automatic optimization techniques from the PAGExporter plugin, animations with cool visual
effects and excellent performance now can be created more effectively.

- **Runtime editable animations**

<img src="resources/readme/intro_5.png" alt="intro_5" width="282"/>

With the flexible editing APIs from PAG SDK, developers can easily change the layer structure of a
single PAG file, mix multiple PAG files into one composition, or replace texts and images with all
pre-designed animation effects applied at runtime. It reduces tons of coding work for product
features like video templates.

## Requirements

- iOS 9.0 or later
- Android 4.4 or later
- macOS 10.13 or later
- Windows 7.0 or later
- Chrome 87.0 or later (Web)
- Safari 11.1 or later (Web)
- GCC 4.9 or later (Linux)

## Getting Started

We currently only publish precompiled libraries for iOS and Android. You can build libraries of
other platforms from the source code. The latest releases can be downloaded
from [here](https://github.com/Tencent/libpag/releases).

### iOS Installation

You can use the framework downloaded from the release page, or add libpag to your project by
CocoaPods:

Add the pod to your Podfile:

```
pod 'libpag'
```

And then run:

```
pod install
```

After installing the cocoapod into your project import libpag with

```
#import <libpag/xxx.h>
```

### Android Installation

You can use the aar downloaded from the release page, or add libpag to your project by Maven:

Edit the `build.gradle` file in the root of your project, add `jcenter()` to `repositories`:

```
buildscript {
    repositories {
        jcenter()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.2.1'
    }
}
```

Add libpag to `app/build.gradle` (*`3.2.7.34` should be replaced with the latest release version*):

```
dependencies {
    implementation 'com.tencent.tav:libpag:3.2.7.34'
}
```

Add the following rule to your proguard rules to avoid the wrong obfuscation.

```
  -keep class org.libpag.* {*;}
```

Finally, run gradle sync and then build the project.

### Documentation

- [iOS API Reference](https://pag.io/api.html#/apis/ios/)
- [Android API Reference](https://pag.io/api.html#/apis/android/org/libpag/package-summary.html)

You can find other docs on [pag.io](https://pag.io/docs/tech/sdk.html)

## Development

We recommend using CLion IDE on macOS platform for development.

### Branch Management

- The `main` branch is our active developing branch. After full testing, we will periodically pull the pre-release branch based on the main branch.
- The `release` branch is our stable release branch, which is fully tested and already used in many apps.

### Dependency Management

libpag uses `depsync` tool to manage third-party dependencies.

**For macOS platform：**

Just simply run the script in the root of libpag project:

```
./sync_deps.sh
```
This script will automatically install necessary tools and synchronize all third-party repositories.

**For other platforms：**

First, make sure you have installed the latest version of node.js (You may need to restart your
computer after this step). And then run the following command to install depsync tool:

```
npm install -g depsync
```

And then run `depsync` in the root directory of libpag project.

```
depsync
```

Git account and password may be required during synchronizing. Please make sure you have enabled the
`git-credential-store` so that `CMakeList.txt` can trigger synchronizing automatically next
time.

### Build

After the synchronization, open the project with CLion, then you are good to go!

## License

libpag is licensed under the [Apache Version 2.0 License](./LICENSE.txt)

## Contribution

If you have any ideas or suggestions to improve libpag, welcome to submit an [issue](https://github.com/Tencent/libpag/issues/new/choose) / [pull request](https://github.com/Tencent/libpag/pulls).
Before making a pull request or issue, please make sure to
read [Contributing Guide](./CONTRIBUTING.md).


