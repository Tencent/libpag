English | [简体中文](./README.zh_CN.md)

# pag-mac

A minimal project that demonstrates how to integrate the libpag library into your macOS project.

## Prerequisites

Before you begin building the demo project, please make sure to carefully follow the instructions
provided in the [README.md](../README.md) file located in the root directory. That documentation
will guide you through the necessary steps to configure your development environment.

## Build & Run

Run the following command in the mac/ directory or simply double-click on it:

```
./gen_mac
```

This will generate a project for the native architecture, for example, `arm64` for Apple Silicon 
Macs and `x64` for Intel Macs. If you want to generate a project for the specific architecture, you 
can use the `-a` option, for example:

```
./gen_mac -a x64
```

Additionally, you can pass cmake options using the `-D` option. For example, if you want to generate
a project with webp encoding support, please run the following command:

```
./gen_mac -DPAG_USE_WEBP_ENCODE=ON
```

At last, launch XCode and open the mac/PAGViewer.xcworkspace. You'll be ready to go!
