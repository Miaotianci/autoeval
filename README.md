# AutoEval - 自动驾驶评估框架

一个全面、可扩展的自动驾驶轨迹和算法评估框架。

[![编译状态](https://img.shields.io/badge/build-passing-green)](https://github.com/yourusername/autoeval)
[![版本](https://img.shields.io/badge/version-0.1.0-blue)](https://github.com/yourusername/autoeval)
[![许可证](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![C++](https://img.shields.io/badge/C++-20-blue)](https://en.cppreference.com/w/cpp/20)

## 功能特性

- **轨迹评估**：直接评估算法输出的轨迹
- **仿真评估**：支持基于仿真的评估（可扩展）
- **多种数据格式**：支持JSON、CSV，具有可扩展的加载器
- **内置指标**：全面的安全性、舒适性和性能指标
- **插件系统**：轻松扩展自定义指标和加载器
- **多格式报告**：支持生成HTML、JSON、PDF报告
- **命令行工具**：用于快速评估的命令行界面

## 安装

### 前置要求

- C++20兼容编译器（GCC 11+、Clang 13+、MSVC 2022）
- xmake 2.8+

### 依赖库

- spdlog - 日志记录
- yaml-cpp - YAML配置
- nlohmann/json - JSON处理
- fmt - 字符串格式化

### 使用xmake构建（推荐）

```bash
# 克隆仓库
git clone https://github.com/yourusername/autoeval.git
cd autoeval

# 安装依赖
xmake require

# 配置并构建
xmake config -m release
xmake build

# 运行测试
xmake test autoeval_test

# 安装（可选）
xmake install
```

## 快速开始

[快速入门指南](docs/QUICKSTART.md) - 5分钟内上手！

### 命令行

```bash
# 使用默认指标快速评估
autoeval_cli evaluate -d trajectory.json -o reports/

# 使用配置文件进行评估
autoeval_cli evaluate -c config.yaml

# 使用特定指标进行评估
autoeval_cli evaluate -d trajectory.json -m collision,ttc,smoothness

# 列出可用指标
autoeval_cli list-metrics

# 生成多种格式报告
autoeval_cli evaluate -d trajectory.json -f json,html,pdf
```

### C++ API

```cpp
#include "autoeval/core/evaluator.h"

// 创建评估器
auto evaluator = std::make_unique<core::Evaluator>();
evaluator->loadScene("trajectory.json");
evaluator->addMetrics({"collision", "ttc", "jerk"});

// 运行评估
auto summary = evaluator->evaluate();

// 生成报告
evaluator->generateReport("report.html");
```

## 配置

YAML配置文件示例：

```yaml
name: "my_evaluation"
description: "示例评估配置"

# 输入文件
trajectory_file: "data/trajectory.json"
output_dir: "./results"

# 要评估的指标
metrics:
  - collision
  - following
  - ttc
  - jerk

# 指标阈值
thresholds:
  - metric_name: collision
    max: 0
    comparison: "max"

  - metric_name: following
    min: 2.0
    comparison: "min"

  - metric_name: ttc
    min: 3.0
    comparison: "min"

# 报告设置
report:
  formats:
    - json
    - html
  include_charts: true
  include_details: true
```

## 内置指标

| 指标 | 类别 | 描述 | 单位 |
|--------|----------|-------------|------|
| `collision` | 安全性 | 碰撞检测 | collision |
| `following` | 安全性 | 最小跟车距离 | m |
| `ttc` | 安全性 | 碰撞时间 | s |
| `lane_change` | 安全性 | 换道安全性 | score |
| `jerk` | 舒适性 | 纵向加加速度 | m/s³ |
| `smoothness` | 舒适性 | 轨迹平滑度 | score |
| `trajectory_deviation` | 性能 | 轨迹偏差 | m |
| `curvature` | 性能 | 路径曲率 | 1/m |
| `speed_compliance` | 合规性 | 速度限制合规性 | km/h |

详见[指标指南](docs/Metrics.md)。

## 数据格式

### JSON轨迹格式

```json
{
  "id": "scene_001",
  "location": "urban_intersection",
  "road_info": {
    "lane_width": 3.5,
    "num_lanes": 2,
    "speed_limit": 13.89
  },
  "trajectories": [
    {
      "id": "ego_vehicle",
      "type": "ego",
      "points": [
        {"x": 0, "y": 0, "t": 0, "heading": 0, "v": 10, "a": 0},
        {"x": 1, "y": 0, "t": 0.1, "heading": 0, "v": 10, "a": 0}
      ]
    }
  ]
}
```

### CSV轨迹格式

```csv
id,x,y,z,t,heading,velocity,acceleration
ego,0.0,0.0,0.0,0.0,0.0,10.0,0.0
ego,1.0,0.0,0.0,0.1,0.0,10.0,0.0
```

## 可扩展性

### 自定义指标

```cpp
class MyMetric : public metrics::IMetric {
public:
    MetricMetadata getMetadata() const override {
        return {
            .name = "my_metric",
            .display_name = "我的指标",
            .description = "描述",
            .category = MetricCategory::Safety,
            .unit = "m",
            .default_comparison = "max"
        };
    }

    MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
        // 您的计算逻辑...
    }
};
```

详见[插件指南](docs/PluginGuide.md)。

### 自定义加载器

实现 `loader::IDataLoader` 以支持新的数据格式。

## 项目结构

```
autoeval/
├── include/autoeval/    # 公共头文件
│   ├── core/           # 核心类型和评估器
│   ├── metrics/        # 指标接口
│   ├── loader/         # 数据加载器
│   ├── plugin/         # 插件系统
│   └── report/         # 报告生成器
├── src/                # 实现
├── cli/                # 命令行工具
├── tests/              # 单元测试和集成测试
├── examples/           # 示例和示例数据
└── docs/               # 文档
```

## 贡献指南

1. Fork仓库
2. 创建功能分支
3. 进行修改
4. 添加测试
5. 提交Pull Request

## 许可证

MIT许可证 - 详见[LICENSE](LICENSE)。


## 致谢

- 基于[spdlog](https://github.com/gabime/spdlog)构建
- 使用[yaml-cpp](https://github.com/jbeder/yaml-cpp)
- 使用[nlohmann/json](https://github.com/nlohmann/json)

## 联系方式

- 问题反馈：[GitHub Issues](https://github.com/yourusername/autoeval/issues)
- 讨论区：[GitHub Discussions](https://github.com/yourusername/autoeval/discussions)
