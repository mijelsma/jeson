/**
 * @file writer.hpp
 * @brief Struct -> JSON string writer, mirroring parser.hpp's dispatch.
 *
 * Builds directly into a std::string - deliberately no std::ostringstream
 * anywhere. The first std::ostringstream/std::ostream ever constructed in a
 * process triggers one-time static initialization of libstdc++'s locale
 * facet cache, which is never freed - a real, measured RAM cost on
 * embedded targets whose firmware otherwise never touches C++ iostreams
 * (most ESP-IDF code uses printf/ESP_LOG, not std::cout). Avoiding
 * std::ostream entirely here means jeson never pays that tax, regardless
 * of what else the firmware does or doesn't touch.
 */
#pragma once

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "field.hpp"
#include "patch.hpp"
#include "reflect.hpp"
#include "required.hpp"
#include "traits.hpp"
#include "validator.hpp"

namespace jeson {

/// Controls Writer's output formatting.
struct WriteOptions {
    /// When true, output is indented and newline-separated; when false
    /// (the default), output is a single compact line.
    bool pretty = false;

    /// Spaces per indent level, only meaningful when pretty is true.
    unsigned indentBy = 2;

    /// When true, an unset std::optional<T> field is omitted from the
    /// output entirely instead of being written as `null` - the equivalent
    /// of reflect-cpp's rfl::json::write<rfl::NoOptionals>(...).
    bool omitNulls = false;
};

/// jeson::toJson(value, jeson::pretty)
inline constexpr WriteOptions pretty{true, 2, false};

/// jeson::toJson(value, jeson::compact_omit_nulls) - smallest possible
/// payload: no whitespace, and unset optionals are left out entirely.
inline constexpr WriteOptions compact_omit_nulls{false, 2, true};

/// The library's struct-to-JSON writer. Stateless - write() is the only
/// entry point, everything else is a private implementation detail.
class Writer {
public:
    /**
     * @brief Serializes t_value to a JSON string.
     * @tparam T A jeson struct, or any leaf/container type jeson supports.
     * @param t_value The object to serialize.
     * @param t_opts Formatting options; defaults to compact, null-writing output.
     * @return The serialized JSON text.
     */
    template <typename T>
    static std::string write(const T& t_value, WriteOptions t_opts = {}) {
        std::string out;
        writeValue(t_value, out, t_opts, 0);
        return out;
    }

private:
    /// Appends a newline + (t_depth * indentBy) spaces, if t_opts.pretty is set; a no-op otherwise.
    static void writeIndent(std::string& t_out, const WriteOptions& t_opts, unsigned t_depth) {
        if (!t_opts.pretty) {
            return;
        }
        t_out += '\n';
        t_out.append(static_cast<std::size_t>(t_depth) * t_opts.indentBy, ' ');
    }

    /// Appends t_in to t_out as a quoted, escaped JSON string literal.
    static void writeEscapedString(std::string_view t_in, std::string& t_out) {
        t_out += '"';
        for (char c : t_in) {
            switch (c) {
                case '"': t_out += "\\\""; break;
                case '\\': t_out += "\\\\"; break;
                case '\b': t_out += "\\b"; break;
                case '\f': t_out += "\\f"; break;
                case '\n': t_out += "\\n"; break;
                case '\r': t_out += "\\r"; break;
                case '\t': t_out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        constexpr char hex[] = "0123456789abcdef";
                        const auto uc = static_cast<unsigned char>(c);
                        t_out += "\\u00";
                        t_out += hex[(uc >> 4) & 0x0f];
                        t_out += hex[uc & 0x0f];
                    } else {
                        t_out += c;
                    }
            }
        }
        t_out += '"';
    }

