/**
 * @file types.h
 * @brief Core data type definitions for AutoEval
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <chrono>
#include <algorithm>
#include <cmath>

// Define M_PI if not available (MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace autoeval {
namespace core {

// ============================================================================
// Object Type Enumeration
// ============================================================================

/**
 * @brief Type of object in the scene
 */
enum class ObjectType {
    Vehicle = 0,
    Pedestrian,
    Cyclist,
    StaticObstacle,
    EgoVehicle,
    Other
};

/**
 * @brief Convert ObjectType to string
 */
inline const char* toString(ObjectType type) {
    switch (type) {
        case ObjectType::Vehicle: return "vehicle";
        case ObjectType::Pedestrian: return "pedestrian";
        case ObjectType::Cyclist: return "cyclist";
        case ObjectType::StaticObstacle: return "obstacle";
        case ObjectType::EgoVehicle: return "ego";
        case ObjectType::Other: return "other";
        default: return "unknown";
    }
}

// ============================================================================
// Trajectory Point
// ============================================================================

/**
 * @brief Single point in a trajectory
 */
struct TrajectoryPoint {
    // Position
    double x = 0.0;          ///< X coordinate (meters)
    double y = 0.0;          ///< Y coordinate (meters)
    double z = 0.0;          ///< Z coordinate (meters), optional

    // Orientation
    double heading = 0.0;    ///< Heading angle (radians)

    // Motion
    double velocity = 0.0;   ///< Longitudinal velocity (m/s)
    double acceleration = 0.0; ///< Longitudinal acceleration (m/s^2)

    // Time
    double timestamp = 0.0;  ///< Timestamp (seconds)

    // Additional curvature information
    double curvature = 0.0;  ///< Path curvature (1/m)

    // Dimensions of the object at this point
    struct Dimensions {
        double length = 4.5;  ///< Length (meters)
        double width = 2.0;   ///< Width (meters)
        double height = 1.5;  ///< Height (meters)
    } dimensions;

    // Confidence score (0.0 to 1.0)
    double confidence = 1.0;

    /**
     * @brief Calculate Euclidean distance to another point
     */
    double distanceTo(const TrajectoryPoint& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        double dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /**
     * @brief Interpolate between this point and another
     * @param ratio Interpolation ratio (0.0 = this, 1.0 = other)
     */
    TrajectoryPoint interpolate(const TrajectoryPoint& other, double ratio) const {
        TrajectoryPoint result;
        result.x = x + (other.x - x) * ratio;
        result.y = y + (other.y - y) * ratio;
        result.z = z + (other.z - z) * ratio;
        result.heading = heading + (other.heading - heading) * ratio;
        result.velocity = velocity + (other.velocity - velocity) * ratio;
        result.acceleration = acceleration + (other.acceleration - acceleration) * ratio;
        result.timestamp = timestamp + (other.timestamp - timestamp) * ratio;
        result.curvature = curvature + (other.curvature - curvature) * ratio;
        result.confidence = confidence + (other.confidence - confidence) * ratio;
        result.dimensions = dimensions;  // Dimensions don't interpolate
        return result;
    }

    /**
     * @brief Get point at a specific time (linear interpolation)
     */
    TrajectoryPoint getPointAtTime(double t, const TrajectoryPoint& next) const {
        if (timestamp >= t) return *this;
        if (next.timestamp <= t) return next;
        double ratio = (t - timestamp) / (next.timestamp - timestamp);
        return interpolate(next, ratio);
    }
};

// ============================================================================
// Trajectory
// ============================================================================

/**
 * @brief Complete trajectory with metadata
 */
struct Trajectory {
    std::string id;                   ///< Unique trajectory identifier
    std::vector<TrajectoryPoint> points; ///< Trajectory points
    ObjectType type = ObjectType::Other; ///< Object type

    // Metadata
    std::string source;               ///< Data source identifier
    std::string algorithm;            ///< Algorithm that generated this

    /**
     * @brief Check if trajectory is valid
     */
    bool isValid() const {
        return !id.empty() && !points.empty();
    }

    /**
     * @brief Get total duration of trajectory
     */
    double duration() const {
        if (points.size() < 2) return 0.0;
        return points.back().timestamp - points.front().timestamp;
    }

