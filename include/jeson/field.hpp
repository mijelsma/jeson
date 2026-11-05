/**
 * @file field.hpp
 * @brief jeson::Field<"name", T> - the only annotation a member needs.
 *
 * No separate registration macro exists or is needed: which members a
 * struct has is discovered automatically (see reflect.hpp) purely from the
 * struct's own definition, exactly like reflect-cpp. What jeson does
 * differently is *how* Field stores things:
 *
 * - If T is itself another jeson-reflectable struct (WiFi, Climate, ...),
 *   Field<Name, T> *publicly inherits* T instead of storing it as a member.
 *   That means `settings.wifi` literally *is* the WiFi struct as far as the
 *   compiler is concerned - `settings.wifi.ssid = "x"` is ordinary,
 *   zero-overhead member access, with no .value() anywhere, at any nesting
 *   depth. This is the main way jeson differs from reflect-cpp day to day.
 *   (Because of the inheritance, `WiFi& plain = settings.wifi;` also just
 *   works via an ordinary implicit upcast - no dedicated accessor needed.)
 *
 * - If T is a leaf type (bool, int, string, enum, std::optional<X>,
 *   std::vector<X>, jeson::Validator<X,...>, jeson::Required<X>), Field
 *   stores a T value and provides operator=/implicit conversions so
 *   `settings.wifi.dhcp = false;` and `bool d = settings.wifi.dhcp;` both
 *   just work without an explicit unwrap.
 *
 * Field<"name", T> stores nothing beyond what T itself needs: the struct
 * inheriting case adds zero bytes over T, and the leaf case adds exactly
 * sizeof(T) and nothing more (the name lives only in the type, not in any
 * object's memory) - the whole "reflection" is free at runtime.
 */
#pragma once

#include <ostream>
#include <type_traits>
#include <utility>

#include "reflect.hpp"
#include "string_literal.hpp"

namespace jeson {

/**
 * @brief A named struct member - the sole annotation jeson needs. See the
 * @file comment above for how the two specializations below differ.
 * @tparam Name The field's JSON key, e.g. Field<"ssid", ...>.
 * @tparam T The field's value type.
 */
template <internal::StringLiteral Name, typename T, bool = internal::is_reflectable_aggregate_v<T>>
struct Field;

/**
 * @brief Leaf specialization: T is a plain value type (not another
 * reflectable struct), stored directly and implicitly convertible to/from T.
 */
template <internal::StringLiteral Name, typename T>
struct Field<Name, T, false> {
    /// The field's JSON key. Read by the parser/writer via reflection;
    /// not intended to be accessed directly from user code.
    static constexpr auto jesonFieldName_ = Name;

    /// The field's underlying value type - what T resolves to for this
    /// particular Field instantiation.
    using ValueType = T;

    T value{};

    /// Default-constructs the underlying value.
    constexpr Field() = default;

    /// Constructs directly from a T.
    constexpr Field(T t_value) : value(std::move(t_value)) {}

    /**
     * @brief Lets a field wrapping Validator<U,...> or Required<U> be
     * initialized directly from a raw U (e.g. `= 900;` on a
     * Validator<uint16_t,...> field) instead of needing an intermediate
     * Validator/Required temporary written out by hand.
     *
     * Constructing T from U happens in one explicit step here, so it isn't
     * subject to C++'s "at most one implicit user-defined conversion" rule
     * the way a chained implicit conversion at the call site would be.
     *
     * @tparam U The raw value type being converted from.
     */
    template <typename U>
        requires(!std::is_same_v<std::decay_t<U>, T> && !std::is_same_v<std::decay_t<U>, Field> &&
                 !std::is_same_v<std::decay_t<U>, internal::UniversalType> && std::is_constructible_v<T, U&&>)
    constexpr Field(U&& t_rawValue) : value(T(std::forward<U>(t_rawValue))) {}

    /// @return A mutable reference to the underlying value.
    constexpr operator T&() { return value; }
    /// @return A read-only reference to the underlying value.
    constexpr operator const T&() const { return value; }
    /// @return A mutable reference to the underlying value (for `*field` access).
    constexpr T& operator*() { return value; }
    /// @return A read-only reference to the underlying value (for `*field` access).
    constexpr const T& operator*() const { return value; }
    /// @return A mutable pointer to the underlying value (for `field->method()` calls).
    constexpr T* operator->() { return &value; }
    /// @return A read-only pointer to the underlying value (for `field->method()` calls).
    constexpr const T* operator->() const { return &value; }

