/**
 * @file enum.hpp
 * @brief Every enum, zero setup, always serializes as its name.
 *
 * @code
 * enum class Mode : uint8_t { OFF, LOW, HIGH };
 *
 * struct Settings {
 *     jeson::Field<"mode", Mode> mode = Mode::OFF;
 * };
 * @endcode
 *
 * That's it. No macro, no registration, no template specialization -
 * `settings.mode = Mode::HIGH;` writes `"mode":"HIGH"`, and parsing
 * `"HIGH"` back gives you `Mode::HIGH`. This works by probing candidate
 * values at compile time and asking the compiler for each one's name via
 * its function-signature intrinsic (the same technique magic_enum and reflect-cpp's own
 * automatic enum support use) - the parser/writer never see an int unless
 * you ask them to.
 *
 * This assumes a normal, sequential `enum class` starting at or near 0
 * (LOW=0, MEDIUM=1, HIGH=2, ...) - the overwhelmingly common case. Two
 * situations need the one opt-in escape hatch below, jeson::EnumNames<T>:
 *  - a wire name that has to differ from the C++ spelling
 *  - negative values, large gaps, or values above the default probe range
 * Everything else just works with no annotation at all.
 */
#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace jeson::internal {

/// @return The raw, compiler-specific __PRETTY_FUNCTION__ string for the
/// template instantiation `<E, V>` - the only place the compiler ever
/// writes V's *name* (if it has one) into a string we can inspect.
template <typename E, E V>
constexpr std::string_view jeson_magic_raw_name_() {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

/// @return The enumerator name for E's value V, or an empty string_view if
/// V isn't a real enumerator (the probe landed on an unused value).
template <typename E, E V>
constexpr std::string_view jeson_magic_extract_name_() {
    constexpr std::string_view full   = jeson_magic_raw_name_<E, V>();
    constexpr std::string_view valueMarker = "V = ";
    const auto valuePos                     = full.find(valueMarker);
    std::string_view rest;
    if (valuePos != std::string_view::npos) {
        // GCC, Clang, and clang-cl expose named template parameters.
        rest = full.substr(valuePos + valueMarker.size());
        rest = rest.substr(0, rest.find_first_of(";]"));
    } else {
        // Native MSVC prints template arguments inside the function name.
        constexpr std::string_view functionMarker = "jeson_magic_raw_name_<";
        const auto functionPos                     = full.find(functionMarker);
        if (functionPos == std::string_view::npos) {
            return {};
        }
        const auto comma = full.find(',', functionPos + functionMarker.size());
        if (comma == std::string_view::npos) {
            return {};
        }
        rest           = full.substr(comma + 1);
        const auto end = rest.find(">(void)");
        if (end == std::string_view::npos) {
            return {};
        }
        rest = rest.substr(0, end);
        while (!rest.empty() && rest.front() == ' ') {
            rest.remove_prefix(1);
        }
        if (rest.find("::") == std::string_view::npos) {
            return {}; // An unused probe value is printed as an integer by MSVC.
        }
    }
    if (!rest.empty() && rest[0] == '(') {
        return {}; // e.g. "(Mode)99" - probed value isn't a real enumerator
    }
    auto scopePos = rest.rfind("::");
    if (scopePos != std::string_view::npos) {
        rest = rest.substr(scopePos + 2);
    }
    return rest;
}

/// Builds the (value, name) table by probing every value in [0, Max).
/// Entries for values that aren't real enumerators get an empty name,
/// filtered out at lookup time rather than at table-build time (keeping
/// this a single, simple compile-time pass).
template <typename E, int Max, int... Is>
constexpr auto jeson_magic_build_table_(std::integer_sequence<int, Is...>) {
    return std::array<std::pair<E, std::string_view>, sizeof...(Is)>{
        std::pair<E, std::string_view>{static_cast<E>(Is), jeson_magic_extract_name_<E, static_cast<E>(Is)>()}...
    };
}

/// The default probe range every plain enum is checked against
/// automatically - covers any sequential enum with 32 or fewer values
/// starting at/near 0, which is nearly always what a config/status enum
/// looks like. Widen it per-enum via jeson::EnumNames<T> (see below) if
/// you genuinely need more.
inline constexpr int JESON_DEFAULT_ENUM_PROBE_RANGE = 32;

/// @tparam E The enum type being probed.
/// @tparam Max Exclusive upper bound of the probe range [0, Max).
template <typename E, int Max = JESON_DEFAULT_ENUM_PROBE_RANGE>
constexpr auto jeson_magic_table_() {
    return jeson_magic_build_table_<E, Max>(std::make_integer_sequence<int, Max>{});
}

} // namespace jeson::internal

namespace jeson {

/**
 * @brief The one opt-in override, for the two cases automatic magic
 * reflection doesn't cover: a wire name that must differ from the C++
 * spelling, or an enum whose values don't fit the default 0..31 probe
 * range (negative, sparse/bitflag, or simply larger).
 *
 * @code
 * // Custom names:
 * template <>
 * struct jeson::EnumNames<LogLevel> {
 *     static constexpr bool has_mapping = true;
 *     static constexpr std::pair<LogLevel, std::string_view> table[] = {
 *         {LogLevel::DEBUG, "debug"}, {LogLevel::INFO, "info"}, ...
 *     };
 * };
 *
 * // Wider probe range, names still auto-derived:
 * template <>
 * struct jeson::EnumNames<BigEnum> : jeson::internal::MagicEnumNames<BigEnum, 128> {};
 * @endcode
 *
 * Write the specialization anywhere - at plain file/global scope, no
 * namespace block needed for either the enum's own namespace or jeson's.
 * Every enum without a specialization here is handled automatically by
 * the magic reflection described at the top of this file; you'll only
 * ever reach for this for the two cases above.
 *
 * @tparam EnumType The enum type being overridden.
 */
template <typename EnumType>
struct EnumNames {
    /// False unless a specialization for this specific EnumType exists -
    /// in which case the parser/writer use it in place of the automatic
    /// default.
    static constexpr bool has_mapping = false;
};

} // namespace jeson

namespace jeson::internal {

/**
 * @brief Explicit form of the automatic default, for widening the probe
 * range on one specific enum - inherit from this in an EnumNames<T>
 * specialization (see the example above).
 * @tparam E The enum type.
 * @tparam MaxProbe Exclusive upper bound of the probe range [0, MaxProbe).
 */
template <typename E, int MaxProbe = JESON_DEFAULT_ENUM_PROBE_RANGE>
struct MagicEnumNames {
    static constexpr bool has_mapping = true;
    static constexpr auto table       = jeson_magic_table_<E, MaxProbe>();
};

} // namespace jeson::internal
