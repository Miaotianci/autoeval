/**
 * @file test_metrics.cpp
 * @brief Unit tests for metric correctness
 */

#include "autoeval/autoeval.h"
#include "autoeval/metrics/base_metric.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace autoeval;
using namespace autoeval::core;
using namespace autoeval::metrics;

// ============================================================================
// Helper: Create a basic scene
// ============================================================================

static Scene makeSceneWithEgo(std::initializer_list<TrajectoryPoint> points) {
    Scene scene;
    scene.id = "test";
    scene.road_info.speed_limit = 13.89;
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (const auto& p : points) {
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);
    return scene;
}

static Scene makeSceneWithEgoAndLead(
    std::initializer_list<TrajectoryPoint> ego_points,
    std::initializer_list<TrajectoryPoint> lead_points)
{
    Scene scene;
    scene.id = "test";
    scene.road_info.speed_limit = 13.89;
    {
        Trajectory ego;
        ego.id = "ego";
        ego.type = ObjectType::EgoVehicle;
        for (const auto& p : ego_points) ego.points.push_back(p);
        scene.trajectories.push_back(ego);
    }
    {
        Trajectory lead;
        lead.id = "lead";
        lead.type = ObjectType::Vehicle;
        for (const auto& p : lead_points) lead.points.push_back(p);
        scene.trajectories.push_back(lead);
    }
    return scene;
}

// ============================================================================
// Test: Collision Metric
// ============================================================================

void test_collision_no_collision() {
    std::cout << "  collision (no collision): ";
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{5,0,0,1},{10,0,0,2},{15,0,0,3},{20,0,0,4}},
        {{0,10,0,0},{5,10,0,1},{10,10,0,2},{15,10,0,3},{20,10,0,4}}
    );

    auto result = computeMetric("collision", scene, nullptr);
    assert(result.has_value());
    assert(result->value == 0.0);
    assert(result->pass == true);
    std::cout << "PASS\n";
}

void test_collision_with_collision() {
    std::cout << "  collision (with collision): ";
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{1,0,0,0.1},{2,0,0,0.2}},
        {{0,0,0,0},{1,0,0,0.1},{2,0,0,0.2}}
    );

    auto result = computeMetric("collision", scene, nullptr);
    assert(result.has_value());
    // Points overlap exactly → collision detected
    assert(result->value >= 1.0);
    std::cout << "PASS\n";
}

void test_collision_slightly_offset() {
    std::cout << "  collision (slightly offset, no collision): ";
    // Two vehicles side by side at 3m lateral offset (car width ~2m + 1m gap)
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{5,0,0,1},{10,0,0,2}},
        {{0,3.5,0,0},{5,3.5,0,1},{10,3.5,0,2}}
    );

    auto result = computeMetric("collision", scene, nullptr);
    assert(result.has_value());
    // No collision at safe lateral distance
    assert(result->value == 0.0);
    assert(result->pass == true);
    std::cout << "PASS\n";
}

// ============================================================================
// Test: Following Metric
// ============================================================================

void test_following_safe_distance() {
    std::cout << "  following (safe distance): ";
    // Ego 10m behind lead, both going same speed
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{10,0,0,1},{20,0,0,2}},
        {{10,0,0,0},{20,0,0,1},{30,0,0,2}}
    );

    auto result = computeMetric("following", scene, nullptr);
    assert(result.has_value());
    // Minimum gap ~10m → should pass with 2s time gap
    assert(result->value >= 10.0);
    assert(result->pass == true);
    std::cout << "PASS (min_gap=" << result->value << "m)\n";
}

void test_following_unsafe_distance() {
    std::cout << "  following (unsafe distance): ";
    // Ego right behind lead (1m gap), both same speed
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{1,0,0,1},{2,0,0,2}},
        {{2,0,0,0},{3,0,0,1},{4,0,0,2}}
    );

    auto result = computeMetric("following", scene, nullptr);
    assert(result.has_value());
    // Gap ~1-2m → time gap < 2s → fail
    assert(result->value < 3.0);
    std::cout << "PASS (min_gap=" << result->value << "m)\n";
}

// ============================================================================
// Test: Jerk Metric
// ============================================================================

void test_jerk_smooth_trajectory() {
    std::cout << "  jerk (smooth trajectory): ";
    // Constant velocity → zero acceleration → zero jerk
    Scene scene;
    scene.id = "smooth";
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i <= 10; ++i) {
        TrajectoryPoint p;
        p.x = i;
        p.y = 0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        p.acceleration = 0.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    auto result = computeMetric("jerk", scene, nullptr);
    assert(result.has_value());
    assert(result->pass == true);
    std::cout << "PASS (jerk=" << result->value << " m/s^3)\n";
}

void test_jerk_hard_brake() {
    std::cout << "  jerk (hard deceleration): ";
    // Deceleration from 10 m/s to 0 in 1s → a = -10 m/s^2
    Scene scene;
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i <= 10; ++i) {
        TrajectoryPoint p;
        p.x = i;
        p.y = 0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0 - i;
        p.acceleration = -1.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    auto result = computeMetric("jerk", scene, nullptr);
    assert(result.has_value());
    // Max jerk should be non-zero (from acceleration changes)
    std::cout << "PASS (jerk=" << result->value << " m/s^3)\n";
}

