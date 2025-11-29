import subprocess
import os
import sys
import shutil

# --- 配置部分 ---
VCPKG_BIN_DIR = r"F:\Software\IncludePackage\vcpkg\installed\x64-windows\bin"
MINGW_BIN_DIR = r""

def find_in_path(filename):
    if MINGW_BIN_DIR and os.path.exists(os.path.join(MINGW_BIN_DIR, filename)):
        return os.path.join(MINGW_BIN_DIR, filename)
    if "PATH" in os.environ:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            if not path: continue
            full_path = os.path.join(path, filename)
            if os.path.exists(full_path):
                return full_path
    return None

def deploy_dlls(exe_path):
    if sys.platform != "win32": return
    exe_dir = os.path.dirname(exe_path)
    if os.path.exists(VCPKG_BIN_DIR):
        try:
            for filename in os.listdir(VCPKG_BIN_DIR):
                if filename.lower().endswith(".dll"):
                    src = os.path.join(VCPKG_BIN_DIR, filename)
                    dst = os.path.join(exe_dir, filename)
                    if not os.path.exists(dst): shutil.copy2(src, dst)
        except Exception as e: print(f"[Warning] Failed to copy vcpkg DLLs: {e}")

    mingw_libs = ["libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"]
    for dll in mingw_libs:
        dst_path = os.path.join(exe_dir, dll)
        if not os.path.exists(dst_path):
            found_path = find_in_path(dll)
            if found_path: shutil.copy2(found_path, dst_path)

def run_simplification(exe_path, input_model, output_model, ratio, w_norm=0.1, w_uv=0.1, w_boundary=10000.0):
    if not os.path.exists(exe_path):
        print(f"Error: Executable not found at {exe_path}")
        return

    output_dir = os.path.dirname(output_model)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if not os.path.exists(input_model):
        print(f"Error: Input file not found at {input_model}")
        return

    cmd = [
        exe_path,
        input_model,
        output_model,
        str(ratio),
        str(w_norm),
        str(w_uv),
        str(w_boundary)
    ]

    print(f"[Python] Executing C++ Core: {' '.join(cmd)}")

    try:
        deploy_dlls(exe_path)
        result = subprocess.run(cmd, capture_output=True, text=True, env=os.environ)

        if result.stdout and result.stdout.strip():
            print("--- C++ Output ---")
            print(result.stdout)

        if result.returncode != 0:
            print(f"ERROR: Failed with Return Code: {result.returncode}")
            if result.stderr: print(result.stderr)
        else:
            print(f"Success! Model and textures processed.")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    project_root = os.path.dirname(os.path.abspath(__file__))
    exe_name = "MACSimplifier"
    if sys.platform == "win32": exe_name += ".exe"

    build_dirs = ["cmake-build-release", "cmake-build-debug", "build/Release", "build"]
    exe_path = None
    for d in build_dirs:
        p = os.path.join(project_root, d, exe_name)
        if os.path.exists(p):
            exe_path = p
            break

    if not exe_path:
        print("Executable not found. Please build the C++ project first.")
        sys.exit(1)

    input_file = os.path.join("data", "mesh", "scene.gltf")
    output_file = os.path.join("out", "mesh", "scene.gltf")

    # 【参数调优】
    # w_norm: 0.5 (增强法线保护，防止光影崩坏)
    # w_uv: 0.5 (保护 UV)
    # ratio: 0.5 (保留一半)
    # w_boundary: 2000.0 (现在边界主要是指真正的建筑外轮廓，内部构件交界处已经合并了，所以不需要极大的值)
    run_simplification(exe_path, input_file, output_file, 0.5, 0.5, 0.5, 2000.0)