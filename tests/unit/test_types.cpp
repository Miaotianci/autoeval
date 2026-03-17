/**
 * @file test_types.cpp
 * @brief Unit tests for core types
 */

#include "autoeval/core/types.h"
#include <cassert>
#include <iostream>

using namespace autoeval::core;

void test_trajectory_point() {
    std::cout << "Testing TrajectoryPoint..." << std::endl;

    TrajectoryPoint p1;
    p1.x = 0.0;
    p1.y = 0.0;
    p1.z = 0.0;
    p1.timestamp = 0.0;
    p1.heading = 0.0;
    p1.velocity = 10.0;

    TrajectoryPoint p2;
    p2.x = 3.0;
    p2.y = 4.0;
    p2.z = 0.0;
    p2.timestamp = 1.0;
    p2.heading = 0.0;
    p2.velocity = 10.0;

    double dist = p1.distanceTo(p2);
    assert(std::abs(dist - 5.0) < 0.001);

    std::cout << "  distanceTo: PASS" << std::endl;
}

void test_trajectory() {
    std::cout << "Testing Trajectory..." << std::endl;

    Trajectory traj;
    traj.id = "test_traj";
    traj.type = ObjectType::EgoVehicle;

    // Add some points
    for (int i = 0; i < 10; ++i) {
        TrajectoryPoint p;
        p.x = i * 1.0;
        p.y = 0.0;
        p.z = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        traj.points.push_back(p);
    }

    assert(traj.isValid());
    assert(traj.duration() == 0.9);
    assert(traj.length() == 9.0);

    auto point = traj.getPointAtTime(0.5);
    assert(point.has_value());
    assert(std::abs(point->x - 5.0) < 0.01);

    std::cout << "  isValid: PASS" << std::endl;
    std::cout << "  duration: PASS" << std::endl;
    std::cout << "  length: PASS" << std::endl;
    std::cout << "  getPointAtTime: PASS" << std::endl;
}

void test_scene() {
    std::cout << "Testing Scene..." << std::endl;

    Scene scene;
    scene.id = "test_scene";

    // Add ego trajectory
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i < 5; ++i) {
        TrajectoryPoint p;
        p.x = i * 1.0;
        p.y = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    // Add surrounding vehicle
    Trajectory vehicle;
    vehicle.id = "v1";
    vehicle.type = ObjectType::Vehicle;
    for (int i = 0; i < 5; ++i) {
        TrajectoryPoint p;
        p.x = 10.0 + i * 0.8;
        p.y = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 8.0;
        vehicle.points.push_back(p);
    }
    scene.trajectories.push_back(vehicle);

    // Test getEgoTrajectory
    auto ego_opt = scene.getEgoTrajectory();
    assert(ego_opt.has_value());
    assert(ego_opt->id == "ego");

    // Test getTrajectoriesByType
    auto vehicles = scene.getTrajectoriesByType(ObjectType::Vehicle);
    assert(vehicles.size() == 1);

    // Test getTrajectoryById
    auto v1_opt = scene.getTrajectoryById("v1");
    assert(v1_opt.has_value());
    assert(v1_opt->id == "v1");

    std::cout << "  getEgoTrajectory: PASS" << std::endl;
    std::cout << "  getTrajectoriesByType: PASS" << std::endl;
    std::cout << "  getTrajectoryById: PASS" << std::endl;
}

void test_metric_result() {
    std::cout << "Testing MetricResult..." << std::endl;

    MetricResult result;
    result.name = "test_metric";
    result.category = "safety";
    result.value = 1.5;
    result.unit = "m";
    result.pass = true;
    result.message = "Test passed";
    result.threshold = 2.0;
    result.threshold_type = "max";

    assert(result.name == "test_metric");
    assert(result.value == 1.5);
    assert(result.pass == true);

    result.addDetail("key1", 42.0);
    result.addDetail("key2", "test_value");

    auto val1 = result.getDetail<double>("key1");
    assert(val1.has_value());
    assert(*val1 == 42.0);

    auto val2 = result.getDetail<std::string>("key2");
    assert(val2.has_value());
    assert(*val2 == "test_value");

    std::cout << "  basic properties: PASS" << std::endl;
    std::cout << "  addDetail/getDetail: PASS" << std::endl;
}

void test_evaluation_summary() {
    std::cout << "Testing EvaluationSummary..." << std::endl;

    EvaluationSummary summary;
    summary.id = "eval_123";
    summary.overall_status = "pass";

    // Add some metric results
    for (int i = 0; i < 5; ++i) {
        MetricResult result;
        result.name = "metric_" + std::to_string(i);
        result.value = i * 1.0;
        result.unit = "m";
        result.pass = (i < 3);  // First 3 pass
        summary.metric_results.push_back(result);
    }

    assert(summary.passedCount() == 3);
    assert(summary.failedCount() == 2);
    assert(std::abs(summary.passRate() - 0.6) < 0.001);

    std::cout << "  passedCount: PASS" << std::endl;
    std::cout << "  failedCount: PASS" << std::endl;
    std::cout << "  passRate: PASS" << std::endl;
}

void test_evaluation_config() {
    std::cout << "Testing EvaluationConfig..." << std::endl;

    EvaluationConfig config;
    config.name = "test_config";
    config.trajectory_file = "test.json";
    config.output_dir = "./results";

    config.metrics = {"collision", "following", "ttc"};

    MetricThreshold th1;
    th1.metric_name = "collision";
    th1.max = 0.0;
    th1.comparison = "max";
    config.thresholds.push_back(th1);

    assert(config.name == "test_config");
    assert(config.metrics.size() == 3);
    assert(config.thresholds.size() == 1);

    auto th = config.getThreshold("collision");
    assert(th.has_value());
    assert(th->max == 0.0);

    std::cout << "  basic properties: PASS" << std::endl;
    std::cout << "  getThreshold: PASS" << std::endl;
}

void test_utility_functions() {
    std::cout << "Testing utility functions..." << std::endl;

    // Test degToRad
    double rad = degToRad(180.0);
    assert(std::abs(rad - M_PI) < 0.001);

    // Test radToDeg
    double deg = radToDeg(M_PI);
    assert(std::abs(deg - 180.0) < 0.001);

    // Test normalizeAngle - mathematical modulo [0, 2*PI)
    double angle1 = normalizeAngle(3.5 * M_PI);
    assert(std::abs(angle1 - 1.5 * M_PI) < 0.001);

    double angle2 = normalizeAngle(-2.5 * M_PI);
    // -2.5*PI mod 2*PI = 1.5*PI (mathematical modulo)
    assert(std::abs(angle2 - 1.5 * M_PI) < 0.001);

    // Test angleDifference
    double diff = angleDifference(0.8 * M_PI, 0.2 * M_PI);
    assert(std::abs(diff - 0.6 * M_PI) < 0.001);

    std::cout << "  degToRad: PASS" << std::endl;
    std::cout << "  radToDeg: PASS" << std::endl;
    std::cout << "  normalizeAngle: PASS" << std::endl;
    std::cout << "  angleDifference: PASS" << std::endl;
}

int main() {
    std::cout << "\nRunning AutoEval Unit Tests\n";
    std::cout << "============================\n\n";

    test_trajectory_point();
    test_trajectory();
    test_scene();
    test_metric_result();
    test_evaluation_summary();
    test_evaluation_config();
    test_utility_functions();

    std::cout << "\n============================\n";
    std::cout << "All tests PASSED!\n\n";

    return 0;
}
