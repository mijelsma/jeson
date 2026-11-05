/**
 * @file string_literal.hpp
 * @brief Compile-time string, usable as a non-type template parameter.
 *
 * C++20 allows class-type non-type template parameters (NTTPs) as long as
 * the type is a "structural type" - a plain aggregate of public members is
 * enough. That's the entire reason this class exists: it lets jeson::Field
 * accept a literal like "ssid" directly as a template argument
 * (Field<"ssid", std::string>), the same way reflect-cpp's rfl::Field does.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace jeson::internal {

/**
 * @brief A fixed-size character array that can be used as a template
 * argument, deduced automatically from a string literal via the deduction
 * guide below.
 *
 * @tparam N Array size, including the terminating null byte (deduced from
 * the source literal's own size, e.g. N=5 for "ssid").
 */
template <std::size_t N>
struct StringLiteral {
    char value[N]{};

    /// Produces an all-zero StringLiteral. Only meaningful as a fallback;
    /// every real use goes through the constructor below.
    constexpr StringLiteral() = default;

    /**
     * @brief Copies a string literal's characters (including the null
     * terminator) into this object at compile time.
     * @param str Source string literal, e.g. "ssid".
     */
    constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

    /// @return The string's length, excluding the null terminator.
    [[nodiscard]] constexpr std::size_t size() const { return N - 1; }

    /// @return A non-owning view over the string, excluding the null terminator.
    [[nodiscard]] constexpr std::string_view view() const { return std::string_view(value, N - 1); }

    /**
     * @brief Compares two StringLiterals by content rather than by size N,
     * so e.g. StringLiteral<4>{"abc"} and StringLiteral<6>{"abcde"} compare
     * unequal (different text) rather than failing to compile (different
     * types) - needed since Field<"a",...> and Field<"bb",...> are
     * different instantiations of the same template and still need to be
     * comparable when jeson looks up a member by name.
     * @tparam M The other StringLiteral's size (may differ from N).
     */
    template <std::size_t M>
    constexpr bool operator==(const StringLiteral<M>& t_other) const {
        return view() == t_other.view();
    }
};

/// Lets `StringLiteral{"ssid"}` (and, more importantly, a bare `"ssid"`
/// passed where a StringLiteral NTTP is expected) deduce N automatically
/// from the literal's own array size, instead of requiring
/// `StringLiteral<5>{"ssid"}` spelled out by hand.
template <std::size_t N>
StringLiteral(const char (&)[N]) -> StringLiteral<N>;

} // namespace jeson::internal
