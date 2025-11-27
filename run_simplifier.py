import subprocess
import os
import sys
import shutil

# --- 配置部分 ---

# 1. vcpkg bin 目录 (第三方库如 tinygltf/zlib)
VCPKG_BIN_DIR = r"F:\Software\IncludePackage\vcpkg\installed\x64-windows\bin"

# 2. MinGW bin 目录 (编译器运行时如 libgcc)
# 如果脚本自动查找失败，请手动将你的 MinGW bin 路径填入这里
# 例如: r"C:\Program Files\JetBrains\CLion 2024.1\bin\mingw\bin"
MINGW_BIN_DIR = r""

def find_in_path(filename):
    """在系统 PATH 和配置目录中查找文件"""
    # 1. 优先检查手动配置的 MINGW_BIN_DIR
    if MINGW_BIN_DIR and os.path.exists(os.path.join(MINGW_BIN_DIR, filename)):
        return os.path.join(MINGW_BIN_DIR, filename)

    # 2. 检查系统 PATH
    if "PATH" in os.environ:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"') # 去除可能存在的引号
            if not path: continue
            full_path = os.path.join(path, filename)
            if os.path.exists(full_path):
                return full_path
    return None

def deploy_dlls(exe_path):
    """
    将所有必要的 DLL (vcpkg 和 MinGW) 复制到 exe 所在目录。
    """
    if sys.platform != "win32":
        return

    exe_dir = os.path.dirname(exe_path)

    # --- 1. 部署 vcpkg 依赖 (zlib, etc.) ---
    if os.path.exists(VCPKG_BIN_DIR):
        try:
            for filename in os.listdir(VCPKG_BIN_DIR):
                if filename.lower().endswith(".dll"):
                    src = os.path.join(VCPKG_BIN_DIR, filename)
                    dst = os.path.join(exe_dir, filename)
                    if not os.path.exists(dst):
                        shutil.copy2(src, dst)
        except Exception as e:
            print(f"[Warning] Failed to copy vcpkg DLLs: {e}")
    else:
        print(f"[Warning] vcpkg bin dir not found: {VCPKG_BIN_DIR}")

    # --- 2. 部署 MinGW 运行时 (libgcc, libstdc++, etc.) ---
    # 这是解决 "找不到 libgcc_s_seh-1.dll" 的关键
    mingw_libs = [
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    ]

    print(f"[Deploy] Checking MinGW runtime libraries in {exe_dir}...")

    for dll in mingw_libs:
        dst_path = os.path.join(exe_dir, dll)
        if not os.path.exists(dst_path):
            found_path = find_in_path(dll)
            if found_path:
                print(f"   -> Found {dll} at {found_path}")
                print(f"   -> Copying to build dir...")
                shutil.copy2(found_path, dst_path)
            else:
                print(f"   [Warning] Could NOT find {dll} in PATH.")
                print("   请确保 MinGW 的 bin 目录在系统 PATH 中，或者在脚本开头手动设置 MINGW_BIN_DIR。")

def run_simplification(exe_path, input_model, output_model, ratio, w_norm=0.1, w_uv=0.1):
    if not os.path.exists(exe_path):
        print(f"Error: Executable not found at {exe_path}")
        return

    # 确保输出目录存在
    output_dir = os.path.dirname(output_model)
    if output_dir and not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir)
        except OSError as e:
            print(f"Error creating output directory: {e}")
            return

    if not os.path.exists(input_model):
        print(f"Error: Input file not found at {input_model}")
        return

    cmd = [
        exe_path,
        input_model,
        output_model,
        str(ratio),
        str(w_norm),
        str(w_uv)
    ]

    print(f"[Python] Executing: {' '.join(cmd)}")

    try:
        # 1. 尝试复制 DLL (包含 MinGW 修复)
        deploy_dlls(exe_path)

        # 2. 调用 C++ 程序
        result = subprocess.run(cmd, capture_output=True, text=True, env=os.environ)

        # 打印 C++ 输出
        if result.stdout and result.stdout.strip():
            print("--- C++ Output ---")
            print(result.stdout)

        # 检查返回值
        if result.returncode != 0:
            print("\n" + "="*40)
            print(f"ERROR: Failed with Return Code: {result.returncode}")
            print(f"Hex Code: {result.returncode & 0xffffffff:X}")
            print("="*40)

            if result.stderr and result.stderr.strip():
                print("--- C++ Error Log ---")
                print(result.stderr)

            # Windows DLL 缺失错误码检测 (0xC0000135)
            if sys.platform == "win32" and (result.returncode == 3221225781 or result.returncode == -1073741515):
                print("\n[严重错误] 依然缺少 DLL 文件！")
                print("请检查上方日志，看是否提示 'Could NOT find libgcc_s_seh-1.dll'。")
                print("-" * 30)
                print(">>> 尝试启动诊断弹窗... <<<")

                try:
                    os.startfile(exe_path)
                except Exception as e:
                    print(f"启动诊断失败: {e}")

        else:
            print(f"Success! Model saved to {output_model}")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    project_root = os.path.dirname(os.path.abspath(__file__))
    exe_name = "MACSimplifier"

    possible_paths = []

    if sys.platform == "win32":
        exe_name += ".exe"
        possible_paths = [
            os.path.join(project_root, "cmake-build-release", exe_name),
            os.path.join(project_root, "cmake-build-debug", exe_name),
            os.path.join(project_root, "cmake-build-relwithdebinfo", exe_name),
            os.path.join(project_root, "build", "Release", exe_name),
            os.path.join(project_root, "build", exe_name)
        ]
    else:
        possible_paths = [
            os.path.join(project_root, "build", exe_name),
            os.path.join(project_root, "cmake-build-release", exe_name)
        ]

    exe_path = None
    for p in possible_paths:
        if os.path.exists(p):
            exe_path = p
            break

    if exe_path is None:
        print(f"Error: Could not find '{exe_name}' executable.")
        print(f"Searched paths: {possible_paths}")
        sys.exit(1)

    print(f"Found executable: {exe_path}\n")

    # --- 输入输出路径 ---
    input_file = os.path.join("data", "SM_Bp_Building05_C_1", "SM_Bp_Building05_C_1.gltf")
    output_file = os.path.join("out", "SM_Bp_Building05_C_1", "SM_Bp_Building05_C_1.gltf")

    run_simplification(exe_path, input_file, output_file, 0.5, 0.2, 0.2)