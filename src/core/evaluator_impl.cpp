/**
 * @file evaluator_impl.cpp
 * @brief Evaluator implementation
 */

#include "autoeval/core/evaluator.h"
#include "autoeval/report/report_generator.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>

namespace autoeval {
namespace core {

// ============================================================================
// Evaluator Implementation (PIMPL)
// ============================================================================

struct Evaluator::Impl {
    std::shared_ptr<EvaluationContext> context;
    EvaluationConfig config;
    std::vector<std::string> metrics_to_evaluate;
    ProgressCallback progress_callback;

    Impl() : context(std::make_shared<EvaluationContext>()) {
        metrics::initializeBuiltInMetrics();
    }

    void reportProgress(size_t current, size_t total, const std::string& phase,
                       const std::string& detail = "") {
        if (progress_callback) {
            EvaluationProgress progress;
            progress.current_step = current;
            progress.total_steps = total;
            progress.current_phase = phase;
            progress.current_metric = detail;
            progress.progress = (total > 0) ? (static_cast<double>(current) / total) : 0.0;
            progress_callback(progress);
        }
    }

    std::optional<MetricThreshold> getEffectiveThreshold(const std::string& metric_name) {
        // Check custom thresholds first
        for (const auto& th : config.thresholds) {
            if (th.metric_name == metric_name) {
                return th;
            }
        }

        // Get default threshold from metric metadata
        auto metric = metrics::MetricRegistry::instance().getMetric(metric_name);
        if (metric) {
            auto meta = metric->getMetadata();
            MetricThreshold th;
            th.metric_name = metric_name;
            th.comparison = meta.default_comparison;
            th.min = meta.default_min;
            th.max = meta.default_max;
            return th;
        }

        return std::nullopt;
    }
};

// ============================================================================
// Evaluator
// ============================================================================

Evaluator::Evaluator()
    : pImpl(std::make_unique<Impl>())
{
    spdlog::info("Evaluator created");
}

Evaluator::~Evaluator() {
    spdlog::info("Evaluator destroyed");
}

// ------------------------------------------------------------------------
// Configuration
// ------------------------------------------------------------------------

bool Evaluator::loadConfig(const std::filesystem::path& config_path) {
    if (!loader::loadConfig(config_path, pImpl->config)) {
        spdlog::error("Failed to load configuration from: {}", config_path.string());
        return false;
    }

    pImpl->context->setConfig(pImpl->config);

    // Load metrics from config
    if (!pImpl->config.metrics.empty()) {
        addMetrics(pImpl->config.metrics);
    }

    spdlog::info("Configuration loaded: {}", pImpl->config.name);
    return true;
}

void Evaluator::setConfig(const EvaluationConfig& config) {
    pImpl->config = config;
    pImpl->context->setConfig(config);
}

const EvaluationConfig& Evaluator::getConfig() const {
    return pImpl->config;
}

EvaluationConfig& Evaluator::getConfig() {
    return pImpl->config;
}

// ------------------------------------------------------------------------
// Data Loading
// ------------------------------------------------------------------------

bool Evaluator::loadTrajectory(const std::filesystem::path& path) {
    Trajectory traj;
    auto result = loader::loadTrajectory(path, traj);

    if (!result.success) {
        spdlog::error("Failed to load trajectory: {}", result.message);
        return false;
    }

    addTrajectory(traj);
    return true;
}

bool Evaluator::loadScene(const std::filesystem::path& path) {
    Scene scene;
    auto result = loader::loadScene(path, scene);

    if (!result.success) {
        spdlog::error("Failed to load scene: {}", result.message);
        return false;
    }

    pImpl->context->setScene(scene);
    pImpl->context->getSummary().data_source = path.string();

    spdlog::info("Scene loaded: {} trajectories", scene.trajectories.size());
    return true;
}

bool Evaluator::loadSceneFromConfig() {
    std::filesystem::path data_path;

    if (!pImpl->config.scene_file.empty()) {
        data_path = pImpl->config.scene_file;
    } else if (!pImpl->config.trajectory_file.empty()) {
        data_path = pImpl->config.trajectory_file;
    } else {
        return false;
    }

    return loadScene(data_path);
}

void Evaluator::addTrajectory(const Trajectory& traj) {
    pImpl->context->addTrajectory(traj);
}

// ------------------------------------------------------------------------
// Metrics
// ------------------------------------------------------------------------

void Evaluator::addMetric(const std::string& metric_name) {
    pImpl->metrics_to_evaluate.push_back(metric_name);
}

void Evaluator::addMetrics(const std::vector<std::string>& metric_names) {
    for (const auto& name : metric_names) {
        addMetric(name);
    }
}

void Evaluator::setMetricThreshold(const std::string& metric_name, const MetricThreshold& threshold) {
    // Remove existing threshold for this metric
    auto& thresholds = pImpl->config.thresholds;
    thresholds.erase(std::remove_if(thresholds.begin(), thresholds.end(),
                                    [&metric_name](const auto& th) {
                                        return th.metric_name == metric_name;
                                    }), thresholds.end());

    // Add new threshold
    thresholds.push_back(threshold);
}

void Evaluator::clearMetrics() {
    pImpl->metrics_to_evaluate.clear();
}

const std::vector<std::string>& Evaluator::getMetrics() const {
    return pImpl->metrics_to_evaluate;
}

// ------------------------------------------------------------------------
// Evaluation
// ------------------------------------------------------------------------

EvaluationSummary Evaluator::evaluate() {
    return evaluate(nullptr);
}

EvaluationSummary Evaluator::evaluate(ProgressCallback progress_callback) {
    pImpl->progress_callback = progress_callback;

    auto& summary = pImpl->context->getSummary();
    auto& scene = pImpl->context->getScene();

    spdlog::info("Starting evaluation: {}", summary.id);

    // Load scene from config if not already loaded
    if (!pImpl->context->hasScene()) {
        if (!loadSceneFromConfig()) {
            spdlog::error("No scene loaded and no data source in config");
            summary.overall_status = "fail";
            summary.end_time = std::chrono::system_clock::now();
            pImpl->context->updateSummary();
            return summary;
        }
    }

    // Determine metrics to evaluate
    std::vector<std::string> metrics;
    if (pImpl->metrics_to_evaluate.empty() && !pImpl->config.metrics.empty()) {
        metrics = pImpl->config.metrics;
    } else {
        metrics = pImpl->metrics_to_evaluate;
    }

    if (metrics.empty()) {
        spdlog::warn("No metrics specified, using all available metrics");
        metrics = metrics::getBuiltInMetricNames();
    }

    size_t total_steps = metrics.size();
    pImpl->reportProgress(0, total_steps, "Initializing");

    // Evaluate each metric
    for (size_t i = 0; i < metrics.size(); ++i) {
        const auto& metric_name = metrics[i];
        pImpl->reportProgress(i, total_steps, "Evaluating", metric_name);

        spdlog::debug("Evaluating metric: {}", metric_name);

        auto result_opt = metrics::computeMetric(metric_name, scene, pImpl->context.get());
        if (result_opt) {
            // Apply threshold
            auto threshold = pImpl->getEffectiveThreshold(metric_name);
            if (threshold) {
                result_opt->pass = threshold->check(result_opt->value);
                result_opt->threshold_type = threshold->comparison;
                result_opt->threshold = (threshold->comparison == "max")
                                      ? threshold->max
                                      : (threshold->comparison == "min")
                                      ? threshold->min
                                      : threshold->max;
            }

            pImpl->context->addMetricResult(*result_opt);
        } else {
            spdlog::warn("Failed to evaluate metric: {}", metric_name);
        }
    }

    pImpl->reportProgress(total_steps, total_steps, "Complete");

    // Update summary
    pImpl->context->updateSummary();

    spdlog::info("Evaluation complete: {}/{} metrics passed, status: {}",
                summary.passedCount(), summary.metric_results.size(),
                summary.overall_status);

    return summary;
}

std::optional<MetricResult> Evaluator::evaluateMetric(const std::string& metric_name) {
    if (!pImpl->context->hasScene()) {
        if (!loadSceneFromConfig()) {
            spdlog::error("No scene loaded");
            return std::nullopt;
        }
    }

    auto result_opt = metrics::computeMetric(metric_name, pImpl->context->getScene(),
                                            pImpl->context.get());
    if (result_opt) {
        auto threshold = pImpl->getEffectiveThreshold(metric_name);
        if (threshold) {
            result_opt->pass = threshold->check(result_opt->value);
        }
        pImpl->context->addMetricResult(*result_opt);
    }

    return result_opt;
}

bool Evaluator::isReady() const {
    return pImpl->context->isReady() && !pImpl->metrics_to_evaluate.empty();
}

std::string Evaluator::getValidationError() const {
    std::string error;

    if (!pImpl->context->hasScene()) {
        error = "No scene loaded";
        if (!pImpl->config.scene_file.empty() && !pImpl->config.trajectory_file.empty()) {
            error += " (check configuration)";
        }
        return error;
    }

    if (!pImpl->context->isReady()) {
        error = "Scene has no trajectories";
        return error;
    }

    if (pImpl->metrics_to_evaluate.empty() && pImpl->config.metrics.empty()) {
        error = "No metrics specified";
        return error;
    }

    return "";
}

// ------------------------------------------------------------------------
// Results
// ------------------------------------------------------------------------

const std::vector<MetricResult>& Evaluator::getResults() const {
    return pImpl->context->getMetricResults();
}

const EvaluationSummary& Evaluator::getSummary() const {
    return pImpl->context->getSummary();
}

std::optional<MetricResult> Evaluator::getResult(const std::string& metric_name) const {
    return pImpl->context->getMetricResult(metric_name);
}

// ------------------------------------------------------------------------
// Reporting
// ------------------------------------------------------------------------

bool Evaluator::generateReport(const std::filesystem::path& output_path) {
    auto ext = output_path.extension().string();

    if (ext == ".json") {
        return generateReport(output_path, "json");
    } else if (ext == ".html") {
        return generateReport(output_path, "html");
    } else if (ext == ".pdf") {
        return generateReport(output_path, "pdf");
    } else {
        spdlog::error("Unknown report format: {}", ext);
        return false;
    }
}

bool Evaluator::generateReport(const std::filesystem::path& output_path,
                               const std::string& format) {
    auto generator = report::ReportGenerator::create(format);
    if (!generator) {
        spdlog::error("Failed to create report generator for format: {}", format);
        return false;
    }

    return generator->generate(pImpl->context->getSummary(), output_path);
}

std::vector<bool> Evaluator::generateReports(const std::filesystem::path& output_dir) {
    std::vector<bool> results;

    for (const auto& format : pImpl->config.report.formats) {
        auto output_path = output_dir / ("report." + format);
        results.push_back(generateReport(output_path, format));
    }

    return results;
}

// ------------------------------------------------------------------------
// Progress Callback
// ------------------------------------------------------------------------

void Evaluator::setProgressCallback(ProgressCallback callback) {
    pImpl->progress_callback = callback;
}

// ------------------------------------------------------------------------
// Context Access
// ------------------------------------------------------------------------

std::shared_ptr<EvaluationContext> Evaluator::getContext() const {
    return pImpl->context;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::unique_ptr<Evaluator> createEvaluator(const std::filesystem::path& config_path) {
    auto evaluator = std::make_unique<Evaluator>();
    if (!evaluator->loadConfig(config_path)) {
        return nullptr;
    }
    return evaluator;
}

EvaluationSummary quickEvaluate(const std::filesystem::path& data_path,
                                const std::vector<std::string>& metrics) {
    auto evaluator = std::make_unique<Evaluator>();
    evaluator->loadScene(data_path);
    evaluator->addMetrics(metrics);
    return evaluator->evaluate();
}

EvaluationSummary evaluateAndReport(const std::filesystem::path& config_path,
                                    const std::filesystem::path& output_dir) {
    auto evaluator = createEvaluator(config_path);
    if (!evaluator) {
        EvaluationSummary summary;
        summary.overall_status = "fail";
        return summary;
    }

    auto summary = evaluator->evaluate();

    if (!output_dir.empty()) {
        std::filesystem::create_directories(output_dir);
        evaluator->generateReports(output_dir);
    }

    return summary;
}

} // namespace core
} // namespace autoeval
