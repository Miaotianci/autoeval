/**
 * @file plugin_manager.cpp
 * @brief Plugin manager implementation
 */

#include "autoeval/plugin/plugin_interface.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace autoeval {
namespace plugin {

// ============================================================================
// Plugin Manager Implementation (PIMPL)
// ============================================================================

struct PluginManager::Impl {
    struct PluginData {
        std::shared_ptr<IPlugin> plugin;
        fs::path path;
        void* handle = nullptr;
        bool initialized = false;
    };

    std::unordered_map<std::string, PluginData> plugins_;
    std::vector<fs::path> search_paths_;

    ~Impl() {
        // Unload all plugins
        for (auto& [name, data] : plugins_) {
            if (data.handle) {
#ifdef _WIN32
                FreeLibrary(static_cast<HMODULE>(data.handle));
#else
                dlclose(data.handle);
#endif
            }
            spdlog::debug("Unloaded plugin: {}", name);
        }
        plugins_.clear();
    }
};

// ============================================================================
// Plugin Manager
// ============================================================================

PluginManager::PluginManager()
    : pImpl(std::make_unique<Impl>())
{
    // Add default search paths
    pImpl->search_paths_.push_back(fs::path(".") / "plugins");
    pImpl->search_paths_.push_back(fs::path(".") / "lib");

#ifdef _WIN32
    pImpl->search_paths_.push_back(fs::path("."));
    pImpl->search_paths_.push_back(fs::path(".\\plugins"));
#else
    pImpl->search_paths_.push_back(fs::path("/usr/local/lib/autoeval"));
    pImpl->search_paths_.push_back(fs::path("/usr/lib/autoeval"));
#endif
}

PluginManager::~PluginManager() = default;

PluginManager& PluginManager::instance() {
    static PluginManager instance;
    return instance;
}

bool PluginManager::loadPlugin(const fs::path& path) {
    spdlog::info("Loading plugin from: {}", path.string());

    if (!fs::exists(path)) {
        spdlog::error("Plugin file does not exist: {}", path.string());
        return false;
    }

    // For now, this is a placeholder for dynamic loading
    // In a full implementation, this would:
    // 1. Load the shared library (dlopen/LoadLibrary)
    // 2. Find the plugin creation function
    // 3. Create the plugin instance
    // 4. Initialize the plugin

    // TODO: Implement actual dynamic loading with platform-specific code
    spdlog::warn("Dynamic plugin loading not yet implemented: {}", path.string());
    return false;
}

size_t PluginManager::loadPluginsFromDirectory(const fs::path& dir_path, bool recursive) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        spdlog::warn("Plugin directory does not exist: {}", dir_path.string());
        return 0;
    }

    size_t loaded = 0;

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();

                // Check for shared library extensions
#ifdef _WIN32
                if (ext == ".dll") {
#else
                if (ext == ".so") {
#endif
                    if (loadPlugin(entry.path())) {
                        loaded++;
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();

                // Check for shared library extensions
#ifdef _WIN32
                if (ext == ".dll") {
#else
                if (ext == ".so") {
#endif
                    if (loadPlugin(entry.path())) {
                        loaded++;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error scanning plugin directory: {}", e.what());
    }

    spdlog::info("Loaded {} plugins from {}", loaded, dir_path.string());
    return loaded;
}

bool PluginManager::unloadPlugin(const std::string& name) {
    auto it = pImpl->plugins_.find(name);
    if (it == pImpl->plugins_.end()) {
        spdlog::warn("Plugin not loaded: {}", name);
        return false;
    }

    // Shutdown the plugin
    it->second.plugin->shutdown();

    // TODO: Unload shared library (dlclose/FreeLibrary)

    pImpl->plugins_.erase(it);
    spdlog::info("Unloaded plugin: {}", name);
    return true;
}

void PluginManager::unloadAll() {
    for (auto& [name, data] : pImpl->plugins_) {
        data.plugin->shutdown();
    }
    pImpl->plugins_.clear();
    spdlog::info("Unloaded all plugins");
}

std::shared_ptr<IPlugin> PluginManager::getPlugin(const std::string& name) const {
    auto it = pImpl->plugins_.find(name);
    if (it != pImpl->plugins_.end()) {
        return it->second.plugin;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginManager::getAllPlugins() const {
    std::vector<std::shared_ptr<IPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        result.push_back(data.plugin);
    }
    return result;
}

std::vector<std::shared_ptr<IPlugin>> PluginManager::getPluginsByType(PluginType type) const {
    std::vector<std::shared_ptr<IPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == type) {
            result.push_back(data.plugin);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IMetricPlugin>> PluginManager::getMetricPlugins() const {
    std::vector<std::shared_ptr<IMetricPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == PluginType::Metric) {
            result.push_back(std::dynamic_pointer_cast<IMetricPlugin>(data.plugin));
        }
    }
    return result;
}

std::vector<std::shared_ptr<ILoaderPlugin>> PluginManager::getLoaderPlugins() const {
    std::vector<std::shared_ptr<ILoaderPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == PluginType::Loader) {
            result.push_back(std::dynamic_pointer_cast<ILoaderPlugin>(data.plugin));
        }
    }
    return result;
}

bool PluginManager::isPluginLoaded(const std::string& name) const {
    return pImpl->plugins_.find(name) != pImpl->plugins_.end();
}

std::optional<PluginManager::PluginInfo> PluginManager::getPluginInfo(const std::string& name) const {
    auto it = pImpl->plugins_.find(name);
    if (it == pImpl->plugins_.end()) {
        return std::nullopt;
    }

    const auto& data = it->second;
    const auto& plugin = data.plugin;

    PluginInfo info;
    info.name = plugin->getName();
    info.version = plugin->getVersion();
    info.description = plugin->getDescription();
    info.author = plugin->getAuthor();
    info.type = plugin->getType();
    info.path = data.path;
    info.initialized = data.initialized;

    return info;
}

void PluginManager::addSearchPath(const fs::path& path) {
    pImpl->search_paths_.push_back(path);
    spdlog::debug("Added plugin search path: {}", path.string());
}

std::vector<fs::path> PluginManager::getSearchPaths() const {
    return pImpl->search_paths_;
}

} // namespace plugin
} // namespace autoeval
