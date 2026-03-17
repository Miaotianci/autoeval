/**
 * @file autoeval.h
 * @brief Main header for AutoEval library
 *
 * Include this header to get access to all AutoEval functionality
 */

#pragma once

// Version information
#include "autoeval/version.h"

// Core types and context
#include "autoeval/core/types.h"
#include "autoeval/core/context.h"

// Metrics (will be added in later phases)
// #include "autoeval/metrics/base_metric.h"
// #include "autoeval/metrics/registry.h"

// Data loaders (will be added in later phases)
// #include "autoeval/loader/data_loader.h"

// Plugin system (will be added in later phases)
// #include "autoeval/plugin/plugin_interface.h"

// Report generators (will be added in later phases)
// #include "autoeval/report/report_generator.h"

// Platform detection
#ifdef _WIN32
    #define AUTOEVAL_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define AUTOEVAL_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define AUTOEVAL_PLATFORM_MACOS
#endif

// Compiler detection
#ifdef _MSC_VER
    #define AUTOEVAL_COMPILER_MSVC
#elif defined(__GNUC__)
    #define AUTOEVAL_COMPILER_GCC
#elif defined(__clang__)
    #define AUTOEVAL_COMPILER_CLANG
#endif

// Export/import macros for Windows shared library
#ifdef AUTOEVAL_CORE_BUILD
    #ifdef AUTOEVAL_PLATFORM_WINDOWS
        #define AUTOEVAL_API __declspec(dllexport)
    #else
        #define AUTOEVAL_API __attribute__((visibility("default")))
    #endif
#else
    #ifdef AUTOEVAL_PLATFORM_WINDOWS
        #define AUTOEVAL_API __declspec(dllimport)
    #else
        #define AUTOEVAL_API
    #endif
#endif

// Utility macros
#define AUTOEVAL_VERSION_STR "0.1.0"
#define AUTOEVAL_UNUSED(x) (void)(x)
#define AUTOEVAL_STRINGIFY(x) AUTOEVAL_STRINGIFY_IMPL(x)
#define AUTOEVAL_STRINGIFY_IMPL(x) #x

namespace autoeval {

/**
 * @brief Initialize the AutoEval library
 * Call this before using any AutoEval functionality
 */
AUTOEVAL_API void initialize();

/**
 * @brief Shutdown the AutoEval library
 * Call this when done using AutoEval
 */
AUTOEVAL_API void shutdown();

/**
 * @brief Check if AutoEval is initialized
 */
AUTOEVAL_API bool isInitialized();

} // namespace autoeval
