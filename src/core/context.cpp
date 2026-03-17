/**
 * @file context.cpp
 * @brief Evaluation context implementation
 */

#include "autoeval/core/context.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <sstream>

namespace autoeval {
namespace core {

// ============================================================================
// Context Implementation (PIMPL)
// ============================================================================

struct EvaluationContext::Impl {
    // Data
    std::optional<Scene> scene;
    EvaluationConfig config;

    // Results
    std::vector<MetricResult> metric_results;
    EvaluationSummary summary;

    // Logging
    std::shared_ptr<spdlog::logger> logger;

    // Metadata
    std::unordered_map<std::string, std::variant<double, int, std::string, bool>> metadata;

    Impl() {
        // Initialize logger
        logger = spdlog::stdout_color_mt("autoeval");
        logger->set_level(spdlog::level::info);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

        // Initialize summary
        summary.id = generateId();
        summary.start_time = std::chrono::system_clock::now();
        summary.overall_status = "pending";
    }

    std::string generateId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return "eval_" + std::to_string(timestamp);
    }

    void validate() {
        if (!scene.has_value()) {
            throw std::runtime_error("No scene loaded");
        }
    }
};

// ============================================================================
// EvaluationContext
// ============================================================================

EvaluationContext::EvaluationContext()
    : pImpl(std::make_unique<Impl>())
{
    pImpl->logger->info("EvaluationContext created: {}", pImpl->summary.id);
}

EvaluationContext::~EvaluationContext() {
    if (pImpl) {
        pImpl->logger->info("EvaluationContext destroyed: {}", pImpl->summary.id);
    }
}

// ------------------------------------------------------------------------
// Data Management
// ------------------------------------------------------------------------

void EvaluationContext::setScene(Scene scene) {
    pImpl->scene = std::move(scene);
    pImpl->logger->info("Scene set: {} trajectories",
                       pImpl->scene->trajectories.size());
}

const Scene& EvaluationContext::getScene() const {
    if (!pImpl->scene.has_value()) {
        throw std::runtime_error("No scene loaded");
    }
    return *pImpl->scene;
}

Scene& EvaluationContext::getScene() {
    if (!pImpl->scene.has_value()) {
        throw std::runtime_error("No scene loaded");
    }
    return *pImpl->scene;
}

bool EvaluationContext::hasScene() const {
    return pImpl->scene.has_value();
}

void EvaluationContext::addTrajectory(const Trajectory& traj) {
    if (!pImpl->scene.has_value()) {
        pImpl->scene = Scene{};
    }
    pImpl->scene->trajectories.push_back(traj);
    pImpl->logger->debug("Added trajectory: {}", traj.id);
}

std::optional<Trajectory> EvaluationContext::getTrajectory(const std::string& id) const {
    if (!pImpl->scene.has_value()) {
        return std::nullopt;
    }
    return pImpl->scene->getTrajectoryById(id);
}

// ------------------------------------------------------------------------
// Configuration Management
// ------------------------------------------------------------------------

void EvaluationContext::setConfig(const EvaluationConfig& config) {
    pImpl->config = config;
    pImpl->summary.config_file = config.name;
    pImpl->logger->info("Configuration set: {}", config.name);
}

const EvaluationConfig& EvaluationContext::getConfig() const {
    return pImpl->config;
}

template<>
std::optional<double> EvaluationContext::getConfigValue(const std::string& key) const {
    // This is a placeholder for future extensibility
    return std::nullopt;
}

template<>
std::optional<std::string> EvaluationContext::getConfigValue(const std::string& key) const {
    return std::nullopt;
}

template<>
std::optional<int> EvaluationContext::getConfigValue(const std::string& key) const {
    return std::nullopt;
}

template<>
std::optional<bool> EvaluationContext::getConfigValue(const std::string& key) const {
    return std::nullopt;
}

template<>
void EvaluationContext::setConfigValue(const std::string& key, const double& value) {
    // Placeholder
}

template<>
void EvaluationContext::setConfigValue(const std::string& key, const std::string& value) {
    // Placeholder
}

template<>
void EvaluationContext::setConfigValue(const std::string& key, const int& value) {
    // Placeholder
}

template<>
void EvaluationContext::setConfigValue(const std::string& key, const bool& value) {
    // Placeholder
}

// ------------------------------------------------------------------------
// Results Management
// ------------------------------------------------------------------------

void EvaluationContext::addMetricResult(const MetricResult& result) {
    pImpl->metric_results.push_back(result);
    pImpl->logger->debug("Added metric result: {} = {} {}",
                        result.name, result.value, result.unit);
}

const std::vector<MetricResult>& EvaluationContext::getMetricResults() const {
    return pImpl->metric_results;
}

std::optional<MetricResult> EvaluationContext::getMetricResult(const std::string& name) const {
    for (const auto& result : pImpl->metric_results) {
        if (result.name == name) {
            return result;
        }
    }
    return std::nullopt;
}

const EvaluationSummary& EvaluationContext::getSummary() const {
    return pImpl->summary;
}

EvaluationSummary& EvaluationContext::getSummary() {
    return pImpl->summary;
}

void EvaluationContext::updateSummary() {
    pImpl->summary.metric_results = pImpl->metric_results;
    pImpl->summary.end_time = std::chrono::system_clock::now();

    // Determine overall status
    size_t passed = pImpl->summary.passedCount();
    size_t total = pImpl->metric_results.size();

    if (total == 0) {
        pImpl->summary.overall_status = "pending";
    } else if (passed == total) {
        pImpl->summary.overall_status = "pass";
    } else if (passed == 0) {
        pImpl->summary.overall_status = "fail";
    } else {
        pImpl->summary.overall_status = "partial";
    }

    pImpl->logger->info("Summary updated: {}/{} metrics passed, status: {}",
                       passed, total, pImpl->summary.overall_status);
}

// ------------------------------------------------------------------------
// Logging
// ------------------------------------------------------------------------

std::shared_ptr<spdlog::logger> EvaluationContext::getLogger() const {
    return pImpl->logger;
}

void EvaluationContext::setLogLevel(spdlog::level::level_enum level) {
    pImpl->logger->set_level(level);
    pImpl->logger->info("Log level set to: {}",
                       spdlog::level::to_string_view(level));
}

// ------------------------------------------------------------------------
// State Management
// ------------------------------------------------------------------------

void EvaluationContext::reset() {
    pImpl->scene.reset();
    pImpl->metric_results.clear();
    pImpl->summary.id = pImpl->generateId();
    pImpl->summary.metric_results.clear();
    pImpl->summary.start_time = std::chrono::system_clock::now();
    pImpl->summary.overall_status = "pending";
    pImpl->logger->info("Context reset: {}", pImpl->summary.id);
}

bool EvaluationContext::isReady() const {
    return hasScene() && !pImpl->scene->trajectories.empty();
}

std::string EvaluationContext::getValidationError() const {
    if (!hasScene()) {
        return "No scene loaded";
    }
    if (pImpl->scene->trajectories.empty()) {
        return "Scene has no trajectories";
    }
    return "";
}

// ------------------------------------------------------------------------
// Metadata
// ------------------------------------------------------------------------

template<>
void EvaluationContext::setMetadata(const std::string& key, const double& value) {
    pImpl->metadata[key] = value;
}

template<>
void EvaluationContext::setMetadata(const std::string& key, const std::string& value) {
    pImpl->metadata[key] = value;
}

template<>
void EvaluationContext::setMetadata(const std::string& key, const int& value) {
    pImpl->metadata[key] = value;
}

template<>
void EvaluationContext::setMetadata(const std::string& key, const bool& value) {
    pImpl->metadata[key] = value;
}

template<>
std::optional<double> EvaluationContext::getMetadata(const std::string& key) const {
    auto it = pImpl->metadata.find(key);
    if (it != pImpl->metadata.end() && std::holds_alternative<double>(it->second)) {
        return std::get<double>(it->second);
    }
    return std::nullopt;
}

template<>
std::optional<std::string> EvaluationContext::getMetadata(const std::string& key) const {
    auto it = pImpl->metadata.find(key);
    if (it != pImpl->metadata.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return std::nullopt;
}

template<>
std::optional<int> EvaluationContext::getMetadata(const std::string& key) const {
    auto it = pImpl->metadata.find(key);
    if (it != pImpl->metadata.end() && std::holds_alternative<int>(it->second)) {
        return std::get<int>(it->second);
    }
    return std::nullopt;
}

template<>
std::optional<bool> EvaluationContext::getMetadata(const std::string& key) const {
    auto it = pImpl->metadata.find(key);
    if (it != pImpl->metadata.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
    }
    return std::nullopt;
}

std::vector<std::string> EvaluationContext::getMetadataKeys() const {
    std::vector<std::string> keys;
    for (const auto& [key, _] : pImpl->metadata) {
        keys.push_back(key);
    }
    return keys;
}

// ============================================================================
// ContextBuilder
// ============================================================================

ContextBuilder::ContextBuilder()
    : log_level_(spdlog::level::info)
{
}

ContextBuilder& ContextBuilder::withConfig(const EvaluationConfig& config) {
    config_ = config;
    return *this;
}

ContextBuilder& ContextBuilder::withScene(const Scene& scene) {
    scene_ = scene;
    return *this;
}

ContextBuilder& ContextBuilder::withTrajectory(const Trajectory& traj) {
    trajectories_.push_back(traj);
    return *this;
}

ContextBuilder& ContextBuilder::withLogLevel(spdlog::level::level_enum level) {
    log_level_ = level;
    return *this;
}

std::unique_ptr<EvaluationContext> ContextBuilder::build() const {
    auto context = std::make_unique<EvaluationContext>();

    // Set log level first
    context->setLogLevel(log_level_);

    // Set configuration
    if (!config_.name.empty()) {
        context->setConfig(config_);
    }

    // Set scene
    if (scene_.has_value()) {
        context->setScene(*scene_);
    } else if (!trajectories_.empty()) {
        // Create scene from trajectories
        Scene scene;
        scene.id = "scene_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
        scene.trajectories = trajectories_;
        context->setScene(scene);
    }

    // Set metadata
    for (const auto& [key, value] : metadata_) {
        if (std::holds_alternative<double>(value)) {
            context->setMetadata(key, std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            context->setMetadata(key, std::get<std::string>(value));
        } else if (std::holds_alternative<int>(value)) {
            context->setMetadata(key, std::get<int>(value));
        } else if (std::holds_alternative<bool>(value)) {
            context->setMetadata(key, std::get<bool>(value));
        }
    }

    return context;
}

} // namespace core
} // namespace autoeval
