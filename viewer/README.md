English | [简体中文](./README.zh_CN.md)

# PAGViewer

PAGViewer is a cross-platform desktop application for previewing and editing PAG animation files, built with Qt 6 and the libpag rendering engine.

## Features

- Real-time preview of PAG animation files
- Layer tree inspection and attribute editing
- Text layer content editing
- Image replacement in PAG files
- Performance profiling and analysis
- Auto-update support (Sparkle on macOS, WinSparkle on Windows)

## Requirements

- **Qt 6.2.0+** (Qt 6.8.1 recommended)
- **CLion** IDE
- **macOS 10.15+** or **Windows 10+** (64-bit)
- Successfully built libpag library (see [main README](../README.md))

## Build Instructions

### macOS

1. Ensure you have successfully built the libpag library following the [build guide](../README.md).

2. Open the `viewer` folder in CLion.

3. On first CMake refresh, a `QTCMAKE.cfg` file will be auto-generated. Update the Qt path to your local installation:
   ```cmake
   set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)
   ```

4. Build and run the `PAGViewer` target.

### Windows

1. Follow the [build guide](../README.md) to install the required VS2019 modules and configure CLion ToolChain to **Visual Studio** with **amd64** architecture.

2. Open the `viewer` folder in CLion.

3. Update the auto-generated `QTCMAKE.cfg` file with your local Qt path:
   ```cmake
   set(CMAKE_PREFIX_PATH C:/Qt/Qt6.2.0/6.2.0/msvc2019_64/lib/cmake)
   ```

4. In CLion, open the `PAGViewer` target configuration and add the Qt DLL path to **Environment Variables**:
   ```
   PATH=C:\Qt\Qt6.2.0\6.2.0\msvc2019_64\bin
   ```

5. Build and run the `PAGViewer` target.

## Command Line Options

```bash
PAGViewer [options] [file]

Options:
  -cpu    Force software rendering (disable GPU acceleration)

Arguments:
  file    Path to a PAG file to open on startup
```

## Project Structure

```
viewer/
├── src/                    # Source code
│   ├── audio/              # Audio playback
│   ├── editing/            # Layer and attribute editing
│   ├── maintenance/        # Update and plugin management
│   ├── platform/           # Platform-specific code (mac/win)
│   ├── profiling/          # Performance analysis
│   ├── rendering/          # PAG rendering integration
│   ├── task/               # Background task management
│   ├── utils/              # Utility functions
│   └── video/              # Video export
├── assets/                 # Resources
│   ├── qml/                # QML UI files
│   ├── images/             # Icons and images
│   └── translation/        # i18n files
├── package/                # Build and packaging scripts
└── vendor/                 # Third-party dependencies
```

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](../LICENSE.txt) file for details.
