/**
 * @file plugin_manager.cpp
 * @brief Plugin manager implementation with cross-platform dynamic loading
 */

#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/version.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

namespace fs = std::filesystem;

namespace autoeval {
namespace plugin {

// ============================================================================
// Cross-Platform Dynamic Loading
// ============================================================================

#ifdef _WIN32
    #include <windows.h>

    using HandleType = HMODULE;

    static void* loadLibrary(const fs::path& path) {
        return static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
    }

    static void freeLibrary(void* handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
    }

    static void* getSymbol(void* handle, const char* name) {
        return static_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
    }

    static std::string getLastErrorStr() {
        DWORD err = GetLastError();
        char* msg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<char*>(&msg), 0, nullptr);
        std::string result = msg ? msg : "Unknown error";
        if (msg) LocalFree(msg);
        return result;
    }

    static fs::path getLibPrefix() { return ""; }
    static fs::path getLibSuffix() { return ".dll"; }
    static std::string getLibPattern() { return "*.dll"; }

#else
    #include <dlfcn.h>

    using HandleType = void*;

    static void* loadLibrary(const fs::path& path) {
        return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    }

    static void freeLibrary(void* handle) {
        dlclose(handle);
    }

    static void* getSymbol(void* handle, const char* name) {
        return dlsym(handle, name);
    }

    static std::string getLastErrorStr() {
        const char* err = dlerror();
        return err ? std::string(err) : "Unknown error";
    }

    #ifdef __APPLE__
        static fs::path getLibPrefix() { return "lib"; }
        static fs::path getLibSuffix() { return ".dylib"; }
        static std::string getLibPattern() { return "lib*.dylib"; }
    #else
        static fs::path getLibPrefix() { return "lib"; }
        static fs::path getLibSuffix() { return ".so"; }
        static std::string getLibPattern() { return "lib*.so"; }
    #endif

#endif

// ============================================================================
// Plugin API Version
// ============================================================================

constexpr int PLUGIN_API_VERSION = 1;

// ============================================================================
// Plugin Manager Implementation (PIMPL)
// ============================================================================

struct PluginManager::Impl {
    struct PluginData {
        std::shared_ptr<IPlugin> plugin;
        fs::path path;
        HandleType handle = nullptr;
        bool initialized = false;
    };

    std::unordered_map<std::string, PluginData> plugins_;
    std::vector<fs::path> search_paths_;
    mutable std::shared_mutex mutex_;

