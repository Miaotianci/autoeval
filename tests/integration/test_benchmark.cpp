/**
 * @file test_benchmark.cpp
 * @brief Performance benchmarks for AutoEval
 *
 * Run: ./autoeval_bench
 */

#include "autoeval/autoeval.h"
#include "autoeval/metrics/base_metric.h"
#include "autoeval/loader/data_loader.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace autoeval;
using namespace autoeval::core;
using namespace autoeval::metrics;

static std::vector<TrajectoryPoint> generateRandomTrajectory(int count, double seed = 0.0) {
    std::vector<TrajectoryPoint> points;
    points.reserve(count);
    std::mt19937 rng(static_cast<unsigned>(seed));
    std::uniform_real_distribution<double> dx(-0.5, 0.5);
    std::uniform_real_distribution<double> dy(-0.1, 0.1);
    std::uniform_real_distribution<double> dv(-1.0, 1.0);

    double x = 0, y = 0, v = 10.0, a = 0.0;
    double t = 0.0;
    double dt = 0.1;

    for (int i = 0; i < count; ++i) {
        TrajectoryPoint p;
        p.x = x;
        p.y = y;
        p.z = 0;
        p.timestamp = t;
        p.velocity = std::max(0.0, v);
        p.acceleration = a;
        p.heading = 0.0;
        p.curvature = 0.0;
        points.push_back(p);

        x += v * dt;
        y += dy(rng);
        a = dx(rng);
        v = std::max(0.0, v + a * dt);
        t += dt;
    }
    return points;
}

static Scene makeLargeScene(int num_vehicles, int points_per_vehicle) {
    Scene scene;
    scene.id = "large_scene";

    for (int v = 0; v < num_vehicles; ++v) {
        Trajectory traj;
        traj.id = "vehicle_" + std::to_string(v);
        traj.type = (v == 0) ? ObjectType::EgoVehicle : ObjectType::Vehicle;
        traj.source = "benchmark";
        traj.points = generateRandomTrajectory(points_per_vehicle, v * 12345.0);
        scene.trajectories.push_back(std::move(traj));
    }
    return scene;
}

template<typename Func>
static double benchmark(const std::string& name, int iterations, Func&& func) {
    // Warmup
    func();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg = ms / iterations;

    std::cout << "  " << std::left << std::setw(40) << name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << avg << " ms/op  (" << std::setw(8) << ms << " ms total)\n";
    return avg;
}

static void bench_trajectory_construction() {
    auto points = generateRandomTrajectory(1000, 42.0);
    Trajectory traj;
    traj.id = "bench";
    traj.type = ObjectType::EgoVehicle;
    traj.points = std::move(points);
    (void)traj.length();
}

static void bench_trajectory_interpolation() {
    auto points = generateRandomTrajectory(1000, 42.0);
    Trajectory traj;
    traj.id = "bench";
    traj.type = ObjectType::EgoVehicle;
    traj.points = std::move(points);

    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(0.0, 100.0);

    for (int i = 0; i < 100; ++i) {
        (void)traj.getPointAtTime(dist(rng));
    }
}

static void bench_collision_metric() {
    auto scene = makeLargeScene(2, 500);
    (void)computeMetric("collision", scene, nullptr);
}

static void bench_following_metric() {
    auto scene = makeLargeScene(2, 500);
    (void)computeMetric("following", scene, nullptr);
}

static void bench_ttc_metric() {
    auto scene = makeLargeScene(2, 500);
    (void)computeMetric("ttc", scene, nullptr);
}

static void bench_jerk_metric() {
    auto scene = makeLargeScene(1, 1000);
    (void)computeMetric("jerk", scene, nullptr);
}

static void bench_curvature_metric() {
    auto scene = makeLargeScene(1, 500);
    (void)computeMetric("curvature", scene, nullptr);
}

static void bench_smoothness_metric() {
    auto scene = makeLargeScene(1, 500);
    (void)computeMetric("smoothness", scene, nullptr);
}

static void bench_all_metrics() {
    auto scene = makeLargeScene(3, 200);
    auto names = std::vector<std::string>{
        "collision", "following", "ttc", "jerk",
        "curvature", "smoothness", "speed_compliance"
    };
    (void)computeMetrics(names, scene, nullptr);
}

static void bench_large_scene_10k() {
    auto scene = makeLargeScene(10, 1000);
    for (const auto& name : {"collision", "following", "ttc", "jerk"}) {
        (void)computeMetric(name, scene, nullptr);
    }
}

static void bench_scene_query() {
    auto scene = makeLargeScene(5, 200);
    (void)scene.getEgoTrajectory();
    (void)scene.getTrajectoryById("vehicle_3");
    (void)scene.getTrajectoriesByType(ObjectType::Vehicle);
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "       AutoEval Performance Benchmarks\n";
    std::cout << "========================================\n\n";

    autoeval::initialize();
    initializeBuiltInMetrics();

    // Suppress spdlog noise
    std::cout << std::left;

    std::cout << "[Trajectory Operations] (1000 iterations)\n";
    benchmark("Trajectory construction (1000 pts)", 1000, bench_trajectory_construction);
    benchmark("Trajectory interpolation (100 queries)", 1000, bench_trajectory_interpolation);
    benchmark("Scene query (5 vehicles)", 1000, bench_scene_query);

    std::cout << "\n[Single Metric - 500 pts, 2 vehicles] (500 iterations)\n";
    benchmark("Collision metric", 500, bench_collision_metric);
    benchmark("Following metric", 500, bench_following_metric);
    benchmark("TTC metric", 500, bench_ttc_metric);

    std::cout << "\n[Single Metric - 1000 pts, 1 vehicle] (500 iterations)\n";
    benchmark("Jerk metric", 500, bench_jerk_metric);
    benchmark("Curvature metric", 500, bench_curvature_metric);
    benchmark("Smoothness metric", 500, bench_smoothness_metric);

    std::cout << "\n[All Metrics - 200 pts, 3 vehicles] (100 iterations)\n";
    benchmark("All 7 built-in metrics", 100, bench_all_metrics);

    std::cout << "\n[Large Scene - 10k pts, 10 vehicles] (50 iterations)\n";
    benchmark("Collision+Following+TTC+Jerk", 50, bench_large_scene_10k);

    std::cout << "\n========================================\n";
    std::cout << "Benchmark complete.\n\n";

    autoeval::shutdown();
    return 0;
}
