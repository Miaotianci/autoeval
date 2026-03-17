# AutoEval 故障排除指南

## 常见问题与解决方案

---

## 构建问题

### "spdlog not found"

**问题**：找不到spdlog

**解决方案**：
```bash
# 通过xmake安装
xmake require spdlog

# 或通过包管理器安装
# Ubuntu/Debian
sudo apt install libspdlog-dev

# macOS
brew install spdlog
```

### "nlohmann/json not found"

**问题**：找不到nlohmann_json

**解决方案**：
```bash
# 通过xmake安装
xmake require nlohmann_json

# 快速修复 - 复制单个头文件
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
sudo cp json.hpp /usr/local/include/nlohmann/json.hpp

# 或通过包管理器安装
sudo apt install nlohmann-json3-dev
```

---

## 运行时问题

### "No scene loaded"

**问题**：评估器报告未加载场景

**解决方案**：
- 检查文件路径是否正确
- 如果相对路径失败，使用绝对路径
- 验证文件格式（JSON、CSV、YAML）

```bash
# 检查文件是否存在
ls -la trajectory.json

# 使用绝对路径
autoeval_cli evaluate -d /full/path/to/trajectory.json
```

### "Metric not found"

**问题**：请求的指标不可用

**解决方案**：
```bash
# 列出可用指标
autoeval_cli list-metrics

# 检查拼写 - 必须完全匹配
collision        # 正确
Collision         # 错误 - 区分大小写
```

### 报告生成失败

**问题**：未创建HTML或PDF报告

**解决方案**：
- 确保输出目录存在
- 检查写入权限
- 先尝试JSON格式

```bash
# 创建输出目录
mkdir -p reports/

# 尝试更简单的格式
autoeval_cli evaluate -d data.json -f json
```

---

## 性能问题

### 评估速度慢

**问题**：评估时间过长

**解决方案**：
- 使用发布模式构建：`xmake config -m release`
- 对轨迹进行重采样减少点数

```cpp
// 在代码中
auto resampled = trajectory.resample(0.2);  // 更少的点
```

### 内存占用高

**问题**：评估使用过多内存

**解决方案**：
- 逐个评估指标，而不是一次评估所有
- 批量处理轨迹
- 在运行之间清除评估上下文

---

## 指标特定问题

### 碰撞检测误报

**问题**：在车辆未碰撞时报告碰撞

**解决方案**：
- 调整安全边距
```cpp
collision.setOption("safety_margin", "0.2");  // 默认0.1
```

### TTC显示不合理值

**问题**：碰撞时间过高或过低

**解决方案**：
- 检查轨迹是否有足够的点
- 验证速度值是否正确
- 调整`max_ttc`参数

```cpp
ttc_metric.setOption("max_ttc", "50.0");
```

### Jerk指标始终失败

**问题**：Jerk值持续偏高

**解决方案**：
- 确保轨迹有足够的分辨率
- 检查加速度计算
- 如需要增加阈值

```yaml
thresholds:
  - metric_name: jerk
    max: 5.0  # 从默认值2.0增加
```

---

## 平台特定问题

### Windows

**路径分隔符**：
```python
# 使用os.path.join实现跨平台路径
import os
path = os.path.join("data", "trajectory.json")
```

**行尾**：
```bash
# 如需要将CRLF转换为LF
dos2unix *.cpp *.h
```

### Linux

**库路径**：
```bash
# 添加库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### macOS

**头文件路径**：
```bash
# 确保能找到包含文件
export CPATH=/usr/local/include:$CPATH
```

---

## 调试

### 启用详细日志

```cpp
// 在C++中
autoeval::initialize();
evaluator.getContext()->setLogLevel(spdlog::level::debug);
```

### 检查依赖

```bash
# 验证库路径
ldd autoeval_cli          # Linux
otool -L autoeval_cli     # macOS
ldd autoeval_cli.exe       # Windows（通过cygwin）
```

---

## 获取帮助

### 查看日志

```bash
# 启用日志
autoeval_cli -v evaluate ...
```

### 最小复现

报告问题时，请提供：
1. AutoEval版本
2. 操作系统和编译器
3. 复现问题的最小代码/数据
4. 错误消息
5. 预期与实际行为

---

## 仍然卡住？

1. 查看[BUILD_GUIDE.md](BUILD_GUIDE.md)
2. 阅读[API.md](API.md)
3. 搜索现有[问题](https://github.com/yourusername/autoeval/issues)
4. 在[讨论区](https://github.com/yourusername/autoeval/discussions)提问
