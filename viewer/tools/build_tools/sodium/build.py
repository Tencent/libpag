import os
import sys
import glob
import shutil
import platform

common_module_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if common_module_dir not in sys.path:
    sys.path.append(common_module_dir)

from common.utils import *

def buildForMacOS(rootDir: str, buildType: str):
    current_os = 'mac'
    origin_working_dir = os.getcwd()

    config_file_path = os.path.join(rootDir, 'third_party', 'sodium', 'configure')
    prefix_path = os.path.join(rootDir, 'third_party', 'out', 'sodium')
    include_out_dir = os.path.join(prefix_path, 'include')
    lib_out_path = os.path.join(prefix_path, 'lib', current_os, buildType)
    if os.path.exists(prefix_path):
        print(f'Sodium path[{prefix_path}] is exist, skip build')
        return

    build_dir = os.path.join(rootDir, 'third_party', 'sodium', 'build', buildType)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir, exist_ok=True)
    os.chdir(build_dir)

    build_params = []
    build_params.append(config_file_path)
    
    if buildType == 'Debug':
        build_params.append('CFLAGS=\"-O0 -g3 -DDEBUG -Wall -Wextra -arch x86_64 -arch arm64\"')
    elif buildType == 'Release':
        build_params.append('CFLAGS=\"-O2 -DNDEBUG -arch x86_64 -arch arm64\"')
    build_params.append('--prefix=' + prefix_path)
    build_params.append('--includedir=' + include_out_dir)
    build_params.append('--libdir=' + lib_out_path)    

    runCommand(' '.join(build_params))

    build_params = []
    build_params.append('make')
    build_params.append('-j10')
    runCommand(' '.join(build_params))

    build_params = []
    build_params.append('make')
    build_params.append('install')
    runCommand(' '.join(build_params))

    os.chdir(origin_working_dir)

def buildForWindows(rootDir: str):
    current_os = 'win'
    visual_studio_versions = getVisualStudioVersions()
    if not visual_studio_versions:
        print('No Visual Studio version found')
        exit(1)

    line_version = visual_studio_versions[0][0]
    display_version = visual_studio_versions[0][1]
    print(f'Use Visual Stidio {line_version} {display_version} to build sodium')

    source_dir = os.path.join(rootDir, 'third_party', 'sodium')
    script_path = os.path.join(source_dir, 'builds', 'msvc', 'build', 'buildbase.bat')
    sin_file_path = os.path.join(source_dir, 'builds', 'msvc', f'vs{line_version}', 'libsodium.sln')
    out_prefix_dir = os.path.join(rootDir, 'third_party', 'out', 'sodium')
    if os.path.exists(out_prefix_dir):
        print(f'Sodium path[{out_prefix_dir}] is exist, skip build')
        return

    build_params = []
    build_params.append(script_path)
    build_params.append(sin_file_path)
    build_params.append(display_version)

    runCommand(' '.join(build_params))

    build_types = ['Debug', 'Release']
    for build_type in build_types:
        libs_out_path = os.path.join(out_prefix_dir, 'lib', current_os, build_type)
        includes_out_path = os.path.join(out_prefix_dir, 'include')

        libps_generated_match_rule = []
        libps_generated_match_rule.append(os.path.join(source_dir, 'bin', 'x64', build_type, '*', 'dynamic', 'libsodium.dll'))
        libps_generated_match_rule.append(os.path.join(source_dir, 'bin', 'x64', build_type, '*', 'dynamic', 'libsodium.lib'))
        copyFileToDirByRule(libps_generated_match_rule, libs_out_path)

        includes_generated_path = []
        includes_generated_path.append(os.path.join(source_dir, 'src', 'libsodium', 'include', 'sodium.h'))
        includes_generated_path.append(os.path.join(source_dir, 'src', 'libsodium', 'include', 'sodium'))
        copyFileToDir(includes_generated_path, includes_out_path)


def main():

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    vendor_source_dir = os.path.join(root_dir, 'third_party', 'sodium')

    if platform.system() == 'Windows':
        patchDiff(vendor_source_dir, os.path.join(script_dir, 'sodium.patch'))
        buildForWindows(root_dir)
        restore(vendor_source_dir)
    else:
        build_types = ['Debug', 'Release']
        for build_type in build_types:
            buildForMacOS(root_dir, build_type)


if __name__ == "__main__":
    main()
