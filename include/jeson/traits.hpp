/**
 * @file traits.hpp
 * @brief Generic type traits (optional/vector detection) used for dispatch.
 *
 * Struct detection (is_reflectable_aggregate_v) lives in reflect.hpp instead
 * since it's tied directly to the reflection mechanism defined there.
 */
#pragma once

#include <optional>
#include <type_traits>
#include <vector>

namespace jeson::internal {

/// True for std::optional<T> (any T), false for everything else.
template <typename T>
struct is_optional : std::false_type {};
/// @copydoc is_optional
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};
/// @copydoc is_optional
template <typename T>
inline constexpr bool is_optional_v = is_optional<std::decay_t<T>>::value;

/// True for std::vector<T, Allocator> (any T/Allocator), false for everything else.
template <typename T>
struct is_vector : std::false_type {};
/// @copydoc is_vector
template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};
/// @copydoc is_vector
template <typename T>
inline constexpr bool is_vector_v = is_vector<std::decay_t<T>>::value;

} // namespace jeson::internal
