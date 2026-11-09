# FFmpeg for pagx MP4 export

This directory builds a real, minimal FFmpeg executable with static x264 support for
`pagx export --format mp4`. It is self-contained and can be moved to a standalone
repository without depending on the parent libpag repository.

The build is intentionally limited to the components used by pagx:

- raw RGBA video from a file or stdin pipe;
- H.264 encoding through libx264;
- scale and format conversion filters;
- MP4 muxing with fast-start support.

Network access, devices, other codecs, ffplay, ffprobe, documentation, and external
library auto-detection are disabled. The resulting executable uses no external x264
or FFmpeg shared libraries, but it may still use normal operating-system libraries.

## Pinned sources

`DEPS` fixes both source revisions. Do not replace them with moving branches.

| Project | Version | Commit |
| --- | --- | --- |
| FFmpeg | n7.1.2 | `f893221c8d89cb798b829bebe71d55e1a3f242fd` |
| x264 | stable | `b35605ace3ddf7c1a5d67a2eb553f034aef41d55` |

## Prerequisites

All platforms require Git, a native compiler, Make, pkg-config, Node.js, and depsync.
Install depsync with:

```bash
npm install -g depsync
```

x64 builds also require NASM. Only native compilation is supported; cross-compilation
is deliberately rejected so that the requested target cannot silently disagree with
the produced executable.

### macOS

```bash
xcode-select --install
brew install nasm pkg-config node
```

### Linux

On Debian or Ubuntu:

```bash
sudo apt-get install build-essential git nasm pkg-config nodejs npm
```

Other distributions need the equivalent development tools.

### Windows

Install MSYS2, then install the x64 toolchain in an MSYS2 shell:

```bash
pacman -S --needed base-devel git make nasm pkgconf nodejs npm \
  mingw-w64-x86_64-toolchain
```

For Windows ARM64, install the LLVM-MinGW toolchain instead:

```bash
pacman -S --needed base-devel git make pkgconf nodejs npm \
  mingw-w64-clang-aarch64-toolchain
```

`build.ps1` selects `MINGW64` for x64 and `CLANGARM64` for ARM64. Set `MSYS2_ROOT`
if MSYS2 is not installed at `C:\msys64`.

## Build

macOS and Linux:

```bash
./build.sh                         # Detect the host platform and architecture
./build.sh --platform mac --arch arm64 --jobs 8
./build.sh --platform linux --arch x64
```

Windows PowerShell:

```powershell
./build.ps1 -Arch x64 -Jobs 8
./build.ps1 -Arch arm64
```

Windows can also use `build.sh --platform win` directly from the matching MSYS2
environment. Sources are synchronized before each normal build. To rebuild existing
sources without network access, pass `--skip-sync`.

The output layout is:

```text
out/<platform>-<arch>/
├── bin/ffmpeg[.exe]
├── build-info.json
└── share/licenses/
```

`build-info.json` records the actual source commits, configure arguments, target, and
SHA-256 digest of the executable. Every build verifies the required FFmpeg components
and encodes three generated RGBA frames into a real MP4 through stdin. A build that
cannot perform this encoding is treated as failed.

To remove one target's intermediate files and output:

```bash
./build.sh --platform mac --arch arm64 --clean
```

On Windows, use `./build.ps1 -Arch x64 -Clean`.

## Use with pagx

Pass the executable explicitly:

```bash
pagx export input.pagx --format mp4 --ffmpeg /absolute/path/to/ffmpeg output.mp4
```

Or set `FFMPEG_BIN` to the absolute executable path. This directory does not alter the
pagx CLI, its PATH lookup, or the npm package layout.

## Licensing

x264 is licensed under the GNU GPL, and this build enables FFmpeg's GPL components.
No non-free component is enabled. The generated program is therefore subject to the
GPL; review the copied FFmpeg and x264 license files and satisfy source-code and
license obligations before redistributing any executable. This note is not legal advice.