    ~Impl() {
        std::unique_lock lock(mutex_);
        for (auto& [name, data] : plugins_) {
            if (data.plugin) {
                data.plugin->shutdown();
            }
            if (data.handle) {
                freeLibrary(data.handle);
            }
            spdlog::debug("Unloaded plugin: {}", name);
        }
        plugins_.clear();
    }
};

// ============================================================================
// Plugin Loading Helpers
// ============================================================================

namespace {

// MSVC-compatible functor for plugin deletion
template<typename T>
struct PluginDeleter {
    void (*destroy_fn_)(T*);
    explicit PluginDeleter(void (*df)(T*)) : destroy_fn_(df) {}
    void operator()(T* p) const { if (destroy_fn_ && p) destroy_fn_(p); }
};

/**
 * @brief Check if a plugin is API-compatible
 */
bool checkPluginApiVersion(void* handle) {
    auto* getApiVersion = reinterpret_cast<int(*)()>(
        getSymbol(handle, "autoeval_get_plugin_api_version"));
    if (!getApiVersion) {
        spdlog::warn("Plugin does not export autoeval_get_plugin_api_version");
        return false;
    }
    int version = getApiVersion();
    if (version != PLUGIN_API_VERSION) {
        spdlog::error("Plugin API version mismatch: expected {}, got {}",
                       PLUGIN_API_VERSION, version);
        return false;
    }
    return true;
}

/**
 * @brief Load and instantiate a metric plugin
 */
std::shared_ptr<IMetricPlugin> loadMetricPlugin(void* handle) {
    using CreateFn = IMetricPlugin* (*)();
    using DestroyFn = void (*)(IMetricPlugin*);

    auto* create = reinterpret_cast<CreateFn>(
        getSymbol(handle, "autoeval_create_metric_plugin"));
    auto* destroy = reinterpret_cast<DestroyFn>(
        getSymbol(handle, "autoeval_destroy_metric_plugin"));

    if (!create || !destroy) {
        spdlog::error("Metric plugin missing create/destroy functions");
        return nullptr;
    }

    IMetricPlugin* raw = create();
    if (!raw) {
        spdlog::error("Failed to create metric plugin instance");
        return nullptr;
    }
    return std::shared_ptr<IMetricPlugin>(raw, PluginDeleter<IMetricPlugin>(destroy));
}

/**
 * @brief Load and instantiate a loader plugin
 */
std::shared_ptr<ILoaderPlugin> loadLoaderPlugin(void* handle) {
    using CreateFn = ILoaderPlugin* (*)();
    using DestroyFn = void (*)(ILoaderPlugin*);

    auto* create = reinterpret_cast<CreateFn>(
        getSymbol(handle, "autoeval_create_loader_plugin"));
    auto* destroy = reinterpret_cast<DestroyFn>(
        getSymbol(handle, "autoeval_destroy_loader_plugin"));

    if (!create || !destroy) {
        spdlog::error("Loader plugin missing create/destroy functions");
        return nullptr;
    }

    ILoaderPlugin* raw = create();
    if (!raw) {
        spdlog::error("Failed to create loader plugin instance");
        return nullptr;
    }
    return std::shared_ptr<ILoaderPlugin>(raw, PluginDeleter<ILoaderPlugin>(destroy));
}

/**
 * @brief Load and instantiate a reporter plugin
 */
std::shared_ptr<IReporterPlugin> loadReporterPlugin(void* handle) {
    using CreateFn = IReporterPlugin* (*)();
    using DestroyFn = void (*)(IReporterPlugin*);

    auto* create = reinterpret_cast<CreateFn>(
        getSymbol(handle, "autoeval_create_reporter_plugin"));
    auto* destroy = reinterpret_cast<DestroyFn>(
        getSymbol(handle, "autoeval_destroy_reporter_plugin"));

    if (!create || !destroy) {
        spdlog::error("Reporter plugin missing create/destroy functions");
        return nullptr;
    }

    IReporterPlugin* raw = create();
    if (!raw) {
        spdlog::error("Failed to create reporter plugin instance");
        return nullptr;
    }
    return std::shared_ptr<IReporterPlugin>(raw, PluginDeleter<IReporterPlugin>(destroy));
}

} // anonymous namespace

// ============================================================================
// Plugin Manager
// ============================================================================

PluginManager::PluginManager()
    : pImpl(std::make_unique<Impl>())
{
    // Add default search paths
    fs::path exe_dir;
    try {
        exe_dir = fs::current_path();
    } catch (...) {
        exe_dir = ".";
    }

    pImpl->search_paths_.push_back(exe_dir / "plugins");
    pImpl->search_paths_.push_back(exe_dir / "lib");
    pImpl->search_paths_.push_back(exe_dir);

#ifdef _WIN32
    pImpl->search_paths_.push_back(exe_dir / "bin" / "plugins");
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        pImpl->search_paths_.push_back(fs::path(localAppData) / "AutoEval" / "plugins");
    }
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        pImpl->search_paths_.push_back(fs::path(appData) / "AutoEval" / "plugins");
    }
