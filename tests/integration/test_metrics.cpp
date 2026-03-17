/**
 * @file test_metrics.cpp
 * @brief Integration tests for metrics
 */

#include "autoeval/autoeval.h"
#include "autoeval/metrics/base_metric.h"
#include <cassert>
#include <iostream>

using namespace autoeval;
using namespace autoeval::core;
using namespace autoeval::metrics;

void test_metric_registry() {
    std::cout << "Testing MetricRegistry..." << std::endl;

    autoeval::initialize();
    initializeBuiltInMetrics();

    auto& registry = MetricRegistry::instance();

    // Check that built-in metrics are registered
    assert(registry.hasMetric("collision"));
    assert(registry.hasMetric("following"));
    assert(registry.hasMetric("ttc"));
    assert(registry.hasMetric("jerk"));
    assert(registry.hasMetric("speed_compliance"));
    assert(registry.hasMetric("curvature"));
    assert(registry.hasMetric("smoothness"));
    assert(registry.hasMetric("lane_change"));
    assert(registry.hasMetric("trajectory_deviation"));

    // Get all metric names
    auto names = registry.getMetricNames();
    assert(!names.empty());

    std::cout << "  built-in metrics registered: PASS" << std::endl;
    std::cout << "  total metrics: " << names.size() << std::endl;
}

void test_single_metric() {
    std::cout << "Testing single metric evaluation..." << std::endl;

    autoeval::initialize();
    initializeBuiltInMetrics();

    // Create a test scene
    Scene scene;
    scene.id = "test_scene";
    scene.road_info.speed_limit = 13.89;

    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;

    // Add points
    for (int i = 0; i < 10; ++i) {
        TrajectoryPoint p;
        p.x = i * 1.0;
        p.y = 0.0;
        p.z = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        p.acceleration = 0.0;
        p.curvature = 0.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    // Compute a single metric
    auto result = computeMetric("collision", scene, nullptr);

    assert(result.has_value());
    assert(result->name == "collision");
    assert(result->category == "safety");
    assert(result->unit == "collision");

    std::cout << "  metric computation: PASS" << std::endl;
    std::cout << "  collision result: value=" << result->value
              << ", pass=" << result->pass << std::endl;
}

void test_multiple_metrics() {
    std::cout << "Testing multiple metrics evaluation..." << std::endl;

    autoeval::initialize();
    initializeBuiltInMetrics();

    // Create a test scene with ego and lead vehicle
    Scene scene;
    scene.id = "test_scene";
    scene.road_info.speed_limit = 13.89;

    // Ego vehicle
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i < 10; ++i) {
        TrajectoryPoint p;
        p.x = i * 1.0;
        p.y = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    // Lead vehicle (10m ahead)
    Trajectory lead;
    lead.id = "lead";
    lead.type = ObjectType::Vehicle;
    for (int i = 0; i < 10; ++i) {
        TrajectoryPoint p;
        p.x = 10.0 + i * 0.8;
        p.y = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 8.0;
        lead.points.push_back(p);
    }
    scene.trajectories.push_back(lead);

    // Compute multiple metrics
    std::vector<std::string> metric_names = {
        "collision", "following", "jerk", "curvature", "speed_compliance"
    };

    auto results = computeMetrics(metric_names, scene, nullptr);

    assert(results.size() == metric_names.size());

    for (const auto& result : results) {
        std::cout << "  " << result.name << ": value=" << result.value
                  << " " << result.unit << ", pass=" << result.pass << std::endl;
    }

    std::cout << "  multiple metrics computation: PASS" << std::endl;
}

void test_metric_metadata() {
    std::cout << "Testing metric metadata..." << std::endl;

    autoeval::initialize();
    initializeBuiltInMetrics();

    auto& registry = MetricRegistry::instance();
    auto all_metadata = registry.getAllMetadata();

    assert(!all_metadata.empty());

    // Check that all metadata has required fields
    for (const auto& meta : all_metadata) {
        assert(!meta.name.empty());
        assert(!meta.display_name.empty());
        assert(!meta.description.empty());
        assert(!meta.unit.empty());
        assert(!meta.default_comparison.empty());
    }

    std::cout << "  metadata validation: PASS" << std::endl;
    std::cout << "  total metrics with metadata: " << all_metadata.size() << std::endl;
}

int main() {
    std::cout << "\nRunning AutoEval Metrics Integration Tests\n";
    std::cout << "=========================================\n\n";

    try {
        test_metric_registry();
        test_single_metric();
        test_multiple_metrics();
        test_metric_metadata();

        std::cout << "\n=========================================\n";
        std::cout << "All metrics integration tests PASSED!\n\n";

        autoeval::shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
