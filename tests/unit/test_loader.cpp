/**
 * @file test_loader.cpp
 * @brief Unit tests for data loaders - error handling and edge cases
 */

#include "autoeval/autoeval.h"
#include "autoeval/core/types.h"
#include "autoeval/loader/data_loader.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace autoeval;
using namespace autoeval::loader;
namespace fs = std::filesystem;

static std::string temp_dir() {
    static std::string dir = (fs::temp_directory_path() / "autoeval_test").string();
    fs::create_directories(dir);
    return dir;
}

// ============================================================================
// JSON Loader Tests
// ============================================================================

void test_json_load_valid() {
    std::cout << "  json (valid file): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "valid.json";

    std::ofstream(path) << R"({
        "id": "traj1",
        "type": "ego",
        "points": [
            {"x": 0, "y": 0, "z": 0, "t": 0.0, "heading": 0.0, "v": 10.0, "a": 0.0},
            {"x": 1, "y": 0, "z": 0, "t": 0.1, "heading": 0.0, "v": 10.0, "a": 0.0}
        ]
    })";

    JsonTrajectoryLoader loader;
    core::Trajectory traj;
    auto result = loader.loadTrajectory(path, traj);

    assert(result.success == true);
    assert(traj.id == "traj1");
    assert(traj.points.size() == 2);
    assert(traj.points[0].x == 0.0);
    assert(traj.points[1].velocity == 10.0);

    fs::remove(path);
    std::cout << "PASS\n";
}

void test_json_load_invalid_syntax() {
    std::cout << "  json (invalid syntax): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "invalid.json";

    std::ofstream(path) << "{ invalid json content }";

    JsonTrajectoryLoader loader;
    core::Trajectory traj;
    auto result = loader.loadTrajectory(path, traj);

    assert(result.success == false);
    assert(!result.message.empty());
    std::cout << "PASS\n";
}

void test_json_load_missing_fields() {
    std::cout << "  json (missing required fields): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "missing.json";

    std::ofstream(path) << R"({
        "id": "traj1",
        "points": [{"x": 0}]
    })";

    JsonTrajectoryLoader loader;
    core::Trajectory traj;
    auto result = loader.loadTrajectory(path, traj);

    // Should fail or return empty (depends on strictness)
    // At minimum, should not crash
    std::cout << "PASS (result=" << result.success << ")\n";
}

void test_json_load_empty_points() {
    std::cout << "  json (empty points array): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "empty.json";

    std::ofstream(path) << R"({"id": "traj1", "type": "ego", "points": []})";

    JsonTrajectoryLoader loader;
    core::Trajectory traj;
    auto result = loader.loadTrajectory(path, traj);

    // Should load but with no points
    assert(traj.points.empty());
    std::cout << "PASS\n";
}

void test_json_load_non_existent_file() {
    std::cout << "  json (non-existent file): ";
    JsonTrajectoryLoader loader;
    core::Trajectory traj;
    auto result = loader.loadTrajectory("/nonexistent/path/file.json", traj);

    assert(result.success == false);
    std::cout << "PASS\n";
}

void test_json_load_scene() {
    std::cout << "  json (scene with multiple trajectories): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "scene.json";

    std::ofstream(path) << R"({
        "id": "scene1",
        "location": "test",
        "trajectories": [
            {"id": "ego", "type": "ego", "points": [
                {"x": 0, "y": 0, "t": 0.0, "v": 10.0}
            ]},
            {"id": "lead", "type": "vehicle", "points": [
                {"x": 10, "y": 0, "t": 0.0, "v": 8.0}
            ]}
        ]
    })";

    JsonTrajectoryLoader loader;
    core::Scene scene;
    auto result = loader.loadScene(path, scene);

    assert(result.success == true);
    assert(scene.trajectories.size() == 2);
    std::cout << "PASS\n";
}

void test_json_load_from_string() {
    std::cout << "  json (from string): ";
    JsonTrajectoryLoader loader;
    std::vector<core::Trajectory> trajs;

    std::string content = R"([
        {"id": "t1", "type": "ego", "points": [
            {"x": 0, "y": 0, "t": 0.0, "v": 5.0},
            {"x": 1, "y": 0, "t": 1.0, "v": 5.0}
        ]}
    ])";

    auto result = loader.loadFromString(content, trajs);
    assert(result.success == true);
    assert(trajs.size() == 1);
    assert(trajs[0].points.size() == 2);
    std::cout << "PASS\n";
}

// ============================================================================
// CSV Loader Tests
// ============================================================================

void test_csv_load_valid() {
    std::cout << "  csv (valid file): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "valid.csv";

    std::ofstream(path) << "id,x,y,z,t,heading,velocity,acceleration\n"
                           "traj1,0.0,0.0,0.0,0.0,0.0,10.0,0.0\n"
                           "traj1,1.0,0.0,0.0,0.1,0.0,10.0,0.0\n";

    CsvTrajectoryLoader loader;
    std::vector<core::Trajectory> trajs;
    auto result = loader.loadTrajectories(path, trajs);

    assert(result.success == true);
    assert(!trajs.empty());
    assert(trajs[0].points.size() == 2);
    assert(trajs[0].points[0].x == 0.0);
    std::cout << "PASS\n";
}

void test_csv_load_no_header() {
    std::cout << "  csv (no header, custom delimiter): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "nohdr.csv";

    std::ofstream(path) << "traj1|0.0|0.0|0.0|0.0|0.0|10.0|0.0\n";

    CsvTrajectoryLoader loader;
    loader.setOption("delimiter", "|");
    loader.setOption("has_header", "false");

    std::vector<core::Trajectory> trajs;
    auto result = loader.loadTrajectories(path, trajs);

    // Should parse without header
    assert(result.success == true);
    std::cout << "PASS\n";
}

