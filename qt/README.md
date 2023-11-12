English | [简体中文](./README.zh_CN.md)

# QT Demo

Please make sure you have built the libpag library successfully before building the QT demo project.

## macOS

Open the `qt` folder with CLion, and then open the `qt/QTCMAKE.cfg` file to modify the QT path to 
your local QT installation path. Then you can build and run the `PAGViewer` target.

## Windows

- Please refer to the build guide in [README.md](./../README.md) to install the necessary modules of VS2019 and make sure the ToolChain of CLion is Visual Studio with **amd64** architecture.
- Open the `qt` folder with CLion, and then open the `qt/QTCMAKE.cfg` file to modify the QT path to your local QT installation path. For example: `C:/Qt/Qt5.13.0/5.13.0/msvc2017_64/lib/cmake`.
- Open the configuration panel of `PAGViewer` target in CLion, and then fill in the local QT DLL library path in the `Environment Variables` row. For example: `PATH=C:\Qt\Qt5.13.0\5.13.0\msvc2017_64\bin`.
- Finally, build and run the `PAGViewer` target.




