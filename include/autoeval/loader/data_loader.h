/**
 * @file data_loader.h
 * @brief Base data loader interface and implementations
 */

#pragma once

#include "autoeval/core/types.h"
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <optional>

namespace autoeval {
namespace loader {

// Forward declarations and type aliases for convenience
using autoeval::core::Trajectory;
using autoeval::core::Scene;
using autoeval::core::TrajectoryPoint;

// ============================================================================
// Load Result
// ============================================================================

/**
 * @brief Result of a data loading operation
 */
struct LoadResult {
    bool success = false;
    std::string message;
    std::string source_path;
    size_t items_loaded = 0;
    double load_time_ms = 0.0;

    /**
     * @brief Check if loading was successful
     */
    explicit operator bool() const { return success; }
};

// ============================================================================
// Base Data Loader Interface
// ============================================================================

/**
 * @brief Abstract base class for data loaders
 *
 * Data loaders are responsible for loading trajectory data from various sources
 * including files, databases, and simulation systems.
 */
class IDataLoader {
public:
    virtual ~IDataLoader() = default;

    /**
     * @brief Get the loader name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get supported file extensions
     */
    virtual std::vector<std::string> getSupportedExtensions() const = 0;

    /**
     * @brief Check if this loader can handle the given path
     */
    virtual bool canLoad(const std::filesystem::path& path) const;

    /**
     * @brief Load a single trajectory from file
     */
    virtual LoadResult loadTrajectory(const std::filesystem::path& path, Trajectory& out) = 0;

    /**
     * @brief Load multiple trajectories from file
     */
    virtual LoadResult loadTrajectories(const std::filesystem::path& path,
                                       std::vector<Trajectory>& out) = 0;

    /**
     * @brief Load a scene from file
     */
    virtual LoadResult loadScene(const std::filesystem::path& path, Scene& out) = 0;

    /**
     * @brief Load data from string content
     */
    virtual LoadResult loadFromString(const std::string& content,
                                      std::vector<Trajectory>& out) = 0;

    /**
     * @brief Set a configuration option for the loader
     */
    virtual void setOption(const std::string& key, const std::string& value) {
        // Default: ignore options
    }

    /**
     * @brief Get a configuration option
     */
    virtual std::optional<std::string> getOption(const std::string& key) const {
        return std::nullopt;
    }

    /**
     * @brief Set progress callback
     */
    using ProgressCallback = std::function<void(size_t current, size_t total, const std::string& status)>;
    virtual void setProgressCallback(ProgressCallback callback) {
        progress_callback_ = std::move(callback);
    }

protected:
    ProgressCallback progress_callback_;

    void reportProgress(size_t current, size_t total, const std::string& status) {
        if (progress_callback_) {
            progress_callback_(current, total, status);
        }
    }
};

// ============================================================================
// Loader Registry
// ============================================================================

/**
 * @brief Registry for data loaders
 *
 * Manages available loaders and provides a way to get the appropriate
 * loader for a given file type.
 */
class LoaderRegistry {
public:
    static LoaderRegistry& instance();

    /**
     * @brief Register a loader
     */
    void registerLoader(std::shared_ptr<IDataLoader> loader);

    /**
     * @brief Unregister a loader by name
     */
    void unregisterLoader(const std::string& name);

    /**
     * @brief Get a loader by name
     */
    std::shared_ptr<IDataLoader> getLoader(const std::string& name) const;

    /**
     * @brief Get a loader for a file path
     */
    std::shared_ptr<IDataLoader> getLoaderForPath(const std::filesystem::path& path) const;

    /**
     * @brief Get all registered loaders
     */
    std::vector<std::shared_ptr<IDataLoader>> getAllLoaders() const;

    /**
     * @brief Get supported extensions
     */
    std::vector<std::string> getSupportedExtensions() const;

private:
    LoaderRegistry() = default;
    std::vector<std::shared_ptr<IDataLoader>> loaders_;
};

// ============================================================================
// JSON Trajectory Loader
// ============================================================================

/**
 * @brief Loader for JSON format trajectory data
 *
 * Expected JSON format:
 * {
 *   "id": "traj_001",
 *   "type": "ego",
 *   "points": [
 *     {"x": 0, "y": 0, "t": 0, "heading": 0, "v": 0},
 *     ...
 *   ]
 * }
 */
class JsonTrajectoryLoader : public IDataLoader {
public:
    JsonTrajectoryLoader();

