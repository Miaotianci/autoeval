# AutoEval API 参考文档

## 目录

- [概述](#概述)
- [核心类型](#核心类型)
- [数据加载](#数据加载)
- [指标](#指标)
- [评估](#评估)
- [报告生成](#报告生成)

---

## 概述

AutoEval是一个用于评估自动驾驶轨迹的C++库。该库提供：

- 轨迹数据加载（JSON、CSV）
- 内置安全性、舒适性和性能指标
- 用于自定义指标的可扩展插件系统
- 多格式报告生成（JSON、HTML、PDF）

---

## 核心类型

### TrajectoryPoint

轨迹中的单个点。

```cpp
struct TrajectoryPoint {
    double x, y, z;          // 位置（米）
    double heading;          // 航向角（弧度）
    double velocity;         // 纵向速度（m/s）
    double acceleration;     // 纵向加速度（m/s^2）
    double timestamp;        // 时间戳（秒）
    double curvature;        // 路径曲率（1/m）
    double confidence;       // 置信度分数（0.0到1.0）

    struct Dimensions {
        double length, width, height;
    } dimensions;

    double distanceTo(const TrajectoryPoint& other) const;
    TrajectoryPoint interpolate(const TrajectoryPoint& other, double ratio) const;
};
```

### Trajectory

具有元数据的完整轨迹。

```cpp
struct Trajectory {
    std::string id;
    std::vector<TrajectoryPoint> points;
    ObjectType type;
    std::string source;
    std::string algorithm;

    bool isValid() const;
    double duration() const;
    double length() const;
    std::optional<TrajectoryPoint> getPointAtTime(double t) const;
    Trajectory resample(double dt) const;
};
```

### Scene

包含多个轨迹的场景。

```cpp
struct Scene {
    std::string id;
    std::vector<Trajectory> trajectories;
    std::string location;
    std::string weather;
    std::string time_of_day;

    struct RoadInfo {
        double lane_width;
        int num_lanes;
        double speed_limit;
    } road_info;

    std::optional<Trajectory> getEgoTrajectory() const;
    std::vector<Trajectory> getTrajectoriesByType(ObjectType type) const;
    std::optional<Trajectory> getTrajectoryById(const std::string& id) const;
};
```

---

## 数据加载

### 加载轨迹

```cpp
#include "autoeval/loader/data_loader.h"

// 加载单个轨迹
Trajectory traj;
auto result = loadTrajectory("trajectory.json", traj);

// 加载多个轨迹
std::vector<Trajectory> trajectories;
loadTrajectories("scene.json", trajectories);

// 加载完整场景
Scene scene;
loadScene("scene.json", scene);
```

### 配置加载

```cpp
EvaluationConfig config;
loadConfig("config.yaml", config);

// 保存配置
saveConfig("output.yaml", config);
```

---

## 指标

### 内置指标

| 名称 | 类别 | 描述 | 单位 |
|------|----------|-------------|------|
| `collision` | 安全性 | 碰撞检测 | collision |
| `following` | 安全性 | 最小跟车距离 | m |
| `lane_change` | 安全性 | 换道安全性 | score |
| `trajectory_deviation` | 性能 | 轨迹偏差 | m |
| `speed_compliance` | 合规性 | 速度限制合规性 | km/h |
| `jerk` | 舒适性 | 纵向加加速度 | m/s^3 |
| `ttc` | 安全性 | 碰撞时间 | s |
| `curvature` | 性能 | 路径曲率 | 1/m |
| `smoothness` | 舒适性 | 轨迹平滑度 | score |

### 计算指标

```cpp
#include "autoeval/metrics/base_metric.h"

// 初始化内置指标
initializeBuiltInMetrics();

// 计算单个指标
auto result = computeMetric("collision", scene, context);
if (result) {
    std::cout << "值: " << result->value << " " << result->unit << std::endl;
    std::cout << "通过: " << result->pass << std::endl;
}

// 计算多个指标
auto results = computeMetrics({"collision", "ttc", "jerk"}, scene, context);
```

### 创建自定义指标

```cpp
class MyCustomMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override {
        return {
            .name = "my_metric",
            .display_name = "我的自定义指标",
            .description = "我的指标描述",
            .category = MetricCategory::Safety,
            .unit = "m",
            .default_comparison = "max"
        };
    }

    MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
        // 在这里计算您的指标
        MetricResult result = createResult(value, pass);
        result.message = "您的消息";
        return result;
    }
};
```

---

## 评估

### 使用评估器

```cpp
#include "autoeval/core/evaluator.h"

// 创建评估器
auto evaluator = std::make_unique<Evaluator>();

// 加载配置
evaluator->loadConfig("config.yaml");

// 或直接加载数据
evaluator->loadScene("trajectory.json");
evaluator->addMetrics({"collision", "ttc", "jerk"});

// 运行评估
auto summary = evaluator->evaluate();

// 获取结果
for (const auto& result : evaluator->getResults()) {
    std::cout << result.name << ": " << result.value << std::endl;
}

// 生成报告
evaluator->generateReport("report.json");
evaluator->generateReport("report.html");
```

### 快速评估

```cpp
// 使用默认指标快速评估
auto summary = quickEvaluate("trajectory.json", {"collision", "ttc"});

// 使用配置评估并生成报告
auto summary = evaluateAndReport("config.yaml", "./reports");
```

---

## 报告生成

### 生成报告

```cpp
#include "autoeval/report/report_generator.h"

// 从扩展名自动检测格式
generateReport(summary, "report.json");
generateReport(summary, "report.html");

// 指定格式
generateReport(summary, "output.report", "json");

// 生成多种格式
auto paths = generateReports(summary, "./reports", {"json", "html", "pdf"});
```

### 自定义报告主题

```cpp
HtmlReportGenerator generator;

ReportTheme theme;
theme.primary_color = "#0066cc";
theme.background_color = "#f5f5f5";

generator.setTheme(theme);
generator.generate(summary, "custom_report.html");
```

---

## 许可证

详见LICENSE文件。