    /// Assigns a T directly, e.g. `settings.wifi.dhcp = false;`.
    Field& operator=(T t_value) {
        value = std::move(t_value);
        return *this;
    }

    /// @copydoc Field(U&&)
    /// Same rationale as the converting constructor above, for assignment
    /// after construction rather than at construction time.
    template <typename U>
        requires(!std::is_same_v<std::decay_t<U>, T> && !std::is_same_v<std::decay_t<U>, Field> &&
                 !std::is_same_v<std::decay_t<U>, internal::UniversalType> && std::is_constructible_v<T, U&&>)
    Field& operator=(U&& t_rawValue) {
        value = T(std::forward<U>(t_rawValue));
        return *this;
    }
};

/**
 * @brief Streams a leaf Field directly to std::ostream (cout, log macros,
 * etc.) without an explicit unwrap.
 *
 * Found via ADL since Field lives in namespace jeson. Necessary because
 * template argument deduction for the standard library's own operator<<
 * overloads can't see through Field's implicit conversion operator
 * (deduction never considers user-defined conversions) - this dedicated
 * overload closes that gap for the common debug-print case.
 */
template <internal::StringLiteral Name, typename T>
std::ostream& operator<<(std::ostream& t_os, const Field<Name, T, false>& t_field) {
    if constexpr (sizeof(T) == 1 && std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        // Same fix as writer.hpp: uint8_t/int8_t are typically char typedefs,
        // and streaming a char prints a glyph, not a number.
        if constexpr (std::is_signed_v<T>) {
            return t_os << static_cast<int>(t_field.value);
        } else {
            return t_os << static_cast<unsigned>(t_field.value);
        }
    } else {
        return t_os << t_field.value;
    }
}

/**
 * @brief Comparisons for leaf fields against their raw value type (and vice
 * versa).
 *
 * Needed for the same reason operator<< above is: template argument
 * deduction for std::operator==(const T&, const T&) can't see through
 * Field's implicit conversion operator, so `field == "x"` would otherwise
 * fail to compile even though `std::string s = field;` works.
 */
template <internal::StringLiteral Name, typename T>
bool operator==(const Field<Name, T, false>& t_field, const T& t_other) {
    return t_field.value == t_other;
}
/// @copydoc operator==(const Field<Name,T,false>&, const T&)
template <internal::StringLiteral Name, typename T>
bool operator==(const T& t_other, const Field<Name, T, false>& t_field) {
    return t_other == t_field.value;
}
/// @copydoc operator==(const Field<Name,T,false>&, const T&)
template <internal::StringLiteral Name, typename T>
bool operator!=(const Field<Name, T, false>& t_field, const T& t_other) {
    return !(t_field == t_other);
}
/// @copydoc operator==(const Field<Name,T,false>&, const T&)
template <internal::StringLiteral Name, typename T>
bool operator!=(const T& t_other, const Field<Name, T, false>& t_field) {
    return !(t_other == t_field);
}

/**
 * @brief Struct-inheriting specialization: T is itself a reflectable
 * aggregate, so Field publicly inherits it for fully transparent nested
 * member access - `settings.wifi.ssid` needs no unwrap at any depth.
 *
 * Whole-struct assignment/reading (`settings.wifi = otherWifiStruct;`,
 * `WiFi& plain = settings.wifi;`) works via the ordinary implicit
 * derived-to-base conversion this inheritance provides for free - no
 * dedicated accessor is needed for that.
 */
template <internal::StringLiteral Name, typename T>
struct Field<Name, T, true> : T {
    /// @copydoc Field<Name,T,false>::jesonFieldName_
    static constexpr auto jesonFieldName_ = Name;

    /// @copydoc Field<Name,T,false>::ValueType
    using ValueType = T;

    /// Tells reflect.hpp to decompose the inherited base T when reflecting
    /// over this Field, not the (member-less) wrapper itself - see
    /// reflect.hpp's reflectionTarget().
    using JesonReflectAs = T;

    /// Default-constructs the underlying struct.
    constexpr Field() = default;

    /// Constructs directly from a T.
    constexpr Field(T t_value) : T(std::move(t_value)) {}
};

namespace internal {

/// True for any Field<Name, T, B> specialization, false for everything else.
template <typename T>
struct is_field : std::false_type {};
/// @copydoc is_field
template <StringLiteral Name, typename T, bool B>
struct is_field<Field<Name, T, B>> : std::true_type {};
/// @copydoc is_field
template <typename T>
inline constexpr bool is_field_v = is_field<std::decay_t<T>>::value;

} // namespace internal

} // namespace jeson
