# MAC-Simplifier: Multi-Attribute Collaborative Simplifier

**MAC-Simplifier** 是一个高性能的 C++ 3D 网格简化工具，硕士论文中MDAC-QEM (Multi-Dimensional Attribute Collaborative Quadratic Error Metric) 算法实现。

该项目旨在复现论文提到的核心预处理算法：在几何简化的同时，协同优化法线连续性和纹理坐标（UV）布局，确保模型在大幅减面后仍保持良好的光照特征和纹理映射。

---

## 📐 算法核心 (MDAC-QEM)

传统的 QEM 仅最小化几何距离误差。本算法引入了扩展的代价函数，实现了多维属性的协同优化：

$$Q_{total} = Q_{geo} + w_{norm} \cdot Q_{norm} + w_{uv} \cdot Q_{uv}$$

- **几何误差 ($Q_{geo}$)**: 基础形状近似，确保模型轮廓不崩坏。
- **法线约束 ($Q_{norm}$)**: 防止法线剧烈变化，保持模型原有的光照和着色特征。
- **UV 约束 ($Q_{uv}$)**: 引入基于 UV 包围盒大小的自适应缩放因子，防止纹理在简化过程中产生严重扭曲或拉伸。

---

## 📂 工程结构

```text
MAC-Simplifier/
├── include/        # 头文件 (.h)
├── src/            # 源代码 (.cpp)
├── scripts/        # 辅助 Python 脚本
├── CMakeLists.txt  # CMake 构建配置
└── README.md       # 项目说明
```

## 🛠️ 构建与编译
本项目使用 CMake 进行构建，并强烈建议使用 vcpkg 管理第三方依赖。

### 1. 环境依赖
- **编译器**: 支持 C++17 标准的编译器 (MSVC, GCC, Clang)
- **构建工具**: CMake >= 3.10
- **包管理器**: vcpkg
- **依赖库**:
    - assimp
    - eigen3

### 2. 安装依赖 (使用 vcpkg)
在构建之前，请确保已安装 vcpkg 并安装了对应架构的依赖库。以 Windows x64 为例：
```PowerShell
# 在 vcpkg 目录下运行
.\vcpkg install assimp
.\vcpkg install eigen3
```
### 3. 编译项目
#### 方式 A: 使用命令行 (通用)
请将 `<path/to/vcpkg>` 替换为你电脑上 vcpkg 的实际安装路径。

```Bash
  mkdir build
  cd build

  # 配置 CMake (指定 vcpkg 工具链)
  cmake .. -DCMAKE_TOOLCHAIN_FILE="<path/to/vcpkg>/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows

  # 编译 (Release 模式)
  cmake --build . --config Release
```

#### 方式 B: 使用 CLion (推荐)
1. 打开 Settings -> Build, Execution, Deployment -> CMake。

2. 在 CMake options 中添加以下参数：
```Plaintext

    -DCMAKE_TOOLCHAIN_FILE=<你的vcpkg路径>/scripts/buildsystems/vcpkg.cmake
    -DVCPKG_TARGET_TRIPLET=x64-windows
  ```
3. 点击 Reload CMake Project，然后点击右上角的构建按钮（🔨）。

## 🚀 运行与使用
编译生成的可执行文件通常位于 `build/Release/MACSimplifier.exe `(Windows) 或 `build/MACSimplifier` (Linux)。

#### 命令行参数
```Bash

MACSimplifier <input_model> <output_model> <ratio> [normal_weight] [uv_weight]
```
