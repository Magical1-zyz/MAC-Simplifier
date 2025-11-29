cmake_minimum_required(VERSION 3.10)

# =========================================================
# 1. 自动判断编译器并设置 VCPKG 架构
# =========================================================
if(MINGW)
    # 检测到 MinGW (CLion 默认编译器)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MinGW Compiler. Target: x64-mingw-dynamic")
else()
    # 检测到 MSVC (Visual Studio)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MSVC Compiler. Target: x64-windows")
endif()

# 设置 vcpkg 工具链路径 (保持你原有的路径)
set(CMAKE_TOOLCHAIN_FILE "F:/Software/IncludePackage/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(MAC-Simplifier)

# --- 强制 MSVC 使用 UTF-8 编码，解决乱码警告 ---
if(MSVC)
    add_compile_options("/utf-8")
    add_compile_options("/W3")
    add_compile_options("/wd4267") # 屏蔽 size_t 转 int 警告
    add_compile_options("/wd4244") # 屏蔽 double 转 float 警告
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================
# 2. 依赖查找
# =========================================================
find_package(assimp CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

# =========================================================
# 3. 源文件与包含路径
# =========================================================
set(SOURCES
        src/main.cpp
        src/MACSimplifier.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/include)

# =========================================================
# 4. 生成与链接
# =========================================================
add_executable(MACSimplifier ${SOURCES})

# 链接库 (Assimp 和 Eigen)
target_link_libraries(MACSimplifier PRIVATE assimp::assimp Eigen3::Eigen)

# 针对 MinGW 的一些兼容性设置
if(MINGW)
    # 动态链接，去掉 -static
endif()

# Linux/Unix 兼容
if(UNIX)
    target_link_libraries(MACSimplifier pthread dl)
endif()