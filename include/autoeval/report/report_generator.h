/**
 * @file report_generator.h
 * @brief Report generation interfaces and implementations
 */

#pragma once

#include "autoeval/core/types.h"
#include "autoeval/core/context.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <filesystem>
#include <optional>
#include <functional>
#include <unordered_map>
#include <vector>

namespace autoeval {
namespace report {

// ============================================================================
// Report Theme
// ============================================================================

/**
 * @brief Theme settings for report generation
 */
struct ReportTheme {
    std::string name = "default";
    std::string primary_color = "#007bff";
    std::string secondary_color = "#6c757d";
    std::string success_color = "#28a745";
    std::string danger_color = "#dc3545";
    std::string warning_color = "#ffc107";
    std::string background_color = "#ffffff";
    std::string text_color = "#212529";
    std::string border_color = "#dee2e6";
    std::string font_family = "Arial, sans-serif";
};

// ============================================================================
// Base Report Generator Interface
// ============================================================================

/**
 * @brief Abstract base class for report generators
 */
class IReportGenerator {
public:
    virtual ~IReportGenerator() = default;

    /**
     * @brief Get the format name
     */
    virtual std::string getFormat() const = 0;

    /**
     * @brief Get supported file extensions
     */
    virtual std::vector<std::string> getExtensions() const = 0;

    /**
     * @brief Generate a report
     */
    virtual bool generate(const core::EvaluationSummary& summary,
                         const std::filesystem::path& output_path) = 0;

    /**
     * @brief Set the report theme
     */
    virtual void setTheme(const ReportTheme& theme) {
        theme_ = theme;
    }

    /**
     * @brief Get the current theme
     */
    virtual const ReportTheme& getTheme() const {
        return theme_;
    }

    /**
     * @brief Include/exclude charts
     */
    virtual void setIncludeCharts(bool include) {
        include_charts_ = include;
    }

    /**
     * @brief Include/exclude detailed results
     */
    virtual void setIncludeDetails(bool include) {
        include_details_ = include;
    }

protected:
    ReportTheme theme_;
    bool include_charts_ = true;
    bool include_details_ = true;
};

// ============================================================================
// JSON Report Generator
// ============================================================================

/**
 * @brief Generates JSON format reports
 */
class JsonReportGenerator : public IReportGenerator {
public:
    std::string getFormat() const override { return "json"; }
    std::vector<std::string> getExtensions() const override {
        return {".json"};
    }

    bool generate(const core::EvaluationSummary& summary,
                  const std::filesystem::path& output_path) override;

private:
    nlohmann::json summaryToJson(const core::EvaluationSummary& summary) const;
    nlohmann::json metricResultToJson(const core::MetricResult& result) const;
};

// ============================================================================
// HTML Report Generator
// ============================================================================

/**
 * @brief Generates HTML format reports with embedded charts
 */
class HtmlReportGenerator : public IReportGenerator {
public:
    std::string getFormat() const override { return "html"; }
    std::vector<std::string> getExtensions() const override {
        return {".html"};
    }

    bool generate(const core::EvaluationSummary& summary,
                  const std::filesystem::path& output_path) override;

    /**
     * @brief Set a custom HTML template
     */
    void setTemplate(const std::string& template_path) {
        custom_template_ = template_path;
    }

private:
    std::string custom_template_;

    std::string generateHtml(const core::EvaluationSummary& summary) const;
    std::string generateSummarySection(const core::EvaluationSummary& summary) const;
    std::string generateMetricsSection(const core::EvaluationSummary& summary) const;
    std::string generateChartsSection(const core::EvaluationSummary& summary) const;
    std::string generateDetailsSection(const core::EvaluationSummary& summary) const;
    std::string generatePieChart(const core::EvaluationSummary& summary) const;
    std::string generateBarChart(const core::EvaluationSummary& summary) const;
};

// ============================================================================
// PDF Report Generator
// ============================================================================
/**
 * @brief Generates PDF format reports
 */
class PdfReportGenerator : public IReportGenerator {
public:
    std::string getFormat() const override { return "pdf"; }
    std::vector<std::string> getExtensions() const override {
        return {".pdf"};
    }

    bool generate(const core::EvaluationSummary& summary,
                  const std::filesystem::path& output_path) override;

    /**
     * @brief Set page margins (in points)
     */
    void setMargins(double top, double bottom, double left, double right) {
        margin_top_ = top;
        margin_bottom_ = bottom;
        margin_left_ = left;
        margin_right_ = right;
    }

    /**
     * @brief Set paper size
     */
    void setPaperSize(const std::string& size) {
        paper_size_ = size;
    }

private:
    double margin_top_ = 72;
    double margin_bottom_ = 72;
    double margin_left_ = 72;
    double margin_right_ = 72;
    std::string paper_size_ = "A4";

    // Note: PDF generation typically requires external libraries or tools
    // This is a simplified implementation that generates HTML then converts
};

// ============================================================================
// Report Generator Factory
// ============================================================================

/**
 * @brief Factory for creating report generators
 */
class ReportGenerator {
public:
    /**
     * @brief Create a report generator for the specified format
     */
    static std::unique_ptr<IReportGenerator> create(const std::string& format);

    /**
     * @brief Register a custom report generator
     */
    static void registerGenerator(const std::string& format,
                                  std::function<std::unique_ptr<IReportGenerator>()> creator);

    /**
     * @brief Get all supported formats
     */
    static std::vector<std::string> getSupportedFormats();

private:
    static std::unordered_map<std::string, std::function<std::unique_ptr<IReportGenerator>()>> generators_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Generate a report with auto-detected format from file extension
 */
bool generateReport(const core::EvaluationSummary& summary,
                   const std::filesystem::path& output_path);

/**
 * @brief Generate multiple reports in different formats
 */
std::vector<std::filesystem::path> generateReports(
    const core::EvaluationSummary& summary,
    const std::filesystem::path& output_dir,
    const std::vector<std::string>& formats);

} // namespace report
} // namespace autoeval
