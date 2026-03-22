/**
 * @file hard_brake_metric.cpp
 * @brief Example metric plugin: Hard Brake Detection
 *
 * This plugin demonstrates how to implement a custom metric plugin for AutoEval.
 * It detects hard braking events where longitudinal acceleration exceeds a threshold.
 *
 * To build:
 *   cd plugins
 *   xmake f -p mingw -m release
 *   xmake build example_metric
 *
 * The compiled plugin (AutoEvalHardBrake.dll / libautoeval_hard_brake.so)
 * will be loaded automatically by AutoEval's PluginManager.
 */

#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/metrics/base_metric.h"
#include <spdlog/spdlog.h>
#include <vector>
#include <cmath>

namespace autoeval {
namespace plugin {

// ============================================================================
// HardBrakeMetric Implementation
// ============================================================================

class HardBrakeMetric : public metrics::IMetric {
public:
    /** @brief Static plugin name constant for the export macro */
    static constexpr const char* PLUGIN_NAME = "hard_brake";

    HardBrakeMetric() = default;

    std::string getName() const override { return "hard_brake"; }

    metrics::MetricMetadata getMetadata() const override {
        return {
            .name = "hard_brake",
            .display_name = "Hard Brake Detection",
            .description = "Detects hard braking events where longitudinal "
                           "deceleration exceeds the threshold. A hard brake is "
                           "typically defined as deceleration > 3 m/s^2 in "
                           "magnitude.",
            .category = metrics::MetricCategory::Safety,
            .unit = "count",
            .default_comparison = "max",
            .requires_ego = true,
            .requires_surrounding = false,
            .requires_road = false,
            .default_min = 0.0,
            .default_max = 5.0
        };
    }

    core::MetricResult compute(const core::Scene& scene,
                               core::EvaluationContext* /*context*/) const override {
        const auto& meta = getMetadata();

        auto ego_opt = scene.getEgoTrajectory();
        if (!ego_opt) {
            return makeResult(0, true, "No ego trajectory");
        }

        const auto& ego = *ego_opt;
        if (ego.points.size() < 2) {
            return makeResult(0, true, "Ego trajectory too short");
        }

        int hard_brake_count = 0;
        double max_decel = 0.0;

        for (size_t i = 1; i < ego.points.size(); ++i) {
            const auto& prev = ego.points[i - 1];
            const auto& curr = ego.points[i];

            // Use acceleration field directly if available, otherwise estimate
            double accel = curr.acceleration;
            if (std::abs(accel) < 1e-9 && i > 0) {
                double dv = curr.velocity - prev.velocity;
                double dt = curr.timestamp - prev.timestamp;
                if (dt > 0) {
                    accel = dv / dt;
                }
            }

            // Negative acceleration means deceleration (braking)
            if (accel < -hard_brake_threshold_) {
                hard_brake_count++;
                max_decel = std::min(max_decel, accel);
            }
        }

        // Determine pass/fail: count within threshold is acceptable
        bool pass = hard_brake_count <= static_cast<int>(meta.default_max);

        core::MetricResult result;
        result.name = meta.name;
        result.category = metrics::getCategoryName(meta.category);
        result.value = static_cast<double>(hard_brake_count);
        result.unit = meta.unit;
        result.pass = pass;
        result.threshold_type = meta.default_comparison;
        result.threshold = meta.default_max;

        if (pass) {
            result.message = "Hard braking events within acceptable range";
        } else {
            result.message = "Too many hard braking events detected";
        }
        result.addDetail("max_deceleration", max_decel);
        result.addDetail("hard_brake_threshold", -hard_brake_threshold_);

        return result;
    }

    bool validate(const core::Scene& scene, std::string& error) const override {
        if (!scene.getEgoTrajectory()) {
            error = "HardBrakeMetric requires an ego trajectory";
            return false;
        }
        return true;
    }

    void setOption(const std::string& key, const std::string& value) override {
        if (key == "threshold") {
            try {
                hard_brake_threshold_ = std::stod(value);
            } catch (...) {
                spdlog::warn("Invalid threshold value for hard_brake metric: {}", value);
            }
        }
    }

    std::optional<std::string> getOption(const std::string& key) const override {
        if (key == "threshold") {
            return std::to_string(hard_brake_threshold_);
        }
        return std::nullopt;
    }

private:
    /** @brief Acceleration threshold (m/s^2), positive value */
    double hard_brake_threshold_ = 3.0;

    core::MetricResult makeResult(double value, bool pass,
                                   const std::string& message) const {
        const auto& meta = getMetadata();
        core::MetricResult result;
        result.name = meta.name;
        result.category = metrics::getCategoryName(meta.category);
        result.value = value;
        result.unit = meta.unit;
        result.pass = pass;
        result.message = message;
        result.threshold_type = meta.default_comparison;
        return result;
    }
};

// ============================================================================
// Plugin Interface Implementation
// ============================================================================

class HardBrakeMetricPlugin : public IMetricPlugin {
public:
    std::string getName() const override { return "hard_brake"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Detects hard braking events in vehicle trajectories";
    }
    std::string getAuthor() const override { return "AutoEval Team"; }
    PluginType getType() const override { return PluginType::Metric; }

    bool initialize() override {
        spdlog::info("HardBrakeMetricPlugin initialized");
        return true;
    }

    void shutdown() override {
        spdlog::info("HardBrakeMetricPlugin shutdown");
    }

    std::vector<std::string> getDependencies() const override {
        return {};
    }

    bool isCompatible(const std::string& version) const override {
        // Compatible with any 0.x.x version
        if (version.empty()) return true;
        if (version[0] == '0') return true;
        return false;
    }

    std::shared_ptr<metrics::IMetric> createMetric() override {
        return std::make_shared<HardBrakeMetric>();
    }

    metrics::MetricMetadata getMetricMetadata() override {
        auto metric = createMetric();
        return metric->getMetadata();
    }
};

} // namespace plugin
} // namespace autoeval

// Export the plugin
AUTOEVAL_EXPORT_METRIC_PLUGIN(autoeval::plugin::HardBrakeMetricPlugin)
