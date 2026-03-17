/**
 * @file plugin_interface.h
 * @brief Plugin system interface for AutoEval
 */

#pragma once

#include "autoeval/core/types.h"
#include "autoeval/metrics/base_metric.h"
#include "autoeval/loader/data_loader.h"
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace autoeval {
namespace plugin {

// ============================================================================
// Plugin Types
// ============================================================================

/**
 * @brief Types of plugins supported by AutoEval
 */
enum class PluginType {
    Metric,       ///< Custom metric plugin
    Loader,       ///< Custom data loader plugin
    Reporter,     ///< Custom report generator plugin
    Adapter       ///< Custom algorithm adapter plugin
};

/**
 * @brief Get plugin type as string
 */
inline const char* getPluginTypeName(PluginType type) {
    switch (type) {
        case PluginType::Metric: return "metric";
        case PluginType::Loader: return "loader";
        case PluginType::Reporter: return "reporter";
        case PluginType::Adapter: return "adapter";
        default: return "unknown";
    }
}

// ============================================================================
// Base Plugin Interface
// ============================================================================

/**
 * @brief Base interface for all plugins
 *
 * All plugins must implement this interface to be loaded by the plugin manager.
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * @brief Get the plugin name (unique identifier)
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get the plugin version
     */
    virtual std::string getVersion() const = 0;

    /**
     * @brief Get the plugin description
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get the plugin type
     */
    virtual PluginType getType() const = 0;

    /**
     * @brief Get the plugin author
     */
    virtual std::string getAuthor() const = 0;

    /**
     * @brief Initialize the plugin
     * @return true if successful
     */
    virtual bool initialize() { return true; }

    /**
     * @brief Shutdown the plugin
     */
    virtual void shutdown() {}

    /**
     * @brief Get plugin dependencies
     */
    virtual std::vector<std::string> getDependencies() const {
        return {};
    }

    /**
     * @brief Check if the plugin is compatible with the current version
     */
    virtual bool isCompatible(const std::string& version) const {
        return true;
    }
};

// ============================================================================
// Metric Plugin Interface
// ============================================================================

/**
 * @brief Interface for custom metric plugins
 *
 * Metric plugins provide custom evaluation metrics.
 */
class IMetricPlugin : public IPlugin {
public:
    PluginType getType() const override { return PluginType::Metric; }

    /**
     * @brief Create a metric instance
     */
    virtual std::shared_ptr<metrics::IMetric> createMetric() = 0;

    /**
     * @brief Get the metric metadata
     */
    metrics::MetricMetadata getMetricMetadata() {
        auto metric = createMetric();
        return metric->getMetadata();
    }
};

// ============================================================================
// Loader Plugin Interface
// ============================================================================

/**
 * @brief Interface for custom data loader plugins
 */
class ILoaderPlugin : public IPlugin {
public:
    PluginType getType() const override { return PluginType::Loader; }

    /**
     * @brief Create a loader instance
     */
    virtual std::shared_ptr<loader::IDataLoader> createLoader() = 0;
};

// ============================================================================
// Reporter Plugin Interface
// ============================================================================

/**
 * @brief Interface for custom report generator plugins
 */
class IReporterPlugin : public IPlugin {
public:
    PluginType getType() const override { return PluginType::Reporter; }

    /**
     * @brief Get supported report formats
     */
    virtual std::vector<std::string> getFormats() const = 0;

    /**
     * @brief Generate a report
     */
    virtual bool generate(const core::EvaluationSummary& summary,
                         const std::filesystem::path& output_path) = 0;
};

// ============================================================================
// Plugin Manager
// ============================================================================

/**
 * @brief Manages plugin loading and lifecycle
 */
class PluginManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static PluginManager& instance();

    /**
     * @brief Load a plugin from a shared library
     * @param path Path to the plugin library file
     * @return true if successful
     */
    bool loadPlugin(const std::filesystem::path& path);

    /**
     * @brief Load plugins from a directory
     * @param dir_path Directory containing plugin libraries
     * @param recursive Whether to search recursively
     * @return Number of plugins loaded
     */
    size_t loadPluginsFromDirectory(const std::filesystem::path& dir_path,
                                    bool recursive = false);

    /**
     * @brief Unload a plugin by name
     */
    bool unloadPlugin(const std::string& name);

    /**
     * @brief Unload all plugins
     */
    void unloadAll();

    /**
     * @brief Get a plugin by name
     */
    std::shared_ptr<IPlugin> getPlugin(const std::string& name) const;

    /**
     * @brief Get all loaded plugins
     */
    std::vector<std::shared_ptr<IPlugin>> getAllPlugins() const;

    /**
     * @brief Get plugins by type
     */
    std::vector<std::shared_ptr<IPlugin>> getPluginsByType(PluginType type) const;

    /**
     * @brief Get all metric plugins
     */
    std::vector<std::shared_ptr<IMetricPlugin>> getMetricPlugins() const;

    /**
     * @brief Get all loader plugins
     */
    std::vector<std::shared_ptr<ILoaderPlugin>> getLoaderPlugins() const;

    /**
     * @brief Check if a plugin is loaded
     */
    bool isPluginLoaded(const std::string& name) const;

    /**
     * @brief Get plugin information
     */
    struct PluginInfo {
        std::string name;
        std::string version;
        std::string description;
        std::string author;
        PluginType type;
        std::filesystem::path path;
        bool initialized = false;
    };

    std::optional<PluginInfo> getPluginInfo(const std::string& name) const;

    /**
     * @brief Set plugin search paths
     */
    void addSearchPath(const std::filesystem::path& path);

    /**
     * @brief Get plugin search paths
     */
    std::vector<std::filesystem::path> getSearchPaths() const;

private:
    PluginManager();
    ~PluginManager();

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// Plugin Export Macros
// ============================================================================

#ifdef _WIN32
    #define AUTOEVAL_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define AUTOEVAL_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Export a metric plugin
 */
#define AUTOEVAL_EXPORT_METRIC_PLUGIN(PluginClass) \
    extern "C" { \
        AUTOEVAL_PLUGIN_EXPORT autoeval::plugin::IMetricPlugin* autoeval_create_metric_plugin() { \
            return new PluginClass(); \
        } \
        AUTOEVAL_PLUGIN_EXPORT void autoeval_destroy_metric_plugin(autoeval::plugin::IMetricPlugin* plugin) { \
            delete plugin; \
        } \
        AUTOEVAL_PLUGIN_EXPORT const char* autoeval_get_plugin_name() { \
            return PluginClass().getName().c_str(); \
        } \
    }

/**
 * @brief Export a loader plugin
 */
#define AUTOEVAL_EXPORT_LOADER_PLUGIN(PluginClass) \
    extern "C" { \
        AUTOEVAL_PLUGIN_EXPORT autoeval::plugin::ILoaderPlugin* autoeval_create_loader_plugin() { \
            return new PluginClass(); \
        } \
        AUTOEVAL_PLUGIN_EXPORT void autoeval_destroy_loader_plugin(autoeval::plugin::ILoaderPlugin* plugin) { \
            delete plugin; \
        } \
    }

/**
 * @brief Export a reporter plugin
 */
#define AUTOEVAL_EXPORT_REPORTER_PLUGIN(PluginClass) \
    extern "C" { \
        AUTOEVAL_PLUGIN_EXPORT autoeval::plugin::IReporterPlugin* autoeval_create_reporter_plugin() { \
            return new PluginClass(); \
        } \
        AUTOEVAL_PLUGIN_EXPORT void autoeval_destroy_reporter_plugin(autoeval::plugin::IReporterPlugin* plugin) { \
            delete plugin; \
        } \
    }

} // namespace plugin
} // namespace autoeval
