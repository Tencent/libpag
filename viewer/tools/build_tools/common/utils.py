import os
import re
import sys
import glob
import json
import shutil
import subprocess

def runCommand(command: str, output_file="build.log"):
    file_handle = None
    if output_file:
        file_handle = open(output_file, 'w', buffering=1)
        print(f'Run cmd[{command}] outoput will be wiritten to \"{os.path.abspath(output_file)}\"')
    
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

def copyFileToDir(files: list, targetDir: str, baseDir = None):
    os.makedirs(targetDir, exist_ok=True)
    for file in files:
        if not os.path.exists(file):
            print(f'Failed to find {file}')
            exit(1)

        if baseDir == None:
            shutil.copy2(file, targetDir)
        elif os.path.isdir(file):
            dir_name = os.path.basename(file)
            target_path = os.path.join(targetDir, dir_name)
            if os.path.exists(target_path):
                shutil.rmtree(target_path)
            shutil.copytree(file, target_path, symlinks=True, dirs_exist_ok=True)
        else:
            rel_path = os.path.relpath(file, baseDir)
            target_path = os.path.join(targetDir, rel_path)
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            shutil.copy2(file, target_path)

def copyFileToDirByRule(rules: list, targetDir: str, baseDir = None):
    for rule in rules:
        rule = os.path.normpath(rule)
        
        if baseDir == None:
            paths = glob.glob(rule)
        elif '**' not in rule:
            file_pattern = os.path.basename(rule)
            recursive_rule = os.path.join(baseDir, '**', file_pattern)
            paths = glob.glob(recursive_rule, recursive=True)
        else:
            paths = glob.glob(rule, recursive=True)
            
        if not paths:
            print(f'Failed to find path by {rule}')
            exit(1)
            
        copyFileToDir(paths, targetDir, baseDir)

def patchDiff(workingDir: str, patchFile: str):
    origin_working_dir = os.getcwd()
    os.chdir(workingDir)

    command = f'git apply {patchFile}'
    if runCommand(command) != True:
        print(f'Failed to patch diff to {workingDir}')
        # exit(1)

    os.chdir(origin_working_dir)

def restore(workingDir: str):
    origin_working_dir = os.getcwd()
    os.chdir(workingDir)

    command = f'git restore .'
    if runCommand(command) != True:
        print(f'Failed to restore path {workingDir}')

    os.chdir(origin_working_dir)

def getCmakePrefixPath(filePath: str):
    with open(filePath, 'r', encoding='utf-8') as f:
        for line in f:
            pattern = r'set\(CMAKE_PREFIX_PATH\s+(.*?)\)'
            match = re.search(pattern, line)
            if match:
                return match.group(1)
    return None

def getVisualStudioVersions():
    try:
        vswhere_path = os.path.expandvars(
            r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
        )
        
        if not os.path.exists(vswhere_path):
            print(f"vswhere.exe not found at: {vswhere_path}")
            return []

        cmd = [
            vswhere_path,
            "-all",
            "-format", "json",
            "-products", "*",
            "-prerelease"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            vs_instances = json.loads(result.stdout)
            versions = []
            for instance in vs_instances:
                installation_version = instance.get('installationVersion', '')
                if installation_version:
                    display_version = installation_version.split('.')[0]
                    version_map = {'17': '2022',
                                   '16': '2019',
                                   '15': '2017',
                                   '14': '2015',
                                   '12': '2013',
                                   '11': "2012",
                                   '10': "2010"}
                    line_version = version_map.get(display_version, '')
                    if line_version:
                        versions.append((line_version, display_version))
            return versions
            
        print(f"vswhere.exe returned error code: {result.returncode}")
        return []
        
    except Exception as e:
        print(f"Error: {e}")
        return []