void test_csv_load_malformed() {
    std::cout << "  csv (malformed data): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "malformed.csv";

    // Wrong number of columns
    std::ofstream(path) << "id,x,y,z,t,heading,velocity,acceleration\n"
                           "traj1,0.0,not_a_number,0.0,0.0,0.0,10.0,0.0\n";

    CsvTrajectoryLoader loader;
    std::vector<core::Trajectory> trajs;
    auto result = loader.loadTrajectories(path, trajs);

    // Should handle gracefully
    std::cout << "PASS (handled gracefully)\n";
}

void test_csv_load_empty_file() {
    std::cout << "  csv (empty file): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "empty.csv";

    std::ofstream(path) << "";

    CsvTrajectoryLoader loader;
    std::vector<core::Trajectory> trajs;
    auto result = loader.loadTrajectories(path, trajs);

    assert(result.success == true);
    assert(trajs.empty());
    std::cout << "PASS\n";
}

void test_csv_loader_get_supported_extensions() {
    std::cout << "  csv (getSupportedExtensions): ";
    CsvTrajectoryLoader loader;
    auto exts = loader.getSupportedExtensions();
    assert(!exts.empty());
    assert(std::find(exts.begin(), exts.end(), ".csv") != exts.end());
    std::cout << "PASS\n";
}

// ============================================================================
// YAML Config Loader Tests
// ============================================================================

void test_yaml_load_config() {
    std::cout << "  yaml (load config): ";
    auto dir = temp_dir();
    auto path = fs::path(dir) / "config.yaml";

    std::ofstream(path) << R"(
name: test_config
description: Test evaluation
trajectory_file: test.json
output_dir: ./results
metrics:
  - collision
  - ttc
  - following
thresholds:
  - metric_name: collision
    max: 0.0
    comparison: max
  - metric_name: ttc
    min: 3.0
    comparison: min
simulation:
  dt: 0.1
  duration: 10.0
  enable_collision_check: true
report:
  formats:
    - json
    - html
  include_charts: true
)";

    core::EvaluationConfig config;
    bool ok = YamlConfigLoader::loadConfig(path, config);

    assert(ok == true);
    assert(config.name == "test_config");
    assert(config.metrics.size() == 3);
    assert(config.thresholds.size() == 2);
    assert(config.thresholds[0].metric_name == "collision");
    assert(config.thresholds[0].max == 0.0);
    assert(config.simulation.dt == 0.1);
    assert(config.report.formats.size() == 2);

    std::cout << "PASS\n";
}

void test_yaml_save_and_load_roundtrip() {
    std::cout << "  yaml (roundtrip save/load): ";
    core::EvaluationConfig config;
    config.name = "roundtrip_test";
    config.metrics = {"collision", "jerk"};
    config.thresholds.push_back(core::MetricThreshold{{"collision"}, 0, 1.0, "range", true});

    auto dir = temp_dir();
    auto path = fs::path(dir) / "roundtrip.yaml";

    bool saved = YamlConfigLoader::saveConfig(path, config);
    assert(saved == true);

    core::EvaluationConfig loaded;
    bool loaded_ok = YamlConfigLoader::loadConfig(path, loaded);
    assert(loaded_ok == true);
    assert(loaded.name == config.name);
    assert(loaded.metrics == config.metrics);

    std::cout << "PASS\n";
}

// ============================================================================
// Loader Registry Tests
// ============================================================================

void test_loader_registry() {
    std::cout << "  loader registry: ";
    auto& registry = LoaderRegistry::instance();

    auto loader = registry.getLoader("json");
    assert(loader != nullptr);
    assert(loader->getName() == "json");

    auto csv = registry.getLoader("csv");
    assert(csv != nullptr);

    auto all = registry.getAllLoaders();
    assert(all.size() >= 2);

    std::cout << "PASS\n";
}

void test_loader_registry_autodetect() {
    std::cout << "  loader registry (auto-detect): ";
    auto& registry = LoaderRegistry::instance();

    auto json_loader = registry.getLoaderForPath("/path/to/data.json");
    assert(json_loader != nullptr);
    assert(json_loader->getName() == "json");

    auto csv_loader = registry.getLoaderForPath("/path/to/data.csv");
    assert(csv_loader != nullptr);
    assert(csv_loader->getName() == "csv");

    std::cout << "PASS\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\nRunning Data Loader Tests\n";
    std::cout << "=========================\n\n";

    autoeval::initialize();

    try {
        std::cout << "[JsonTrajectoryLoader]\n";
        test_json_load_valid();
        test_json_load_invalid_syntax();
        test_json_load_missing_fields();
        test_json_load_empty_points();
        test_json_load_non_existent_file();
        test_json_load_scene();
        test_json_load_from_string();

        std::cout << "[CsvTrajectoryLoader]\n";
        test_csv_load_valid();
        test_csv_load_no_header();
        test_csv_load_malformed();
        test_csv_load_empty_file();
        test_csv_loader_get_supported_extensions();

        std::cout << "[YamlConfigLoader]\n";
        test_yaml_load_config();
        test_yaml_save_and_load_roundtrip();

        std::cout << "[LoaderRegistry]\n";
        test_loader_registry();
        test_loader_registry_autodetect();

        std::cout << "\n=========================\n";
        std::cout << "All loader tests PASSED!\n\n";

        autoeval::shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
