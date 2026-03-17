# 项目架构说明 (Architecture Documentation)

本文档描述了 AutoEval 项目的整体软件架构、核心组件及其交互流程。

## 1. 架构总览

AutoEval 是一个模块化的自动评价系统，旨在为自动驾驶轨迹或场景提供标准化的评估框架。系统采用分层架构，支持通过插件系统进行横向扩展。

### 核心模块 (Core Modules)

- **Core (核心)**: 负责评价流程的调度、配置管理和上下文维护。
- **Loader (加载器)**: 处理不同源和格式（JSON, YAML 等）的数据输入。
- **Metrics (指标)**: 实现具体的评价逻辑（如碰撞检测、跟随精度等）。
- **Plugin (插件)**: 提供统一的扩展接口，允许运行时动态加载第三方组件。
- **Report (报告)**: 将评价结果导出为可视化或结构化的报告（HTML, JSON, Markdown）。

---

## 2. 核心组件与职责

### 2.1 评价控制器 (Evaluator)
`Evaluator` 是系统的主入口，负责协调各模块。
- **PIMPL 模式**: 隐藏实现细节，保证库的 ABI 稳定性。
- **职责**: 初始化环境、加载配置、执行评价循环、调用报告生成。

### 2.2 评价上下文 (EvaluationContext)
贯穿整个评价生命周期的容器，保存了：
- 当前运行的所有场景数据。
- 累积的评价指标结果。
- 运行时的配置参数。

### 2.3 指标注册表 (MetricRegistry)
采用注册表模式管理所有评价指标。
- **单例模式**: 全局唯一的指标访问点。
- **发现机制**: 允许核心模块按需查询并实例化特定的评价指标。

---

## 3. 数据与执行流

典型的评价执行流程如下：

1.  **启动与初始化**: CLI 或外部调用者初始化 `autoeval` 环境。
2.  **配置解析**: `Evaluator` 通过 `Loader` 加载 YAML 配置文件。
3.  **数据读取**: `Loader` 将原始轨迹数据转换为系统内部的 `Scene` 和 `Trajectory` 模型。
4.  **指标选取**: `Evaluator` 根据配置从 `MetricRegistry` 中检索对应的 `IMetric` 实例。
5.  **循环计算**:
    - 系统遍历所有场景。
    - 对每个场景调用 `IMetric::compute(scene, context)`。
    - 计算结果汇总至 `EvaluationContext`。
6.  **报告生成**: `ReportGenerator` 读取上下文数据，生成最终报告文件。

---

## 4. 设计模式

本项目在其架构中应用了多种设计模式，以确保系统的灵活性和可维护性：

- **PIMPL (Pointer to Implementation)**: 用于 `Evaluator` 类，减少编译依赖并保护内部逻辑。
- **Registry Pattern (注册表模式)**: 用于指标管理，支持动态添加新指标而无需修改核心代码。
- **Plugin System (插件系统)**: 定义了 `IPlugin` 接口，支持通过 DLL/SO 动态扩展功能。
- **Singleton (单例模式)**: 用于 `MetricRegistry`，确保指标注册的一致性。
- **Interface Segregation (接口隔离)**: `IMetric`, `ILoader`, `IReportGenerator` 等接口确保了模块间的松耦合。

---

## 5. 关键接口定义

| 接口 | 文件路径 | 描述 |
| :--- | :--- | :--- |
| `IMetric` | `include/autoeval/metrics/base_metric.h` | 所有评价指标的基类 |
| `IPlugin` | `include/autoeval/plugin/plugin_interface.h` | 插件系统的基础接口 |
| `IReportGenerator` | `include/autoeval/report/report_generator.h` | 报告输出的通用标准 |
| `IDataLoader` | `include/autoeval/loader/data_loader.h` | 外部数据接入的接口 |

---

## 6. CLI 与库的交互

CLI (`autoeval_cli`) 是库的一个薄封装：
- **参数解析**: 使用高性能命令行库处理用户输入。
- **回调机制**: 注册 `ProgressCallback` 到 `Evaluator`，实现终端进度条实时更新。
- **日志管理**: 集成 `spdlog` 进行结构化日志输出。
