import subprocess
import os
import sys
import shutil  # 新增：用于复制文件

# --- 配置你的 vcpkg bin 目录 ---
VCPKG_BIN_DIR = r"F:\Software\IncludePackage\vcpkg\installed\x64-windows\bin"

def deploy_dlls(exe_path):
    """
    将 vcpkg bin 目录下的所有 DLL 复制到 exe 所在目录。
    这是解决 Windows DLL 缺失或版本冲突（如 Anaconda 环境冲突）的最稳健方法。
    """
    if sys.platform != "win32":
        return

    if not os.path.exists(VCPKG_BIN_DIR):
        print(f"[Warning] vcpkg bin dir not found at: {VCPKG_BIN_DIR}")
        print("无法自动复制 DLL，请手动检查路径。")
        return

    exe_dir = os.path.dirname(exe_path)
    print(f"[Deploy] Checking dependencies in: {VCPKG_BIN_DIR}")

    dll_count = 0
    try:
        # 遍历 vcpkg bin 目录
        for filename in os.listdir(VCPKG_BIN_DIR):
            if filename.lower().endswith(".dll"):
                src_file = os.path.join(VCPKG_BIN_DIR, filename)
                dst_file = os.path.join(exe_dir, filename)

                # 如果目标不存在，或者源文件更新，则复制
                # 这里简单起见，只要不存在就复制
                if not os.path.exists(dst_file):
                    print(f"   -> Copying {filename} to build dir...")
                    shutil.copy2(src_file, dst_file)
                    dll_count += 1
                else:
                    # 可选：如果想强制覆盖，可以取消注释下面这行
                    # shutil.copy2(src_file, dst_file)
                    pass

        if dll_count > 0:
            print(f"[Deploy] Copied {dll_count} DLLs to {exe_dir}")
        else:
            print("[Deploy] DLLs already present (or none found).")

    except Exception as e:
        print(f"[Error] Failed to copy DLLs: {e}")

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
        print(f"       (Absolute path: {os.path.abspath(input_model)})")
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
        # 1. 运行前先尝试自动复制 DLL
        deploy_dlls(exe_path)

        # 2. 调用 C++ 程序
        result = subprocess.run(cmd, capture_output=True, text=True)

        # 打印 C++ 输出
        if result.stdout.strip():
            print("--- C++ Output ---")
            print(result.stdout)

        if result.returncode != 0:
            print("\n" + "="*30)
            print(f"ERROR: Simplification failed with Return Code: {result.returncode}")
            print(f"Hex Code: {result.returncode & 0xffffffff:X}")
            print("="*30)

            if result.stderr.strip():
                print("--- C++ Error Log ---")
                print(result.stderr)
            else:
                if sys.platform == "win32" and (result.returncode == 3221225781 or result.returncode == -1073741515):
                    print("\n[DIAGNOSIS]: DLL Issue Persists.")
                    print("虽然尝试了复制 DLL，但程序依然无法启动。")
                    print("可能原因：")
                    print("1. vcpkg 路径配置错误，没找到真正的 bin 目录。")
                    print("2. 你的 Anaconda 环境里的 DLL (如 zlib.dll) 依然在干扰。")
                    print("   建议：直接进入 cmake-build-release 文件夹双击运行 exe 看看报错弹窗。")
        else:
            print(f"Simplification finished successfully. Saved to {output_model}")

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
            os.path.join(project_root, "build", "Debug", exe_name),
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
        sys.exit(1)

    print(f"Found executable: {exe_path}\n")

    # --- 输入输出路径配置 ---
    input_file = os.path.join("data", "SM_Bp_Building05_C_1", "SM_Bp_Building05_C_1.gltf")
    output_file = os.path.join("out", "SM_Bp_Building05_C_1", "SM_Bp_Building05_C_1.gltf")

    run_simplification(exe_path, input_file, output_file, 0.5, 0.2, 0.2)