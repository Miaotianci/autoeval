# AutoEval 快速入门指南

5分钟内上手AutoEval！

---

## 安装（5分钟）

### 从源码构建

```bash
# 克隆仓库
git clone https://github.com/yourusername/autoeval.git
cd autoeval

# 使用xmake构建
xmake config -m release
xmake build
```

有关详细说明，请参阅[BUILD_GUIDE.md](BUILD_GUIDE.md)。

---

## 首次评估（2分钟）

### 1. 准备数据

创建一个包含轨迹数据的JSON文件：

```json
{
  "id": "my_scene",
  "trajectories": [
    {
      "id": "ego",
      "type": "ego",
      "points": [
        {"x": 0, "y": 0, "t": 0, "v": 10},
        {"x": 1, "y": 0, "t": 0.1, "v": 10},
        {"x": 2, "y": 0, "t": 0.2, "v": 10}
      ]
    }
  ]
}
```

### 2. 运行评估

```bash
autoeval_cli evaluate -d my_trajectory.json -o results/
```

完成！查看 `results/report.html` 获取评估报告。

---

## 常见工作流程

### 评估多个文件

```bash
# 遍历文件
for file in trajectories/*.json; do
    autoeval_cli evaluate -d "$file" -o "results/$(basename $file .json)/"
done
```

### 自定义阈值

创建 `config.yaml`：

```yaml
metrics:
  - collision
  - ttc

thresholds:
  - metric_name: ttc
    min: 2.0
    comparison: "min"
```

```bash
autoeval_cli evaluate -c config.yaml -d trajectory.json
```

---

## 下一步

- [阅读完整API文档](API.md)
- [探索所有可用指标](Metrics.md)
- [创建自定义插件](PluginGuide.md)
- [加入讨论](https://github.com/yourusername/autoeval/discussions)
