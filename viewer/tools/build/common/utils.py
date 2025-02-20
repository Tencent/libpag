import os
import re
import sys
import shutil
import subprocess

def runCommand(command: str, output_file="build.log"):
    file_handle = None
    if output_file:
        file_handle = open(output_file, 'w', buffering=1)
        print(f'outoput will be wiritten to \"{os.path.abspath(output_file)}\"')
    
    try:
        proc = subprocess.Popen(
            command,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # 合并标准错误到标准输出
            text=True,
            bufsize=1  # 行缓冲模式
        )

        while True:
            line = proc.stdout.readline()
            if not line and proc.poll() is not None:
                break

            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
                if file_handle:
                    file_handle.write(line)
                    file_handle.flush()
                    
        return proc.returncode
        
    finally:
        if file_handle:
            file_handle.close()

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

def patchDiff(workingDir: str, patchFile: str):
    origin_working_dir = os.getcwd()
    os.chdir(workingDir)

    command = f'git apply {patchFile}'
    if runCommand(command) != True:
        print(f'Failed to patch diff to {workingDir}')
        os.chdir(origin_working_dir)
        # exit(1)

    os.chdir(origin_working_dir)

def getCmakePrefixPath(filePath: str):
    with open(filePath, 'r', encoding='utf-8') as f:
        for line in f:
            pattern = r'set\(CMAKE_PREFIX_PATH\s+(.*?)\)'
            match = re.search(pattern, line)
            if match:
                return match.group(1)
    return None
