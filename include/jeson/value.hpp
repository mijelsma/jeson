/**
 * @file value.hpp
 * @brief Optional dynamic JSON value for schema-less documents and tooling.
 *
 * JESON's primary API parses directly into typed aggregate models. Value is
 * the opt-in escape hatch for applications that genuinely cannot know a
 * document's shape at compile time, and for generic tooling such as the
 * nativejson-benchmark adapter.
 */
#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace jeson {

class Parser;

class Value {
public:
    using Null = std::nullptr_t;
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;
    using Storage = std::variant<Null, bool, std::int64_t, std::uint64_t, double, std::string, Array, Object>;

    Value() : m_value(nullptr) {}
    Value(Null) : m_value(nullptr) {}
    Value(bool t_value) : m_value(t_value) {}

    template <typename T>
        requires(std::is_integral_v<std::decay_t<T>> && std::is_signed_v<std::decay_t<T>> &&
                 !std::is_same_v<std::decay_t<T>, bool>)
    Value(T t_value) : m_value(static_cast<std::int64_t>(t_value)) {}

    template <typename T>
        requires(std::is_integral_v<std::decay_t<T>> && std::is_unsigned_v<std::decay_t<T>> &&
                 !std::is_same_v<std::decay_t<T>, bool>)
    Value(T t_value) : m_value(static_cast<std::uint64_t>(t_value)) {}

    template <typename T>
        requires std::is_floating_point_v<std::decay_t<T>>
    Value(T t_value) : m_value(static_cast<double>(t_value)) {}

    Value(const char* t_value) : m_value(std::string(t_value)) {}
    Value(std::string t_value) : m_value(std::move(t_value)) {}
    Value(Array t_value) : m_value(std::move(t_value)) {}
    Value(Object t_value) : m_value(std::move(t_value)) {}

    [[nodiscard]] const Storage& storage() const { return m_value; }
    [[nodiscard]] Storage& storage() { return m_value; }

    template <typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(m_value);
    }

    template <typename T>
    [[nodiscard]] const T& as() const {
        return std::get<T>(m_value);
    }

    template <typename T>
    [[nodiscard]] T& as() {
        return std::get<T>(m_value);
    }

private:
    friend class Parser;

    template <typename T, typename... Args>
    T& emplace(Args&&... t_args) {
        return m_value.template emplace<T>(std::forward<Args>(t_args)...);
    }

    Storage m_value;
};

} // namespace jeson
