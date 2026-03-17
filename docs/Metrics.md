# AutoEval 指标指南

## 概述

AutoEval提供了一套全面的指标用于评估自动驾驶轨迹。指标按类别组织：

- **安全性**：碰撞检测、跟车距离、碰撞时间
- **舒适性**：加加速度、平滑度
- **性能**：轨迹偏差、曲率
- **合规性**：速度限制合规性

---

## 安全性指标

### 碰撞检测

**指标名称**：`collision`

检测自车是否与周围物体发生碰撞。

- **单位**：碰撞（计数）
- **比较**：max（越小越好）
- **默认阈值**：0.0

**实现细节**：
- 使用边界框碰撞检测
- 检查时间戳相似的物体
- 考虑车辆方向和尺寸

**选项**：
- `safety_margin`：碰撞检测的额外边距（默认：0.1m）

**示例**：
```yaml
thresholds:
  - metric_name: collision
    max: 0
    comparison: "max"
```

---

### 跟车距离

**指标名称**：`following`

测量自车与前车之间的最小跟车距离和时间gap。

- **单位**：m
- **比较**：min（越大越好）
- **默认阈值**：5.0m（或2.0s时间gap）

**实现细节**：
- 识别同一车道的前车
- 计算距离和时间gap
- 调整车辆长度（后到前）

**选项**：
- `min_time_gap`：最小安全时间gap（默认：2.0s）
- `look_ahead_time`：向前看前车的时间（默认：5.0s）

**示例**：
```yaml
thresholds:
  - metric_name: following
    min: 5.0
    comparison: "min"
```

---

### 碰撞时间（TTC）

**指标名称**：`ttc`

计算与周围物体的最小碰撞时间。

- **单位**：s
- **比较**：min（越大越好）
- **默认阈值**：3.0s

**实现细节**：
- 向前投影轨迹
- 基于相对速度计算碰撞时间
- 考虑所有周围物体

**选项**：
- `max_ttc`：最大TTC考虑值（默认：100s）
- `min_ttc_threshold`：最小安全TTC（默认：3.0s）

**示例**：
```yaml
thresholds:
  - metric_name: ttc
    min: 3.0
    comparison: "min"
```

---

### 换道安全性

**指标名称**：`lane_change`

评估换道操作的安全性。

- **单位**：分数
- **比较**：min（越大越好，1.0 = 安全）
- **默认阈值**：1.0

**实现细节**：
- 检测换道的横向移动
- 检查与相邻车辆的安全间隙
- 返回安全分数（0.0到1.0）

**选项**：
- `safety_time_gap`：换道期间最小安全时间gap（默认：1.5s）
- `lane_width`：车道宽度（默认：3.5m）

**示例**：
```yaml
thresholds:
  - metric_name: lane_change
    min: 1.0
    comparison: "min"
```

---

## 舒适性指标

### 纵向加加速度

**指标名称**：`jerk`

测量加速度的变化率。

- **单位**：m/s³
- **比较**：max（越小越好）
- **默认阈值**：2.0 m/s³

**实现细节**：
- 计算加速度的导数
- 值越高表示舒适性越差

**选项**：
- `max_jerk_threshold`：最大舒适加加速度（默认：2.0 m/s³）

**示例**：
```yaml
thresholds:
  - metric_name: jerk
    max: 2.0
    comparison: "max"
```

---

### 轨迹平滑度

**指标名称**：`smoothness`

测量轨迹的整体平滑度。

- **单位**：分数
- **比较**：max（越小越好）
- **默认阈值**：10.0

**实现细节**：
- 计算加加速度的平方积分
- 值越低表示轨迹越平滑

**示例**：
```yaml
thresholds:
  - metric_name: smoothness
    max: 10.0
    comparison: "max"
```

---

## 性能指标

### 轨迹偏差

**指标名称**：`trajectory_deviation`

测量与参考轨迹的偏差。

