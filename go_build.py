# -*- encoding: utf-8 -*-

"""
全平台一键编译 isolated-vm 的工具脚本

author: Xia
"""

import os
import shutil
import subprocess
import argparse

def run_command_only_value(*cmd):
    try:
        return subprocess.check_output(
            [str(_) for _ in cmd],
            text=True,
            stderr=subprocess.DEVNULL,
            shell=os.sep != '/'
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    pass

def run_command_for_ok(*cmd):
    try:
        return subprocess.run(
            [str(_) for _ in cmd],
            cwd='.',
            shell=os.sep != '/'
        ).returncode
    except (subprocess.CalledProcessError, FileNotFoundError):
        return 1
    pass

if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description="isolated-vm编译工具")
    parser.add_argument("-C", "--init", help="是否生成 compile_commands.json", action="store_true")
    args = parser.parse_args()
    
    # Node
    if not shutil.which("node"):
        print()
        print("Build >> 未检测到 Node.js 环境, 请安装 Node.js 环境.")
        print("Build >> 推荐 Node v22.19.0 -> https://nodejs.org/en/download")
        print()
        exit(1)
        pass
    
    print(f"\n检测到 Node.js 环境: {run_command_only_value('node', '--version')}")
    
    # npm
    if not shutil.which("npm"):
        print()
        print("Build >> 未检测到 NPM 环境, 请安装 NPM 环境.")
        print()
        exit(1)
        pass
    
    print(f"检测到 NPM 环境: v{run_command_only_value('npm', '--version')}")
    
    # node-gyp
    if not shutil.which("node-gyp"):
        print()
        gyp_flag = str(input("Build >> 未检测到 node-gyp 环境, 是否安装? (y/n)[默认y]: "))
        
        if gyp_flag == 'n':
            print("正在退出编译程序...")
            print()
            exit(1)
            pass
        
        os.system("npm install -g node-gyp")
        print()
        pass
    
    print(f"检测到 node-gyp: {run_command_only_value('node-gyp', '--version')}")
    
    # compile_commands.json
    if args.init:
        
        print("\nBuild >> 生成 compile_commands.json 中...\n")
        
        os.system("node-gyp configure -- -f gyp.generator.compile_commands_json.py")
        
        print("\nBuild >> compile_commands.json 生成完毕\n")
        
        # 自动将 compile_commands.json 移动到根目录
        shutil.move("./build/Release/compile_commands.json", './compile_commands.json')
        
        pass
    
    print("\nBuild >> 开始编译任务...")
    
    compile_flag = run_command_for_ok("npm", "run", 'rebuild')
    
    if compile_flag == 0:
        
        shutil.move("./build/Release/isolated_vm.node", './out/isolated_vm.node')
        
        print("\nBuild >> 编译成功, 已自动移动结果到 ./out 下\n")
    else:
        print("\nBuild >> 编译失败!")
        pass
    
    
    pass