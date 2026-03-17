/**
 * @file evaluator.cpp
 * @brief Core evaluator implementation
 */

#include "autoeval/core/context.h"
#include "autoeval/version.h"
#include <spdlog/spdlog.h>

namespace autoeval {
namespace core {

// ============================================================================
// Library Initialization
// ============================================================================

bool g_initialized = false;

void initialize() {
    if (g_initialized) {
        spdlog::warn("AutoEval already initialized");
        return;
    }

    spdlog::info("AutoEval v{} initializing...", getVersion());
    g_initialized = true;
    spdlog::info("AutoEval initialized successfully");
}

void shutdown() {
    if (!g_initialized) {
        spdlog::warn("AutoEval not initialized");
        return;
    }

    spdlog::info("AutoEval shutting down...");
    g_initialized = false;
}

} // namespace core

// Public API in autoeval namespace
void initialize() {
    core::initialize();
}

void shutdown() {
    core::shutdown();
}

bool isInitialized() {
    return core::g_initialized;
}

} // namespace autoeval
