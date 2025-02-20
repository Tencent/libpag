import os
import sys
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

    lib_out_path = os.path.join(rootDir, 'third_party', 'out', 'winsparkle', 'lib', current_os, buildType)
    include_out_path = os.path.join(rootDir, 'third_party', 'out', 'winsparkle', 'include')
    if os.path.exists(lib_out_path) and os.path.exists(include_out_path):
        print(f'WinSparkle path[{lib_out_path}, {include_out_path}] is exist, skip build')
        return

    source_dir = os.path.join(rootDir, 'third_party', 'winsparkle')
    libps_generated_match_rule = []
    libps_generated_match_rule.append(os.path.join(source_dir, 'WinSparkle-*', 'x64', buildType, 'WinSparkle.lib'))
    libps_generated_match_rule.append(os.path.join(source_dir, 'WinSparkle-*', 'x64', buildType, 'WinSparkle.dll'))
    libps_generated_match_rule.append(os.path.join(source_dir, 'WinSparkle-*', 'x64', buildType, 'WinSparkle.pdb'))
    copyFileToDirByRule(libps_generated_match_rule, lib_out_path)

    includes_generated_match_rule = []
    includes_generated_match_rule.append(os.path.join(source_dir, 'WinSparkle-*', 'include'))
    copyFileToDirByRule(includes_generated_match_rule, include_out_path)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    build_types = ['Release']

    for build_type in build_types:
        build(root_dir, build_type)


if __name__ == "__main__":
    main()
