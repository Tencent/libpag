English | [简体中文](./README.zh_CN.md)

## Development
***

We recommend using the CLion IDE on macOS for development.

### Environment Setup

First, download the AESDK from the following link:

`https://developer.adobe.com/console/3505738/servicesandapis`

We recommend downloading the 2023 Windows version. After extraction, write the absolute path of the AESDK to `./AESDK.cfg`. Example content of the file:

`set(AE_SDK_PATH /Users/[username]/libpag/exporter/vendor/AfterEffectsSDK/Examples)`

Next, write the QT cmake path to `QTCMAKE.cfg`. Example content of the file:

`set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)`

Other environment dependencies are the same as [libpag](../README.md).

### Building the Project

Ensure that libpag can be compiled successfully before attempting to compile the exporter. Open the project root directory directly in CLion to start the compilation.

**For macOS:**

No additional CLion configuration is needed.

**For Windows:**

Follow these steps to configure CLion correctly:

- Ensure you have installed the **[Desktop development with C++]** and **[Universal Windows Platform development]** components for VS2019.
- Open the **File->Settings** panel, go to **Build, Execution, Deployment->Toolchains**, and set the toolchain to **Visual Studio** with **amd64 (Recommended)** or **x86** architecture.

**Note: If you encounter issues during the CMake build, update to the latest version of the CMake
command-line tool and try again.**