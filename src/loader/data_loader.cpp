/**
 * @file data_loader.cpp
 * @brief Data loader implementations
 */

#include "autoeval/loader/data_loader.h"
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace autoeval {
namespace loader {

// ============================================================================
// IDataLoader Base Implementation
// ============================================================================

bool IDataLoader::canLoad(const fs::path& path) const {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto supported = getSupportedExtensions();
    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

// ============================================================================
// Loader Registry Implementation
// ============================================================================

LoaderRegistry& LoaderRegistry::instance() {
    static LoaderRegistry instance;
    return instance;
}

void LoaderRegistry::registerLoader(std::shared_ptr<IDataLoader> loader) {
    if (!loader) {
        spdlog::warn("Attempted to register null loader");
        return;
    }
    loaders_.push_back(loader);
    spdlog::info("Registered loader: {}", loader->getName());
}

void LoaderRegistry::unregisterLoader(const std::string& name) {
    auto it = std::remove_if(loaders_.begin(), loaders_.end(),
                            [&name](const auto& loader) {
                                return loader->getName() == name;
                            });
    loaders_.erase(it, loaders_.end());
    spdlog::info("Unregistered loader: {}", name);
}

std::shared_ptr<IDataLoader> LoaderRegistry::getLoader(const std::string& name) const {
    for (const auto& loader : loaders_) {
        if (loader->getName() == name) {
            return loader;
        }
    }
    return nullptr;
}

std::shared_ptr<IDataLoader> LoaderRegistry::getLoaderForPath(const fs::path& path) const {
    for (const auto& loader : loaders_) {
        if (loader->canLoad(path)) {
            return loader;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<IDataLoader>> LoaderRegistry::getAllLoaders() const {
    return loaders_;
}

std::vector<std::string> LoaderRegistry::getSupportedExtensions() const {
    std::vector<std::string> extensions;
    for (const auto& loader : loaders_) {
        auto loader_exts = loader->getSupportedExtensions();
        extensions.insert(extensions.end(), loader_exts.begin(), loader_exts.end());
    }
    // Remove duplicates
    std::sort(extensions.begin(), extensions.end());
    extensions.erase(std::unique(extensions.begin(), extensions.end()), extensions.end());
    return extensions;
}

// ============================================================================
// JsonTrajectoryLoader Implementation
// ============================================================================

JsonTrajectoryLoader::JsonTrajectoryLoader() {
    spdlog::debug("JsonTrajectoryLoader created");
}

std::vector<std::string> JsonTrajectoryLoader::getSupportedExtensions() const {
    return {".json"};
}

bool JsonTrajectoryLoader::parseTrajectoryPoint(const json& j, TrajectoryPoint& point, std::string& error) {
    try {
        point.x = j.value("x", 0.0);
        point.y = j.value("y", 0.0);
        point.z = j.value("z", 0.0);

        // Support both "t" and "timestamp" for time
        point.timestamp = j.value("t", j.value("timestamp", 0.0));

        point.heading = j.value("heading", 0.0);
        point.velocity = j.value("velocity", j.value("v", 0.0));
        point.acceleration = j.value("acceleration", j.value("a", 0.0));
        point.curvature = j.value("curvature", 0.0);
        point.confidence = j.value("confidence", 1.0);

        return true;
    } catch (const json::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool JsonTrajectoryLoader::parseTrajectory(const json& j, Trajectory& traj, std::string& error) {
    try {
        traj.id = j.value("id", j.value("trajectory_id", ""));

        // Parse type
        std::string type_str = j.value("type", "other");
        if (type_str == "ego" || type_str == "ego_vehicle") {
            traj.type = core::ObjectType::EgoVehicle;
        } else if (type_str == "vehicle") {
            traj.type = core::ObjectType::Vehicle;
        } else if (type_str == "pedestrian") {
            traj.type = core::ObjectType::Pedestrian;
        } else if (type_str == "cyclist") {
            traj.type = core::ObjectType::Cyclist;
        } else if (type_str == "obstacle") {
            traj.type = core::ObjectType::StaticObstacle;
        } else {
            traj.type = core::ObjectType::Other;
        }

        traj.source = j.value("source", "");
        traj.algorithm = j.value("algorithm", "");

        // Parse points
        auto points_json = j.at("points");
        traj.points.clear();
        traj.points.reserve(points_json.size());

        for (const auto& point_json : points_json) {
            TrajectoryPoint point;
            if (!parseTrajectoryPoint(point_json, point, error)) {
                return false;
            }
            traj.points.push_back(std::move(point));
        }

        return true;
    } catch (const json::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

LoadResult JsonTrajectoryLoader::loadTrajectory(const fs::path& path, Trajectory& out) {
    auto start = std::chrono::high_resolution_clock::now();

    LoadResult result;
    result.source_path = path.string();

    if (!fs::exists(path)) {
        result.message = "File does not exist: " + path.string();
        spdlog::error(result.message);
        return result;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        result.message = "Failed to open file: " + path.string();
        spdlog::error(result.message);
        return result;
    }

    try {
        json j;
        file >> j;

        std::string error;
        if (!parseTrajectory(j, out, error)) {
            result.message = "Failed to parse trajectory: " + error;
            spdlog::error(result.message);
            return result;
        }

        result.success = true;
        result.message = "Successfully loaded trajectory: " + out.id;
        result.items_loaded = out.points.size();

    } catch (const json::exception& e) {
        result.message = std::string("JSON parse error: ") + e.what();
        spdlog::error(result.message);
        return result;
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    spdlog::info("Loaded trajectory {} from {} ({} points, {:.2f}ms)",
                out.id, path.string(), result.items_loaded, result.load_time_ms);

    return result;
}

LoadResult JsonTrajectoryLoader::loadTrajectories(const fs::path& path,
                                                   std::vector<Trajectory>& out) {
    auto start = std::chrono::high_resolution_clock::now();

    LoadResult result;
    result.source_path = path.string();

    if (!fs::exists(path)) {
        result.message = "File does not exist: " + path.string();
        return result;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        result.message = "Failed to open file: " + path.string();
        return result;
    }

    try {
        json j;
        file >> j;

        out.clear();

        // Support both array of trajectories and single trajectory
        json trajectories_json;
        if (j.is_array()) {
            trajectories_json = j;
        } else if (j.contains("trajectories")) {
            trajectories_json = j["trajectories"];
        } else {
            // Single trajectory
            Trajectory traj;
            std::string error;
            if (parseTrajectory(j, traj, error)) {
                out.push_back(std::move(traj));
            }
        }

        for (const auto& traj_json : trajectories_json) {
            Trajectory traj;
            std::string error;
            if (!parseTrajectory(traj_json, traj, error)) {
                result.message = "Failed to parse trajectory: " + error;
                continue;
            }
            out.push_back(std::move(traj));
        }

        result.success = !out.empty();
        result.message = "Loaded " + std::to_string(out.size()) + " trajectories";
        result.items_loaded = out.size();

    } catch (const json::exception& e) {
        result.message = std::string("JSON parse error: ") + e.what();
        return result;
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

LoadResult JsonTrajectoryLoader::loadScene(const fs::path& path, Scene& out) {
    auto start = std::chrono::high_resolution_clock::now();

    LoadResult result;
    result.source_path = path.string();

    std::vector<Trajectory> trajectories;
    result = loadTrajectories(path, trajectories);

    if (!result.success) {
        return result;
    }

    // Load scene metadata from JSON if present
    std::ifstream file(path);
    json j;
    file >> j;

    out.id = j.value("id", j.value("scene_id", path.stem().string()));
    out.location = j.value("location", "");
    out.weather = j.value("weather", "");
    out.time_of_day = j.value("time_of_day", "");

    if (j.contains("road_info")) {
        auto road = j["road_info"];
        out.road_info.lane_width = road.value("lane_width", 3.5);
        out.road_info.num_lanes = road.value("num_lanes", 1);
        out.road_info.speed_limit = road.value("speed_limit", 13.89);
    }

    out.trajectories = std::move(trajectories);
    result.success = true;
    result.items_loaded = out.trajectories.size();

    auto end = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    spdlog::info("Loaded scene {} from {} ({} trajectories, {:.2f}ms)",
                out.id, path.string(), result.items_loaded, result.load_time_ms);

    return result;
}

LoadResult JsonTrajectoryLoader::loadFromString(const std::string& content,
                                                std::vector<Trajectory>& out) {
    LoadResult result;
    result.source_path = "<string>";

    try {
        json j = json::parse(content);
        out.clear();

        json trajectories_json;
        if (j.is_array()) {
            trajectories_json = j;
        } else if (j.contains("trajectories")) {
            trajectories_json = j["trajectories"];
        } else {
            // Single trajectory
            Trajectory traj;
            std::string error;
            if (parseTrajectory(j, traj, error)) {
                out.push_back(std::move(traj));
            }
        }

        for (const auto& traj_json : trajectories_json) {
            Trajectory traj;
            std::string error;
            if (!parseTrajectory(traj_json, traj, error)) {
                result.message = "Failed to parse trajectory: " + error;
                continue;
            }
            out.push_back(std::move(traj));
        }

        result.success = !out.empty();
        result.message = "Loaded " + std::to_string(out.size()) + " trajectories from string";
        result.items_loaded = out.size();

    } catch (const json::exception& e) {
        result.message = std::string("JSON parse error: ") + e.what();
        return result;
    }

    return result;
}

// ============================================================================
// CsvTrajectoryLoader Implementation
// ============================================================================

CsvTrajectoryLoader::CsvTrajectoryLoader() {
    spdlog::debug("CsvTrajectoryLoader created");
}

std::vector<std::string> CsvTrajectoryLoader::getSupportedExtensions() const {
    return {".csv"};
}

void CsvTrajectoryLoader::setOption(const std::string& key, const std::string& value) {
    if (key == "delimiter") {
        if (value == "tab" || value == "\\t") {
            delimiter_ = '\t';
        } else if (value == "semicolon") {
            delimiter_ = ';';
        } else if (value.length() == 1) {
            delimiter_ = value[0];
        }
    } else if (key == "has_header") {
        has_header_ = (value == "true" || value == "1");
    }
}

void CsvTrajectoryLoader::parseHeader(const std::string& line, std::vector<std::string>& headers) {
    headers.clear();
    std::stringstream ss(line);
    std::string token;

    while (std::getline(ss, token, delimiter_)) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        // Convert to lowercase
        std::transform(token.begin(), token.end(), token.begin(), ::tolower);
        headers.push_back(token);
    }
}

bool CsvTrajectoryLoader::parseTrajectoryPoint(const std::vector<std::string>& values,
                                                const std::vector<std::string>& headers,
                                                TrajectoryPoint& point,
                                                std::string& error) {
    // Initialize with defaults
    point = TrajectoryPoint{};

    // Map header to value
    std::unordered_map<std::string, std::string> data;
    for (size_t i = 0; i < headers.size() && i < values.size(); ++i) {
        data[headers[i]] = values[i];
    }

    auto getDouble = [&](const std::string& key, double default_val = 0.0) -> double {
        auto it = data.find(key);
        if (it != data.end() && !it->second.empty()) {
            try {
                return std::stod(it->second);
            } catch (...) {}
        }
        return default_val;
    };

    point.x = getDouble("x");
    point.y = getDouble("y");
    point.z = getDouble("z");
    point.timestamp = getDouble("t", getDouble("time", getDouble("timestamp")));
    point.heading = getDouble("heading", getDouble("h"));
    point.velocity = getDouble("velocity", getDouble("v", getDouble("speed")));
    point.acceleration = getDouble("acceleration", getDouble("a"));
    point.curvature = getDouble("curvature", getDouble("k"));
    point.confidence = getDouble("confidence", 1.0);

    return true;
}

LoadResult CsvTrajectoryLoader::loadTrajectory(const fs::path& path, Trajectory& out) {
    auto start = std::chrono::high_resolution_clock::now();

    LoadResult result;
    result.source_path = path.string();

    if (!fs::exists(path)) {
        result.message = "File does not exist: " + path.string();
        return result;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        result.message = "Failed to open file: " + path.string();
        return result;
    }

    out = Trajectory{};

    std::string line;
    std::vector<std::string> headers;

    // Read header
    if (has_header_ && std::getline(file, line)) {
        parseHeader(line, headers);
    } else {
        // Default headers
        headers = {"id", "x", "y", "z", "t", "heading", "velocity", "acceleration"};
    }

    // Find trajectory ID from first data line
    std::unordered_map<std::string, std::vector<TrajectoryPoint>> traj_points;

    size_t line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;

        std::vector<std::string> values;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, delimiter_)) {
            values.push_back(token);
        }

        if (values.empty()) continue;

        TrajectoryPoint point;
        std::string error;
        if (!parseTrajectoryPoint(values, headers, point, error)) {
            spdlog::warn("Failed to parse line {}: {}", line_num, error);
            continue;
        }

        // Get trajectory ID
        std::string traj_id = path.stem().string();
        auto it = std::find(headers.begin(), headers.end(), "id");
        if (it != headers.end() && values.size() > static_cast<size_t>(it - headers.begin())) {
            traj_id = values[it - headers.begin()];
        }

        traj_points[traj_id].push_back(point);
    }

    // If single trajectory, use it
    if (traj_points.size() == 1) {
        out.id = traj_points.begin()->first;
        out.points = traj_points.begin()->second;
    } else if (!traj_points.empty()) {
        // Use the first one
        out.id = traj_points.begin()->first;
        out.points = traj_points.begin()->second;
    }

    result.success = !out.points.empty();
    result.message = "Loaded trajectory " + out.id + " from " + path.string();
    result.items_loaded = out.points.size();

    auto end = std::chrono::high_resolution_clock::now();
    result.load_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

LoadResult CsvTrajectoryLoader::loadTrajectories(const fs::path& path,
                                                   std::vector<Trajectory>& out) {
    LoadResult result;
    result.source_path = path.string();

    if (!fs::exists(path)) {
        result.message = "File does not exist: " + path.string();
        return result;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        result.message = "Failed to open file: " + path.string();
        return result;
    }

    out.clear();

    std::string line;
    std::vector<std::string> headers;

    if (has_header_ && std::getline(file, line)) {
        parseHeader(line, headers);
    } else {
        headers = {"id", "x", "y", "z", "t", "heading", "velocity", "acceleration"};
    }

    std::unordered_map<std::string, std::vector<TrajectoryPoint>> traj_points;

    size_t line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;

        std::vector<std::string> values;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, delimiter_)) {
            values.push_back(token);
        }

        if (values.empty()) continue;

        TrajectoryPoint point;
        std::string error;
        if (!parseTrajectoryPoint(values, headers, point, error)) {
            continue;
        }

        std::string traj_id = path.stem().string();
        auto it = std::find(headers.begin(), headers.end(), "id");
        if (it != headers.end() && values.size() > static_cast<size_t>(it - headers.begin())) {
            traj_id = values[it - headers.begin()];
        }

        traj_points[traj_id].push_back(point);
    }

    for (auto& [id, points] : traj_points) {
        Trajectory traj;
        traj.id = id;
        traj.points = std::move(points);
        out.push_back(std::move(traj));
    }

    result.success = !out.empty();
    result.message = "Loaded " + std::to_string(out.size()) + " trajectories from " + path.string();
    result.items_loaded = out.size();

    return result;
}

LoadResult CsvTrajectoryLoader::loadScene(const fs::path& path, Scene& out) {
    LoadResult result = loadTrajectories(path, out.trajectories);

    if (!result.success) {
        return result;
    }

    out.id = path.stem().string();
    result.items_loaded = out.trajectories.size();
    return result;
}

LoadResult CsvTrajectoryLoader::loadFromString(const std::string& content,
                                                std::vector<Trajectory>& out) {
    LoadResult result;
    result.source_path = "<string>";

    std::stringstream ss(content);
    std::string line;
    std::vector<std::string> headers;

    out.clear();

    if (has_header_ && std::getline(ss, line)) {
        parseHeader(line, headers);
    } else {
        headers = {"id", "x", "y", "z", "t", "heading", "velocity", "acceleration"};
    }

    std::unordered_map<std::string, std::vector<TrajectoryPoint>> traj_points;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        std::vector<std::string> values;
        std::stringstream line_ss(line);
        std::string token;
        while (std::getline(line_ss, token, delimiter_)) {
            values.push_back(token);
        }

        if (values.empty()) continue;

        TrajectoryPoint point;
        std::string error;
        if (!parseTrajectoryPoint(values, headers, point, error)) {
            continue;
        }

        std::string traj_id = "default";
        auto it = std::find(headers.begin(), headers.end(), "id");
        if (it != headers.end() && values.size() > static_cast<size_t>(it - headers.begin())) {
            traj_id = values[it - headers.begin()];
        }

        traj_points[traj_id].push_back(point);
    }

    for (auto& [id, points] : traj_points) {
        Trajectory traj;
        traj.id = id;
        traj.points = std::move(points);
        out.push_back(std::move(traj));
    }

    result.success = !out.empty();
    result.message = "Loaded " + std::to_string(out.size()) + " trajectories from string";
    result.items_loaded = out.size();

    return result;
}

// ============================================================================
// YamlConfigLoader Implementation
// ============================================================================

bool YamlConfigLoader::loadConfig(const fs::path& path, core::EvaluationConfig& out) {
    try {
        YAML::Node config = YAML::LoadFile(path.string());

        out.name = config["name"].as<std::string>("default");
        out.description = config["description"].as<std::string>("");

        out.trajectory_file = config["trajectory_file"].as<std::string>("");
        out.scene_file = config["scene_file"].as<std::string>("");
        out.output_dir = config["output_dir"].as<std::string>("");

        if (config["metrics"]) {
            out.metrics = config["metrics"].as<std::vector<std::string>>();
        }

        if (config["thresholds"]) {
            for (const auto& th : config["thresholds"]) {
                core::MetricThreshold threshold;
                if (parseThreshold(th, threshold)) {
                    out.thresholds.push_back(threshold);
                }
            }
        }

        if (config["simulation"]) {
            parseSimulationSettings(config["simulation"], out.simulation);
        }

        if (config["report"]) {
            parseReportSettings(config["report"], out.report);
        }

        return true;
    } catch (const YAML::Exception& e) {
        spdlog::error("Failed to load YAML config: {}", e.what());
        return false;
    }
}

bool YamlConfigLoader::loadConfigFromString(const std::string& content, core::EvaluationConfig& out) {
    try {
        YAML::Node config = YAML::Load(content);

        out.name = config["name"].as<std::string>("default");
        out.description = config["description"].as<std::string>("");

        out.trajectory_file = config["trajectory_file"].as<std::string>("");
        out.scene_file = config["scene_file"].as<std::string>("");
        out.output_dir = config["output_dir"].as<std::string>("");

        if (config["metrics"]) {
            out.metrics = config["metrics"].as<std::vector<std::string>>();
        }

        if (config["thresholds"]) {
            for (const auto& th : config["thresholds"]) {
                core::MetricThreshold threshold;
                if (parseThreshold(th, threshold)) {
                    out.thresholds.push_back(threshold);
                }
            }
        }

        if (config["simulation"]) {
            parseSimulationSettings(config["simulation"], out.simulation);
        }

        if (config["report"]) {
            parseReportSettings(config["report"], out.report);
        }

        return true;
    } catch (const YAML::Exception& e) {
        spdlog::error("Failed to load YAML config: {}", e.what());
        return false;
    }
}

bool YamlConfigLoader::saveConfig(const fs::path& path, const core::EvaluationConfig& config) {
    try {
        YAML::Node node;

        node["name"] = config.name;
        if (!config.description.empty()) {
            node["description"] = config.description;
        }

        if (!config.trajectory_file.empty()) {
            node["trajectory_file"] = config.trajectory_file;
        }
        if (!config.scene_file.empty()) {
            node["scene_file"] = config.scene_file;
        }
        if (!config.output_dir.empty()) {
            node["output_dir"] = config.output_dir;
        }

        if (!config.metrics.empty()) {
            node["metrics"] = config.metrics;
        }

        if (!config.thresholds.empty()) {
            node["thresholds"] = YAML::Node(YAML::NodeType::Sequence);
            for (const auto& th : config.thresholds) {
                YAML::Node th_node;
                th_node["metric_name"] = th.metric_name;
                if (th.comparison == "min") {
                    th_node["min"] = th.min;
                } else if (th.comparison == "max") {
                    th_node["max"] = th.max;
                } else {
                    th_node["min"] = th.min;
                    th_node["max"] = th.max;
                }
                th_node["comparison"] = th.comparison;
                th_node["enabled"] = th.enabled;
                node["thresholds"].push_back(th_node);
            }
        }

        YAML::Node sim_node;
        sim_node["dt"] = config.simulation.dt;
        sim_node["duration"] = config.simulation.duration;
        sim_node["enable_collision_check"] = config.simulation.enable_collision_check;
        sim_node["enable_road_boundary_check"] = config.simulation.enable_road_boundary_check;
        node["simulation"] = sim_node;

        YAML::Node report_node;
        report_node["formats"] = config.report.formats;
        report_node["include_charts"] = config.report.include_charts;
        report_node["include_details"] = config.report.include_details;
        if (!config.report.template_path.empty()) {
            report_node["template_path"] = config.report.template_path;
        }
        node["report"] = report_node;

        std::ofstream file(path);
        file << node;

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save YAML config: {}", e.what());
        return false;
    }
}

bool YamlConfigLoader::parseThreshold(const YAML::Node& node, core::MetricThreshold& out) {
    if (!node["metric_name"]) return false;

    out.metric_name = node["metric_name"].as<std::string>();
    out.min = node["min"].as<double>(std::numeric_limits<double>::lowest());
    out.max = node["max"].as<double>(std::numeric_limits<double>::max());
    out.comparison = node["comparison"].as<std::string>("range");
    out.enabled = node["enabled"].as<bool>(true);

    return true;
}

bool YamlConfigLoader::parseSimulationSettings(const YAML::Node& node,
                                                core::EvaluationConfig::SimulationSettings& out) {
    out.dt = node["dt"].as<double>(0.1);
    out.duration = node["duration"].as<double>(10.0);
    out.enable_collision_check = node["enable_collision_check"].as<bool>(true);
    out.enable_road_boundary_check = node["enable_road_boundary_check"].as<bool>(true);
    return true;
}

bool YamlConfigLoader::parseReportSettings(const YAML::Node& node,
                                           core::EvaluationConfig::ReportSettings& out) {
    if (node["formats"]) {
        out.formats = node["formats"].as<std::vector<std::string>>();
    } else {
        out.formats = {"json"};
    }
    if (node["include_charts"]) {
        out.include_charts = node["include_charts"].as<bool>();
    } else {
        out.include_charts = true;
    }
    if (node["include_details"]) {
        out.include_details = node["include_details"].as<bool>();
    } else {
        out.include_details = true;
    }
    if (node["template_path"]) {
        out.template_path = node["template_path"].as<std::string>();
    } else {
        out.template_path = "";
    }
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

static bool loaders_initialized = false;

static void initialize_loaders() {
    if (loaders_initialized) return;

    auto& registry = LoaderRegistry::instance();
    registry.registerLoader(std::make_shared<JsonTrajectoryLoader>());
    registry.registerLoader(std::make_shared<CsvTrajectoryLoader>());

    loaders_initialized = true;
}

LoadResult loadTrajectory(const fs::path& path, core::Trajectory& out) {
    initialize_loaders();

    auto loader = LoaderRegistry::instance().getLoaderForPath(path);
    if (!loader) {
        LoadResult result;
        result.message = "No loader found for file: " + path.string();
        return result;
    }

    return loader->loadTrajectory(path, out);
}

LoadResult loadTrajectories(const fs::path& path, std::vector<core::Trajectory>& out) {
    initialize_loaders();

    auto loader = LoaderRegistry::instance().getLoaderForPath(path);
    if (!loader) {
        LoadResult result;
        result.message = "No loader found for file: " + path.string();
        return result;
    }

    return loader->loadTrajectories(path, out);
}

LoadResult loadScene(const fs::path& path, core::Scene& out) {
    initialize_loaders();

    auto loader = LoaderRegistry::instance().getLoaderForPath(path);
    if (!loader) {
        LoadResult result;
        result.message = "No loader found for file: " + path.string();
        return result;
    }

    return loader->loadScene(path, out);
}

bool loadConfig(const fs::path& path, core::EvaluationConfig& out) {
    return YamlConfigLoader::loadConfig(path, out);
}

bool saveConfig(const fs::path& path, const core::EvaluationConfig& config) {
    return YamlConfigLoader::saveConfig(path, config);
}

} // namespace loader
} // namespace autoeval
