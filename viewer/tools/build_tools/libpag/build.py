import os
import sys
import platform

common_module_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if common_module_dir not in sys.path:
    sys.path.append(common_module_dir)

from common.utils import  *

def build(cmakePrefixPath: str, rootDir: str, sourceDir: str, buildType: str):
    arch = ''
    suffix = ''
    current_os = ''
    tgfx_archs = []
    if platform.system() == 'Windows':
        arch = 'x64'
        suffix = '.lib'
        current_os = 'win'
        tgfx_archs = ['x64']
    else:
        arch = 'universal'
        suffix = '.a'
        current_os = 'mac'
        tgfx_archs = ['x64', 'arm64']

    lib_out_path = os.path.join(rootDir, 'third_party', 'out', 'libpag', 'lib', buildType)
    include_out_path = os.path.join(rootDir, 'third_party', 'out', 'libpag', 'include')
    if os.path.exists(lib_out_path) and os.path.exists(include_out_path):
        print(f'Libpag path[{lib_out_path}] is exist, skip build')
        return

    build_params = []
    build_params.append('node')
    build_params.append(os.path.join(rootDir, 'tools', 'vendor_tools', 'cmake-build'))
    build_params.append('pag')
    build_params.append('-p')
    build_params.append(current_os)
    build_params.append('-a')
    build_params.append(arch)
    build_params.append('-s')
    build_params.append(sourceDir)
    if buildType == 'Debug':
        build_params.append('--debug')
    build_params.append('--verbose')
    build_params.append('-DPAG_BUILD_SHARED=OFF')
    build_params.append('-DPAG_USE_ENCRYPT_ENCODE=ON')
    if current_os == 'win':
        build_params.append('-DPAG_USE_FFMPEG=OFF')
    build_params.append('-DPAG_BUILD_FRAMEWORK=OFF')
    build_params.append('-DPAG_USE_MOVIE=ON')
    build_params.append('-DPAG_USE_QT=ON')
    build_params.append('-DPAG_USE_RTTR=ON')
    build_params.append('-DPAG_USE_LIBAVC=OFF')
    build_params.append('-DPAG_BUILD_TESTS=OFF')
    build_params.append(f'-DCMAKE_PREFIX_PATH={cmakePrefixPath}')

    runCommand(' '.join(build_params))


    for tgfx_arch in tgfx_archs:
        build_params = []
        build_params.append('node')
        build_params.append(os.path.join(rootDir, 'tools', 'vendor_tools', 'cmake-build'))
        build_params.append('tgfx-vendor')
        build_params.append('pag-vendor')
        build_params.append('-p')
        build_params.append(current_os)
        build_params.append('-a')
        build_params.append(tgfx_arch)
        build_params.append('-s')
        build_params.append(sourceDir)
        if buildType == 'Debug':
            build_params.append('--debug')
        build_params.append('--verbose')
        build_params.append('-DPAG_BUILD_SHARED=OFF')
        build_params.append('-DPAG_USE_ENCRYPT_ENCODE=ON')
        if current_os == 'win':
            build_params.append('-DPAG_USE_FFMPEG=OFF')
        build_params.append('-DPAG_BUILD_FRAMEWORK=OFF')
        build_params.append('-DPAG_USE_MOVIE=ON')
        build_params.append('-DPAG_USE_QT=ON')
        build_params.append('-DPAG_USE_RTTR=ON')
        build_params.append('-DPAG_USE_LIBAVC=OFF')
        build_params.append('-DPAG_BUILD_TESTS=OFF')
        build_params.append(f'-DCMAKE_PREFIX_PATH={cmakePrefixPath}')

        runCommand(' '.join(build_params))

    if current_os == "mac":
        lib_name = f'libtgfx-vendor{suffix}'
        x64_lib = os.path.join(sourceDir, 'out', 'x64', 'x64', 'tgfx', 'CMakeFiles', 'tgfx-vendor.dir', 'x64', lib_name)
        arm64_lib = os.path.join(sourceDir, 'out', 'arm64', 'arm64', 'tgfx', 'CMakeFiles', 'tgfx-vendor.dir', 'arm64', lib_name)
        command = f'lipo -create {x64_lib} {arm64_lib} -output {os.path.join(sourceDir, 'out', arch, lib_name)}'
        runCommand(command)

        lib_name = f'libpag-vendor{suffix}'
        x64_lib = os.path.join(sourceDir, 'out', 'x64', lib_name)
        arm64_lib = os.path.join(sourceDir, 'out', 'arm64', lib_name)
        command = f'lipo -create {x64_lib} {arm64_lib} -output {os.path.join(sourceDir, 'out', arch, lib_name)}'
        runCommand(command)


    libs_generated_path = []
    libs_generated_path.append(os.path.join(sourceDir, 'out', arch, f'libpag{suffix}'))
    libs_generated_path.append(os.path.join(sourceDir, 'out', arch, f'libpag-vendor{suffix}'))
    copyFileToDir(libs_generated_path, lib_out_path, os.path.join(sourceDir, 'out', arch))

    libs_generated_path = []
    libs_generated_path.append(os.path.join(sourceDir, 'out', arch, arch, 'tgfx', 'CMakeFiles', 'tgfx-vendor.dir', arch, f'libtgfx-vendor{suffix}'))
    copyFileToDir(libs_generated_path, lib_out_path)

    includes_generated_path = []
    includes_generated_path.append(os.path.join(sourceDir, 'include', 'pag'))
    copyFileToDir(includes_generated_path, include_out_path, os.path.join(sourceDir, 'include'))

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    vendor_source_dir = os.path.dirname(root_dir)

    qt6_path = os.path.join(root_dir, 'QTCMAKE.cfg')
    cmake_prefix_path = getCmakePrefixPath(qt6_path)
    if cmake_prefix_path is None:
        print('Failed to get cmake prefix path')
        sys.exit(1)

    build_types = ['Release', 'Debug']

    for build_type in build_types:
        build(cmake_prefix_path, root_dir, vendor_source_dir, build_type)


if __name__ == "__main__":
    main()
