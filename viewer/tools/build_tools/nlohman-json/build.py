import os
import sys
import shutil

common_module_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if common_module_dir not in sys.path:
    sys.path.append(common_module_dir)

from common.utils import *

def build(rootDir: str):
    include_out_dir = os.path.join(rootDir, 'third_party', 'out', 'nlohman-json', 'include')
    if os.path.exists(include_out_dir):
        print(f'log4qt path[{include_out_dir}] is exist, skip build')
        return
    
    include_src_path = []
    include_src_path.append(os.path.join(rootDir, 'third_party', 'nlohman-json', 'json.hpp'))

    copyFileToDir(include_src_path, include_out_dir)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    build(root_dir)


if __name__ == "__main__":
    main()