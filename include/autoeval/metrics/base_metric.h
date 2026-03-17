/**
 * @file base_metric.h
 * @brief Base metric interface and core metric implementations
 */

#pragma once

#include "autoeval/core/types.h"
#include "autoeval/core/context.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <limits>
#include <cmath>

namespace autoeval {
namespace metrics {

// ============================================================================
// Metric Category
// ============================================================================

/**
 * @brief Categories of evaluation metrics
 */
enum class MetricCategory {
    Safety,      ///< Safety-related metrics (collision, TTC, etc.)
    Comfort,     ///< Comfort-related metrics (jerk, acceleration)
    Efficiency,  ///< Efficiency metrics (fuel, time)
    Compliance,  ///< Rule compliance metrics (speed limit, traffic lights)
    Performance, ///< Performance metrics (tracking error, deviation)
    General      ///< General purpose metrics
};

/**
 * @brief Get category name as string
 */
inline const char* getCategoryName(MetricCategory category) {
    switch (category) {
        case MetricCategory::Safety: return "safety";
        case MetricCategory::Comfort: return "comfort";
        case MetricCategory::Efficiency: return "efficiency";
        case MetricCategory::Compliance: return "compliance";
        case MetricCategory::Performance: return "performance";
        case MetricCategory::General: return "general";
        default: return "unknown";
    }
}

// ============================================================================
// Metric Metadata
// ============================================================================

/**
 * @brief Metadata describing a metric
 */
struct MetricMetadata {
    std::string name;                  ///< Unique metric name
    std::string display_name;          ///< Human-readable name
    std::string description;           ///< Detailed description
    MetricCategory category;           ///< Metric category
    std::string unit;                  ///< Unit of measurement
    std::string default_comparison;    ///< "min", "max", "range"

    /// Whether the metric requires an ego trajectory
    bool requires_ego = true;

    /// Whether the metric requires surrounding trajectories
    bool requires_surrounding = true;

    /// Whether the metric requires road information
    bool requires_road = false;

    /// Default threshold values
    double default_min = std::numeric_limits<double>::lowest();
    double default_max = std::numeric_limits<double>::max();
};

// ============================================================================
// Base Metric Interface
// ============================================================================

/**
 * @brief Abstract base class for evaluation metrics
 *
 * Metrics compute a value for a given scene and trajectory.
 * They can be extended via plugins for custom metrics.
 */
class IMetric {
public:
    virtual ~IMetric() = default;

    /**
     * @brief Get the metric metadata
     */
    virtual MetricMetadata getMetadata() const = 0;

    /**
     * @brief Get the unique metric name
     */
    virtual std::string getName() const {
        return getMetadata().name;
    }

    /**
     * @brief Compute the metric value
     *
     * @param scene The scene containing all trajectories
     * @param context Optional evaluation context for additional data
     * @return The computed metric result
     */
    virtual core::MetricResult compute(const core::Scene& scene,
                                       core::EvaluationContext* context) const = 0;

    /**
     * @brief Validate that the scene has all required data
     */
    virtual bool validate(const core::Scene& scene, std::string& error) const {
        auto meta = getMetadata();

        if (meta.requires_ego && !scene.getEgoTrajectory()) {
            error = "This metric requires an ego trajectory";
            return false;
        }

        if (meta.requires_surrounding && scene.trajectories.size() < 2) {
            error = "This metric requires surrounding trajectories";
            return false;
        }

        return true;
    }

    /**
     * @brief Set a metric-specific option
     */
    virtual void setOption(const std::string& key, const std::string& value) {
        // Default: ignore options
    }

    /**
     * @brief Get a metric-specific option
     */
    virtual std::optional<std::string> getOption(const std::string& key) const {
        return std::nullopt;
    }

    /**
     * @brief Check if metric result passes a threshold
     */
    virtual bool checkThreshold(const core::MetricResult& result,
                                 const core::MetricThreshold& threshold) const {
        return threshold.check(result.value);
    }

protected:
    /**
     * @brief Create a basic metric result
     */
    core::MetricResult createResult(double value, bool pass = true,
                                     const std::string& message = "") const {
        core::MetricResult result;
        auto meta = getMetadata();
        result.name = meta.name;
        result.category = getCategoryName(meta.category);
        result.value = value;
        result.unit = meta.unit;
        result.pass = pass;
        result.message = message.empty() ? "Computed successfully" : message;
        result.threshold_type = meta.default_comparison;
        return result;
    }
};

// ============================================================================
// Metric Registry
// ============================================================================

/**
 * @brief Registry for metric implementations
 *
 * Manages available metrics and provides lookup by name.
 */
class MetricRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static MetricRegistry& instance();

    /**
     * @brief Register a metric
     */
    void registerMetric(std::shared_ptr<IMetric> metric);

    /**
     * @brief Unregister a metric by name
     */
    void unregisterMetric(const std::string& name);

    /**
     * @brief Get a metric by name
     */
    std::shared_ptr<IMetric> getMetric(const std::string& name) const;

    /**
     * @brief Check if a metric is registered
     */
    bool hasMetric(const std::string& name) const;

    /**
     * @brief Get all registered metric names
     */
    std::vector<std::string> getMetricNames() const;

