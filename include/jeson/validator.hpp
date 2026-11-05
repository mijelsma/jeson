/**
 * @file validator.hpp
 * @brief Minimal constraint set for embedded config validation.
 */
#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace jeson {

/**
 * @brief Constraint: rejects values less than N.
 * @tparam N The inclusive lower bound.
 */
template <auto N>
struct Minimum {
    /// @return An error message if t_value < N, else std::nullopt.
    template <typename T>
    static std::optional<std::string> check(const T& t_value) {
        if (t_value < static_cast<T>(N)) {
            return "must be >= " + std::to_string(N);
        }
        return std::nullopt;
    }
};

/**
 * @brief Constraint: rejects values greater than N.
 * @tparam N The inclusive upper bound.
 */
template <auto N>
struct Maximum {
    /// @return An error message if t_value > N, else std::nullopt.
    template <typename T>
    static std::optional<std::string> check(const T& t_value) {
        if (t_value > static_cast<T>(N)) {
            return "must be <= " + std::to_string(N);
        }
        return std::nullopt;
    }
};

/**
 * @brief Constraint: rejects strings/containers shorter than N.
 * @tparam N The inclusive minimum size.
 */
template <std::size_t N>
struct MinSize {
    /// @return An error message if t_value.size() < N, else std::nullopt.
    template <typename T>
    static std::optional<std::string> check(const T& t_value) {
        if (t_value.size() < N) {
            return "size must be >= " + std::to_string(N);
        }
        return std::nullopt;
    }
};

/**
 * @brief Constraint: rejects strings/containers longer than N.
 * @tparam N The inclusive maximum size.
 */
template <std::size_t N>
struct MaxSize {
    /// @return An error message if t_value.size() > N, else std::nullopt.
    template <typename T>
    static std::optional<std::string> check(const T& t_value) {
        if (t_value.size() > N) {
            return "size must be <= " + std::to_string(N);
        }
        return std::nullopt;
    }
};

/**
 * @brief Wraps a leaf field's type with one or more compile-time
 * constraints, checked during JSON parsing.
 *
 * @code
 * struct Climate {
 *     jeson::Field<"channel", jeson::Validator<uint8_t, jeson::Minimum<11>, jeson::Maximum<26>>> channel;
 * };
 * @endcode
 *
 * On parse, the raw JSON value is decoded first, then every constraint is
 * checked in order; the first violation's message is attached to the
 * field's path in ParseErrors.
 *
 * Assigning a raw value directly in ordinary C++ code (`settings.channel =
 * 15;`) goes through Validator's converting constructor and is NOT
 * re-checked against the constraints - constraints are enforced at the JSON
 * parse boundary (untrusted external input), not on every in-process
 * assignment (trusted internal code). Call set() directly if you want a
 * checked assignment outside of parsing.
 *
 * @tparam T The wrapped value's real type.
 * @tparam Constraints One or more constraint policies (Minimum/Maximum/MinSize/MaxSize).
 */
template <typename T, typename... Constraints>
class Validator {
public:
    /// The wrapped value's real type.
    using ValueType = T;

    /// Default-constructs the underlying value, unchecked.
    Validator() = default;

    /// Constructs directly from a T, unchecked - see the @brief note above
    /// on why in-process assignment deliberately skips validation.
    Validator(T t_value) : m_value(std::move(t_value)) {}

    /**
     * @brief Applies t_value only if every constraint accepts it.
     * @param t_value The candidate value.
     * @return An error message describing the first violated constraint,
     * or std::nullopt if t_value was accepted and applied.
     */
    std::optional<std::string> set(T t_value) {
        std::optional<std::string> err;
        ((err = err ? err : Constraints::check(t_value)), ...);
        if (!err) {
            m_value = std::move(t_value);
        }
        return err;
    }

    /// @return The current (already-validated) value.
    [[nodiscard]] const T& value() const { return m_value; }
    /// @return The current (already-validated) value.
    operator const T&() const { return m_value; }

private:
    T m_value{};
};

namespace internal {

/// True for any Validator<T, Constraints...> specialization, false for everything else.
template <typename T>
struct is_validator : std::false_type {};
/// @copydoc is_validator
template <typename T, typename... C>
struct is_validator<Validator<T, C...>> : std::true_type {};
/// @copydoc is_validator
template <typename T>
inline constexpr bool is_validator_v = is_validator<std::decay_t<T>>::value;

} // namespace internal

} // namespace jeson
