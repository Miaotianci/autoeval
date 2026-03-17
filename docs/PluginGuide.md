# AutoEval 插件指南

## 概述

AutoEval提供了一个插件系统，允许您使用自定义指标、数据加载器和报告生成器扩展框架。插件可以在运行时动态加载。

---

## 插件类型

1. **指标插件**：添加自定义评估指标
2. **加载器插件**：支持新的数据格式
3. **报告器插件**：生成新格式的报告

---

## 创建指标插件

### 基本结构

```cpp
#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/metrics/base_metric.h"

// 定义您的自定义指标
class MyCustomMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override {
        return {
            .name = "my_metric",
            .display_name = "我的自定义指标",
            .description = "该指标评估的内容描述",
            .category = MetricCategory::Safety,
            .unit = "m",
            .default_comparison = "max",
            .default_max = 5.0
        };
    }

    MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
        // 验证输入
        auto ego = scene.getEgoTrajectory();
        if (!ego) {
            auto result = createResult(0.0, false);
            result.message = "未找到自车轨迹";
            return result;
        }

        // 计算指标值
        double value = 0.0;
        for (const auto& point : ego->points) {
            value += point.velocity;
        }
        value /= ego->points.size();

        // 判断通过/失败
        bool pass = value < 15.0;

        // 创建结果
        auto result = createResult(value, pass);
        result.message = "平均速度: " + std::to_string(value) + " m/s";

        // 添加可选的详细信息
        result.addDetail("num_points", static_cast<int>(ego->points.size()));
        result.addDetail("duration", ego->duration());

        return result;
    }

    bool validate(const Scene& scene, std::string& error) const override {
        if (!scene.getEgoTrajectory()) {
            error = "该指标需要自车轨迹";
            return false;
        }
        return true;
    }
};

// 定义插件
class MyMetricPlugin : public IMetricPlugin {
public:
    std::string getName() const override {
        return "my_metric_plugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::string getDescription() const override {
        return "一个自定义指标插件示例";
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

### 构建插件

使用xmake构建您的插件：

```lua
-- xmake.lua for plugin
set_project("my_metric_plugin")
set_version("1.0.0")
set_languages("c++20")

add_includedirs("/path/to/autoeval/include")
add_links("autoeval_core")

target("my_metric_plugin")
    set_kind("shared")
    add_files("my_metric.cpp")
```

构建和安装：

```bash
xmake
cp libmy_metric_plugin.so /path/to/autoeval/plugins/
```

### 加载插件

```cpp
#include "autoeval/plugin/plugin_interface.h"

auto& manager = PluginManager::instance();

// 添加插件搜索路径
manager.addSearchPath("./plugins");

// 从目录加载所有插件
size_t loaded = manager.loadPluginsFromDirectory("./plugins");

// 或加载特定插件
bool success = manager.loadPlugin("./plugins/my_metric_plugin.so");

// 获取指标插件
auto metric_plugins = manager.getMetricPlugins();

// 使用自定义指标
auto registry = MetricRegistry::instance();
auto metric = registry.getMetric("my_metric");
if (metric) {
    auto result = metric->compute(scene, context);
}
```

---

## 创建加载器插件

### 基本结构

```cpp
#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/loader/data_loader.h"

class MyCustomLoader : public IDataLoader {
public:
    std::string getName() const override {
        return "my_loader";
    }

    std::vector<std::string> getSupportedExtensions() const override {
        return {".myformat"};
    }

    LoadResult loadScene(const std::filesystem::path& path, Scene& out) override {
        auto start = std::chrono::high_resolution_clock::now();

        LoadResult result;
        result.source_path = path.string();

        // 解析您的自定义格式
        if (!parseMyFormat(path, out)) {
            result.message = "无法解析自定义格式";
            return result;
        }

        result.success = true;
        result.items_loaded = out.trajectories.size();

        auto end = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        return result;
    }

    LoadResult loadTrajectory(const std::filesystem::path& path, Trajectory& out) override {
        // 单条轨迹的实现
    }

    LoadResult loadTrajectories(const std::filesystem::path& path,
                               std::vector<Trajectory>& out) override {
        // 多条轨迹的实现
    }

    LoadResult loadFromString(const std::string& content,
                             std::vector<Trajectory>& out) override {
        // 从字符串加载的实现
    }

private:
    bool parseMyFormat(const std::filesystem::path& path, Scene& scene) {
        // 您的解析逻辑
        return true;
    }
};

class MyLoaderPlugin : public ILoaderPlugin {
public:
    std::string getName() const override {
        return "my_loader_plugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::string getDescription() const override {
        return "自定义数据格式加载器";
    }

    std::string getAuthor() const override {
        return "您的名字";
    }

    std::shared_ptr<IDataLoader> createLoader() override {
        return std::make_shared<MyCustomLoader>();
    }
};

AUTOEVAL_EXPORT_LOADER_PLUGIN(MyLoaderPlugin)
```

---

## 创建报告器插件

### 基本结构

```cpp
#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/core/types.h"

