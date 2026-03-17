/**
 * @file main.cpp
 * @brief AutoEval CLI main entry point
 */

#include "autoeval/autoeval.h"
#include "autoeval/core/evaluator.h"
#include "autoeval/loader/data_loader.h"
#include "autoeval/metrics/base_metric.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <iomanip>

// Define YAML_VERSION if not defined
#ifndef YAML_VERSION
#define YAML_VERSION "0.8.0"
#endif

namespace fs = std::filesystem;
using namespace autoeval;

// ============================================================================
// Color Codes
// ============================================================================

namespace colors {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* BOLD = "\033[1m";

    bool enableColors() {
#ifdef _WIN32
        return false;  // Windows console colors need special handling
#else
        return true;
#endif
    }

    std::string color(const std::string& text, const std::string& code) {
        return enableColors() ? code + text + RESET : text;
    }
}

// ============================================================================
// Print Functions
// ============================================================================

void printHeader() {
    std::cout << "\n";
    std::cout << colors::color("AutoEval", colors::BOLD) << " - Autonomous Driving Evaluation Framework\n";
    std::cout << "Version " << getVersion() << "\n";
    std::cout << colors::color("============================================================", colors::CYAN) << "\n\n";
}

void printUsage() {
    std::cout << colors::color("USAGE", colors::BOLD) << "\n\n";
    std::cout << "  autoeval_cli <command> [options]\n\n";

    std::cout << colors::color("COMMANDS", colors::BOLD) << "\n\n";
    std::cout << "  evaluate      Run evaluation on trajectory data\n";
    std::cout << "  list-metrics  List all available metrics\n";
    std::cout << "  list-loaders  List all available data loaders\n";
    std::cout << "  version       Show version information\n";
    std::cout << "  help          Show this help message\n\n";

    std::cout << colors::color("OPTIONS", colors::BOLD) << "\n\n";
    std::cout << "  Global:\n";
    std::cout << "    -v, --verbose      Enable verbose output\n";
    std::cout << "    -q, --quiet        Quiet mode (minimal output)\n";
    std::cout << "    --log-level <lvl>  Set log level (trace, debug, info, warn, error)\n\n";

    std::cout << "  evaluate:\n";
    std::cout << "    -c, --config <file>   Configuration file (YAML)\n";
    std::cout << "    -d, --data <file>     Trajectory/scene data file\n";
    std::cout << "    -o, --output <path>   Output directory for reports\n";
    std::cout << "    -m, --metrics <list>  Comma-separated list of metrics\n";
    std::cout << "    -f, --formats <list>  Comma-separated list of report formats (json,html,pdf)\n";
    std::cout << "    --no-charts          Disable charts in reports\n";
    std::cout << "    --no-details         Disable detailed results\n\n";

    std::cout << colors::color("EXAMPLES", colors::BOLD) << "\n\n";
    std::cout << "  # Quick evaluation with default metrics\n";
    std::cout << "  autoeval_cli evaluate -d trajectory.json -o reports/\n\n";
    std::cout << "  # Evaluation with configuration file\n";
    std::cout << "  autoeval_cli evaluate -c config.yaml\n\n";
    std::cout << "  # Evaluation with specific metrics\n";
    std::cout << "  autoeval_cli evaluate -d trajectory.json -m collision,ttc,smoothness\n\n";
    std::cout << "  # List all metrics\n";
    std::cout << "  autoeval_cli list-metrics\n\n";
}

void printVersion() {
    std::cout << "AutoEval version " << getVersion() << "\n";
    std::cout << "Build: " << getBuildInfo() << "\n";
    std::cout << "\n";

    std::cout << "Dependencies:\n";
    std::cout << "  spdlog: " << SPDLOG_VERSION << "\n";
    std::cout << "  yaml-cpp: " << YAML_VERSION << "\n";
    std::cout << "  nlohmann/json: " << NLOHMANN_JSON_VERSION_MAJOR << "."
              << NLOHMANN_JSON_VERSION_MINOR << "."
              << NLOHMANN_JSON_VERSION_PATCH << "\n";
}

void printMetrics() {
    auto names = metrics::getBuiltInMetricNames();
    auto& registry = metrics::MetricRegistry::instance();

    std::cout << colors::color("Available Metrics", colors::BOLD) << "\n\n";

    // Group by category
    std::unordered_map<std::string, std::vector<metrics::MetricMetadata>> by_category;

    for (const auto& name : names) {
        auto metric = registry.getMetric(name);
        if (metric) {
            auto meta = metric->getMetadata();
            by_category[metrics::getCategoryName(meta.category)].push_back(meta);
        }
    }

    // Print by category
    const char* category_colors[] = {
        colors::RED,    // Safety
        colors::BLUE,   // Comfort
        colors::GREEN,  // Efficiency
        colors::YELLOW, // Compliance
        colors::CYAN    // Performance
    };

    for (const auto& [category, metas] : by_category) {
        int color_idx = 0;
        if (category == "safety") color_idx = 0;
        else if (category == "comfort") color_idx = 1;
        else if (category == "efficiency") color_idx = 2;
        else if (category == "compliance") color_idx = 3;
        else color_idx = 4;

        std::cout << colors::color(category, category_colors[color_idx]) << ":\n";

        for (const auto& meta : metas) {
            std::cout << "  " << colors::color(meta.name, colors::BOLD);
            std::cout << " (" << meta.display_name << ")\n";
            std::cout << "    " << meta.description << "\n";
            std::cout << "    Unit: " << meta.unit;
            std::cout << " | Comparison: " << meta.default_comparison << "\n\n";
        }
    }
}

