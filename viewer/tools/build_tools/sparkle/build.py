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

    lib_out_path = os.path.join(rootDir, 'third_party', 'out', 'sparkle', 'lib', current_os, buildType)
    if os.path.exists(lib_out_path):
        print(f'Sparkle path[{lib_out_path}] is exist, skip build')
        return

    working_dir = os.path.join(rootDir, 'third_party', 'sparkle')
    os.chdir(working_dir)

    if os.path.exists(os.path.join(working_dir, 'build')):
        shutil.rmtree(os.path.join(working_dir, 'build'), ignore_errors=True)

    build_params = []
    build_params.append('make')
    build_params.append('release')

    runCommand(' '.join(build_params))

    libps_generated_match_rule = []
    libps_generated_match_rule.append(os.path.join(working_dir, 'build', 'Sparkle.*', 'build', 'Products', buildType, 'Sparkle.framework'))
    copyFileToDirByRule(libps_generated_match_rule, lib_out_path)

    os.chdir(origin_working_dir)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    build_types = ['Release']

    for build_type in build_types:
        build(root_dir, build_type)


if __name__ == "__main__":
    main()