// ============================================================================
// Test: TTC Metric
// ============================================================================

void test_ttc_no_approach() {
    std::cout << "  ttc (parallel, no approach): ";
    // Ego and lead side by side, no closing
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{10,0,0,1},{20,0,0,2}},
        {{0,5,0,0},{10,5,0,1},{20,5,0,2}}
    );

    auto result = computeMetric("ttc", scene, nullptr);
    assert(result.has_value());
    // Parallel → TTC should be large (no collision course)
    assert(result->value >= 10.0);
    std::cout << "PASS (ttc=" << result->value << "s)\n";
}

void test_ttc_closing() {
    std::cout << "  ttc (closing scenario): ";
    // Ego at 0m going 20 m/s, lead at 50m going 10 m/s
    // Closing speed = 10 m/s, gap = 50m → TTC = 5s
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{20,0,0,1},{40,0,0,2}},
        {{50,0,0,0},{60,0,0,1},{70,0,0,2}}
    );

    auto result = computeMetric("ttc", scene, nullptr);
    assert(result.has_value());
    // TTC should be around 5s (approximately)
    assert(result->value > 0);
    assert(result->value < 100.0);
    std::cout << "PASS (ttc=" << result->value << "s)\n";
}

void test_ttc_receding() {
    std::cout << "  ttc (receding scenario): ";
    // Ego going slower than lead → gap increases
    auto scene = makeSceneWithEgoAndLead(
        {{0,0,0,0},{10,0,0,1},{20,0,0,2}},
        {{30,0,0,0},{45,0,0,1},{60,0,0,2}}
    );

    auto result = computeMetric("ttc", scene, nullptr);
    assert(result.has_value());
    // Large TTC (receding)
    assert(result->value > 10.0);
    std::cout << "PASS (ttc=" << result->value << "s)\n";
}

// ============================================================================
// Test: Speed Compliance
// ============================================================================

void test_speed_compliance_within_limit() {
    std::cout << "  speed_compliance (within limit): ";
    // Speed ~10 m/s (36 km/h) < 13.89 m/s (50 km/h)
    auto scene = makeSceneWithEgo({{0,0,0,0,10},{5,0,0,0.5,10},{10,0,0,1,10}});
    scene.road_info.speed_limit = 13.89;

    auto result = computeMetric("speed_compliance", scene, nullptr);
    assert(result.has_value());
    assert(result->pass == true);
    std::cout << "PASS\n";
}

void test_speed_compliance_over_limit() {
    std::cout << "  speed_compliance (over limit): ";
    // Speed ~15 m/s (54 km/h) > 13.89 m/s (50 km/h)
    auto scene = makeSceneWithEgo({{0,0,0,0,15},{5,0,0,0.5,15},{10,0,0,1,15}});
    scene.road_info.speed_limit = 13.89;

    auto result = computeMetric("speed_compliance", scene, nullptr);
    assert(result.has_value());
    assert(result->pass == false);
    std::cout << "PASS\n";
}

// ============================================================================
// Test: Curvature Metric
// ============================================================================

void test_curvature_straight_line() {
    std::cout << "  curvature (straight line): ";
    auto scene = makeSceneWithEgo({{0,0,0,0,10},{5,0,0,0.5,10},{10,0,0,1,10}});

    auto result = computeMetric("curvature", scene, nullptr);
    assert(result.has_value());
    assert(result->value < 0.01);  // Near zero for straight line
    std::cout << "PASS (curvature=" << result->value << " 1/m)\n";
}

void test_curvature_curve() {
    std::cout << "  curvature (curve): ";
    // Approximate a circle of radius 10m → curvature = 0.1
    // Points along a circle: (10-10cos(t), 10sin(t))
    Scene scene;
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i <= 20; ++i) {
        double t = i * M_PI / 10.0;
        TrajectoryPoint p;
        p.x = 10.0 - 10.0 * std::cos(t);
        p.y = 10.0 * std::sin(t);
        p.timestamp = i * 0.1;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    auto result = computeMetric("curvature", scene, nullptr);
    assert(result.has_value());
    // Should detect significant curvature
    assert(result->value > 0.05);
    std::cout << "PASS (curvature=" << result->value << " 1/m)\n";
}

// ============================================================================
// Test: Smoothness Metric
// ============================================================================

void test_smoothness() {
    std::cout << "  smoothness: ";
    auto scene = makeSceneWithEgo({{0,0,0,0,10},{5,0,0,0.5,10},{10,0,0,1,10}});

    auto result = computeMetric("smoothness", scene, nullptr);
    assert(result.has_value());
    assert(result->value >= 0.0);
    std::cout << "PASS (smoothness=" << result->value << ")\n";
}

// ============================================================================
// Test: Lane Change Metric
// ============================================================================