- **单位**：m
- **比较**：max（越小越好）
- **默认阈值**：0.5m

**实现细节**：
- 至少需要2条轨迹（评估的、参考的）
- 在匹配的时间戳计算欧几里得距离
- 返回最大偏差

**选项**：
- `reference_id`：参考轨迹的ID（默认：第二条轨迹）

**示例**：
```yaml
thresholds:
  - metric_name: trajectory_deviation
    max: 0.5
    comparison: "max"
```

---

### 路径曲率

**指标名称**：`curvature`

计算最大路径曲率。

- **单位**：1/m
- **比较**：max（越小越好）
- **默认阈值**：0.1 1/m

**实现细节**：
- 使用预计算的曲率值
- 返回最大绝对曲率

**示例**：
```yaml
thresholds:
  - metric_name: curvature
    max: 0.1
    comparison: "max"
```

---

## 合规性指标

### 速度合规性

**指标名称**：`speed_compliance`

检查车辆是否保持在速度限制内。

- **单位**：km/h
- **比较**：max（越小越好）
- **默认阈值**：0.0 km/h

**实现细节**：
- 需要带有限速的道路信息
- 计算最大超速量
- 报告超速量和持续时间

**示例**：
```yaml
thresholds:
  - metric_name: speed_compliance
    max: 0.0
    comparison: "max"
```

---

## 自定义指标

### 在C++中创建自定义指标

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
            .default_comparison = "max",
            .default_max = 1.0
        };
    }

    MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
        // 获取自车轨迹
        auto ego = scene.getEgoTrajectory();
        if (!ego) {
            MetricResult result;
            result.name = "my_metric";
            result.pass = false;
            result.message = "没有自车轨迹";
            return result;
        }

        // 计算您的指标值
        double value = computeMyValue(ego);

        // 与阈值比较
        bool pass = value < getMetadata().default_max;

        // 创建结果
        MetricResult result = createResult(value, pass);
        result.message = "我的计算结果";

        return result;
    }

private:
    double computeMyValue(const Trajectory* ego) const {
        // 您的计算逻辑
        return 0.0;
    }
};
```

### 作为插件创建自定义指标

```cpp
class MyMetricPlugin : public IMetricPlugin {
public:
    std::string getName() const override {
        return "my_metric_plugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::string getDescription() const override {
        return "我的自定义指标插件";
    }

    std::string getAuthor() const override {
        return "您的名字";
    }

    std::shared_ptr<IMetric> createMetric() override {
        return std::make_shared<MyCustomMetric>();
    }
};

// 导出插件
AUTOEVAL_EXPORT_METRIC_PLUGIN(MyMetricPlugin)
```

---

## 指标阈值配置

### 阈值类型

- **max**：值必须小于或等于阈值
- **min**：值必须大于或等于阈值
- **range**：值必须在[min, max]范围内
- **equal**：值必须等于阈值

### 示例配置

```yaml
thresholds:
  # 安全性指标（越小越好）
  - metric_name: collision
    max: 0
    comparison: "max"

  - metric_name: jerk
    max: 2.0
    comparison: "max"

  # 跟车距离（越大越好）
  - metric_name: following
    min: 5.0
    comparison: "min"

  # 速度合规性（不允许超速）
  - metric_name: speed_compliance
    min: 0
    max: 0
    comparison: "range"
```

---

## 最佳实践

1. **组合多个指标**：使用安全性、舒适性和性能指标的组合进行全面评估。

2. **设置适当的阈值**：根据您的具体用例和要求调整阈值。

3. **考虑时间gap**：对于跟车距离，尽可能使用时间gap阈值而不是绝对距离。

4. **分析详细信息**：查看指标详细信息（随结果提供的附加信息）以获得更深入的见解。

5. **使用参考轨迹**：使用 `trajectory_deviation` 与参考轨迹（真实值、计划路径）进行比较。

---

## 参考资料

- [API文档](API.md)
- [插件指南](PluginGuide.md)