void printLoaders() {
    auto& registry = loader::LoaderRegistry::instance();

    std::cout << colors::color("Available Data Loaders", colors::BOLD) << "\n\n";

    auto loaders = registry.getAllLoaders();
    auto extensions = registry.getSupportedExtensions();

    for (const auto& loader : loaders) {
        std::cout << colors::color(loader->getName(), colors::BOLD) << "\n";
        auto exts = loader->getSupportedExtensions();
        std::cout << "  Extensions: ";
        for (size_t i = 0; i < exts.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << exts[i];
        }
        std::cout << "\n\n";
    }
}

// ============================================================================
// Command: Evaluate
// ============================================================================

int cmdEvaluate(int argc, char* argv[]) {
    std::string config_file;
    std::string data_file;
    std::string output_dir = ".";
    std::vector<std::string> metrics_list;
    std::vector<std::string> formats = {"json", "html"};
    bool no_charts = false;
    bool no_details = false;

    // Parse arguments
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (arg == "-d" || arg == "--data") {
            if (i + 1 < argc) data_file = argv[++i];
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) output_dir = argv[++i];
        } else if (arg == "-m" || arg == "--metrics") {
            if (i + 1 < argc) {
                std::string metrics_str = argv[++i];
                std::istringstream iss(metrics_str);
                std::string metric;
                while (std::getline(iss, metric, ',')) {
                    metrics_list.push_back(metric);
                }
            }
        } else if (arg == "-f" || arg == "--formats") {
            if (i + 1 < argc) {
                std::string formats_str = argv[++i];
                std::istringstream iss(formats_str);
                std::string format;
                formats.clear();
                while (std::getline(iss, format, ',')) {
                    formats.push_back(format);
                }
            }
        } else if (arg == "--no-charts") {
            no_charts = true;
        } else if (arg == "--no-details") {
            no_details = true;
        }
    }

    // Validate inputs
    if (config_file.empty() && data_file.empty()) {
        std::cerr << colors::color("Error: ", colors::RED)
                  << "Either --config or --data must be specified\n";
        return 1;
    }

    // Create evaluator
    auto evaluator = std::make_unique<core::Evaluator>();

    // Load config if provided
    if (!config_file.empty()) {
        if (!fs::exists(config_file)) {
            std::cerr << colors::color("Error: ", colors::RED)
                      << "Config file not found: " << config_file << "\n";
            return 1;
        }
        if (!evaluator->loadConfig(config_file)) {
            std::cerr << colors::color("Error: ", colors::RED)
                      << "Failed to load config file\n";
            return 1;
        }
    }

    // Load data if provided
    if (!data_file.empty()) {
        if (!fs::exists(data_file)) {
            std::cerr << colors::color("Error: ", colors::RED)
                      << "Data file not found: " << data_file << "\n";
            return 1;
        }

        // Detect file type
        auto ext = fs::path(data_file).extension().string();
        if (ext == ".json" || ext == ".csv") {
            if (!evaluator->loadScene(data_file)) {
                std::cerr << colors::color("Error: ", colors::RED)
                          << "Failed to load scene data\n";
                return 1;
            }
        } else {
            std::cerr << colors::color("Error: ", colors::RED)
                      << "Unsupported file format: " << ext << "\n";
            return 1;
        }
    }

    // Set metrics if specified
    if (!metrics_list.empty()) {
        evaluator->clearMetrics();
        evaluator->addMetrics(metrics_list);
    }

    // Set report options
    auto& config = evaluator->getConfig();
    config.report.formats = formats;
    config.report.include_charts = !no_charts;
    config.report.include_details = !no_details;
    config.output_dir = output_dir;

    // Check readiness
    if (!evaluator->isReady()) {
        std::cerr << colors::color("Error: ", colors::RED)
                  << evaluator->getValidationError() << "\n";
        return 1;
    }

    // Run evaluation with progress
    std::cout << colors::color("Running evaluation...", colors::CYAN) << "\n\n";

    auto summary = evaluator->evaluate([](const core::EvaluationProgress& p) {
        int bar_width = 40;
        int filled = static_cast<int>(bar_width * p.progress);
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            std::cout << (i < filled ? "=" : " ");
        }
        std::cout << "] " << std::setw(3) << static_cast<int>(p.progress * 100) << "%";
        std::cout << " (" << p.current_phase;
        if (!p.current_metric.empty()) {
            std::cout << ": " << p.current_metric;
        }
        std::cout << ")";
        std::cout.flush();
    });

    std::cout << "\n\n";

    // Print summary
    std::cout << colors::color("Evaluation Complete", colors::BOLD) << "\n\n";

    std::string status_color;
    if (summary.overall_status == "pass") {
        status_color = colors::GREEN;
    } else if (summary.overall_status == "fail") {
        status_color = colors::RED;
    } else {
        status_color = colors::YELLOW;
    }

    std::cout << "Status: " << colors::color(summary.overall_status, status_color) << "\n";
    std::cout << "Metrics: " << summary.passedCount() << "/" << summary.metric_results.size()
              << " passed (" << std::fixed << std::setprecision(1)
              << (summary.passRate() * 100) << "%)\n";
    std::cout << "Duration: " << summary.duration().count() << " ms\n\n";

    // Print individual results
    std::cout << colors::color("Metric Results", colors::BOLD) << "\n\n";

    for (const auto& result : summary.metric_results) {
        std::string status_str = result.pass ? "PASS" : "FAIL";
        std::string status_color = result.pass ? colors::GREEN : colors::RED;

        std::cout << "  " << colors::color(result.name, colors::CYAN)
                  << " " << std::setw(15) << std::right
                  << colors::color(status_str, status_color)
                  << "  " << std::fixed << std::setprecision(3)
                  << result.value << " " << result.unit
                  << "\n";

        if (!result.message.empty()) {
            std::cout << "    " << result.message << "\n";
        }
    }
    std::cout << "\n";

    // Generate reports
    if (!formats.empty()) {
        std::cout << colors::color("Generating reports...", colors::CYAN) << "\n";

        fs::create_directories(output_dir);

        for (const auto& format : formats) {
            fs::path output_path = fs::path(output_dir) / ("report." + format);
            if (evaluator->generateReport(output_path)) {
                std::cout << "  " << colors::color("✓", colors::GREEN)
                          << " " << output_path.string() << "\n";
            } else {
                std::cout << "  " << colors::color("✗", colors::RED)
                          << " Failed to generate " << format << " report\n";
            }
        }
        std::cout << "\n";
    }

    return summary.overall_status == "pass" ? 0 : 1;
}

