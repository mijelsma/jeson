/**
 * @file jeson.hpp
 * @brief Single include for the jeson library.
 *
 * #define JESON_MAX_FIELDS to a higher value before this include if any of
 * your structs have more than 25 direct members (see reflect.hpp).
 */
#pragma once

#include "enum.hpp"
#include "errors.hpp"
#include "field.hpp"
#include "patch.hpp"
#include "parser.hpp"
#include "reflect.hpp"
#include "required.hpp"
#include "string_literal.hpp"
#include "traits.hpp"
#include "validator.hpp"
#include "value.hpp"
#include "version.hpp"
#include "writer.hpp"

namespace jeson {

/**
 * @brief Parses JSON text into a jeson struct.
 * @tparam T A jeson struct (or any leaf/container type jeson supports).
 * @param t_json The JSON text to parse.
 * @param t_target The object to decode into. Only fields whose JSON keys
 * are present get overwritten; everything else keeps its current value.
 * @return Every error found (empty on success) - see errors.hpp.
 */
template <typename T>
ParseErrors parse(std::string_view t_json, T& t_target) {
    Parser parser(t_json);
    return parser.parseInto(t_target);
}

/**
 * @brief Serializes a jeson struct to a JSON string.
 * @tparam T A jeson struct (or any leaf/container type jeson supports).
 * @param t_value The object to serialize.
 * @param t_opts Formatting options - pass jeson::pretty for indented
 * output, or jeson::compact_omit_nulls to drop unset optional fields
 * entirely (matching reflect-cpp's rfl::json::write<rfl::NoOptionals>).
 * Defaults to compact output that writes unset optionals as `null`.
 * @return The serialized JSON text.
 */
template <typename T>
std::string toJson(const T& t_value, WriteOptions t_opts = {}) {
    return Writer::write(t_value, t_opts);
}

} // namespace jeson