#else
    pImpl->search_paths_.push_back(fs::path("/usr/local/lib/autoeval"));
    pImpl->search_paths_.push_back(fs::path("/usr/lib/autoeval"));
    const char* home = std::getenv("HOME");
    if (home) {
        pImpl->search_paths_.push_back(fs::path(home) / ".local" / "lib" / "autoeval");
    }
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

    // Load the shared library
    void* handle = loadLibrary(path);
    if (!handle) {
        spdlog::error("Failed to load plugin library '{}': {}",
                       path.string(), getLastErrorStr());
        return false;
    }

    // Check API version compatibility
    if (!checkPluginApiVersion(handle)) {
        spdlog::error("Plugin '{}' is incompatible with this AutoEval version", path.string());
        freeLibrary(handle);
        return false;
    }

    // Determine plugin type and load
    std::shared_ptr<IPlugin> plugin;
    std::string plugin_name;

    if (auto* getName = reinterpret_cast<const char*(*)()>(
            getSymbol(handle, "autoeval_get_plugin_name"))) {
        plugin_name = getName();
    }

    // Try loading as each plugin type
    if (auto metric = loadMetricPlugin(handle)) {
        plugin = metric;
        if (plugin_name.empty()) plugin_name = metric->getName();
        spdlog::info("Loaded metric plugin: {}", metric->getName());
    } else if (auto loader = loadLoaderPlugin(handle)) {
        plugin = loader;
        if (plugin_name.empty()) plugin_name = loader->getName();
        spdlog::info("Loaded loader plugin: {}", loader->getName());
    } else if (auto reporter = loadReporterPlugin(handle)) {
        plugin = reporter;
        if (plugin_name.empty()) plugin_name = reporter->getName();
        spdlog::info("Loaded reporter plugin: {}", reporter->getName());
    } else {
        spdlog::error("Cannot determine plugin type for: {}", path.string());
        freeLibrary(handle);
        return false;
    }

    // Check version compatibility with host
    const auto& host_version = autoeval::getVersion();
    if (!plugin->isCompatible(host_version)) {
        spdlog::error("Plugin '{}' (v{}) is not compatible with AutoEval v{}",
                       plugin_name, plugin->getVersion(), host_version);
        freeLibrary(handle);
        return false;
    }

    // Initialize the plugin
    if (!plugin->initialize()) {
        spdlog::error("Plugin '{}' initialization failed", plugin_name);
        freeLibrary(handle);
        return false;
    }

    // Register metric/loader with registries
    if (auto* metric_plugin = dynamic_cast<IMetricPlugin*>(plugin.get())) {
        auto metric = metric_plugin->createMetric();
        metrics::MetricRegistry::instance().registerMetric(metric);
        spdlog::info("Registered metric from plugin: {}", metric->getName());
    } else if (auto* loader_plugin = dynamic_cast<ILoaderPlugin*>(plugin.get())) {
        auto loader = loader_plugin->createLoader();
        loader::LoaderRegistry::instance().registerLoader(loader);
        spdlog::info("Registered loader from plugin: {}", loader->getName());
    }

    // Store plugin data
    {
        std::unique_lock lock(pImpl->mutex_);
        Impl::PluginData data;
        data.plugin = plugin;
        data.path = path;
        data.handle = static_cast<HandleType>(handle);
        data.initialized = true;
        pImpl->plugins_[plugin_name] = std::move(data);
    }

    spdlog::info("Successfully loaded plugin '{}' (v{})",
                  plugin_name, plugin->getVersion());
    return true;
}