class MyCustomReporter : public IReporterPlugin {
public:
    std::string getName() const override {
        return "my_reporter";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::string getDescription() const override {
        return "自定义报告生成器";
    }

    std::string getAuthor() const override {
        return "您的名字";
    }

    std::vector<std::string> getFormats() const override {
        return {"custom"};
    }

    bool generate(const EvaluationSummary& summary,
                 const std::filesystem::path& output_path) override {
        // 需要时创建输出目录
        std::filesystem::create_directories(output_path.parent_path());

        // 打开输出文件
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return false;
        }

        // 写入自定义格式
        file << "自定义报告\n";
        file << "==============\n";
        file << "ID: " << summary.id << "\n";
        file << "状态: " << summary.overall_status << "\n";

        for (const auto& result : summary.metric_results) {
            file << result.name << ": " << result.value << " " << result.unit << "\n";
        }

        file.close();
        return true;
    }
};

class MyReporterPlugin : public IReporterPlugin {
public:
    std::string getName() const override {
        return "my_reporter_plugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::string getDescription() const override {
        return "自定义报告生成器插件";
    }

    std::string getAuthor() const override {
        return "您的名字";
    }

    std::vector<std::string> getFormats() const override {
        return {"custom"};
    }

    std::shared_ptr<IMetric> createMetric() override {
        return std::make_shared<MyCustomMetric>();
    }
};

AUTOEVAL_EXPORT_REPORTER_PLUGIN(MyReporterPlugin)
```

---

## 插件最佳实践

### 1. 错误处理

始终妥善处理错误：

```cpp
MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
    auto result = createResult(0.0, false);

    auto ego = scene.getEgoTrajectory();
    if (!ego) {
        result.message = "未找到自车轨迹";
        return result;
    }

    // 您的计算...

    return result;
}
```

### 2. 验证

实现 `validate` 方法：

```cpp
bool validate(const Scene& scene, std::string& error) const override {
    auto meta = getMetadata();

    if (meta.requires_ego && !scene.getEgoTrajectory()) {
        error = "该指标需要自车轨迹";
        return false;
    }

    if (meta.requires_surrounding && scene.trajectories.size() < 2) {
        error = "该指标需要周围轨迹";
        return false;
    }

    return true;
}
```

### 3. 元数据

提供清晰准确的元数据：

```cpp
MetricMetadata getMetadata() const override {
    return {
        .name = "my_metric",              // 唯一标识符
        .display_name = "我的指标",        // 人类可读的名称
        .description = "它做什么",          // 清晰的描述
        .category = MetricCategory::Safety, // 适当的类别
        .unit = "m",                      // 测量单位
        .default_comparison = "max",       // 如何解释值
        .requires_ego = true,              // 需求
        .requires_surrounding = false
    };
}
```

### 4. 线程安全

如果您的插件将用于并行评估，请确保线程安全：

```cpp
class ThreadSafeMetric : public IMetric {
private:
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, double> value_cache_;

public:
    MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        // 线程安全的计算...
    }
};
```

### 5. 性能

优化性能：

```cpp
MetricResult compute(const Scene& scene, EvaluationContext* context) const override {
    // 预先分配内存
    std::vector<double> values;
    values.reserve(scene.trajectories.size());

    // 预先计算可重用的值
    auto ego = scene.getEgoTrajectory();
    double ego_speed = ego->points.empty() ? 0.0 : ego->points[0].velocity;

    // 高效循环
    for (const auto& traj : scene.trajectories) {
        // ...
    }

    return result;
}
```

---

## 插件分发

### 目录结构

```
my_plugin/
├── xmake.lua
├── include/
│   └── my_plugin/
│       └── my_metric.h
├── src/
│   └── my_metric.cpp
├── README.md
└── LICENSE
```

### 打包

1. **动态库**：分发为`.so`（Linux）、`.dll`（Windows）或`.dylib`（macOS）
2. **源码包**：包含源码和构建说明
3. **包管理器**：提交到相关的包仓库

### 安装

用户可以通过以下方式安装插件：

```bash
# 复制到插件目录
cp my_plugin.so ~/.local/share/autoeval/plugins/

# 或设置环境变量
export AUTOEVAL_PLUGIN_PATH=/path/to/plugins
```

---

## 高级主题

### 插件依赖

插件可以声明依赖：

```cpp
std::vector<std::string> getDependencies() const override {
    return {"base_metric", "math_utils"};
}
```

### 版本兼容性

检查与AutoEval版本的兼容性：

```cpp
bool isCompatible(const std::string& version) const override {
    // 仅兼容AutoEval 0.1.x
    return version.find("0.1.") == 0;
}
```

### 配置

支持插件特定选项：

```cpp
void setOption(const std::string& key, const std::string& value) override {
    if (key == "tolerance") {
        tolerance_ = std::stod(value);
    }
}

std::optional<std::string> getOption(const std::string& key) const override {
    if (key == "tolerance") {
        return std::to_string(tolerance_);
    }
    return std::nullopt;
}
```

---

## 参考资料

- [API文档](API.md)
- [指标指南](Metrics.md)
