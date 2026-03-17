/**
 * @file context.h
 * @brief Evaluation context for managing evaluation state
 */

#pragma once

#include "autoeval/core/types.h"
#include <memory>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace autoeval {
namespace core {

// Forward declarations
class IMetric;
class IDataLoader;

/**
 * @brief Evaluation context that holds all evaluation state
 *
 * The context provides:
 * - Access to loaded data (scenes, trajectories)
 * - Configuration management
 * - Logger access
 * - Plugin system integration
 */
class EvaluationContext {
public:
    /**
     * @brief Constructor
     */
    explicit EvaluationContext();

    /**
     * @brief Destructor
     */
    ~EvaluationContext();

    // ========================================================================
    // Data Management
    // ========================================================================

    /**
     * @brief Set the current scene
     */
    void setScene(Scene scene);

    /**
     * @brief Get the current scene
     */
    const Scene& getScene() const;

    /**
     * @brief Get the current scene (non-const)
     */
    Scene& getScene();

    /**
     * @brief Check if a scene is loaded
     */
    bool hasScene() const;

    /**
     * @brief Add a trajectory to the scene
     */
    void addTrajectory(const Trajectory& traj);

    /**
     * @brief Get trajectory by ID
     */
    std::optional<Trajectory> getTrajectory(const std::string& id) const;

    // ========================================================================
    // Configuration Management
    // ========================================================================

    /**
     * @brief Set the evaluation configuration
     */
    void setConfig(const EvaluationConfig& config);

    /**
     * @brief Get the evaluation configuration
     */
    const EvaluationConfig& getConfig() const;

    /**
     * @brief Get a configuration value by key (future extensibility)
     */
    template<typename T>
    std::optional<T> getConfigValue(const std::string& key) const;

    /**
     * @brief Set a configuration value by key
     */
    template<typename T>
    void setConfigValue(const std::string& key, const T& value);

    // ========================================================================
    // Results Management
    // ========================================================================

    /**
     * @brief Add a metric result
     */
    void addMetricResult(const MetricResult& result);

    /**
     * @brief Get all metric results
     */
    const std::vector<MetricResult>& getMetricResults() const;

    /**
     * @brief Get metric result by name
     */
    std::optional<MetricResult> getMetricResult(const std::string& name) const;

    /**
     * @brief Get the evaluation summary (const version)
     */
    const EvaluationSummary& getSummary() const;

    /**
     * @brief Get the evaluation summary (non-const version)
     */
    EvaluationSummary& getSummary();

    /**
     * @brief Update the summary based on current results
     */
    void updateSummary();

    // ========================================================================
    // Logging
    // ========================================================================

    /**
     * @brief Get the logger
     */
    std::shared_ptr<spdlog::logger> getLogger() const;

    /**
     * @brief Set logger verbosity level
     */
    void setLogLevel(spdlog::level::level_enum level);

    // ========================================================================
    // State Management
    // ========================================================================

    /**
     * @brief Reset the context to initial state
     */
    void reset();

    /**
     * @brief Check if the context is ready for evaluation
     */
    bool isReady() const;

    /**
     * @brief Get a validation error message if not ready
     */
    std::string getValidationError() const;

    // ========================================================================
    // Metadata
    // ========================================================================

    /**
     * @brief Set a metadata value
     */
    template<typename T>
    void setMetadata(const std::string& key, const T& value);

    /**
     * @brief Get a metadata value
     */
    template<typename T>
    std::optional<T> getMetadata(const std::string& key) const;

    /**
     * @brief Get all metadata keys
     */
    std::vector<std::string> getMetadataKeys() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// Context Builder (Builder Pattern)
// ============================================================================

/**
 * @brief Builder for creating EvaluationContext instances
 */
class ContextBuilder {
public:
    ContextBuilder();

    /**
     * @brief Set configuration
     */
    ContextBuilder& withConfig(const EvaluationConfig& config);

    /**
     * @brief Set scene
     */
    ContextBuilder& withScene(const Scene& scene);

    /**
     * @brief Add trajectory
     */
    ContextBuilder& withTrajectory(const Trajectory& traj);

    /**
     * @brief Set log level
     */
    ContextBuilder& withLogLevel(spdlog::level::level_enum level);

    /**
     * @brief Add metadata
     */
    template<typename T>
    ContextBuilder& withMetadata(const std::string& key, const T& value) {
        metadata_[key] = value;
        return *this;
    }

    /**
     * @brief Build the context
     */
    std::unique_ptr<EvaluationContext> build() const;

private:
    EvaluationConfig config_;
    std::optional<Scene> scene_;
    std::vector<Trajectory> trajectories_;
    spdlog::level::level_enum log_level_;
    std::unordered_map<std::string, std::variant<double, int, std::string, bool>> metadata_;
};

} // namespace core
} // namespace autoeval
