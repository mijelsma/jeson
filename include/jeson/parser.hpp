/**
 * @file parser.hpp
 * @brief Single-pass recursive-descent parser that decodes JSON directly
 * into reflected structs. No intermediate DOM/tree is built, and no field
 * registration macro is needed - member discovery is automatic (reflect.hpp).
 *
 * Layout: low-level cursor helpers (skipWs/consume/skipValue/skipString)
 * operate on the raw text; parseValue() dispatches per leaf/container/enum
 * type; parseObject() (plus dispatchKey/tryOne/checkRequired) handles
 * struct-shaped JSON objects specifically, including matching keys to
 * jeson::Field<> members by name and checking jeson::Required<T> fields
 * were all present.
 */
#pragma once

#include <cerrno>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "errors.hpp"
#include "field.hpp"
#include "patch.hpp"
#include "reflect.hpp"
#include "required.hpp"
#include "traits.hpp"
#include "validator.hpp"
#include "value.hpp"

namespace jeson {

/// The library's single-pass JSON parser. Construct with the JSON text,
/// then call parseInto() once with the (single) struct to decode into.
class Parser {
public:
    /// @param t_json The JSON text to parse. Must outlive this Parser -
    /// only a std::string_view over it is kept, nothing is copied.
    explicit Parser(std::string_view t_json) : m_s(t_json) {}

    /**
     * @brief Parses the JSON text into t_target.
     * @tparam T A jeson struct, or any leaf/container type jeson supports.
     * @param t_target The object to decode into. Only fields whose JSON
     * keys are present get overwritten; everything else keeps its current
     * value (its struct default, if t_target was freshly constructed).
     * @return Every error found (empty on success) - see errors.hpp.
     */
    template <typename T>
    ParseErrors parseInto(T& t_target) {
        ParseErrors errors;
        skipWs();
        parseValue<ObjectPolicy::NORMAL>(t_target, "", errors);
        skipWs();
        if (m_pos != m_s.size()) {
            errors.add("", "trailing data after JSON value");
        }
        return errors;
    }

private:
    enum class ObjectPolicy
    {
        NORMAL,
        PATCH_ROOT,
        COMPLETE,
        PARTIAL
    };

    std::string_view m_s;
    std::size_t m_pos = 0;

    // ---- low-level cursor helpers -----------------------------------
    // Operate directly on m_s/m_pos; none of these interpret JSON
    // structure beyond their own name (skipValue() is the exception -
    // it recursively skips one entire well-formed value of any shape,
    // used to discard unknown keys and to resynchronize after an error).

    /// Advances m_pos past any run of JSON whitespace (space/tab/CR/LF).
    void skipWs() {
        while (m_pos < m_s.size() && (m_s[m_pos] == ' ' || m_s[m_pos] == '\t' || m_s[m_pos] == '\n' || m_s[m_pos] == '\r')) {
            ++m_pos;
        }
    }

    /// @return The character at the cursor, or '\0' at end of input (never advances).
    [[nodiscard]] char peek() const { return m_pos < m_s.size() ? m_s[m_pos] : '\0'; }