size_t PluginManager::loadPluginsFromDirectory(const fs::path& dir_path, bool recursive) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        spdlog::warn("Plugin directory does not exist: {}", dir_path.string());
        return 0;
    }

    size_t loaded = 0;

    try {
        const auto pattern = getLibPattern();
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() == getLibSuffix()) {
                    if (loadPlugin(entry.path())) loaded++;
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() == getLibSuffix()) {
                    if (loadPlugin(entry.path())) loaded++;
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
    std::unique_lock lock(pImpl->mutex_);
    auto it = pImpl->plugins_.find(name);
    if (it == pImpl->plugins_.end()) {
        spdlog::warn("Plugin not loaded: {}", name);
        return false;
    }

    auto& data = it->second;

    // Unregister from registries
    if (auto* metric_plugin = dynamic_cast<IMetricPlugin*>(data.plugin.get())) {
        auto metric = metric_plugin->createMetric();
        metrics::MetricRegistry::instance().unregisterMetric(metric->getName());
    } else if (auto* loader_plugin = dynamic_cast<ILoaderPlugin*>(data.plugin.get())) {
        auto loader = loader_plugin->createLoader();
        loader::LoaderRegistry::instance().unregisterLoader(loader->getName());
    }

    data.plugin->shutdown();

    if (data.handle) {
        freeLibrary(data.handle);
    }

    pImpl->plugins_.erase(it);
    spdlog::info("Unloaded plugin: {}", name);
    return true;
}

void PluginManager::unloadAll() {
    std::unique_lock lock(pImpl->mutex_);
    for (auto& [name, data] : pImpl->plugins_) {
        if (auto* metric_plugin = dynamic_cast<IMetricPlugin*>(data.plugin.get())) {
            auto metric = metric_plugin->createMetric();
            metrics::MetricRegistry::instance().unregisterMetric(metric->getName());
        } else if (auto* loader_plugin = dynamic_cast<ILoaderPlugin*>(data.plugin.get())) {
            auto loader = loader_plugin->createLoader();
            loader::LoaderRegistry::instance().unregisterLoader(loader->getName());
        }
        data.plugin->shutdown();
        if (data.handle) {
            freeLibrary(data.handle);
        }
    }
    pImpl->plugins_.clear();
    spdlog::info("Unloaded all plugins");
}

std::shared_ptr<IPlugin> PluginManager::getPlugin(const std::string& name) const {
    std::shared_lock lock(pImpl->mutex_);
    auto it = pImpl->plugins_.find(name);
    if (it != pImpl->plugins_.end()) {
        return it->second.plugin;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginManager::getAllPlugins() const {
    std::shared_lock lock(pImpl->mutex_);
    std::vector<std::shared_ptr<IPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        result.push_back(data.plugin);
    }
    return result;
}

std::vector<std::shared_ptr<IPlugin>>
PluginManager::getPluginsByType(PluginType type) const {
    std::shared_lock lock(pImpl->mutex_);
    std::vector<std::shared_ptr<IPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == type) {
            result.push_back(data.plugin);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IMetricPlugin>>
PluginManager::getMetricPlugins() const {
    std::shared_lock lock(pImpl->mutex_);
    std::vector<std::shared_ptr<IMetricPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == PluginType::Metric) {
            result.push_back(std::dynamic_pointer_cast<IMetricPlugin>(data.plugin));
        }
    }
    return result;
}

std::vector<std::shared_ptr<ILoaderPlugin>>
PluginManager::getLoaderPlugins() const {
    std::shared_lock lock(pImpl->mutex_);
    std::vector<std::shared_ptr<ILoaderPlugin>> result;
    for (const auto& [_, data] : pImpl->plugins_) {
        if (data.plugin->getType() == PluginType::Loader) {
            result.push_back(std::dynamic_pointer_cast<ILoaderPlugin>(data.plugin));
        }
    }
    return result;
}

bool PluginManager::isPluginLoaded(const std::string& name) const {
    std::shared_lock lock(pImpl->mutex_);
    return pImpl->plugins_.find(name) != pImpl->plugins_.end();
}

std::optional<PluginManager::PluginInfo>
PluginManager::getPluginInfo(const std::string& name) const {
    std::shared_lock lock(pImpl->mutex_);
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
    std::unique_lock lock(pImpl->mutex_);
    pImpl->search_paths_.push_back(path);
    spdlog::debug("Added plugin search path: {}", path.string());
}

std::vector<fs::path> PluginManager::getSearchPaths() const {
    std::shared_lock lock(pImpl->mutex_);
    return pImpl->search_paths_;
}

} // namespace plugin
} // namespace autoeval