    /**
     * @brief Get total length of trajectory
     */
    double length() const {
        double total = 0.0;
        for (size_t i = 1; i < points.size(); ++i) {
            total += points[i].distanceTo(points[i - 1]);
        }
        return total;
    }

    /**
     * @brief Get point at specific time (interpolated)
     */
    /**
     * @brief Get interpolated point at given time using binary search - O(log n)
     * @param t Time to interpolate at
     * @return Interpolated point at time t, or nullopt if trajectory is empty
     */
    std::optional<TrajectoryPoint> getPointAtTime(double t) const {
        if (points.empty()) return std::nullopt;
        if (t <= points.front().timestamp) return points.front();
        if (t >= points.back().timestamp) return points.back();

        // Binary search for the segment containing t - O(log n)
        size_t left = 0;
        size_t right = points.size() - 1;

        while (left < right) {
            size_t mid = left + (right - left + 1) / 2;
            if (points[mid].timestamp <= t) {
                left = mid;
            } else {
                right = mid - 1;
            }
        }

        // Now points[left] <= t < points[left + 1] (if left + 1 exists)
        if (left + 1 < points.size()) {
            return points[left].getPointAtTime(t, points[left + 1]);
        }
        return points[left];
    }

    /**
     * @brief Resample trajectory to fixed time interval
     */
    Trajectory resample(double dt) const {
        Trajectory resampled;
        resampled.id = id;
        resampled.type = type;
        resampled.source = source;
        resampled.algorithm = algorithm;

        if (points.empty()) return resampled;

        double t = points.front().timestamp;
        double end_time = points.back().timestamp;

        while (t <= end_time) {
            auto point = getPointAtTime(t);
            if (point) {
                resampled.points.push_back(*point);
            }
            t += dt;
        }

        return resampled;
    }
};

// ============================================================================
// Scene Information
// ============================================================================

/**
 * @brief Represents a scene with multiple objects and their trajectories
 */
struct Scene {
    std::string id;                           ///< Scene identifier
    std::vector<Trajectory> trajectories;     ///< All object trajectories

    // Scene metadata
    std::string location;                     ///< Location of scene
    std::string weather;                     ///< Weather condition
    std::string time_of_day;                  ///< Time of day

    // Road information
    struct RoadInfo {
        double lane_width = 3.5;             ///< Lane width (meters)
        int num_lanes = 1;                   ///< Number of lanes
        double speed_limit = 13.89;          ///< Speed limit (m/s, default 50 km/h)
    } road_info;

