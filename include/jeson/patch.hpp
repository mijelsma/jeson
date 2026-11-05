/**
 * @file patch.hpp
 * @brief Marks a reflected object as a section-aware patch.
 */
#pragma once

#include <optional>
#include <type_traits>
#include <utility>

namespace jeson {

/** @brief Controls whether a supplied patch section must contain every field. */
enum class PatchSectionPolicy
{
    COMPLETE,
    PARTIAL
};

/**
 * @brief Parse wrapper for configuration-style object patches.
 *
 * Direct members of T must be std::optional fields. Missing direct members
 * remain unset. Supplied sections are complete by default, but a section type
 * can declare `jesonPatchPolicy = PatchSectionPolicy::PARTIAL` to allow its
 * fields to be updated independently. Parsing is performed against the
 * provided baseline so omitted fields in partial sections retain their values.
 *
 * @tparam T The canonical reflected object type being patched.
 */
template <typename T>
struct Patch {
    using ValueType = T;

    T value{};

    constexpr Patch() = default;
    constexpr explicit Patch(T t_baseline) : value(std::move(t_baseline)), m_hasBaseline(true) {}

    constexpr operator T&() { return value; }
    constexpr operator const T&() const { return value; }
    constexpr T& operator*() { return value; }
    constexpr const T& operator*() const { return value; }
    constexpr T* operator->() { return &value; }
    constexpr const T* operator->() const { return &value; }

    [[nodiscard]] constexpr bool hasBaseline() const { return m_hasBaseline; }
private:
    bool m_hasBaseline = false;
};

namespace internal {

template <typename T>
struct is_patch : std::false_type {};

template <typename T>
struct is_patch<Patch<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_patch_v = is_patch<std::decay_t<T>>::value;

template <typename T>
struct patch_section_type;

template <typename T>
struct patch_section_type<std::optional<T>> {
    using type = T;
};

template <typename T>
using patch_section_type_t = typename patch_section_type<std::decay_t<T>>::type;

template <typename T>
inline constexpr PatchSectionPolicy patch_section_policy_v = []() constexpr {
    if constexpr (requires { T::jesonPatchPolicy; }) {
        return T::jesonPatchPolicy;
    } else {
        return PatchSectionPolicy::COMPLETE;
    }
}();

} // namespace internal

} // namespace jeson
