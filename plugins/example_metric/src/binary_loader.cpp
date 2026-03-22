/**
 * @file binary_loader.cpp
 * @brief Example loader plugin: Binary trajectory format loader
 *
 * Demonstrates how to implement a custom data loader plugin for AutoEval.
 * This plugin loads a simple binary format: [float x][float y][float t] per point.
 */

#include "autoeval/plugin/plugin_interface.h"
#include "autoeval/loader/data_loader.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace autoeval {
namespace plugin {

// ============================================================================
// BinaryTrajectoryLoader Implementation
// ============================================================================

class BinaryTrajectoryLoader : public loader::IDataLoader {
public:
    /** @brief Static plugin name for export macro */
    static constexpr const char* PLUGIN_NAME = "binary";

    BinaryTrajectoryLoader() = default;

    std::string getName() const override { return "binary"; }

    std::vector<std::string> getSupportedExtensions() const override {
        return {".bin", ".traj", ".binary"};
    }

    bool canLoad(const std::filesystem::path& path) const override {
        auto ext = path.extension().string();
        return ext == ".bin" || ext == ".traj" || ext == ".binary";
    }

    loader::LoadResult loadTrajectory(const std::filesystem::path& path,
                                     core::Trajectory& out) override {
        std::vector<core::Trajectory> trajs;
        auto result = loadTrajectories(path, trajs);
        if (!trajs.empty()) {
            out = trajs[0];
        }
        return result;
    }

    loader::LoadResult loadTrajectories(const std::filesystem::path& path,
                                       std::vector<core::Trajectory>& out) override {
        loader::LoadResult result;
        result.source_path = path.string();

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            result.success = false;
            result.message = "Cannot open file: " + path.string();
            return result;
        }

        // Read number of trajectories
        uint32_t num_trajs = 0;
        file.read(reinterpret_cast<char*>(&num_trajs), sizeof(num_trajs));
        if (!file) {
            result.success = false;
            result.message = "Failed to read trajectory count";
            return result;
        }

        out.reserve(num_trajs);

        for (uint32_t i = 0; i < num_trajs; ++i) {
            // Read trajectory header
            uint32_t id_len = 0;
            file.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));

            std::string traj_id(id_len, '\0');
            if (id_len > 0) {
                file.read(traj_id.data(), static_cast<std::streamsize>(id_len));
            }

            uint32_t num_points = 0;
            file.read(reinterpret_cast<char*>(&num_points), sizeof(num_points));

            core::Trajectory traj;
            traj.id = traj_id;
            traj.type = core::ObjectType::EgoVehicle;

            for (uint32_t j = 0; j < num_points; ++j) {
                float x, y, t;
                file.read(reinterpret_cast<char*>(&x), sizeof(x));
                file.read(reinterpret_cast<char*>(&y), sizeof(y));
                file.read(reinterpret_cast<char*>(&t), sizeof(t));

                core::TrajectoryPoint pt;
                pt.x = x;
                pt.y = y;
                pt.timestamp = t;
                traj.points.push_back(pt);
            }

            out.push_back(std::move(traj));
        }

        if (!file) {
            result.success = false;
            result.message = "Error reading binary data";
            return result;
        }

        result.success = true;
        result.message = "Loaded " + std::to_string(out.size()) + " trajectories";
        result.items_loaded = out.size();
        return result;
    }

    loader::LoadResult loadScene(const std::filesystem::path& path,
                                 core::Scene& out) override {
        loader::LoadResult result;
        result.source_path = path.string();

        std::vector<core::Trajectory> trajs;
        result = loadTrajectories(path, trajs);
        if (!result.success) {
            return result;
        }

        out.id = path.stem().string();
        out.trajectories = std::move(trajs);
        return result;
    }

    loader::LoadResult loadFromString(const std::string& /*content*/,
                                      std::vector<core::Trajectory>& /*out*/) override {
        loader::LoadResult result;
        result.success = false;
        result.message = "Binary loader does not support string content";
        return result;
    }
};

// ============================================================================
// Plugin Interface Implementation
// ============================================================================

class BinaryLoaderPlugin : public ILoaderPlugin {
public:
    std::string getName() const override { return "binary"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Binary trajectory format loader";
    }
    std::string getAuthor() const override { return "AutoEval Team"; }
    PluginType getType() const override { return PluginType::Loader; }

    bool initialize() override {
        spdlog::info("BinaryLoaderPlugin initialized");
        return true;
    }

    void shutdown() override {
        spdlog::info("BinaryLoaderPlugin shutdown");
    }

    std::vector<std::string> getDependencies() const override { return {}; }

    bool isCompatible(const std::string& version) const override {
        if (version.empty() || version[0] == '0') return true;
        return false;
    }

    std::shared_ptr<loader::IDataLoader> createLoader() override {
        return std::make_shared<BinaryTrajectoryLoader>();
    }
};

} // namespace plugin
} // namespace autoeval

AUTOEVAL_EXPORT_LOADER_PLUGIN(autoeval::plugin::BinaryLoaderPlugin)
