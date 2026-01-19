English | [简体中文](./README.zh_CN.md)

# PAGExporter

PAGExporter is an Adobe After Effects plugin that enables designers to export motion graphics created in AE into PAG animation files with one click.

## Features

- **Pure Vector Export**: Export AE compositions as pure vector files for small file sizes and infinite scalability
- **Hybrid Export**: Combine BMP pre-compositions with vectors to support all AE features while maintaining runtime editability
- **Real-time Preview**: Preview PAG output directly within After Effects before exporting
- **Batch Export**: Export multiple compositions at once via the Panel interface
- **Export Validation**: Automatic checks for unsupported features with detailed warnings
- **PAGViewer Integration**: One-click installation of PAGViewer for desktop preview

## Menu Commands

After installation, the following commands are available in After Effects:

| Menu | Command | Description |
|------|---------|-------------|
| Edit > Preferences | PAG Config... | Configure export settings and preferences |
| File > Export | PAG File... | Export current composition to PAG file |
| File > Export | PAG File (Panel)... | Open export panel for batch operations |
| File > Export | PAG Preview... | Preview current composition as PAG |

## Requirements

- **Adobe After Effects 2020+** (CC 2023 recommended)
- **Qt 6.2.0+** (Qt 6.6.1 recommended)
- **CLion** IDE
- **macOS 10.15+** or **Windows 10+** (64-bit)
- **After Effects SDK** (2023 Windows version recommended)
- Successfully built libpag library (see [main README](../README.md))

## Build Instructions

### Environment Setup

1. **Download After Effects SDK**

   Download from: https://developer.adobe.com/console/3505738/servicesandapis

   We recommend the 2023 Windows version. After extraction, create `AESDK.cfg` in the exporter directory:
   ```cmake
   set(AE_SDK_PATH /path/to/AfterEffectsSDK/Examples)
   ```

2. **Configure Qt Path**

   Create `QTCMAKE.cfg` in the exporter directory:
   ```cmake
   set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)
   ```

3. **Build libpag First**

   Ensure libpag compiles successfully before building PAGExporter. See [build guide](../README.md).

### macOS

1. Open the `exporter` folder in CLion.
2. Build the `PAGExporter` target.
3. The plugin will be generated as `PAGExporter.plugin`.

### Windows

1. Ensure VS2019 has **Desktop development with C++** and **Universal Windows Platform development** components installed.

2. In CLion, go to **File > Settings > Build, Execution, Deployment > Toolchains** and set:
   - Toolchain: **Visual Studio**
   - Architecture: **amd64** (recommended) or **x86**

3. Open the `exporter` folder in CLion.

4. Build the `PAGExporter` target.

5. The plugin will be generated as `PAGExporter.aex`.

## Installation

### macOS
Copy `PAGExporter.plugin` to:
```
/Applications/Adobe After Effects [version]/Plug-ins/
```

### Windows
Copy the `PAGExporter.aex` and its dependencies to:
```
C:\Program Files\Adobe\Adobe After Effects [version]\Support Files\Plug-ins\
```

## Project Structure

```
exporter/
├── src/
│   ├── aecommand/          # AE menu command handlers
│   ├── config/             # Export configuration management
│   ├── export/             # Core export logic
│   ├── platform/           # Platform-specific code (mac/win)
│   ├── ui/                 # Qt window and model classes
│   └── utils/              # AE data conversion utilities
├── assets/
│   ├── qml/                # QML UI files
│   ├── images/             # Icons and images
│   └── translation/        # i18n files
├── scripts/                # Build and deployment scripts
├── templates/              # Version file templates
└── vendor/                 # Third-party dependencies
```

## Troubleshooting

- **CMake build errors**: Update CMake to the latest version and rebuild.
- **Plugin not appearing in AE**: Verify the plugin is in the correct Plug-ins directory and restart After Effects.
- **Export failures**: Check the AE console for detailed error messages.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](../LICENSE.txt) file for details.
