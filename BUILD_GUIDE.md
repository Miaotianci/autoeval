# AutoEval 构建指南

本指南提供在不同平台上构建AutoEval的详细说明。

## 目录

- [前置要求](#前置要求)
- [Windows](#windows)
- [Linux](#linux)
- [macOS](#macos)
- [故障排除](#故障排除)
- [测试](#测试)

---

## 前置要求

### 必需工具

| 工具 | 版本 | 用途 |
|------|---------|---------|
| C++ 编译器 | C++20兼容 | 编译 |
| xmake | 2.8+ | 构建系统 |

### C++ 编译器选项

**Windows：**
- Visual Studio 2022（MSVC 19.3+）
- MinGW-w64（GCC 11+）

**Linux：**
- GCC 11+
- Clang 13+

**macOS：**
- Clang 14+（Xcode 14+）

---

## Windows

### 使用Visual Studio（MSVC）

1. **安装Visual Studio 2022**
   ```
   下载地址：https://visualstudio.microsoft.com/
   所需组件：
   - C++ 桌面开发
   - CMake 工具（可选，用于代码浏览）
   ```

2. **通过vcpkg安装依赖**
   ```cmd
   # 克隆vcpkg
   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg

   # 引导vcpkg
   .\bootstrap-vcpkg.bat

   # 安装依赖
   .\vcpkg install spdlog:x64-windows yaml-cpp:x64-windows nlohmann-json:x64-windows fmt:x64-windows
   ```

3. **构建**
   ```cmd
   # 打开"x64 Native Tools Command Prompt for VS 2022"
   cd autoeval

   # 使用xmake构建
   xmake config -m release
   xmake build
   ```

### 使用MinGW

1. **安装MinGW-w64**
   ```
   下载地址：https://www.mingw-w64.org/
   或通过MSYS2：pacman -S mingw-w64-x86_64-gcc
   ```

2. **安装依赖**
   ```bash
   # 使用xmake（推荐）
   xmake require spdlog yaml-cpp nlohmann_json fmt
   ```

3. **构建**
   ```bash
   cd autoeval
   xmake config -m release
   xmake build
   ```

---

## Linux

### 使用包管理器

**Ubuntu/Debian：**
```bash
# 安装依赖
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libspdlog-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libfmt-dev

# 构建
cd autoeval
xmake config -m release
xmake build
```

**Fedora/RHEL：**
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    spdlog-devel \
    yaml-cpp-devel \
    nlohmann-json-devel \
    fmt-devel

cd autoeval
xmake config -m release
xmake build
```

**Arch Linux：**
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    git \
    spdlog \
    yaml-cpp \
    nlohmann-json \
    fmt

cd autoeval
xmake config -m release
xmake build
```

### 使用xmake

```bash
cd autoeval
xmake require spdlog yaml-cpp nlohmann_json fmt
xmake config -m release
xmake build
```

---

## macOS

### 使用Homebrew

```bash
# 安装依赖
brew install cmake git spdlog yaml-cpp nlohmann-json fmt

# 构建
cd autoeval
xmake config -m release
xmake build
```

### 使用MacPorts

```bash
sudo port install \
    cmake \
    git \
    spdlog \
    yaml-cpp \
    nlohmann-json3 \
    fmt

cd autoeval
xmake config -m release
xmake build
```

---

## 故障排除

### 常见问题

**"spdlog not found"**
- 通过xmake安装：`xmake require spdlog`
- 或通过包管理器安装spdlog

**"nlohmann/json not found"**
- 通过xmake安装：`xmake require nlohmann_json`
- 或通过包管理器安装nlohmann-json

**"yaml-cpp not found"**
- 通过xmake安装：`xmake require yaml-cpp`
- 或通过包管理器安装yaml-cpp

### 构建错误

**未定义引用错误**
- 检查所有依赖是否正确链接
- 验证依赖安装目录

**编译错误**
- 确保C++20支持可用
- 检查编译器版本是否符合要求

---

## 测试

### 运行测试

```bash
# 单元测试
xmake build autoeval_test
./build/windows/x64/debug/autoeval_test.exe

# 运行CLI
xmake build autoeval_cli
./build/windows/x64/debug/autoeval_cli.exe --help

# 列出指标
./build/windows/x64/debug/autoeval_cli.exe list-metrics

# 运行评估
./build/windows/x64/debug/autoeval_cli.exe evaluate -d examples/data/sample_trajectory.json -o reports/
```

---

## 高级配置

### 自定义安装前缀

```bash
xmake config --prefix=/opt/autoeval
```

### 构建类型

```bash
# 发布模式（默认）
xmake config -m release

# 调试模式
xmake config -m debug
```

### 启用 sanitizer（调试）

```bash
xmake fconfig -m debug -a sanitize=address,thread
```