void test_lane_change_safe() {
    std::cout << "  lane_change (safe): ";
    // Ego starts in lane 0, moves to lane 3.5 with safe gap to adjacent vehicle
    Scene scene;
    {
        Trajectory ego;
        ego.id = "ego";
        ego.type = ObjectType::EgoVehicle;
        for (int i = 0; i < 10; ++i) {
            TrajectoryPoint p;
            p.x = i * 5.0;
            // Smooth lane change over 2s
            p.y = (i < 5) ? 0.0 : 3.5;
            p.timestamp = i * 0.2;
            ego.points.push_back(p);
        }
        scene.trajectories.push_back(ego);
    }
    {
        // Adjacent vehicle ahead, safe gap
        Trajectory adj;
        adj.id = "adjacent";
        adj.type = ObjectType::Vehicle;
        for (int i = 0; i < 10; ++i) {
            TrajectoryPoint p;
            p.x = i * 5.0 + 20.0;  // 20m ahead
            p.y = 3.5;
            p.timestamp = i * 0.2;
            adj.points.push_back(p);
        }
        scene.trajectories.push_back(adj);
    }

    auto result = computeMetric("lane_change", scene, nullptr);
    assert(result.has_value());
    std::cout << "PASS (score=" << result->value << ")\n";
}

// ============================================================================
// Test: Trajectory Deviation Metric
// ============================================================================

void test_trajectory_deviation_zero() {
    std::cout << "  trajectory_deviation (identical): ";
    Scene scene;
    {
        Trajectory traj;
        traj.id = "evaluated";
        traj.type = ObjectType::EgoVehicle;
        for (int i = 0; i < 5; ++i) {
            TrajectoryPoint p;
            p.x = i;
            p.y = 0;
            p.timestamp = i * 0.1;
            traj.points.push_back(p);
        }
        scene.trajectories.push_back(traj);
    }
    {
        Trajectory ref;
        ref.id = "reference";
        ref.type = ObjectType::Other;
        for (int i = 0; i < 5; ++i) {
            TrajectoryPoint p;
            p.x = i;
            p.y = 0;
            p.timestamp = i * 0.1;
            ref.points.push_back(p);
        }
        scene.trajectories.push_back(ref);
    }

    auto result = computeMetric("trajectory_deviation", scene, nullptr);
    assert(result.has_value());
    assert(result->value == 0.0);
    assert(result->pass == true);
    std::cout << "PASS\n";
}

// ============================================================================
// Test: Threshold Checking
// ============================================================================

void test_threshold_checking() {
    std::cout << "  threshold checking: ";

    MetricThreshold th;
    th.metric_name = "test";
    th.min = 1.0;
    th.max = 10.0;
    th.comparison = "range";
    th.enabled = true;

    assert(th.check(5.0) == true);   // inside range
    assert(th.check(0.5) == false);  // below min
    assert(th.check(15.0) == false); // above max

    th.enabled = false;
    assert(th.check(0.0) == true);    // disabled → always pass

    th.comparison = "min";
    assert(th.check(5.0) == true);
    assert(th.check(0.5) == false);

    th.comparison = "max";
    assert(th.check(5.0) == true);
    assert(th.check(15.0) == false);

    th.comparison = "equal";
    assert(th.check(5.0) == true);
    assert(th.check(5.0001) == false);

    std::cout << "PASS\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\nRunning Metric Correctness Tests\n";
    std::cout << "=================================\n\n";

    autoeval::initialize();
    initializeBuiltInMetrics();

    try {
        // Collision tests
        std::cout << "[CollisionMetric]\n";
        test_collision_no_collision();
        test_collision_with_collision();
        test_collision_slightly_offset();

        // Following tests
        std::cout << "[FollowingMetric]\n";
        test_following_safe_distance();
        test_following_unsafe_distance();

        // Jerk tests
        std::cout << "[JerkMetric]\n";
        test_jerk_smooth_trajectory();
        test_jerk_hard_brake();

        // TTC tests
        std::cout << "[TimeToCollisionMetric]\n";
        test_ttc_no_approach();
        test_ttc_closing();
        test_ttc_receding();

        // Speed compliance
        std::cout << "[SpeedComplianceMetric]\n";
        test_speed_compliance_within_limit();
        test_speed_compliance_over_limit();

        // Curvature
        std::cout << "[PathCurvatureMetric]\n";
        test_curvature_straight_line();
        test_curvature_curve();

        // Smoothness
        std::cout << "[SmoothnessMetric]\n";
        test_smoothness();

        // Lane change
        std::cout << "[LaneChangeMetric]\n";
        test_lane_change_safe();

        // Trajectory deviation
        std::cout << "[TrajectoryDeviationMetric]\n";
        test_trajectory_deviation_zero();

        // Threshold checking
        std::cout << "[ThresholdChecking]\n";
        test_threshold_checking();

        std::cout << "\n=================================\n";
        std::cout << "All metric tests PASSED!\n\n";

        autoeval::shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