    std::string getName() const override { return "json"; }
    std::vector<std::string> getSupportedExtensions() const override;

    LoadResult loadTrajectory(const std::filesystem::path& path, Trajectory& out) override;
    LoadResult loadTrajectories(const std::filesystem::path& path,
                               std::vector<Trajectory>& out) override;
    LoadResult loadScene(const std::filesystem::path& path, Scene& out) override;
    LoadResult loadFromString(const std::string& content,
                             std::vector<Trajectory>& out) override;

private:
    bool parseTrajectoryPoint(const nlohmann::json& j, TrajectoryPoint& point, std::string& error);
    bool parseTrajectory(const nlohmann::json& j, Trajectory& traj, std::string& error);
};

// ============================================================================
// CSV Trajectory Loader
// ============================================================================

/**
 * @brief Loader for CSV format trajectory data
 *
 * Expected CSV format:
 * id,x,y,z,t,heading,velocity,acceleration
 * traj_001,0.0,0.0,0.0,0.0,0.0,10.0,0.0
 * traj_001,1.0,0.0,0.0,0.1,0.0,10.0,0.0
 * ...
 */
class CsvTrajectoryLoader : public IDataLoader {
public:
    CsvTrajectoryLoader();

    std::string getName() const override { return "csv"; }
    std::vector<std::string> getSupportedExtensions() const override;

    LoadResult loadTrajectory(const std::filesystem::path& path, Trajectory& out) override;
    LoadResult loadTrajectories(const std::filesystem::path& path,
                               std::vector<Trajectory>& out) override;
    LoadResult loadScene(const std::filesystem::path& path, Scene& out) override;
    LoadResult loadFromString(const std::string& content,
                             std::vector<Trajectory>& out) override;

    void setOption(const std::string& key, const std::string& value) override;

private:
    char delimiter_ = ',';
    bool has_header_ = true;

    void parseHeader(const std::string& line, std::vector<std::string>& headers);
    bool parseTrajectoryPoint(const std::vector<std::string>& values,
                              const std::vector<std::string>& headers,
                              TrajectoryPoint& point,
                              std::string& error);
};

// ============================================================================
// YAML Configuration Loader
// ============================================================================

/**
 * @brief Loader for YAML configuration files
 *
 * Loads evaluation configuration from YAML files.
 */
class YamlConfigLoader {
public:
    static bool loadConfig(const std::filesystem::path& path, core::EvaluationConfig& out);
    static bool loadConfigFromString(const std::string& content, core::EvaluationConfig& out);
    static bool saveConfig(const std::filesystem::path& path, const core::EvaluationConfig& config);

private:
    static bool parseThreshold(const YAML::Node& node, core::MetricThreshold& out);
    static bool parseSimulationSettings(const YAML::Node& node,
                                       core::EvaluationConfig::SimulationSettings& out);
    static bool parseReportSettings(const YAML::Node& node,
                                    core::EvaluationConfig::ReportSettings& out);
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Load trajectory from file (auto-detect format)
 */
LoadResult loadTrajectory(const std::filesystem::path& path, Trajectory& out);

/**
 * @brief Load trajectories from file (auto-detect format)
 */
LoadResult loadTrajectories(const std::filesystem::path& path, std::vector<Trajectory>& out);

/**
 * @brief Load scene from file (auto-detect format)
 */
LoadResult loadScene(const std::filesystem::path& path, Scene& out);

/**
 * @brief Load configuration from file (YAML)
 */
bool loadConfig(const std::filesystem::path& path, core::EvaluationConfig& out);

/**
 * @brief Save configuration to file (YAML)
 */
bool saveConfig(const std::filesystem::path& path, const core::EvaluationConfig& config);

} // namespace loader
} // namespace autoeval
