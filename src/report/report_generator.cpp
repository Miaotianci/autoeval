/**
 * @file report_generator.cpp
 * @brief Report generator implementations
 */

#include "autoeval/report/report_generator.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace autoeval {
namespace report {

// ============================================================================
// Factory Implementation
// ============================================================================

std::unordered_map<std::string, std::function<std::unique_ptr<IReportGenerator>()>>
    ReportGenerator::generators_ = {
        {"json", []() { return std::make_unique<JsonReportGenerator>(); }},
        {"html", []() { return std::make_unique<HtmlReportGenerator>(); }},
        {"pdf", []() { return std::make_unique<PdfReportGenerator>(); }}
};

std::unique_ptr<IReportGenerator> ReportGenerator::create(const std::string& format) {
    auto it = generators_.find(format);
    if (it != generators_.end()) {
        return it->second();
    }
    return nullptr;
}

void ReportGenerator::registerGenerator(const std::string& format,
                                        std::function<std::unique_ptr<IReportGenerator>()> creator) {
    generators_[format] = creator;
}

std::vector<std::string> ReportGenerator::getSupportedFormats() {
    std::vector<std::string> formats;
    for (const auto& [name, _] : generators_) {
        formats.push_back(name);
    }
    return formats;
}

// ============================================================================
// JSON Report Generator
// ============================================================================

json JsonReportGenerator::summaryToJson(const core::EvaluationSummary& summary) const {
    json j;
    j["id"] = summary.id;
    j["config_file"] = summary.config_file;
    j["data_source"] = summary.data_source;

    // Timestamps
    auto start_t = std::chrono::system_clock::to_time_t(summary.start_time);
    auto end_t = std::chrono::system_clock::to_time_t(summary.end_time);

    std::ostringstream start_ss, end_ss;
    start_ss << std::put_time(std::localtime(&start_t), "%Y-%m-%d %H:%M:%S");
    end_ss << std::put_time(std::localtime(&end_t), "%Y-%m-%d %H:%M:%S");
    j["start_time"] = start_ss.str();
    j["end_time"] = end_ss.str();
    j["duration_ms"] = summary.duration().count();

    // Status
    j["overall_status"] = summary.overall_status;
    j["total_metrics"] = summary.metric_results.size();
    j["passed_metrics"] = summary.passedCount();
    j["failed_metrics"] = summary.failedCount();
    j["pass_rate"] = summary.passRate();

    // Results
    j["metric_results"] = json::array();
    for (const auto& result : summary.metric_results) {
        j["metric_results"].push_back(metricResultToJson(result));
    }

    return j;
}

json JsonReportGenerator::metricResultToJson(const core::MetricResult& result) const {
    json j;
    j["name"] = result.name;
    j["category"] = result.category;
    j["value"] = result.value;
    j["unit"] = result.unit;
    j["pass"] = result.pass;
    j["message"] = result.message;
    j["threshold"] = result.threshold;
    j["threshold_type"] = result.threshold_type;

    // Details
    j["details"] = json::object();
    for (const auto& detail : result.details) {
        if (std::holds_alternative<double>(detail.value)) {
            j["details"][detail.key] = std::get<double>(detail.value);
        } else if (std::holds_alternative<int>(detail.value)) {
            j["details"][detail.key] = std::get<int>(detail.value);
        } else if (std::holds_alternative<std::string>(detail.value)) {
            j["details"][detail.key] = std::get<std::string>(detail.value);
        } else if (std::holds_alternative<bool>(detail.value)) {
            j["details"][detail.key] = std::get<bool>(detail.value);
        }
    }

    return j;
}

bool JsonReportGenerator::generate(const core::EvaluationSummary& summary,
                                   const fs::path& output_path) {
    try {
        json j = summaryToJson(summary);

        // Create output directory if needed
        fs::create_directories(output_path.parent_path());

        // Write to file
        std::ofstream file(output_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open output file: {}", output_path.string());
            return false;
        }

        file << j.dump(2);  // Pretty print with 2 spaces indent
        file.close();

        spdlog::info("JSON report generated: {}", output_path.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to generate JSON report: {}", e.what());
        return false;
    }
}

// ============================================================================
// HTML Report Generator
// ============================================================================

std::string HtmlReportGenerator::generateHtml(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "  <meta charset=\"UTF-8\">\n";
    html << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "  <title>AutoEval Report - " << summary.id << "</title>\n";
    html << "  <style>\n";
    html << "    * { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "    body { font-family: " << theme_.font_family << "; "
        << "background-color: " << theme_.background_color << "; "
        << "color: " << theme_.text_color << "; "
        << "padding: 20px; }\n";
    html << "    .container { max-width: 1200px; margin: 0 auto; }\n";
    html << "    .header { text-align: center; margin-bottom: 30px; padding: 20px; "
        << "background: linear-gradient(135deg, " << theme_.primary_color << ", #0056b3); "
        << "color: white; border-radius: 10px; }\n";
    html << "    .header h1 { margin-bottom: 10px; }\n";
    html << "    .card { background: white; border-radius: 10px; padding: 20px; "
        << "margin-bottom: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
    html << "    .card h2 { color: " << theme_.primary_color << "; margin-bottom: 15px; "
        << "border-bottom: 2px solid " << theme_.border_color << "; padding-bottom: 10px; }\n";
    html << "    .status-pass { color: " << theme_.success_color << "; font-weight: bold; }\n";
    html << "    .status-fail { color: " << theme_.danger_color << "; font-weight: bold; }\n";
    html << "    .status-partial { color: " << theme_.warning_color << "; font-weight: bold; }\n";
    html << "    .metrics-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); "
        << "gap: 15px; }\n";
    html << "    .metric-card { padding: 15px; border: 1px solid " << theme_.border_color << "; "
        << "border-radius: 5px; background: #f8f9fa; }\n";
    html << "    .metric-card.pass { border-left: 4px solid " << theme_.success_color << "; }\n";
    html << "    .metric-card.fail { border-left: 4px solid " << theme_.danger_color << "; }\n";
    html << "    .metric-name { font-weight: bold; color: " << theme_.primary_color << "; }\n";
    html << "    .metric-value { font-size: 24px; font-weight: bold; margin: 10px 0; }\n";
    html << "    .metric-unit { font-size: 14px; color: " << theme_.secondary_color << "; }\n";
    html << "    .metric-message { font-size: 14px; color: " << theme_.secondary_color << "; }\n";
    html << "    .chart-container { height: 300px; margin: 20px 0; }\n";
    html << "    table { width: 100%; border-collapse: collapse; margin-top: 15px; }\n";
    html << "    th, td { padding: 10px; text-align: left; border-bottom: 1px solid " << theme_.border_color << "; }\n";
    html << "    th { background-color: " << theme_.primary_color << "; color: white; }\n";
    html << "    tr:hover { background-color: #f5f5f5; }\n";
    html << "  </style>\n";

    if (include_charts_) {
        html << "  <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n";
    }

    html << "</head>\n";
    html << "<body>\n";
    html << "  <div class=\"container\">\n";

    // Header
    html << "    <div class=\"header\">\n";
    html << "      <h1>AutoEval Evaluation Report</h1>\n";
    html << "      <p>ID: " << summary.id << "</p>\n";
    html << "      <p>Overall Status: <span class=\"status-" << summary.overall_status << "\">"
        << summary.overall_status << "</span></p>\n";
    html << "      <p>Pass Rate: " << std::fixed << std::setprecision(1)
        << (summary.passRate() * 100) << "%</p>\n";
    html << "    </div>\n";

    // Summary section
    html << generateSummarySection(summary);

    // Metrics section
    html << generateMetricsSection(summary);

    // Charts section
    if (include_charts_) {
        html << generateChartsSection(summary);
    }

    // Details section
    if (include_details_) {
        html << generateDetailsSection(summary);
    }

    html << "  </div>\n";
    html << "</body>\n";
    html << "</html>";

    return html.str();
}

std::string HtmlReportGenerator::generateSummarySection(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "    <div class=\"card\">\n";
    html << "      <h2>Summary</h2>\n";
    html << "      <table>\n";
    html << "        <tr><th>Metric</th><th>Value</th></tr>\n";
    html << "        <tr><td>Total Metrics</td><td>" << summary.metric_results.size() << "</td></tr>\n";
    html << "        <tr><td>Passed</td><td class=\"status-pass\">" << summary.passedCount() << "</td></tr>\n";
    html << "        <tr><td>Failed</td><td class=\"status-fail\">" << summary.failedCount() << "</td></tr>\n";
    html << "        <tr><td>Pass Rate</td><td>" << std::fixed << std::setprecision(1)
        << (summary.passRate() * 100) << "%</td></tr>\n";

    auto duration = summary.duration().count();
    html << "        <tr><td>Duration</td><td>" << duration << " ms</td></tr>\n";
    html << "      </table>\n";
    html << "    </div>\n";

    return html.str();
}

std::string HtmlReportGenerator::generateMetricsSection(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "    <div class=\"card\">\n";
    html << "      <h2>Metric Results</h2>\n";
    html << "      <div class=\"metrics-grid\">\n";

    for (const auto& result : summary.metric_results) {
        std::string status_class = result.pass ? "pass" : "fail";

        html << "        <div class=\"metric-card " << status_class << "\">\n";
        html << "          <div class=\"metric-name\">" << result.name << "</div>\n";
        html << "          <div class=\"metric-value\">" << std::fixed << std::setprecision(3)
            << result.value << " <span class=\"metric-unit\">" << result.unit << "</span></div>\n";
        html << "          <div class=\"metric-message\">" << result.message << "</div>\n";
        html << "        </div>\n";
    }

    html << "      </div>\n";
    html << "    </div>\n";

    return html.str();
}

std::string HtmlReportGenerator::generateChartsSection(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "    <div class=\"card\">\n";
    html << "      <h2>Charts</h2>\n";
    html << generatePieChart(summary);
    html << generateBarChart(summary);
    html << "    </div>\n";

    return html.str();
}

std::string HtmlReportGenerator::generatePieChart(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    size_t passed = summary.passedCount();
    size_t failed = summary.failedCount();

    html << "      <div class=\"chart-container\">\n";
    html << "        <canvas id=\"passFailChart\"></canvas>\n";
    html << "      </div>\n";
    html << "      <script>\n";
    html << "        new Chart(document.getElementById('passFailChart'), {\n";
    html << "          type: 'pie',\n";
    html << "          data: {\n";
    html << "            labels: ['Passed', 'Failed'],\n";
    html << "            datasets: [{\n";
    html << "              data: [" << passed << ", " << failed << "],\n";
    html << "              backgroundColor: ['" << theme_.success_color << "', '" << theme_.danger_color << "']\n";
    html << "            }]\n";
    html << "          },\n";
    html << "          options: {\n";
    html << "            responsive: true,\n";
    html << "            maintainAspectRatio: false,\n";
    html << "            plugins: {\n";
    html << "              legend: { position: 'bottom' },\n";
    html << "              title: { display: true, text: 'Pass/Fail Ratio' }\n";
    html << "            }\n";
    html << "          }\n";
    html << "        });\n";
    html << "      </script>\n";

    return html.str();
}

std::string HtmlReportGenerator::generateBarChart(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "      <div class=\"chart-container\">\n";
    html << "        <canvas id=\"metricValuesChart\"></canvas>\n";
    html << "      </div>\n";
    html << "      <script>\n";
    html << "        new Chart(document.getElementById('metricValuesChart'), {\n";
    html << "          type: 'bar',\n";
    html << "          data: {\n";
    html << "            labels: [";

    for (size_t i = 0; i < summary.metric_results.size(); ++i) {
        if (i > 0) html << ", ";
        html << "'" << summary.metric_results[i].name << "'";
    }

    html << "],\n";
    html << "            datasets: [{\n";
    html << "              label: 'Metric Values',\n";
    html << "              data: [";

    for (size_t i = 0; i < summary.metric_results.size(); ++i) {
        if (i > 0) html << ", ";
        html << summary.metric_results[i].value;
    }

    html << "],\n";
    html << "              backgroundColor: '";

    for (size_t i = 0; i < summary.metric_results.size(); ++i) {
        if (i > 0) html << ", ";
        html << (summary.metric_results[i].pass ? theme_.success_color : theme_.danger_color);
    }

    html << "'\n";
    html << "            }]\n";
    html << "          },\n";
    html << "          options: {\n";
    html << "            responsive: true,\n";
    html << "            maintainAspectRatio: false,\n";
    html << "            plugins: {\n";
    html << "              legend: { display: false },\n";
    html << "              title: { display: true, text: 'Metric Values' }\n";
    html << "            },\n";
    html << "            scales: {\n";
    html << "              y: { beginAtZero: true }\n";
    html << "            }\n";
    html << "          }\n";
    html << "        });\n";
    html << "      </script>\n";

    return html.str();
}

std::string HtmlReportGenerator::generateDetailsSection(const core::EvaluationSummary& summary) const {
    std::ostringstream html;

    html << "    <div class=\"card\">\n";
    html << "      <h2>Detailed Results</h2>\n";
    html << "      <table>\n";
    html << "        <thead>\n";
    html << "          <tr>\n";
    html << "            <th>Metric</th>\n";
    html << "            <th>Category</th>\n";
    html << "            <th>Value</th>\n";
    html << "            <th>Unit</th>\n";
    html << "            <th>Status</th>\n";
    html << "            <th>Message</th>\n";
    html << "          </tr>\n";
    html << "        </thead>\n";
    html << "        <tbody>\n";

    for (const auto& result : summary.metric_results) {
        std::string status_class = result.pass ? "pass" : "fail";
        std::string status_text = result.pass ? "PASS" : "FAIL";

        html << "          <tr>\n";
        html << "            <td>" << result.name << "</td>\n";
        html << "            <td>" << result.category << "</td>\n";
        html << "            <td>" << std::fixed << std::setprecision(3) << result.value << "</td>\n";
        html << "            <td>" << result.unit << "</td>\n";
        html << "            <td class=\"status-" << status_class << "\">" << status_text << "</td>\n";
        html << "            <td>" << result.message << "</td>\n";
        html << "          </tr>\n";

        // Add details as sub-rows
        if (!result.details.empty()) {
            html << "          <tr><td colspan=\"6\"><details>\n";
            html << "            <summary>Details</summary>\n";
            html << "            <table style=\"margin-top:10px;\">\n";

            for (const auto& detail : result.details) {
                html << "              <tr><td>" << detail.key << ":</td>";
                if (std::holds_alternative<double>(detail.value)) {
                    html << "<td>" << std::get<double>(detail.value) << "</td>";
                } else if (std::holds_alternative<int>(detail.value)) {
                    html << "<td>" << std::get<int>(detail.value) << "</td>";
                } else if (std::holds_alternative<std::string>(detail.value)) {
                    html << "<td>" << std::get<std::string>(detail.value) << "</td>";
                } else if (std::holds_alternative<bool>(detail.value)) {
                    html << "<td>" << (std::get<bool>(detail.value) ? "true" : "false") << "</td>";
                }
                html << "</tr>\n";
            }

            html << "            </table>\n";
            html << "          </details></td></tr>\n";
        }
    }

    html << "        </tbody>\n";
    html << "      </table>\n";
    html << "    </div>\n";

    return html.str();
}

bool HtmlReportGenerator::generate(const core::EvaluationSummary& summary,
                                    const fs::path& output_path) {
    try {
        std::string html = generateHtml(summary);

        // Create output directory if needed
        fs::create_directories(output_path.parent_path());

        // Write to file
        std::ofstream file(output_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open output file: {}", output_path.string());
            return false;
        }

        file << html;
        file.close();

        spdlog::info("HTML report generated: {}", output_path.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to generate HTML report: {}", e.what());
        return false;
    }
}

// ============================================================================
// PDF Report Generator
// ============================================================================

bool PdfReportGenerator::generate(const core::EvaluationSummary& summary,
                                   const fs::path& output_path) {
    // For now, generate HTML then note about conversion
    // In a full implementation, this would use a PDF library or convert HTML to PDF

    spdlog::info("PDF generation requires external tool (weasyprint or similar)");
    spdlog::info("Generating intermediate HTML...");

    fs::path html_path = output_path;
    html_path.replace_extension(".html");

    HtmlReportGenerator html_gen;
    html_gen.setTheme(theme_);
    html_gen.setIncludeCharts(include_charts_);
    html_gen.setIncludeDetails(include_details_);

    if (!html_gen.generate(summary, html_path)) {
        return false;
    }

    spdlog::info("To convert to PDF, use: weasyprint {} {}", html_path.string(), output_path.string());

    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool generateReport(const core::EvaluationSummary& summary,
                    const fs::path& output_path) {
    auto ext = output_path.extension().string();

    if (ext.empty()) {
        spdlog::error("No file extension specified");
        return false;
    }

    // Remove the dot for format lookup
    std::string format = ext.substr(1);

    auto generator = ReportGenerator::create(format);
    if (!generator) {
        spdlog::error("Unsupported format: {}", format);
        return false;
    }

    return generator->generate(summary, output_path);
}

std::vector<fs::path> generateReports(const core::EvaluationSummary& summary,
                                        const fs::path& output_dir,
                                        const std::vector<std::string>& formats) {
    std::vector<fs::path> generated;

    fs::create_directories(output_dir);

    for (const auto& format : formats) {
        fs::path output_path = output_dir / ("report." + format);
        if (generateReport(summary, output_path)) {
            generated.push_back(output_path);
        }
    }

    return generated;
}

} // namespace report
} // namespace autoeval