    /**
     * @brief Get metrics by category
     */
    std::vector<std::shared_ptr<IMetric>> getMetricsByCategory(MetricCategory category) const;

    /**
     * @brief Get all registered metrics
     */
    std::vector<std::shared_ptr<IMetric>> getAllMetrics() const;

    /**
     * @brief Get metadata for all metrics
     */
    std::vector<MetricMetadata> getAllMetadata() const;

private:
    MetricRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<IMetric>> metrics_;
};

// ============================================================================
// Helper Macros for Metric Registration
// ============================================================================

/**
 * @brief Helper class for automatic metric registration
 */
template<typename MetricClass>
class MetricRegistrar {
public:
    explicit MetricRegistrar(const std::string& name) {
        auto metric = std::make_shared<MetricClass>();
        MetricRegistry::instance().registerMetric(metric);
    }
};

/**
 * @brief Macro to register a metric in global scope
 * Usage: AUTOEVAL_REGISTER_METRIC(MyMetric)
 */
#define AUTOEVAL_REGISTER_METRIC(Class) \
    namespace { \
        ::autoeval::metrics::MetricRegistrar<Class> g_registrar_##Class(#Class); \
    }

// ============================================================================
// Core Metrics
// ============================================================================

/**
 * @brief Collision detection metric
 *
 * Detects if the ego vehicle collides with any surrounding object.
 */
class CollisionMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

private:
    double safety_margin_ = 0.1;  // Additional margin for collision detection (meters)
};

/**
 * @brief Following distance metric
 *
 * Measures the minimum following distance from the ego vehicle
 * to the vehicle ahead.
 */
class FollowingMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

    void setOption(const std::string& key, const std::string& value) override;

private:
    double min_time_gap_ = 2.0;  // Minimum safe time gap (seconds)
    double look_ahead_time_ = 5.0;  // Time to look ahead (seconds)
};

/**
 * @brief Lane change safety metric
 *
 * Evaluates the safety of lane change maneuvers.
 */
class LaneChangeMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

    void setOption(const std::string& key, const std::string& value) override;

private:
    double safety_time_gap_ = 1.5;  // Minimum safety time gap during lane change
    double lane_width_ = 3.5;  // Lane width (meters)
};

/**
 * @brief Trajectory deviation metric
 *
 * Measures the deviation between the evaluated trajectory and a reference trajectory.
 */
class TrajectoryDeviationMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

    void setOption(const std::string& key, const std::string& value) override;

    bool requires_ego = false;
    bool requires_surrounding = false;

private:
    std::string reference_id_ = "";  // ID of reference trajectory
};

/**
 * @brief Speed compliance metric
 *
 * Checks if the ego vehicle stays within speed limits.
 */
class SpeedComplianceMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

    bool validate(const core::Scene& scene, std::string& error) const override;

private:
    bool requires_road = true;
};

/**
 * @brief Comfort - Jerk metric
 *
 * Measures longitudinal jerk (rate of change of acceleration).
 */
class JerkMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

private:
    double max_jerk_threshold_ = 2.0;  // m/s^3
};

/**
 * @brief Time to collision (TTC) metric
 *
 * Computes the minimum time to collision with surrounding objects.
 */
class TimeToCollisionMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;

    void setOption(const std::string& key, const std::string& value) override;

private:
    double max_ttc_ = 100.0;  // Maximum TTC to consider
    double min_ttc_threshold_ = 3.0;  // Minimum safe TTC (seconds)
};

/**
 * @brief Path curvature metric
 *
 * Computes the maximum curvature of the trajectory.
 */
class PathCurvatureMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;
};

/**
 * @brief Smoothness metric
 *
 * Measures the smoothness of the trajectory using higher-order derivatives.
 */
class SmoothnessMetric : public IMetric {
public:
    MetricMetadata getMetadata() const override;
    core::MetricResult compute(const core::Scene& scene,
                              core::EvaluationContext* context) const override;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Initialize all built-in metrics
 */
void initializeBuiltInMetrics();

/**
 * @brief Get all built-in metric names
 */
std::vector<std::string> getBuiltInMetricNames();

/**
 * @brief Compute a metric by name
 */
std::optional<core::MetricResult> computeMetric(const std::string& name,
                                               const core::Scene& scene,
                                               core::EvaluationContext* context);

/**
 * @brief Compute multiple metrics
 */
std::vector<core::MetricResult> computeMetrics(const std::vector<std::string>& names,
                                               const core::Scene& scene,
                                               core::EvaluationContext* context);

/**
 * @brief Check if two objects are in collision
 */
bool checkCollision(const core::TrajectoryPoint& p1, const core::TrajectoryPoint& p2,
                   double margin = 0.0);

/**
 * @brief Compute Time to Collision (TTC) between two trajectories
 */
std::optional<double> computeTTC(const core::Trajectory& traj1, const core::Trajectory& traj2);

/**
 * @brief Find the vehicle ahead in the same lane
 */
std::optional<core::Trajectory> findLeadVehicle(const core::Scene& scene,
                                               const core::Trajectory& ego,
                                               double look_ahead_time = 5.0);

} // namespace metrics
} // namespace autoeval
