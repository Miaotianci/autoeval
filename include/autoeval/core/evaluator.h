/**
 * @file evaluator.h
 * @brief Main evaluator interface
 */

#pragma once

#include "autoeval/core/types.h"
#include "autoeval/core/context.h"
#include "autoeval/loader/data_loader.h"
#include "autoeval/metrics/base_metric.h"
#include <memory>
#include <functional>
#include <filesystem>

namespace autoeval {
namespace core {

// ============================================================================
// Evaluation Progress
// ============================================================================

/**
 * @brief Progress information during evaluation
 */
struct EvaluationProgress {
    size_t current_step = 0;
    size_t total_steps = 0;
    std::string current_phase;
    std::string current_metric;
    double progress = 0.0;  // 0.0 to 1.0
};

/**
 * @brief Progress callback type
 */
using ProgressCallback = std::function<void(const EvaluationProgress&)>;

// ============================================================================
// Evaluator Interface
// ============================================================================

/**
 * @brief Main evaluator class for running evaluations
 *
 * The evaluator coordinates the entire evaluation process:
 * 1. Load configuration
 * 2. Load data (trajectories/scenes)
 * 3. Initialize metrics
 * 4. Execute evaluation
 * 5. Generate reports
 */
class Evaluator {
public:
    /**
     * @brief Constructor
     */
    Evaluator();

    /**
     * @brief Destructor
     */
    ~Evaluator();

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Load configuration from file
     */
    bool loadConfig(const std::filesystem::path& config_path);

    /**
     * @brief Set configuration directly
     */
    void setConfig(const EvaluationConfig& config);

    /**
     * @brief Get current configuration (const version)
     */
    const EvaluationConfig& getConfig() const;

    /**
     * @brief Get current configuration (non-const version)
     */
    EvaluationConfig& getConfig();

    // ========================================================================
    // Data Loading
    // ========================================================================

    /**
     * @brief Load trajectory data from file
     */
    bool loadTrajectory(const std::filesystem::path& path);

    /**
     * @brief Load scene data from file
     */
    bool loadScene(const std::filesystem::path& path);

    /**
     * @brief Load scene from configuration
     */
    bool loadSceneFromConfig();

    /**
     * @brief Add a trajectory directly
     */
    void addTrajectory(const Trajectory& traj);

    // ========================================================================
    // Metrics
    // ========================================================================

    /**
     * @brief Add a metric to evaluate
     */
    void addMetric(const std::string& metric_name);

    /**
     * @brief Add multiple metrics
     */
    void addMetrics(const std::vector<std::string>& metric_names);

    /**
     * @brief Set custom metric threshold
     */
    void setMetricThreshold(const std::string& metric_name, const MetricThreshold& threshold);

    /**
     * @brief Clear all metrics
     */
    void clearMetrics();

    /**
     * @brief Get list of metrics to evaluate
     */
    const std::vector<std::string>& getMetrics() const;

    // ========================================================================
    // Evaluation
    // ========================================================================

    /**
     * @brief Run the evaluation
     */
    EvaluationSummary evaluate();

    /**
     * @brief Run evaluation with custom progress callback
     */
    EvaluationSummary evaluate(ProgressCallback progress_callback);

    /**
     * @brief Run a single metric
     */
    std::optional<MetricResult> evaluateMetric(const std::string& metric_name);

    /**
     * @brief Check if ready for evaluation
     */
    bool isReady() const;

    /**
     * @brief Get validation error if not ready
     */
    std::string getValidationError() const;

    // ========================================================================
    // Results
    // ========================================================================

    /**
     * @brief Get evaluation results
     */
    const std::vector<MetricResult>& getResults() const;

    /**
     * @brief Get evaluation summary
     */
    const EvaluationSummary& getSummary() const;

    /**
     * @brief Get result by metric name
     */
    std::optional<MetricResult> getResult(const std::string& metric_name) const;

    // ========================================================================
    // Reporting
    // ========================================================================

    /**
     * @brief Generate a report
     */
    bool generateReport(const std::filesystem::path& output_path);

    /**
     * @brief Generate report in specific format
     */
    bool generateReport(const std::filesystem::path& output_path,
                       const std::string& format);

    /**
     * @brief Generate multiple reports
     */
    std::vector<bool> generateReports(const std::filesystem::path& output_dir);

    // ========================================================================
    // Progress Callback
    // ========================================================================

    /**
     * @brief Set progress callback
     */
    void setProgressCallback(ProgressCallback callback);

    // ========================================================================
    // Context Access
    // ========================================================================

    /**
     * @brief Get the evaluation context
     */
    std::shared_ptr<EvaluationContext> getContext() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// Utility Functions
// ========================================================================

/**
 * @brief Create an evaluator from configuration file
 */
std::unique_ptr<Evaluator> createEvaluator(const std::filesystem::path& config_path);

/**
 * @brief Run a quick evaluation
 */
EvaluationSummary quickEvaluate(const std::filesystem::path& data_path,
                                const std::vector<std::string>& metrics);

/**
 * @brief Run evaluation and generate reports
 */
EvaluationSummary evaluateAndReport(const std::filesystem::path& config_path,
                                    const std::filesystem::path& output_dir);

} // namespace core
} // namespace autoeval
