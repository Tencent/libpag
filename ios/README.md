English | [简体中文](./README.zh_CN.md)

# pag-ios

A minimal project that demonstrates how to integrate the libpag library into your iOS project.

## Prerequisites

Before you begin building the demo project, please make sure to carefully follow the instructions
provided in the [README.md](../README.md) file located in the root directory. That documentation
will guide you through the necessary steps to configure your development environment.

## Build & Run

Run the following command in the ios/ directory or simply double-click on it:

```
./gen_ios
```

This will generate an XCode project for iPhone devices. If you prefer to generate a project for the
simulator, use the following command instead:

```
./gen_simulator
```

This will generate a simulator project for the native architecture, for example, `arm64` for
Apple Silicon Macs and `x64` for Intel Macs. If you want to generate a project for the specific
architecture, you can use the `-a` option, for example:

```
./gen_simulator -a x64
```

Additionally, you can pass cmake options using the `-D` option. For example, if you want to generate
a project with webp encoding support, please run the following command:

```
./gen_ios -DPAG_USE_WEBP_ENCODE=ON
```

At last, launch XCode and open the ios/PAGViewer.xcworkspace. You'll be ready to go!
