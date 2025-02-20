import os
import sys
import shutil
import platform

common_module_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if common_module_dir not in sys.path:
    sys.path.append(common_module_dir)

from common.utils import *

def copyFileToDir(files: list, targetDir: str):
    os.makedirs(targetDir, exist_ok=True)
    for file in files:
        if not os.path.exists(file):
            print(f'Failed to find {file}')
            exit(1)
        if os.path.isdir(file):
            dir_name = os.path.basename(file)
            target_path = os.path.join(targetDir, dir_name)
            if os.path.exists(target_path):
                shutil.rmtree(target_path)
            shutil.copytree(file, target_path, symlinks=True, dirs_exist_ok=True)
        else:
            shutil.copy2(file, targetDir)

def build(cmakePrefixPath: str, rootDir: str, buildType: str):
    current_os = ''
    if platform.system() == 'Windows':
        current_os = 'win'
    else:
        current_os = 'mac'

    lib_out_path = os.path.join(rootDir, 'third_party', 'out', 'log4qt', 'lib', current_os, buildType)
    if os.path.exists(lib_out_path):
        print(f'log4qt path[{lib_out_path}] is exist, skip build')
        return

    build_params = []
    build_params.append('node')
    build_params.append(os.path.join(rootDir, 'tools', 'vendor_tools', 'cmake-build'))
    build_params.append('log4qt')
    build_params.append('-p')
    build_params.append(current_os)
    build_params.append('-a')
    build_params.append('universal')
    if buildType == 'Debug':
        build_params.append('--debug')
    build_params.append('--verbose')
    build_params.append('-DBUILD_SHARED_LIBS=OFF')
    build_params.append(f'-DCMAKE_PREFIX_PATH={cmakePrefixPath}')

    runCommand(' '.join(build_params))
    
    libs_generated_path = []
    libs_generated_path.append(os.path.join(rootDir, 'third_party', 'log4qt', 'out', 'universal', 'liblog4qt.a'))

    copyFileToDir(libs_generated_path, lib_out_path)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    vendor_source_dir = os.path.join(root_dir, 'third_party', 'log4qt')

    patchDiff(vendor_source_dir, os.path.join(script_dir, 'log4qt.patch'))

    qt6_path = os.path.join(root_dir, 'QTCMAKE.cfg')
    cmake_prefix_path = getCmakePrefixPath(qt6_path)
    if cmake_prefix_path is None:
        print('Failed to get cmake prefix path')
        sys.exit(1)

    build_types = ['Debug', 'Release']

    for build_type in build_types:
        build(cmake_prefix_path, root_dir, build_type)


if __name__ == "__main__":
    main()
