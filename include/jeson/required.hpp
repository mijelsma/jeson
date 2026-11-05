/**
 * @file required.hpp
 * @brief Marks a leaf field as mandatory in the JSON input.
 */
#pragma once

#include <type_traits>
#include <utility>

namespace jeson {

/**
 * @brief Wraps a leaf field's type to make parsing fail with a clear error
 * when the corresponding JSON key is absent.
 *
 * @code
 * struct Command {
 *     jeson::Field<"response_id", jeson::Required<std::string>> responseID;
 * };
 * @endcode
 *
 * Every other leaf type simply keeps its current/default value when the
 * corresponding key is absent from the input; Required<T> is the explicit
 * opt-in for "parsing must fail if this key is missing."
 *
 * @tparam T The wrapped value's real type.
 */
template <typename T>
struct Required {
    /// The wrapped value's real type.
    using ValueType = T;

    T value{};

    /// Default-constructs the underlying value.
    constexpr Required() = default;

    /// Constructs directly from a T.
    constexpr Required(T t_value) : value(std::move(t_value)) {}

    /// @return A mutable reference to the underlying value.
    constexpr operator T&() { return value; }
    /// @return A read-only reference to the underlying value.
    constexpr operator const T&() const { return value; }
    /// @return A mutable reference to the underlying value (for `*required` access).
    constexpr T& operator*() { return value; }
    /// @return A read-only reference to the underlying value (for `*required` access).
    constexpr const T& operator*() const { return value; }
};

namespace internal {

/// True for any Required<T> specialization, false for everything else.
template <typename T>
struct is_required : std::false_type {};
/// @copydoc is_required
template <typename T>
struct is_required<Required<T>> : std::true_type {};
/// @copydoc is_required
template <typename T>
inline constexpr bool is_required_v = is_required<std::decay_t<T>>::value;

} // namespace internal

} // namespace jeson
