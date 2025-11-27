cmake_minimum_required(VERSION 3.10)

# --- VCPKG 配置 ---
set(CMAKE_TOOLCHAIN_FILE "F:/Software/IncludePackage/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)

project(MAC-Simplifier)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 核心修改开始 ---

# 1. 查找 tinygltf 的依赖项 nlohmann_json (vcpkg 必定会自动安装这个)
find_package(nlohmann_json CONFIG REQUIRED)

# 2. 手动查找 tinygltf 头文件路径，而不是依赖 Config 文件
# 因为 tinygltf 是纯头文件库，找到 tiny_gltf.h 所在的目录即可
find_path(TINYGLTF_INCLUDE_DIR NAMES tiny_gltf.h)

if(NOT TINYGLTF_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find tiny_gltf.h. Please run: .\\vcpkg install tinygltf:x64-windows")
else()
    message(STATUS "Found tinygltf headers: ${TINYGLTF_INCLUDE_DIR}")
endif()

# --- 核心修改结束 ---

# 添加源文件
set(SOURCES
        src/main.cpp
        src/MACSimplifier.cpp
)

# 设置本地 include 目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 生成可执行文件
add_executable(MACSimplifier ${SOURCES})

# --- 链接库 ---
# 1. 添加 tinygltf 的头文件路径
target_include_directories(MACSimplifier PRIVATE ${TINYGLTF_INCLUDE_DIR})

# 2. 链接 nlohmann_json (tinygltf 需要它)
target_link_libraries(MACSimplifier PRIVATE nlohmann_json::nlohmann_json)

# Linux/Unix 平台特定的系统库链接
if(UNIX)
    target_link_libraries(MACSimplifier pthread dl)
endif()