English | [简体中文](./README.zh_CN.md)

# QT Demo

Before building the QT demo project, make sure you have successfully built the libpag library.

## macOS

Open the `qt` folder in CLion, then open the `qt/QTCMAKE.cfg` file and update the QT path to your 
local QT installation path. After that, you can build and run the `PAGViewer` target.

## Windows

- Follow the build guide in [README.md](./../README.md) to install the necessary VS2019 modules and ensure the CLion ToolChain is set to Visual Studio with **amd64** architecture.
- Open the `qt` folder in CLion, then open the `qt/QTCMAKE.cfg` file and update the QT path to your local QT installation path. For example: `C:/Qt/Qt6.8.1/6.8.1/msvc2017_64/lib/cmake`.
- In CLion, open the configuration panel for the `PAGViewer` target and set the local QT DLL library path in the `Environment Variables` field. For example: `PATH=C:\Qt\Qt6.8.1\6.8.1\msvc2017_64\bin`.
- Finally, build and run the `PAGViewer` target.