    /// If the cursor is at t_c, advances past it and returns true; otherwise leaves the cursor untouched.
    bool consume(char t_c) {
        if (peek() == t_c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    /// If the text at the cursor matches t_lit exactly (e.g. "true"/"false"/"null"), advances past it and returns true.
    bool consumeLiteral(std::string_view t_lit) {
        if (m_s.substr(m_pos, t_lit.size()) == t_lit) {
            m_pos += t_lit.size();
            return true;
        }
        return false;
    }

    /// Skips over one complete, well-formed JSON value of any shape
    /// (object, array, string, number, or literal) without interpreting
    /// it. Used to discard unknown object keys, and to resynchronize the
    /// cursor after a type-mismatch error so one bad field doesn't corrupt
    /// parsing of the rest of the document.
    void skipValue() {
        skipWs();
        char c = peek();
        if (c == '{') {
            ++m_pos;
            skipWs();
            if (consume('}')) {
                return;
            }
            while (true) {
                skipWs();
                skipString();
                skipWs();
                consume(':');
                skipValue();
                skipWs();
                if (consume(',')) {
                    continue;
                }
                consume('}');
                break;
            }
        } else if (c == '[') {
            ++m_pos;
            skipWs();
            if (consume(']')) {
                return;
            }
            while (true) {
                skipValue();
                skipWs();
                if (consume(',')) {
                    continue;
                }
                consume(']');
                break;
            }
        } else if (c == '"') {
            skipString();
        } else if (consumeLiteral("true") || consumeLiteral("false") || consumeLiteral("null")) {
            // nothing else to do
        } else {
            while (m_pos < m_s.size() && (std::isdigit(static_cast<unsigned char>(m_s[m_pos])) || m_s[m_pos] == '-' || m_s[m_pos] == '+' || m_s[m_pos] == '.' || m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
                ++m_pos;
            }
        }
    }

    /// Skips over one JSON string literal, honoring backslash escapes so an
    /// escaped quote doesn't terminate the skip early.
    void skipString() {
        if (!consume('"')) {
            return;
        }
        while (m_pos < m_s.size() && m_s[m_pos] != '"') {
            if (m_s[m_pos] == '\\' && m_pos + 1 < m_s.size()) {
                ++m_pos;
            }
            ++m_pos;
        }
        consume('"');
    }

    /// Appends "/" + t_key to t_parent, building up a JSON-pointer-ish
    /// error path as parsing descends into nested objects/arrays.
    static std::string joinPath(const std::string& t_parent, std::string_view t_key) {
        std::string p = t_parent;
        p += '/';
        p += t_key;
        return p;
    }

    /// Checks the JSON number grammar before a standard-library conversion.
    /// This rejects C/C++ extensions such as a leading '+', leading zeroes,
    /// and incomplete fraction/exponent parts.
    static bool isValidJsonNumber(std::string_view t_text) {
        std::size_t i = 0;
        if (i < t_text.size() && t_text[i] == '-') {
            ++i;
        }
        if (i == t_text.size()) {
            return false;
        }

        if (t_text[i] == '0') {
            ++i;
            if (i < t_text.size() && std::isdigit(static_cast<unsigned char>(t_text[i]))) {
                return false;
            }
        } else if (t_text[i] >= '1' && t_text[i] <= '9') {
            do {
                ++i;
            } while (i < t_text.size() && std::isdigit(static_cast<unsigned char>(t_text[i])));
        } else {
            return false;
        }

        if (i < t_text.size() && t_text[i] == '.') {
            ++i;
            const std::size_t fractionStart = i;
            while (i < t_text.size() && std::isdigit(static_cast<unsigned char>(t_text[i]))) {
                ++i;
            }
            if (i == fractionStart) {
                return false;
            }
        }

        if (i < t_text.size() && (t_text[i] == 'e' || t_text[i] == 'E')) {
            ++i;
            if (i < t_text.size() && (t_text[i] == '+' || t_text[i] == '-')) {
                ++i;
            }
            const std::size_t exponentStart = i;
            while (i < t_text.size() && std::isdigit(static_cast<unsigned char>(t_text[i]))) {
                ++i;
            }
            if (i == exponentStart) {
                return false;
            }
        }

        return i == t_text.size();
    }

    static int hexValue(char t_c) {
        if (t_c >= '0' && t_c <= '9') {
            return t_c - '0';
        }
        if (t_c >= 'a' && t_c <= 'f') {
            return t_c - 'a' + 10;
        }
        if (t_c >= 'A' && t_c <= 'F') {
            return t_c - 'A' + 10;
        }
        return -1;
    }

    bool readHex4(std::size_t t_start, std::uint32_t& t_out) const {
        if (t_start + 4 > m_s.size()) {
            return false;
        }
        t_out = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const int digit = hexValue(m_s[t_start + i]);
            if (digit < 0) {
                return false;
            }
            t_out = (t_out << 4U) | static_cast<std::uint32_t>(digit);
        }
        return true;
    }

    static void appendUtf8(std::uint32_t t_codePoint, std::string& t_out) {
        if (t_codePoint <= 0x7f) {
            t_out += static_cast<char>(t_codePoint);
        } else if (t_codePoint <= 0x7ff) {
            t_out += static_cast<char>(0xc0 | (t_codePoint >> 6U));
            t_out += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        } else if (t_codePoint <= 0xffff) {
            t_out += static_cast<char>(0xe0 | (t_codePoint >> 12U));
            t_out += static_cast<char>(0x80 | ((t_codePoint >> 6U) & 0x3f));
            t_out += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        } else {
            t_out += static_cast<char>(0xf0 | (t_codePoint >> 18U));
            t_out += static_cast<char>(0x80 | ((t_codePoint >> 12U) & 0x3f));
            t_out += static_cast<char>(0x80 | ((t_codePoint >> 6U) & 0x3f));
            t_out += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        }
    }

    void parseDynamicValue(Value& t_target, const std::string& t_path, ParseErrors& t_errors) {
        skipWs();
        if (consumeLiteral("null")) {
            t_target = Value(nullptr);
            return;
        }
        if (consumeLiteral("true")) {
            t_target = Value(true);
            return;
        }
        if (consumeLiteral("false")) {
            t_target = Value(false);
            return;
        }
        if (peek() == '"') {
            std::string value;
            if (!parseRawString(value)) {
                t_errors.add(t_path, "invalid string");
                return;
            }
            t_target = Value(std::move(value));
            return;
        }
        if (consume('[')) {
            Value::Array values;
            skipWs();
            if (consume(']')) {
                t_target = Value(std::move(values));
                return;
            }
            std::size_t index = 0;
            while (true) {
                Value value;
                parseDynamicValue(value, t_path + "/" + std::to_string(index), t_errors);
                values.push_back(std::move(value));
                ++index;
                skipWs();
                if (consume(',')) {
                    skipWs();
                    if (peek() == ']') {
                        t_errors.add(t_path, "expected array value after ','");
                        consume(']');
                        break;
                    }
                    continue;
                }
                if (!consume(']')) {
                    t_errors.add(t_path, "expected ',' or ']'");
                }
                break;
            }
            t_target = Value(std::move(values));
            return;
        }
        if (consume('{')) {
            Value::Object values;
            skipWs();
            if (consume('}')) {
                t_target = Value(std::move(values));
                return;
            }
            while (true) {
                std::string key;
                if (!parseRawString(key)) {
                    t_errors.add(t_path, "expected object key");
                    break;
                }
                skipWs();
                if (!consume(':')) {
                    t_errors.add(t_path, "expected ':' after key '" + key + "'");
                    break;
                }
                skipWs();
                Value value;
                parseDynamicValue(value, joinPath(t_path, key), t_errors);
                values.emplace_back(std::move(key), std::move(value));
                skipWs();
                if (consume(',')) {
                    skipWs();
                    if (peek() == '}') {
                        t_errors.add(t_path, "expected object member after ','");
                        consume('}');
                        break;
                    }
                    continue;
                }
                if (!consume('}')) {
                    t_errors.add(t_path, "expected ',' or '}'");
                }
                break;
            }
            t_target = Value(std::move(values));
            return;
        }

        const std::size_t start = m_pos;
        while (m_pos < m_s.size() &&
               (std::isdigit(static_cast<unsigned char>(m_s[m_pos])) || m_s[m_pos] == '-' ||
                m_s[m_pos] == '+' || m_s[m_pos] == '.' || m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
            ++m_pos;
        }
        const std::string_view number = m_s.substr(start, m_pos - start);
        if (!isValidJsonNumber(number)) {
            t_errors.add(t_path, "expected JSON value");
            if (m_pos == start && m_pos < m_s.size()) {
                ++m_pos;
            }
            return;
        }

        if (number.find_first_of(".eE") != std::string_view::npos) {
            std::string buffer(number);
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(buffer.c_str(), &end);
            if (end != buffer.c_str() + buffer.size() || errno == ERANGE || !std::isfinite(value)) {
                t_errors.add(t_path, "invalid number");
                return;
            }
            t_target = Value(value);
        } else if (!number.empty() && number.front() == '-') {
            std::int64_t value = 0;
            const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
            if (result.ec != std::errc() || result.ptr != number.data() + number.size()) {
                t_errors.add(t_path, "invalid integer");
                return;
            }
            t_target = Value(value);
        } else {
            std::uint64_t value = 0;
            const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
            if (result.ec != std::errc() || result.ptr != number.data() + number.size()) {
                t_errors.add(t_path, "invalid integer");
                return;
            }
            t_target = Value(value);
        }
    }

    // ---- leaf parsers --------------------------------------------------

    /// Parses one JSON string literal (including escape decoding) into
    /// t_out. @return False if the cursor wasn't at an opening quote or the
    /// string was left unterminated; t_out is left cleared either way.
    bool parseRawString(std::string& t_out) {
        if (!consume('"')) {
            return false;
        }
        t_out.clear();
        while (m_pos < m_s.size() && m_s[m_pos] != '"') {
            char c = m_s[m_pos];
            if (c == '\\' && m_pos + 1 < m_s.size()) {
                ++m_pos;
                char esc = m_s[m_pos];
                switch (esc) {
                    case 'b': t_out += '\b'; break;
                    case 'f': t_out += '\f'; break;
                    case 'n': t_out += '\n'; break;
                    case 't': t_out += '\t'; break;
                    case 'r': t_out += '\r'; break;
                    case '"': t_out += '"'; break;
                    case '\\': t_out += '\\'; break;
                    case '/': t_out += '/'; break;
                    case 'u': {
                        std::uint32_t codePoint = 0;
                        if (!readHex4(m_pos + 1, codePoint)) {
                            return false;
                        }
                        if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                            if (m_pos + 10 >= m_s.size() || m_s[m_pos + 5] != '\\' || m_s[m_pos + 6] != 'u') {
                                return false;
                            }
                            std::uint32_t low = 0;
                            if (!readHex4(m_pos + 7, low) || low < 0xdc00 || low > 0xdfff) {
                                return false;
                            }
                            codePoint = 0x10000 + ((codePoint - 0xd800) << 10U) + (low - 0xdc00);
                            m_pos += 10;
                        } else {
                            if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                                return false;
                            }
                            m_pos += 4;
                        }
                        appendUtf8(codePoint, t_out);
                        break;
                    }
                    default: return false;
                }
            } else {
                if (static_cast<unsigned char>(c) < 0x20) {
                    return false;
                }
                t_out += c;
            }
            ++m_pos;
        }
        return consume('"');
    }

    /**
     * @brief Decodes one JSON value into t_target, dispatching on T's
     * decayed type. This is the parser's core: every other case (leaf
     * value, jeson::Required<T>/Validator<T,...>, enum, std::optional<T>,
     * std::vector<T>, or a whole reflectable struct) recurses back through
     * here, so the entire type-shape traversal lives in this one function.
     * @tparam T The static type being decoded into (a Field<>, a leaf
     * type, or a struct - see the @file comment above for the dispatch order).
     * @param t_target The object to decode into.
     * @param t_path This value's JSON-pointer-ish path, for error reporting.
     * @param t_errors Errors found while decoding are appended here; parsing
     * continues past a single bad value rather than aborting the whole document.
     */
    template <ObjectPolicy Policy, typename T>
    void parseValue(T& t_target, const std::string& t_path, ParseErrors& t_errors) {
        using D = std::decay_t<T>;

        if constexpr (std::is_same_v<D, Value>) {
            parseDynamicValue(t_target, t_path, t_errors);
        } else if constexpr (internal::is_field_v<D>) {
            if constexpr (internal::is_reflectable_aggregate_v<typename D::ValueType>) {
                // Field<Name, Struct, true> - publicly inherits Struct, so
                // t_target itself decomposes correctly (reflect.hpp peels
                // the wrapper via JesonReflectAs).
                parseObject<Policy>(t_target, t_path, t_errors);
            } else {
                // Field<Name, T, false> - leaf, peel .value and recurse.
                parseValue<Policy>(t_target.value, t_path, t_errors);
            }
        } else if constexpr (internal::is_required_v<D>) {
            parseValue<Policy>(t_target.value, t_path, t_errors);
        } else if constexpr (internal::is_patch_v<D>) {
            if (!t_target.hasBaseline()) {
                t_errors.add(t_path, "patch requires a baseline object");
            }
            parseValue<ObjectPolicy::PATCH_ROOT>(t_target.value, t_path, t_errors);
        } else if constexpr (internal::is_validator_v<D>) {
            typename D::ValueType raw{};
            parseValue<Policy>(raw, t_path, t_errors);
            if (auto err = t_target.set(raw)) {
                t_errors.add(t_path, *err);
            }
        } else if constexpr (std::is_same_v<D, bool>) {
            if (consumeLiteral("true")) {
                t_target = true;
            } else if (consumeLiteral("false")) {
                t_target = false;
            } else {
                t_errors.add(t_path, "expected boolean");
                skipValue();
            }
        } else if constexpr (std::is_same_v<D, std::string>) {
            std::string tmp;
            if (peek() != '"' || !parseRawString(tmp)) {
                t_errors.add(t_path, "expected string");
                skipValue();
                return;
            }
            t_target = std::move(tmp);
        } else if constexpr (std::is_enum_v<D>) {
            // Every plain enum is handled automatically here, with zero
            // setup - see enum.hpp's @file comment. EnumNames<D>::table is
            // either an explicit override (custom names, or a widened
            // probe range for a big enum) or, for the overwhelming common
            // case, the automatic magic-reflection table computed with no
            // annotation required at all.
            using EnumTable = std::conditional_t<EnumNames<D>::has_mapping, EnumNames<D>, internal::MagicEnumNames<D>>;
            std::string tmp;
            if (peek() != '"' || !parseRawString(tmp)) {
                t_errors.add(t_path, "expected string (enum)");
                skipValue();
                return;
            }
            bool found = false;
            for (const auto& [enumValue, name] : EnumTable::table) {
                if (!name.empty() && name == tmp) {
                    t_target = enumValue;
                    found     = true;
                    break;
                }
            }
            if (!found) {
                t_errors.add(t_path, "unrecognized enum value '" + tmp + "'");
            }
        } else if constexpr (std::is_arithmetic_v<D>) {
            std::size_t start = m_pos;
            while (m_pos < m_s.size() && (std::isdigit(static_cast<unsigned char>(m_s[m_pos])) || m_s[m_pos] == '-' || m_s[m_pos] == '+' || m_s[m_pos] == '.' || m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
                ++m_pos;
            }
            std::string_view numText = m_s.substr(start, m_pos - start);
            if (numText.empty()) {
                t_errors.add(t_path, "expected number");
                skipValue(); // consume the malformed token so the enclosing object/array can resync at the next ',' or closing bracket
                return;
            }
            if (!isValidJsonNumber(numText)) {
                t_errors.add(t_path, "invalid number");
                return;
            }
            if constexpr (std::is_integral_v<D>) {
                D value{};
                auto res        = std::from_chars(numText.data(), numText.data() + numText.size(), value);
                if (res.ec != std::errc() || res.ptr != numText.data() + numText.size()) {
                    t_errors.add(t_path, "invalid integer");
                } else {
                    t_target = value;
                }
            } else {
                char* endPtr = nullptr;
                std::string buf(numText);
                errno        = 0;
                double value = std::strtod(buf.c_str(), &endPtr);
                const D converted = static_cast<D>(value);
                if (endPtr != buf.c_str() + buf.size() || errno == ERANGE || !std::isfinite(converted)) {
                    t_errors.add(t_path, "invalid number");
                } else {
                    t_target = converted;
                }
            }
        } else if constexpr (internal::is_optional_v<D>) {
            skipWs();
            if (consumeLiteral("null")) {
                t_target = std::nullopt;
                return;
            }
            if constexpr (Policy == ObjectPolicy::PARTIAL) {
                if (!t_target.has_value()) {
                    t_target.emplace();
                }
                parseValue<Policy>(*t_target, t_path, t_errors);
            } else {
                typename D::value_type value{};
                parseValue<Policy>(value, t_path, t_errors);
                t_target = std::move(value);
            }
        } else if constexpr (internal::is_vector_v<D>) {
            skipWs();
            if (!consume('[')) {
                t_errors.add(t_path, "expected array");
                skipValue();
                return;
            }
            t_target.clear();
            skipWs();
            if (consume(']')) {
                return;
            }
            std::size_t index = 0;
            while (true) {
                skipWs();
                typename D::value_type element{};
                parseValue<Policy>(element, t_path + "/" + std::to_string(index), t_errors);
                t_target.push_back(std::move(element));
                ++index;
                skipWs();
                if (consume(',')) {
                    continue;
                }
                if (!consume(']')) {
                    t_errors.add(t_path, "expected ',' or ']'");
                }
                break;
            }
        } else if constexpr (internal::is_reflectable_aggregate_v<D>) {
            // A raw (unwrapped) reflectable struct - the top-level parse
            // target, or an element of std::vector<SomeStruct>.
            parseObject<Policy>(t_target, t_path, t_errors);
        } else {
            static_assert(sizeof(D) == 0, "jeson: unsupported field type - add a branch in Parser::parseValue");
        }
    }

    /**
     * @brief Decodes a JSON object into t_target, matching each key against
     * t_target's reflected members by name (see reflect.hpp) and recursing
     * into parseValue() for each matched value. Unknown keys are skipped
     * (lenient - old persisted configs with since-removed fields still
     * load); jeson::Required<T> members not seen by the end are reported
     * as missing (checkRequired()).
     * @tparam Struct A reflectable struct, or a Field<> wrapping one.
     */
    template <ObjectPolicy Policy, typename Struct>
    void parseObject(Struct& t_target, const std::string& t_path, ParseErrors& t_errors) {
        skipWs();
        if (!consume('{')) {
            t_errors.add(t_path, "expected object");
            skipValue();
            return;
        }

        auto members             = internal::tie_members(t_target);
        constexpr auto memberCnt = std::tuple_size_v<decltype(members)>;
        bool found[memberCnt == 0 ? 1 : memberCnt] = {};

        if constexpr (Policy == ObjectPolicy::PATCH_ROOT) {
            clearPatchRoot(members, std::make_index_sequence<memberCnt>{});
        }

        skipWs();
        if (!consume('}')) {
            while (true) {
                skipWs();
                std::string key;
                if (!parseRawString(key)) {
                    t_errors.add(t_path, "expected object key");
                    break;
                }
                skipWs();
                if (!consume(':')) {
                    t_errors.add(t_path, "expected ':' after key '" + key + "'");
                    break;
                }
                skipWs();

                bool matched = dispatchKey<Policy>(members, key, joinPath(t_path, key), t_errors, found, std::make_index_sequence<memberCnt>{});
                if (!matched) {
                    // Unknown key: ignore its value (lenient, so old configs
                    // with fields removed in a newer firmware still load).
                    skipValue();
                }

                skipWs();
                if (consume(',')) {
                    continue;
                }
                if (!consume('}')) {
                    t_errors.add(t_path, "expected ',' or '}'");
                }
                break;
            }
        }

        checkRequired(members, t_path, t_errors, found, std::make_index_sequence<memberCnt>{});
        if constexpr (Policy == ObjectPolicy::PATCH_ROOT) {
            checkPatchRoot(members, t_path, t_errors, found, std::make_index_sequence<memberCnt>{});
        } else if constexpr (Policy == ObjectPolicy::COMPLETE) {
            checkComplete(members, t_path, t_errors, found, std::make_index_sequence<memberCnt>{});
        }
    }

    /// Tries t_key against every member in t_members (via the fold
    /// expression's expansion of tryOne() over index sequence I...),
    /// stopping at the first match. @return True if some member matched.
    template <ObjectPolicy Policy, typename Members, std::size_t... I>
    bool dispatchKey(Members& t_members, const std::string& t_key, const std::string& t_fieldPath, ParseErrors& t_errors, bool* t_found, std::index_sequence<I...>) {
        bool matched = false;
        (tryOne<Policy, I>(t_members, t_key, t_fieldPath, t_errors, t_found, matched), ...);
        return matched;
    }

    /// Checks member I's name against t_key; if it matches, marks it found
    /// and parses its value. A no-op once t_matched is already true (so the
    /// fold expression in dispatchKey() short-circuits after the first hit
    /// without needing an early-exit-capable loop construct).
    template <ObjectPolicy Policy, std::size_t I, typename Members>
    void tryOne(Members& t_members, const std::string& t_key, const std::string& t_fieldPath, ParseErrors& t_errors, bool* t_found, bool& t_matched) {
        if (t_matched) {
            return;
        }
        auto& m         = std::get<I>(t_members);
        using FieldType = std::decay_t<decltype(m)>;
        if constexpr (internal::is_field_v<FieldType>) {
            constexpr auto nm = FieldType::jesonFieldName_;
            if (nm.view() == t_key) {
                t_matched  = true;
                t_found[I] = true;
                constexpr ObjectPolicy memberPolicy = []() constexpr {
                    if constexpr (Policy == ObjectPolicy::PATCH_ROOT) {
                        using SectionType = internal::patch_section_type_t<typename FieldType::ValueType>;
                        if constexpr (internal::patch_section_policy_v<SectionType> == PatchSectionPolicy::PARTIAL) {
                            return ObjectPolicy::PARTIAL;
                        }
                        return ObjectPolicy::COMPLETE;
                    }
                    return Policy;
                }();
                parseValue<memberPolicy>(m, t_fieldPath, t_errors);
            }
        }
    }

    /// Runs checkRequiredOne() over every member (via fold expression
    /// expansion), reporting a "missing required field" error for each
    /// jeson::Required<T> member whose key was never seen.
    template <typename Members, std::size_t... I>
    void checkRequired(Members& t_members, const std::string& t_path, ParseErrors& t_errors, const bool* t_found, std::index_sequence<I...>) {
        (checkRequiredOne(std::get<I>(t_members), t_path, t_errors, t_found[I]), ...);
    }

    /// A no-op unless this member's ValueType is jeson::Required<T> and it
    /// wasn't found in the input, in which case a "missing required field"
    /// error is added.
    template <typename FieldRef>
    void checkRequiredOne(FieldRef& /*t_member*/, const std::string& t_path, ParseErrors& t_errors, bool t_found) {
        using FieldType = std::decay_t<FieldRef>;
        if constexpr (internal::is_field_v<FieldType>) {
            using ValueType = typename FieldType::ValueType;
            if constexpr (internal::is_required_v<ValueType>) {
                if (!t_found) {
                    constexpr auto nm = FieldType::jesonFieldName_;
                    t_errors.add(joinPath(t_path, nm.view()), "missing required field");
                }
            }
        }
    }

    /// Clears all direct optional members before parsing a Patch<T>, so
    /// omitted sections remain distinguishable from supplied sections.
    template <typename Members, std::size_t... I>
    void clearPatchRoot(Members& t_members, std::index_sequence<I...>) {
        (clearPatchRootOne(std::get<I>(t_members)), ...);
    }

    template <typename FieldRef>
    void clearPatchRootOne(FieldRef& t_member) {
        using FieldType = std::decay_t<FieldRef>;
        static_assert(internal::is_field_v<FieldType>, "jeson::Patch<T> requires T to contain only jeson::Field members");
        using ValueType = typename FieldType::ValueType;
        static_assert(internal::is_optional_v<ValueType>, "Direct jeson::Patch<T> members must be std::optional");
        using SectionType = internal::patch_section_type_t<ValueType>;
        if constexpr (internal::patch_section_policy_v<SectionType> == PatchSectionPolicy::COMPLETE) {
            t_member.value = std::nullopt;
        }
    }

    /// Rejects an explicitly supplied null section. Omitted sections were
    /// cleared above and are intentionally accepted by the patch root.
    template <typename Members, std::size_t... I>
    void checkPatchRoot(Members& t_members, const std::string& t_path, ParseErrors& t_errors, const bool* t_found, std::index_sequence<I...>) {
        (checkPatchRootOne(std::get<I>(t_members), t_path, t_errors, t_found[I]), ...);
    }

    template <typename FieldRef>
    void checkPatchRootOne(FieldRef& t_member, const std::string& t_path, ParseErrors& t_errors, bool t_found) {
        using FieldType = std::decay_t<FieldRef>;
        using SectionType = internal::patch_section_type_t<typename FieldType::ValueType>;
        if (!t_found) {
            if constexpr (internal::patch_section_policy_v<SectionType> == PatchSectionPolicy::PARTIAL) {
                t_member.value = std::nullopt;
            }
        } else if (!t_member.value.has_value()) {
            constexpr auto name = FieldType::jesonFieldName_;
            t_errors.add(joinPath(t_path, name.view()), "expected complete object, got null");
        }
    }

    /// Requires every field in a supplied section, without needing a second
    /// schema made from Required<T> wrappers.
    template <typename Members, std::size_t... I>
    void checkComplete(Members& t_members, const std::string& t_path, ParseErrors& t_errors, const bool* t_found, std::index_sequence<I...>) {
        (checkCompleteOne(std::get<I>(t_members), t_path, t_errors, t_found[I]), ...);
    }

    template <typename FieldRef>
    void checkCompleteOne(FieldRef& /*t_member*/, const std::string& t_path, ParseErrors& t_errors, bool t_found) {
        using FieldType = std::decay_t<FieldRef>;
        if constexpr (internal::is_field_v<FieldType>) {
            using ValueType = typename FieldType::ValueType;
            if constexpr (!internal::is_required_v<ValueType>) {
                if (!t_found) {
                    constexpr auto name = FieldType::jesonFieldName_;
                    t_errors.add(joinPath(t_path, name.view()), "missing required field");
                }
            }
        }
    }
};

} // namespace jeson
