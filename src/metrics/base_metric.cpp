/**
 * @file base_metric.cpp
 * @brief Metric implementations
 */

#include "autoeval/metrics/base_metric.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>
#include <cmath>

namespace autoeval {
namespace metrics {

// ============================================================================
// Metric Registry Implementation
// ============================================================================

MetricRegistry& MetricRegistry::instance() {
    static MetricRegistry instance;
    return instance;
}

void MetricRegistry::registerMetric(std::shared_ptr<IMetric> metric) {
    if (!metric) {
        spdlog::warn("Attempted to register null metric");
        return;
    }
    std::string name = metric->getName();
    metrics_[name] = metric;
    spdlog::info("Registered metric: {}", name);
}

void MetricRegistry::unregisterMetric(const std::string& name) {
    metrics_.erase(name);
    spdlog::info("Unregistered metric: {}", name);
}

std::shared_ptr<IMetric> MetricRegistry::getMetric(const std::string& name) const {
    auto it = metrics_.find(name);
    return (it != metrics_.end()) ? it->second : nullptr;
}

bool MetricRegistry::hasMetric(const std::string& name) const {
    return metrics_.find(name) != metrics_.end();
}

std::vector<std::string> MetricRegistry::getMetricNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : metrics_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::shared_ptr<IMetric>> MetricRegistry::getMetricsByCategory(MetricCategory category) const {
    std::vector<std::shared_ptr<IMetric>> result;
    for (const auto& [_, metric] : metrics_) {
        if (metric->getMetadata().category == category) {
            result.push_back(metric);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IMetric>> MetricRegistry::getAllMetrics() const {
    std::vector<std::shared_ptr<IMetric>> result;
    for (const auto& [_, metric] : metrics_) {
        result.push_back(metric);
    }
    return result;
}

std::vector<MetricMetadata> MetricRegistry::getAllMetadata() const {
    std::vector<MetricMetadata> result;
    for (const auto& [_, metric] : metrics_) {
        result.push_back(metric->getMetadata());
    }
    return result;
}

// ============================================================================
// Collision Metric
// ============================================================================

MetricMetadata CollisionMetric::getMetadata() const {
    return {
        .name = "collision",
        .display_name = "Collision Detection",
        .description = "Detects if the ego vehicle collides with any surrounding object",
        .category = MetricCategory::Safety,
        .unit = "collision",
        .default_comparison = "max",
        .requires_ego = true,
        .requires_surrounding = true,
        .requires_road = false,
        .default_max = 0.0  // 0 = no collision
    };
}

core::MetricResult CollisionMetric::compute(const core::Scene& scene,
                                            core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;
    int collision_count = 0;

    // Pre-compute vehicle dimensions for quick rejection
    double ego_half_length = ego.points.empty() ? 2.5 : ego.points[0].dimensions.length / 2.0;
    double ego_half_width = ego.points.empty() ? 1.0 : ego.points[0].dimensions.width / 2.0;

    for (const auto& traj : scene.trajectories) {
        if (traj.type == core::ObjectType::EgoVehicle) continue;
        if (traj.type == core::ObjectType::StaticObstacle) continue;

        // Use binary search to find nearby points - O(log n) instead of O(n)
        for (const auto& ego_point : ego.points) {
            auto near_point = traj.getPointAtTime(ego_point.timestamp);
            if (!near_point) continue;

            // Quick distance check before expensive collision detection
            double dx = ego_point.x - near_point->x;
            double dy = ego_point.y - near_point->y;
            double dist_sq = dx * dx + dy * dy;
            double quick_reject_dist = (ego_half_length + near_point->dimensions.length / 2.0 + safety_margin_) * 2.0;

            if (dist_sq > quick_reject_dist * quick_reject_dist) {
                continue;  // Too far, skip expensive check
            }

            // Full collision check
            if (checkCollision(ego_point, *near_point, safety_margin_)) {
                collision_count++;
            }
        }
    }

    auto result = createResult(static_cast<double>(collision_count), collision_count == 0);
    result.message = collision_count == 0
        ? "No collisions detected"
        : "Detected " + std::to_string(collision_count) + " collision(s)";
    result.threshold = 0.0;
    result.threshold_type = "max";

    return result;
}

// ============================================================================
// Following Metric
// ============================================================================

MetricMetadata FollowingMetric::getMetadata() const {
    return {
        .name = "following",
        .display_name = "Following Distance",
        .description = "Minimum following distance from ego to lead vehicle",
        .category = MetricCategory::Safety,
        .unit = "m",
        .default_comparison = "min",
        .requires_ego = true,
        .requires_surrounding = true,
        .requires_road = false,
        .default_min = 5.0  // Minimum safe distance
    };
}

void FollowingMetric::setOption(const std::string& key, const std::string& value) {
    if (key == "min_time_gap") {
        min_time_gap_ = std::stod(value);
    } else if (key == "look_ahead_time") {
        look_ahead_time_ = std::stod(value);
    }
}

core::MetricResult FollowingMetric::compute(const core::Scene& scene,
                                            core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    auto lead_opt = findLeadVehicle(scene, *ego_opt, look_ahead_time_);
    if (!lead_opt) {
        auto result = createResult(std::numeric_limits<double>::max(), true);
        result.message = "No lead vehicle found";
        return result;
    }

    const auto& ego = *ego_opt;
    const auto& lead = *lead_opt;

    double min_distance = std::numeric_limits<double>::max();
    double min_time_gap = std::numeric_limits<double>::max();

    // Compute minimum following distance and time gap
    for (const auto& ego_point : ego.points) {
        auto lead_point = lead.getPointAtTime(ego_point.timestamp);
        if (!lead_point) continue;

        double dist = ego_point.distanceTo(*lead_point);

        // Adjust for vehicle length (rear to front distance)
        double adjusted_dist = dist - ego_point.dimensions.length / 2
                               - lead_point->dimensions.length / 2;

        if (adjusted_dist > 0 && adjusted_dist < min_distance) {
            min_distance = adjusted_dist;

            // Compute time gap
            if (ego_point.velocity > 0.1) {
                double time_gap = adjusted_dist / ego_point.velocity;
                min_time_gap = std::min(min_time_gap, time_gap);
            }
        }
    }

    auto result = createResult(min_distance, min_time_gap >= min_time_gap_);
    result.addDetail("min_time_gap", min_time_gap);
    result.addDetail("lead_vehicle_id", lead.id);
    result.threshold = min_time_gap_;

    if (min_time_gap < min_time_gap_) {
        result.message = "Unsafe following: " + std::to_string(min_distance) + "m, time gap: "
                        + std::to_string(min_time_gap) + "s (minimum: " + std::to_string(min_time_gap_) + "s)";
    } else {
        result.message = "Safe following distance maintained: " + std::to_string(min_distance) + "m";
    }

    return result;
}

// ============================================================================
// Lane Change Metric
// ============================================================================

MetricMetadata LaneChangeMetric::getMetadata() const {
    return {
        .name = "lane_change",
        .display_name = "Lane Change Safety",
        .description = "Evaluates safety of lane change maneuvers",
        .category = MetricCategory::Safety,
        .unit = "score",
        .default_comparison = "min",
        .requires_ego = true,
        .requires_surrounding = true,
        .requires_road = false,
        .default_min = 1.0  // 1.0 = safe, 0.0 = unsafe
    };
}

void LaneChangeMetric::setOption(const std::string& key, const std::string& value) {
    if (key == "safety_time_gap") {
        safety_time_gap_ = std::stod(value);
    } else if (key == "lane_width") {
        lane_width_ = std::stod(value);
    }
}

core::MetricResult LaneChangeMetric::compute(const core::Scene& scene,
                                            core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;

    // Detect lane changes (significant lateral movement)
    int lane_changes = 0;
    double safety_score = 1.0;

    // Simple lane change detection: significant y-velocity or lateral acceleration
    double total_lateral_displacement = 0.0;
    double start_y = ego.points.empty() ? 0.0 : ego.points.front().y;

    for (const auto& point : ego.points) {
        total_lateral_displacement = std::max(total_lateral_displacement,
                                             std::abs(point.y - start_y));
    }

    // Count lane changes based on lane width
    lane_changes = static_cast<int>(total_lateral_displacement / (lane_width_ * 0.5));

    // Check safety during lane changes
    for (const auto& traj : scene.trajectories) {
        if (traj.type == core::ObjectType::EgoVehicle) continue;

        for (const auto& ego_point : ego.points) {
            auto obj_point = traj.getPointAtTime(ego_point.timestamp);
            if (!obj_point) continue;

            // Check if vehicles are close laterally (potential lane change)
            double lateral_dist = std::abs(ego_point.y - obj_point->y);
            if (lateral_dist < lane_width_ * 0.5) {
                double longitudinal_dist = std::abs(ego_point.x - obj_point->x);

                // Check time gap during potential lane change
                if (longitudinal_dist > 0 && ego_point.velocity > 0) {
                    double time_gap = longitudinal_dist / ego_point.velocity;
                    if (time_gap < safety_time_gap_) {
                        safety_score = std::min(safety_score, time_gap / safety_time_gap_);
                    }
                }
            }
        }
    }

    auto result = createResult(safety_score, safety_score >= 1.0);
    result.addDetail("lane_changes_detected", lane_changes);
    result.addDetail("lateral_displacement", total_lateral_displacement);
    result.threshold = 1.0;

    if (safety_score < 1.0) {
        result.message = "Unsafe lane change detected, safety score: "
                        + std::to_string(safety_score);
    } else if (lane_changes > 0) {
        result.message = "Safe lane change(s) detected, count: "
                        + std::to_string(lane_changes);
    } else {
        result.message = "No lane changes detected";
    }

    return result;
}

// ============================================================================
// Trajectory Deviation Metric
// ============================================================================

MetricMetadata TrajectoryDeviationMetric::getMetadata() const {
    return {
        .name = "trajectory_deviation",
        .display_name = "Trajectory Deviation",
        .description = "Deviation from reference trajectory",
        .category = MetricCategory::Performance,
        .unit = "m",
        .default_comparison = "max",
        .requires_ego = false,
        .requires_surrounding = false,
        .requires_road = false,
        .default_max = 0.5  // Maximum allowed deviation
    };
}

void TrajectoryDeviationMetric::setOption(const std::string& key, const std::string& value) {
    if (key == "reference_id") {
        reference_id_ = value;
    }
}

core::MetricResult TrajectoryDeviationMetric::compute(const core::Scene& scene,
                                                      core::EvaluationContext* context) const {
    // Use the first trajectory as evaluated, second as reference
    if (scene.trajectories.size() < 2) {
        auto result = createResult(0.0, false);
        result.message = "Need at least 2 trajectories for deviation analysis";
        return result;
    }

    const auto& evaluated = scene.trajectories[0];
    const auto& reference = reference_id_.empty() ? scene.trajectories[1]
                            : scene.getTrajectoryById(reference_id_).value_or(scene.trajectories[1]);

    double max_deviation = 0.0;
    double mean_deviation = 0.0;
    size_t comparison_count = 0;

    // Compute deviation at matching timestamps
    for (const auto& eval_point : evaluated.points) {
        auto ref_point = reference.getPointAtTime(eval_point.timestamp);
        if (!ref_point) continue;

        double deviation = eval_point.distanceTo(*ref_point);
        max_deviation = std::max(max_deviation, deviation);
        mean_deviation += deviation;
        comparison_count++;
    }

    if (comparison_count > 0) {
        mean_deviation /= comparison_count;
    }

    auto result = createResult(max_deviation, max_deviation <= getMetadata().default_max);
    result.addDetail("mean_deviation", mean_deviation);
    result.addDetail("reference_id", reference.id);
    result.addDetail("evaluated_id", evaluated.id);
    result.addDetail("comparison_points", static_cast<int>(comparison_count));
    result.threshold = getMetadata().default_max;

    result.message = "Max deviation: " + std::to_string(max_deviation) + "m, "
                    + "Mean deviation: " + std::to_string(mean_deviation) + "m";

    return result;
}

// ============================================================================
// Speed Compliance Metric
// ============================================================================

MetricMetadata SpeedComplianceMetric::getMetadata() const {
    return {
        .name = "speed_compliance",
        .display_name = "Speed Compliance",
        .description = "Checks if the vehicle stays within speed limits",
        .category = MetricCategory::Compliance,
        .unit = "km/h",
        .default_comparison = "max",
        .requires_ego = true,
        .requires_surrounding = false,
        .requires_road = true,
        .default_max = 0.0  // Maximum speed violation
    };
}

bool SpeedComplianceMetric::validate(const core::Scene& scene, std::string& error) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        error = "No ego trajectory found";
        return false;
    }
    return IMetric::validate(scene, error);
}

core::MetricResult SpeedComplianceMetric::compute(const core::Scene& scene,
                                                  core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;
    double speed_limit_ms = scene.road_info.speed_limit;
    double max_speed = 0.0;
    double max_violation = 0.0;
    double violation_duration = 0.0;

    for (size_t i = 1; i < ego.points.size(); ++i) {
        double speed = ego.points[i].velocity;
        max_speed = std::max(max_speed, speed);

        if (speed > speed_limit_ms) {
            double violation = speed - speed_limit_ms;
            max_violation = std::max(max_violation, violation);

            double dt = ego.points[i].timestamp - ego.points[i - 1].timestamp;
            violation_duration += dt;
        }
    }

    // Convert to km/h for output
    double max_violation_kmh = max_violation * 3.6;

    auto result = createResult(max_violation_kmh, max_violation <= 0.1);
    result.addDetail("max_speed_ms", max_speed);
    result.addDetail("max_speed_kmh", max_speed * 3.6);
    result.addDetail("speed_limit_ms", speed_limit_ms);
    result.addDetail("speed_limit_kmh", speed_limit_ms * 3.6);
    result.addDetail("violation_duration", violation_duration);
    result.threshold = 0.0;

    if (max_violation > 0.1) {
        result.message = "Speed violation detected: max "
                        + std::to_string(max_violation_kmh) + " km/h over limit ("
                        + std::to_string(speed_limit_ms * 3.6) + " km/h)";
    } else {
        result.message = "Speed limit compliance verified: "
                        + std::to_string(max_speed * 3.6) + " km/h (limit: "
                        + std::to_string(speed_limit_ms * 3.6) + " km/h)";
    }

    return result;
}

// ============================================================================
// Jerk Metric
// ============================================================================

MetricMetadata JerkMetric::getMetadata() const {
    return {
        .name = "jerk",
        .display_name = "Longitudinal Jerk",
        .description = "Measures longitudinal jerk (rate of change of acceleration)",
        .category = MetricCategory::Comfort,
        .unit = "m/s^3",
        .default_comparison = "max",
        .requires_ego = true,
        .requires_surrounding = false,
        .requires_road = false,
        .default_max = 2.0  // Maximum comfortable jerk
    };
}

core::MetricResult JerkMetric::compute(const core::Scene& scene,
                                      core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;

    if (ego.points.size() < 3) {
        auto result = createResult(0.0, true);
        result.message = "Not enough points to compute jerk";
        return result;
    }

    double max_jerk = 0.0;
    double mean_jerk = 0.0;
    size_t jerk_count = 0;

    // Compute jerk: derivative of acceleration
    for (size_t i = 2; i < ego.points.size(); ++i) {
        double dt1 = ego.points[i - 1].timestamp - ego.points[i - 2].timestamp;
        double dt2 = ego.points[i].timestamp - ego.points[i - 1].timestamp;

        if (dt1 > 0 && dt2 > 0) {
            double jerk = (ego.points[i].acceleration - ego.points[i - 1].acceleration)
                         / ((dt1 + dt2) / 2.0);

            max_jerk = std::max(max_jerk, std::abs(jerk));
            mean_jerk += std::abs(jerk);
            jerk_count++;
        }
    }

    if (jerk_count > 0) {
        mean_jerk /= jerk_count;
    }

    auto result = createResult(max_jerk, max_jerk <= max_jerk_threshold_);
    result.addDetail("mean_jerk", mean_jerk);
    result.addDetail("jerk_threshold", max_jerk_threshold_);
    result.threshold = max_jerk_threshold_;

    result.message = "Max jerk: " + std::to_string(max_jerk) + " m/s^3, "
                    + "Mean jerk: " + std::to_string(mean_jerk) + " m/s^3";

    return result;
}

// ============================================================================
// Time to Collision Metric
// ============================================================================

MetricMetadata TimeToCollisionMetric::getMetadata() const {
    return {
        .name = "ttc",
        .display_name = "Time to Collision",
        .description = "Minimum time to collision with surrounding objects",
        .category = MetricCategory::Safety,
        .unit = "s",
        .default_comparison = "min",
        .requires_ego = true,
        .requires_surrounding = true,
        .requires_road = false,
        .default_min = 3.0  // Minimum safe TTC
    };
}

void TimeToCollisionMetric::setOption(const std::string& key, const std::string& value) {
    if (key == "max_ttc") {
        max_ttc_ = std::stod(value);
    } else if (key == "min_ttc_threshold") {
        min_ttc_threshold_ = std::stod(value);
    }
}

core::MetricResult TimeToCollisionMetric::compute(const core::Scene& scene,
                                                   core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;
    double min_ttc = max_ttc_;

    for (const auto& traj : scene.trajectories) {
        if (traj.type == core::ObjectType::EgoVehicle) continue;

        auto ttc = computeTTC(ego, traj);
        if (ttc && *ttc < min_ttc) {
            min_ttc = *ttc;
        }
    }

    bool safe = min_ttc >= min_ttc_threshold_;
    auto result = createResult(min_ttc, safe);
    result.addDetail("ttc_threshold", min_ttc_threshold_);
    result.threshold = min_ttc_threshold_;

    if (safe) {
        result.message = "Safe TTC: " + std::to_string(min_ttc) + "s";
    } else {
        result.message = "Critical TTC: " + std::to_string(min_ttc) + "s (threshold: "
                        + std::to_string(min_ttc_threshold_) + "s)";
    }

    return result;
}

// ============================================================================
// Path Curvature Metric
// ============================================================================

MetricMetadata PathCurvatureMetric::getMetadata() const {
    return {
        .name = "curvature",
        .display_name = "Path Curvature",
        .description = "Maximum path curvature",
        .category = MetricCategory::Performance,
        .unit = "1/m",
        .default_comparison = "max",
        .requires_ego = true,
        .requires_surrounding = false,
        .requires_road = false,
        .default_max = 0.1  // Maximum comfortable curvature
    };
}

core::MetricResult PathCurvatureMetric::compute(const core::Scene& scene,
                                               core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;
    double max_curvature = 0.0;
    double mean_curvature = 0.0;

    for (const auto& point : ego.points) {
        double curv = std::abs(point.curvature);
        max_curvature = std::max(max_curvature, curv);
        mean_curvature += curv;
    }

    if (!ego.points.empty()) {
        mean_curvature /= ego.points.size();
    }

    auto result = createResult(max_curvature, max_curvature <= getMetadata().default_max);
    result.addDetail("mean_curvature", mean_curvature);
    result.threshold = getMetadata().default_max;

    result.message = "Max curvature: " + std::to_string(max_curvature) + " 1/m, "
                    + "Mean curvature: " + std::to_string(mean_curvature) + " 1/m";

    return result;
}

// ============================================================================
// Smoothness Metric
// ============================================================================

MetricMetadata SmoothnessMetric::getMetadata() const {
    return {
        .name = "smoothness",
        .display_name = "Trajectory Smoothness",
        .description = "Measures the smoothness of the trajectory using higher-order derivatives",
        .category = MetricCategory::Comfort,
        .unit = "score",
        .default_comparison = "min",
        .requires_ego = true,
        .requires_surrounding = false,
        .requires_road = false,
        .default_max = 10.0  // Lower is better
    };
}

core::MetricResult SmoothnessMetric::compute(const core::Scene& scene,
                                           core::EvaluationContext* context) const {
    auto ego_opt = scene.getEgoTrajectory();
    if (!ego_opt) {
        auto result = createResult(0.0, false);
        result.message = "No ego trajectory found";
        return result;
    }

    const auto& ego = *ego_opt;

    if (ego.points.size() < 4) {
        auto result = createResult(0.0, true);
        result.message = "Not enough points to compute smoothness";
        return result;
    }

    // Compute smoothness as the total variation of acceleration (jerk squared integrated)
    double total_jerk_squared = 0.0;

    for (size_t i = 2; i < ego.points.size(); ++i) {
        double dt1 = ego.points[i - 1].timestamp - ego.points[i - 2].timestamp;
        double dt2 = ego.points[i].timestamp - ego.points[i - 1].timestamp;

        if (dt1 > 0 && dt2 > 0) {
            double jerk = (ego.points[i].acceleration - ego.points[i - 1].acceleration)
                         / ((dt1 + dt2) / 2.0);
            total_jerk_squared += jerk * jerk;
        }
    }

    // Compute smoothness score (lower is better)
    double smoothness = std::sqrt(total_jerk_squared);
    bool smooth = smoothness <= getMetadata().default_max;

    auto result = createResult(smoothness, smooth);
    result.addDetail("jerk_squared_integral", total_jerk_squared);
    result.threshold = getMetadata().default_max;

    result.message = "Smoothness score: " + std::to_string(smoothness);

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

static bool metrics_initialized = false;

void initializeBuiltInMetrics() {
    if (metrics_initialized) return;

    auto& registry = MetricRegistry::instance();

    registry.registerMetric(std::make_shared<CollisionMetric>());
    registry.registerMetric(std::make_shared<FollowingMetric>());
    registry.registerMetric(std::make_shared<LaneChangeMetric>());
    registry.registerMetric(std::make_shared<TrajectoryDeviationMetric>());
    registry.registerMetric(std::make_shared<SpeedComplianceMetric>());
    registry.registerMetric(std::make_shared<JerkMetric>());
    registry.registerMetric(std::make_shared<TimeToCollisionMetric>());
    registry.registerMetric(std::make_shared<PathCurvatureMetric>());
    registry.registerMetric(std::make_shared<SmoothnessMetric>());

    metrics_initialized = true;
    spdlog::info("Initialized {} built-in metrics", registry.getAllMetrics().size());
}

std::vector<std::string> getBuiltInMetricNames() {
    initializeBuiltInMetrics();
    return {
        "collision",
        "following",
        "lane_change",
        "trajectory_deviation",
        "speed_compliance",
        "jerk",
        "ttc",
        "curvature",
        "smoothness"
    };
}

std::optional<core::MetricResult> computeMetric(const std::string& name,
                                               const core::Scene& scene,
                                               core::EvaluationContext* context) {
    initializeBuiltInMetrics();

    auto metric = MetricRegistry::instance().getMetric(name);
    if (!metric) {
        spdlog::error("Metric not found: {}", name);
        return std::nullopt;
    }

    std::string error;
    if (!metric->validate(scene, error)) {
        if (context) {
            context->getLogger()->warn("Metric {} validation failed: {}", name, error);
        }
        return std::nullopt;
    }

    return metric->compute(scene, context);
}

std::vector<core::MetricResult> computeMetrics(const std::vector<std::string>& names,
                                               const core::Scene& scene,
                                               core::EvaluationContext* context) {
    std::vector<core::MetricResult> results;

    for (const auto& name : names) {
        auto result = computeMetric(name, scene, context);
        if (result) {
            results.push_back(*result);
        }
    }

    return results;
}

bool checkCollision(const core::TrajectoryPoint& p1, const core::TrajectoryPoint& p2, double margin) {
    // Simple bounding box collision detection
    // Transform p2 to p1's local frame
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;

    double half_width1 = p1.dimensions.width / 2.0;
    double half_length1 = p1.dimensions.length / 2.0;
    double half_width2 = p2.dimensions.width / 2.0;
    double half_length2 = p2.dimensions.length / 2.0;

    // Transform to p1's local coordinate system
    double cos_h = std::cos(p1.heading);
    double sin_h = std::sin(p1.heading);

    double dx_local = dx * cos_h + dy * sin_h;
    double dy_local = -dx * sin_h + dy * cos_h;

    double collision_threshold_x = half_length1 + half_length2 + margin;
    double collision_threshold_y = half_width1 + half_width2 + margin;

    return (std::abs(dx_local) < collision_threshold_x) && (std::abs(dy_local) < collision_threshold_y);
}

std::optional<double> computeTTC(const core::Trajectory& traj1, const core::Trajectory& traj2) {
    double min_ttc = std::numeric_limits<double>::max();

    // Use binary search for each point in traj1 - O(n log m) instead of O(n*m)
    for (const auto& p1 : traj1.points) {
        auto p2_opt = traj2.getPointAtTime(p1.timestamp);
        if (!p2_opt) continue;

        const auto& p2 = *p2_opt;

        // Compute relative velocity
        double dvx = p1.velocity * std::cos(p1.heading) - p2.velocity * std::cos(p2.heading);
        double dvy = p1.velocity * std::sin(p1.heading) - p2.velocity * std::sin(p2.heading);

        // Compute relative position vector
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;

        // Project relative position onto relative velocity
        double relative_dist = std::sqrt(dx * dx + dy * dy);
        double rel_v_mag = std::sqrt(dvx * dvx + dvy * dvy);

        // Simple TTC: distance / closing speed
        if (rel_v_mag > 0.01) {
            // Check if approaching
            double closing_speed = -(dx * dvx + dy * dvy) / relative_dist;
            if (closing_speed > 0.01) {
                double ttc = relative_dist / closing_speed;
                min_ttc = std::min(min_ttc, ttc);
            }
        }
    }

    if (min_ttc < std::numeric_limits<double>::max() * 0.5) {
        return min_ttc;
    }
    return std::nullopt;
}

std::optional<core::Trajectory> findLeadVehicle(const core::Scene& scene,
                                                const core::Trajectory& ego,
                                                double look_ahead_time) {
    const auto& ego_start = ego.points.front();
    double look_ahead_dist = ego_start.velocity * look_ahead_time;

    const core::Trajectory* lead_vehicle = nullptr;
    double min_longitudinal_dist = std::numeric_limits<double>::max();

    for (const auto& traj : scene.trajectories) {
        if (traj.type == core::ObjectType::EgoVehicle) continue;
        if (traj.type == core::ObjectType::StaticObstacle) continue;

        // Check first point for initial relative position
        const auto& traj_start = traj.points.front();
        double dx = traj_start.x - ego_start.x;
        double dy = traj_start.y - ego_start.y;

        // Transform to ego's local frame
        double cos_h = std::cos(ego_start.heading);
        double sin_h = std::sin(ego_start.heading);

        double local_x = dx * cos_h + dy * sin_h;
        double local_y = -dx * sin_h + dy * cos_h;

        // Check if vehicle is ahead and in the same lane
        if (local_x > 0 && local_x < look_ahead_dist && std::abs(local_y) < 2.0) {
            if (local_x < min_longitudinal_dist) {
                min_longitudinal_dist = local_x;
                lead_vehicle = &traj;
            }
        }
    }

    if (lead_vehicle) {
        return *lead_vehicle;
    }
    return std::nullopt;
}

} // namespace metrics
} // namespace autoeval
