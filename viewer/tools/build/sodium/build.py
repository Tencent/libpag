import os
import sys
import shutil
import platform

common_module_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if common_module_dir not in sys.path:
    sys.path.append(common_module_dir)

from common.utils import *

def build(rootDir: str, buildType: str):
    current_os = ''
    if platform.system() == 'Windows':
        current_os = 'win'
    else:
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

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    build_types = ['Debug', 'Release']

    for build_type in build_types:
        build(root_dir, build_type)


if __name__ == "__main__":
    main()
