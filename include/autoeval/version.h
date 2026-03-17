/**
 * @file version.h
 * @brief AutoEval version information
 */

#pragma once

#include <string>

namespace autoeval {

/**
 * @brief Get the AutoEval version string
 */
inline const std::string& getVersion() {
    static const std::string version = "0.1.0";
    return version;
}

/**
 * @brief Get the AutoEval version as integer components
 */
inline void getVersionComponents(int& major, int& minor, int& patch) {
    major = 0;
    minor = 1;
    patch = 0;
}

/**
 * @brief Get the build information
 */
inline const char* getBuildInfo() {
    #ifdef AUTOEVAL_BUILD_TIME
    return "Built: " AUTOEVAL_BUILD_TIME;
    #else
    return "Unknown build time";
    #endif
}

/**
 * @brief Version comparison
 * @return Negative if current < other, 0 if equal, positive if current > other
 */
inline int compareVersion(int other_major, int other_minor, int other_patch) {
    int major, minor, patch;
    getVersionComponents(major, minor, patch);

    if (major != other_major) return major - other_major;
    if (minor != other_minor) return minor - other_minor;
    return patch - other_patch;
}

} // namespace autoeval
