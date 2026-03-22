/**
 * @file test_plugin.cpp
 * @brief Unit tests for plugin manager
 */

#include "autoeval/autoeval.h"
#include "autoeval/plugin/plugin_interface.h"
#include <cassert>
#include <iostream>

using namespace autoeval;
using namespace autoeval::plugin;

void test_plugin_manager_singleton() {
    std::cout << "  singleton: ";
    auto& mgr1 = PluginManager::instance();
    auto& mgr2 = PluginManager::instance();
    assert(&mgr1 == &mgr2);
    std::cout << "PASS\n";
}

void test_plugin_manager_search_paths() {
    std::cout << "  search paths: ";
    auto& mgr = PluginManager::instance();

    auto paths = mgr.getSearchPaths();
    assert(!paths.empty());

    mgr.addSearchPath("/tmp/plugins");
    auto paths2 = mgr.getSearchPaths();
    assert(paths2.size() == paths.size() + 1);

    std::cout << "PASS\n";
}

void test_plugin_manager_empty_state() {
    std::cout << "  empty state: ";
    auto& mgr = PluginManager::instance();

    assert(mgr.getAllPlugins().empty());
    assert(mgr.getPlugin("nonexistent") == nullptr);
    assert(!mgr.isPluginLoaded("nonexistent"));
    assert(mgr.getPluginInfo("nonexistent") == std::nullopt);
    assert(mgr.getMetricPlugins().empty());
    assert(mgr.getLoaderPlugins().empty());

    std::cout << "PASS\n";
}

void test_plugin_manager_unload_nonexistent() {
    std::cout << "  unload nonexistent: ";
    auto& mgr = PluginManager::instance();

    // Unloading nonexistent should return false, not crash
    assert(mgr.unloadPlugin("nonexistent_plugin") == false);

    std::cout << "PASS\n";
}

void test_plugin_manager_get_by_type() {
    std::cout << "  get by type: ";
    auto& mgr = PluginManager::instance();

    auto reporters = mgr.getPluginsByType(PluginType::Reporter);
    auto metrics = mgr.getPluginsByType(PluginType::Metric);
    auto loaders = mgr.getPluginsByType(PluginType::Loader);
    auto adapters = mgr.getPluginsByType(PluginType::Adapter);

    assert(reporters.empty());
    assert(metrics.empty());
    assert(loaders.empty());
    assert(adapters.empty());

    std::cout << "PASS\n";
}

void test_plugin_manager_unload_all() {
    std::cout << "  unload all: ";
    auto& mgr = PluginManager::instance();

    // Should not crash on empty state
    mgr.unloadAll();
    assert(mgr.getAllPlugins().empty());

    std::cout << "PASS\n";
}

void test_plugin_type_names() {
    std::cout << "  plugin type names: ";
    assert(std::string(getPluginTypeName(PluginType::Metric)) == "metric");
    assert(std::string(getPluginTypeName(PluginType::Loader)) == "loader");
    assert(std::string(getPluginTypeName(PluginType::Reporter)) == "reporter");
    assert(std::string(getPluginTypeName(PluginType::Adapter)) == "adapter");
    assert(std::string(getPluginTypeName(static_cast<PluginType>(99))) == "unknown");
    std::cout << "PASS\n";
}

void test_plugin_info() {
    std::cout << "  plugin info (nonexistent): ";
    auto& mgr = PluginManager::instance();

    auto info = mgr.getPluginInfo("nonexistent");
    assert(info == std::nullopt);

    std::cout << "PASS\n";
}

int main() {
    std::cout << "\nRunning Plugin Manager Tests\n";
    std::cout << "=============================\n\n";

    autoeval::initialize();

    try {
        test_plugin_manager_singleton();
        test_plugin_manager_search_paths();
        test_plugin_manager_empty_state();
        test_plugin_manager_unload_nonexistent();
        test_plugin_manager_get_by_type();
        test_plugin_manager_unload_all();
        test_plugin_type_names();
        test_plugin_info();

        std::cout << "\n=============================\n";
        std::cout << "All plugin manager tests PASSED!\n\n";

        autoeval::shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
