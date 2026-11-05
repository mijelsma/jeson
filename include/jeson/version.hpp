/**
 * @file version.hpp
 * @brief Compile-time JESON version information.
 */
#pragma once

#define JESON_VERSION_MAJOR 0
#define JESON_VERSION_MINOR 1
#define JESON_VERSION_PATCH 0
#define JESON_VERSION_STRING "0.1.0"

namespace jeson {

inline constexpr unsigned versionMajor = JESON_VERSION_MAJOR;
inline constexpr unsigned versionMinor = JESON_VERSION_MINOR;
inline constexpr unsigned versionPatch = JESON_VERSION_PATCH;
inline constexpr const char* versionString = JESON_VERSION_STRING;

} // namespace jeson