// ============================================================================
// Command: List Metrics
// ============================================================================

int cmdListMetrics(int argc, char* argv[]) {
    printMetrics();
    return 0;
}

// ============================================================================
// Command: List Loaders
// ============================================================================

int cmdListLoaders(int argc, char* argv[]) {
    printLoaders();
    return 0;
}

// ============================================================================
// Command: Version
// ============================================================================

int cmdVersion(int argc, char* argv[]) {
    printVersion();
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    autoeval::initialize();

    spdlog::set_level(spdlog::level::info);

    // Parse global options
    spdlog::level::level_enum log_level = spdlog::level::info;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--verbose") {
            log_level = spdlog::level::debug;
        } else if (arg == "-q" || arg == "--quiet") {
            log_level = spdlog::level::warn;
        } else if (arg == "--log-level") {
            if (i + 1 < argc) {
                std::string level_str = argv[++i];
                if (level_str == "trace") log_level = spdlog::level::trace;
                else if (level_str == "debug") log_level = spdlog::level::debug;
                else if (level_str == "info") log_level = spdlog::level::info;
                else if (level_str == "warn") log_level = spdlog::level::warn;
                else if (level_str == "err") log_level = spdlog::level::err;
            }
        }
    }

    spdlog::set_level(log_level);

    // No command - print help
    if (argc < 2) {
        printHeader();
        printUsage();
        autoeval::shutdown();
        return 0;
    }

    std::string command = argv[1];

    // Help command
    if (command == "help" || command == "--help" || command == "-h") {
        printHeader();
        printUsage();
        autoeval::shutdown();
        return 0;
    }

    printHeader();

    // Dispatch command
    int result = 0;

    if (command == "evaluate") {
        result = cmdEvaluate(argc - 2, argv + 2);
    } else if (command == "list-metrics") {
        result = cmdListMetrics(argc - 2, argv + 2);
    } else if (command == "list-loaders") {
        result = cmdListLoaders(argc - 2, argv + 2);
    } else if (command == "version" || command == "--version" || command == "-V") {
        result = cmdVersion(argc - 2, argv + 2);
    } else {
        std::cerr << colors::color("Error: ", colors::RED)
                  << "Unknown command: " << command << "\n\n";
        printUsage();
        result = 1;
    }

    autoeval::shutdown();
    return result;
}
