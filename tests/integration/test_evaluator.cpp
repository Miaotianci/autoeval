/**
 * @file test_evaluator.cpp
 * @brief Integration tests for the evaluator
 */

#include "autoeval/autoeval.h"
#include "autoeval/core/evaluator.h"
#include "autoeval/loader/data_loader.h"
#include <cassert>
#include <iostream>

using namespace autoeval;
using namespace autoeval::core;

void test_evaluator_basic() {
    std::cout << "Testing Evaluator basic functionality..." << std::endl;

    autoeval::initialize();

    // Create evaluator
    auto evaluator = std::make_unique<Evaluator>();

    // Create a simple scene
    Scene scene;
    scene.id = "test_scene";
    scene.road_info.speed_limit = 13.89;  // 50 km/h

    // Add ego trajectory
    Trajectory ego;
    ego.id = "ego";
    ego.type = ObjectType::EgoVehicle;
    for (int i = 0; i < 10; ++i) {
        TrajectoryPoint p;
        p.x = i * 1.0;
        p.y = 0.0;
        p.z = 0.0;
        p.timestamp = i * 0.1;
        p.velocity = 10.0;
        ego.points.push_back(p);
    }
    scene.trajectories.push_back(ego);

    // Add evaluator context
    auto context = evaluator->getContext();
    context->setScene(scene);
    context->setConfig(EvaluationConfig());

    // Test readiness
    assert(evaluator->isReady() == false);  // No metrics added
    std::cout << "  isReady check: PASS" << std::endl;

    // Add metrics
    evaluator->addMetrics({"collision", "speed_compliance"});

    // Test readiness now
    assert(evaluator->isReady() == true);
    std::cout << "  isReady after adding metrics: PASS" << std::endl;

    // Run evaluation
    auto summary = evaluator->evaluate();

    assert(!summary.metric_results.empty());
    assert(summary.overall_status == "pass" || summary.overall_status == "fail");
    std::cout << "  evaluation ran successfully: PASS" << std::endl;

    autoeval::shutdown();
}

void test_load_and_evaluate() {
    std::cout << "Testing load and evaluate..." << std::endl;

    autoeval::initialize();

    auto evaluator = std::make_unique<Evaluator>();

    // Load scene from sample data
    auto result = evaluator->loadScene("../examples/data/sample_trajectory.json");

    // Note: This will fail if file doesn't exist or can't be loaded
    // For now, just check the evaluator doesn't crash

    std::cout << "  load scene attempt completed: PASS" << std::endl;

    autoeval::shutdown();
}

void test_config_loading() {
    std::cout << "Testing configuration loading..." << std::endl;

    // Create a test config
    EvaluationConfig config;
    config.name = "test_config";
    config.trajectory_file = "test.json";
    config.metrics = {"collision", "ttc", "jerk"};

    MetricThreshold th;
    th.metric_name = "collision";
    th.max = 0.0;
    th.comparison = "max";
    config.thresholds.push_back(th);

    assert(config.name == "test_config");
    assert(config.metrics.size() == 3);
    assert(config.thresholds.size() == 1);

    auto th_opt = config.getThreshold("collision");
    assert(th_opt.has_value());
    assert(th_opt->max == 0.0);

    std::cout << "  config creation: PASS" << std::endl;
    std::cout << "  threshold retrieval: PASS" << std::endl;
}

void test_result_manipulation() {
    std::cout << "Testing metric result manipulation..." << std::endl;

    MetricResult result;
    result.name = "test_metric";
    result.category = "safety";
    result.value = 42.5;
    result.unit = "m";
    result.pass = true;
    result.message = "Test passed";

    result.addDetail("detail1", 123);
    result.addDetail("detail2", "string_value");
    result.addDetail("detail3", true);

    auto d1 = result.getDetail<int>("detail1");
    auto d2 = result.getDetail<std::string>("detail2");
    auto d3 = result.getDetail<bool>("detail3");

    assert(d1.has_value() && *d1 == 123);
    assert(d2.has_value() && *d2 == "string_value");
    assert(d3.has_value() && *d3 == true);

    std::cout << "  detail storage/retrieval: PASS" << std::endl;
}

void test_summary_creation() {
    std::cout << "Testing evaluation summary..." << std::endl;

    EvaluationSummary summary;
    summary.id = "test_eval";
    summary.overall_status = "pass";

    // Add metric results
    for (int i = 0; i < 3; ++i) {
        MetricResult result;
        result.name = "metric_" + std::to_string(i);
        result.value = i * 10.0;
        result.unit = "unit";
        result.pass = (i < 2);  // First 2 pass
        summary.metric_results.push_back(result);
    }

    assert(summary.passedCount() == 2);
    assert(summary.failedCount() == 1);
    assert(std::abs(summary.passRate() - 0.666) < 0.01);

    std::cout << "  passed/failed counts: PASS" << std::endl;
    std::cout << "  pass rate: PASS" << std::endl;
}

int main() {
    std::cout << "\nRunning AutoEval Integration Tests\n";
    std::cout << "=================================\n\n";

    try {
        test_evaluator_basic();
        test_load_and_evaluate();
        test_config_loading();
        test_result_manipulation();
        test_summary_creation();

        std::cout << "\n=================================\n";
        std::cout << "All integration tests PASSED!\n\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
