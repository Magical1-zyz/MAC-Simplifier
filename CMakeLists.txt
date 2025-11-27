cmake_minimum_required(VERSION 3.10)

# --- VCPKG 配置 ---
set(CMAKE_TOOLCHAIN_FILE "F:/Software/IncludePackage/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)

project(MAC-Simplifier)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 依赖查找 ---
# 移除 tinygltf，添加 assimp
find_package(assimp CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

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
# 链接 Assimp 和 Eigen
target_link_libraries(MACSimplifier PRIVATE assimp::assimp Eigen3::Eigen)

if(MINGW)
    target_link_options(MACSimplifier PRIVATE -static)
endif()

if(UNIX)
    target_link_libraries(MACSimplifier pthread dl)
endif()