    /**
     * @brief Get ego vehicle trajectory
     */
    std::optional<Trajectory> getEgoTrajectory() const {
        for (const auto& traj : trajectories) {
            if (traj.type == ObjectType::EgoVehicle) {
                return traj;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get trajectories by type
     */
    std::vector<Trajectory> getTrajectoriesByType(ObjectType type) const {
        std::vector<Trajectory> result;
        for (const auto& traj : trajectories) {
            if (traj.type == type) {
                result.push_back(traj);
            }
        }
        return result;
    }

    /**
     * @brief Get trajectory by ID
     */
    std::optional<Trajectory> getTrajectoryById(const std::string& id) const {
        for (const auto& traj : trajectories) {
            if (traj.id == id) {
                return traj;
            }
        }
        return std::nullopt;
    }
};

// ============================================================================
// Metric Result
// ============================================================================

/**
 * @brief Result of a metric evaluation
 */
struct MetricResult {
    std::string name;                    ///< Metric name
    std::string category;                ///< Metric category (e.g., "safety", "comfort")
    double value;                         ///< Computed value
    std::string unit;                     ///< Unit of measurement
    bool pass;                            ///< Whether the metric passed threshold
    std::string message;                 ///< Human-readable description

    // Threshold information
    double threshold = 0.0;              ///< Threshold value used
    std::string threshold_type;           ///< "min", "max", "equal", etc.

    // Additional details
    struct Detail {
        std::string key;
        std::variant<double, int, std::string, bool> value;
    };
    std::vector<Detail> details;          ///< Additional metric details

    /**
     * @brief Add a detail to the result
     */
    template<typename T>
    void addDetail(const std::string& key, const T& value) {
        details.push_back({key, value});
    }

    /**
     * @brief Get detail value by key
     */
    template<typename T>
    std::optional<T> getDetail(const std::string& key) const {
        for (const auto& detail : details) {
            if (detail.key == key) {
                if (std::holds_alternative<T>(detail.value)) {
                    return std::get<T>(detail.value);
                }
            }
        }
        return std::nullopt;
    }
};

// ============================================================================
// Evaluation Summary
// ============================================================================

/**
 * @brief Complete summary of an evaluation run
 */
struct EvaluationSummary {
    std::string id;                        ///< Evaluation run identifier
    std::string config_file;               ///< Configuration file used
    std::string data_source;               ///< Data source used
    std::chrono::system_clock::time_point start_time;  ///< Start time
    std::chrono::system_clock::time_point end_time;    ///< End time

    std::vector<MetricResult> metric_results;  ///< All metric results
    std::string overall_status;            ///< "pass", "fail", "partial"

    /**
     * @brief Get total duration of evaluation
     */
    std::chrono::milliseconds duration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
    }

    /**
     * @brief Count passed metrics
     */
    size_t passedCount() const {
        return std::count_if(metric_results.begin(), metric_results.end(),
                            [](const MetricResult& r) { return r.pass; });
    }

    /**
     * @brief Count failed metrics
     */
    size_t failedCount() const {
        return std::count_if(metric_results.begin(), metric_results.end(),
                            [](const MetricResult& r) { return !r.pass; });
    }

    /**
     * @brief Get overall pass rate
     */
    double passRate() const {
        if (metric_results.empty()) return 1.0;
        return static_cast<double>(passedCount()) / metric_results.size();
    }
};

// ============================================================================
// Configuration Types
// ============================================================================

/**
 * @brief Threshold configuration for a metric
 */
struct MetricThreshold {
    std::string metric_name;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();
    std::string comparison = "range";  // "min", "max", "range", "equal"
    bool enabled = true;

    /**
     * @brief Check if a value passes this threshold
     */
    bool check(double value) const {
        if (!enabled) return true;
        if (comparison == "min") return value >= min;
        if (comparison == "max") return value <= max;
        if (comparison == "range") return value >= min && value <= max;
        if (comparison == "equal") return std::abs(value - min) < 1e-6;
        return true;
    }
};

/**
 * @brief Evaluation configuration
 */
struct EvaluationConfig {
    std::string name = "default";
    std::string description;

    // Input settings
    std::string trajectory_file;
    std::string scene_file;
    std::string output_dir;

    // Metrics to evaluate
    std::vector<std::string> metrics;

    // Metric thresholds
    std::vector<MetricThreshold> thresholds;

    // Simulation settings
    struct SimulationSettings {
        double dt = 0.1;                  ///< Time step (seconds)
        double duration = 10.0;            ///< Simulation duration (seconds)
        bool enable_collision_check = true;
        bool enable_road_boundary_check = true;
    } simulation;

    // Report settings
    struct ReportSettings {
        std::vector<std::string> formats = {"json"};  // "json", "html", "pdf"
        bool include_charts = true;
        bool include_details = true;
        std::string template_path;
    } report;

    /**
     * @brief Get threshold for a metric
     */
    std::optional<MetricThreshold> getThreshold(const std::string& metric_name) const {
        for (const auto& th : thresholds) {
            if (th.metric_name == metric_name) {
                return th;
            }
        }
        return std::nullopt;
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert degrees to radians
 */
constexpr inline double degToRad(double degrees) {
    return degrees * M_PI / 180.0;
}

/**
 * @brief Convert radians to degrees
 */
constexpr inline double radToDeg(double radians) {
    return radians * 180.0 / M_PI;
}

/**
 * @brief Normalize angle to range [0, 2*PI)
 * Using fmod for mathematical modulo
 */
inline double normalizeAngle(double angle) {
    double result = fmod(angle, 2 * M_PI);
    if (result < 0) result += 2 * M_PI;
    return result;
}

/**
 * @brief Calculate angular difference
 */
inline double angleDifference(double a, double b) {
    return normalizeAngle(a - b);
}

} // namespace core
} // namespace autoeval