    /**
     * @brief Appends t_value's decimal representation to t_out.
     *
     * Integers go through std::to_string (locale-independent per the
     * standard, no streams involved). Floating point goes through
     * snprintf ("%g") rather than std::to_chars: matches the parser's own
     * std::strtod choice for the same portability reason (not every
     * libstdc++ shipped by every ESP-IDF toolchain version has full
     * floating-point std::to_chars support yet) - both assume the process
     * never changes away from the "C" locale, which is already true of
     * strtod on the parse side, so this doesn't add a new assumption,
     * just stays consistent with it.
     *
     * uint8_t/int8_t are explicitly promoted before formatting: they're
     * typically char typedefs, and both std::to_string and iostream
     * treat char-sized types as a character to print, not a number - a
     * well-known footgun this sidesteps.
     */
    template <typename N>
    static void writeNumber(N t_value, std::string& t_out) {
        if constexpr (std::is_floating_point_v<N>) {
            char buf[64];
            int len = std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(t_value));
            t_out.append(buf, len > 0 ? static_cast<std::size_t>(len) : 0);
        } else if constexpr (sizeof(N) == 1 && std::is_integral_v<N>) {
            if constexpr (std::is_signed_v<N>) {
                t_out += std::to_string(static_cast<int>(t_value));
            } else {
                t_out += std::to_string(static_cast<unsigned>(t_value));
            }
        } else {
            t_out += std::to_string(t_value);
        }
    }

    /**
     * @brief Serializes one value into t_out, dispatching on T's decayed
     * type - the writer's mirror image of parser.hpp's parseValue().
     * @tparam T The static type being serialized (a Field<>, a leaf type,
     * or a struct).
     */
    template <typename T>
    static void writeValue(const T& t_value, std::string& t_out, const WriteOptions& t_opts, unsigned t_depth) {
        using D = std::decay_t<T>;

        if constexpr (internal::is_field_v<D>) {
            if constexpr (internal::is_reflectable_aggregate_v<typename D::ValueType>) {
                writeObject(t_value, t_out, t_opts, t_depth);
            } else {
                writeValue(t_value.value, t_out, t_opts, t_depth);
            }
        } else if constexpr (internal::is_required_v<D>) {
            writeValue(t_value.value, t_out, t_opts, t_depth);
        } else if constexpr (internal::is_patch_v<D>) {
            writeValue(t_value.value, t_out, t_opts, t_depth);
        } else if constexpr (internal::is_validator_v<D>) {
            writeValue(t_value.value(), t_out, t_opts, t_depth);
        } else if constexpr (std::is_same_v<D, bool>) {
            t_out += (t_value ? "true" : "false");
        } else if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>) {
            writeEscapedString(t_value, t_out);
        } else if constexpr (std::is_enum_v<D>) {
            // Same automatic-by-default logic as parser.hpp's mirror image.
            using EnumTable = std::conditional_t<EnumNames<D>::has_mapping, EnumNames<D>, internal::MagicEnumNames<D>>;
            for (const auto& [enumValue, name] : EnumTable::table) {
                if (!name.empty() && enumValue == t_value) {
                    writeEscapedString(name, t_out);
                    return;
                }
            }
            writeEscapedString("UNKNOWN", t_out);
        } else if constexpr (std::is_arithmetic_v<D>) {
            writeNumber(t_value, t_out);
        } else if constexpr (internal::is_optional_v<D>) {
            if (t_value.has_value()) {
                writeValue(*t_value, t_out, t_opts, t_depth);
            } else {
                t_out += "null";
            }
        } else if constexpr (internal::is_vector_v<D>) {
            if (t_value.empty()) {
                t_out += "[]";
                return;
            }
            t_out += '[';
            for (std::size_t i = 0; i < t_value.size(); ++i) {
                if (i != 0) {
                    t_out += ',';
                }
                writeIndent(t_out, t_opts, t_depth + 1);
                writeValue(t_value[i], t_out, t_opts, t_depth + 1);
            }
            writeIndent(t_out, t_opts, t_depth);
            t_out += ']';
        } else if constexpr (internal::is_reflectable_aggregate_v<D>) {
            writeObject(t_value, t_out, t_opts, t_depth);
        } else {
            static_assert(sizeof(D) == 0, "jeson: unsupported field type - add a branch in Writer::writeValue");
        }
    }

    /// Serializes t_value's reflected members as a JSON object.
    /// @tparam Struct A reflectable struct, or a Field<> wrapping one.
    template <typename Struct>
    static void writeObject(const Struct& t_value, std::string& t_out, const WriteOptions& t_opts, unsigned t_depth) {
        auto& mutableRef        = const_cast<Struct&>(t_value); // tie_members needs a non-const ref; we only ever read through it here
        auto members             = internal::tie_members(mutableRef);
        constexpr auto memberCnt = std::tuple_size_v<decltype(members)>;
        t_out += '{';
        bool first = true;
        writeMembers(members, t_out, t_opts, t_depth + 1, first, std::make_index_sequence<memberCnt>{});
        if (!first) {
            writeIndent(t_out, t_opts, t_depth);
        }
        t_out += '}';
    }

    /// Runs writeOne() over every member (via fold expression expansion).
    template <typename Members, std::size_t... I>
    static void writeMembers(Members& t_members, std::string& t_out, const WriteOptions& t_opts, unsigned t_depth, bool& t_first, std::index_sequence<I...>) {
        (writeOne(std::get<I>(t_members), t_out, t_opts, t_depth, t_first), ...);
    }

    /// Writes one "key": value pair (unless shouldOmit() says to skip it
    /// entirely), handling the leading comma via t_first.
    template <typename FieldRef>
    static void writeOne(FieldRef& t_member, std::string& t_out, const WriteOptions& t_opts, unsigned t_depth, bool& t_first) {
        using FieldType = std::decay_t<FieldRef>;
        if constexpr (!internal::is_field_v<FieldType>) {
            return; // shouldn't happen for well-formed jeson structs
        } else {
            if (shouldOmit(t_member, t_opts)) {
                return;
            }
            constexpr auto nm = FieldType::jesonFieldName_;
            if (!t_first) {
                t_out += ',';
            }
            t_first = false;
            writeIndent(t_out, t_opts, t_depth);
            writeEscapedString(nm.view(), t_out);
            t_out += ':';
            if (t_opts.pretty) {
                t_out += ' ';
            }
            writeValue(t_member, t_out, t_opts, t_depth);
        }
    }

    /// @return True if this member is std::optional<T>, currently unset,
    /// and t_opts.omitNulls is set - the one case where a member is
    /// skipped entirely rather than written as `null`.
    template <typename FieldType>
    static bool shouldOmit(const FieldType& t_member, const WriteOptions& t_opts) {
        if constexpr (internal::is_field_v<FieldType> && internal::is_optional_v<typename FieldType::ValueType>) {
            return t_opts.omitNulls && !t_member.value.has_value();
        } else {
            return false;
        }
    }
};

} // namespace jeson
